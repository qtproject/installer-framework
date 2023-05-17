/**************************************************************************
**
** Copyright (C) 2023 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
**************************************************************************/

#include "extractarchiveoperation_p.h"

#include "constants.h"
#include "globals.h"
#include "fileguard.h"

#include <QEventLoop>
#include <QThreadPool>
#include <QFileInfo>
#include <QDataStream>

namespace QInstaller {

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::ExtractArchiveOperation
    \internal
*/

/*!
    \typedef QInstaller::ExtractArchiveOperation::Backup

    Synonym for QPair<QString, QString>. Contains a pair
    of an original and a generated backup filename for a file.
*/

/*!
    \typedef QInstaller::ExtractArchiveOperation::BackupFiles

    Synonym for QVector<Backup>.
*/

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::WorkerThread
    \internal
*/

ExtractArchiveOperation::ExtractArchiveOperation(PackageManagerCore *core)
    : UpdateOperation(core)
    , m_totalEntries(0)
{
    setName(QLatin1String("Extract"));
    setGroup(OperationGroup::Unpack);
}

void ExtractArchiveOperation::backup()
{
    if (!checkArgumentCount(2))
        return;

    const QStringList args = arguments();
    const QString archivePath = args.at(0);
    const QString targetDir = args.at(1);

    QScopedPointer<AbstractArchive> archive(ArchiveFactory::instance().create(archivePath));
    if (!archive) {
        setError(UserDefinedError);
        setErrorString(tr("Unsupported archive \"%1\": no handler registered for file suffix \"%2\".")
            .arg(archivePath, QFileInfo(archivePath).suffix()));
        return;
    }

    if (!(archive->open(QIODevice::ReadOnly) && archive->isSupported())) {
        setError(UserDefinedError);
        setErrorString(tr("Cannot open archive \"%1\" for reading: %2")
            .arg(archivePath, archive->errorString()));
        return;
    }
    const QVector<ArchiveEntry> entries = archive->list();
    if (entries.isEmpty()) {
        setError(UserDefinedError);
        setErrorString(tr("Error while reading contents of archive \"%1\": %2")
            .arg(archivePath, archive->errorString()));
        return;
    }

    const bool hasAdminRights = (packageManager() && packageManager()->hasAdminRights());
    const bool canCreateSymLinks = QInstaller::canCreateSymbolicLinks();
    bool needsAdminRights = false;

    for (auto &entry : entries) {
        const QString completeFilePath = targetDir + QDir::separator() + entry.path;
        if (!entry.isDirectory) {
            // Ignore failed backups, existing files are overwritten when extracting.
            // Should the backups be used on rollback too, this may not be the
            // desired behavior anymore.
            prepareForFile(completeFilePath);
        }
        if (!hasAdminRights && !canCreateSymLinks && entry.isSymbolicLink)
            needsAdminRights = true;
    }
    m_totalEntries = entries.size();
    if (needsAdminRights)
        setValue(QLatin1String("admin"), true);

    // Show something was done
    emit progressChanged(scBackupProgressPart);
}

bool ExtractArchiveOperation::performOperation()
{
    if (!checkArgumentCount(2))
        return false;

    const QStringList args = arguments();
    const QString archivePath = args.at(0);
    const QString targetDir = args.at(1);

    Receiver receiver;
    Callback callback;

    connect(&callback, &Callback::progressChanged, this, &ExtractArchiveOperation::progressChanged);

    Worker *worker = new Worker(archivePath, targetDir, m_totalEntries, &callback);
    connect(worker, &Worker::finished, &receiver, &Receiver::workerFinished,
        Qt::QueuedConnection);

    if (PackageManagerCore *core = packageManager())
        connect(core, &PackageManagerCore::statusChanged, worker, &Worker::onStatusChanged);

    QFileInfo fileInfo(archivePath);
    emit outputTextChanged(tr("Extracting \"%1\"").arg(fileInfo.fileName()));
    {
        QEventLoop loop;
        QThread workerThread;
        worker->moveToThread(&workerThread);

        connect(&workerThread, &QThread::started, worker, &Worker::run);
        connect(&receiver, &Receiver::finished, &workerThread, &QThread::quit);
        connect(&workerThread, &QThread::finished, worker, &QObject::deleteLater);
        connect(&workerThread, &QThread::finished, &loop, &QEventLoop::quit);

        workerThread.start();
        loop.exec();
    }
    // Write all file names which belongs to a package to a separate file and only the separate
    // filename to a .dat file. There can be enormous amount of files in a package, which makes
    // the dat file very slow to read and write. The .dat file is read into memory in startup,
    // writing the file names to a separate file we don't need to load all the file names into
    // memory as we need those only in uninstall. This will save a lot of memory.
    // Parse a file and directorory structure using archivepath syntax
    // installer://<component_name>/<filename>.7z Resulting structure is:
    // -installerResources (dir)
    //   -<component_name> (dir)
    //    -<filename>.txt (file)

    QStringList files = callback.extractedFiles();

    QString installDir = targetDir;
    // If we have package manager in use (normal installer run) then use
    // TargetDir for saving filenames, otherwise those would be saved to
    // extracted folder. Also initialize installerbasebinary which we use later
    // to check if the extracted file in question is the maintenancetool itself.
    QString installerBaseBinary;
    if (PackageManagerCore *core = packageManager()) {
        installDir = core->value(scTargetDir);
        installerBaseBinary = core->toNativeSeparators(core->replaceVariables(core->installerBaseBinary()));
    }
    const QString resourcesPath = installDir + QLatin1Char('/') + QLatin1String("installerResources");

    QString fileDirectory = resourcesPath + QLatin1Char('/') + archivePath.section(QLatin1Char('/'), 1, 1,
                            QString::SectionSkipEmpty) + QLatin1Char('/');
    QString archiveFileName = archivePath.section(QLatin1Char('/'), 2, 2, QString::SectionSkipEmpty);
    QFileInfo fileInfo2(archiveFileName);
    QString suffix = fileInfo2.suffix();
    archiveFileName.chop(suffix.length() + 1); // removes suffix (e.g. '.7z') from archive filename
    QString fileName = archiveFileName + QLatin1String(".txt");

    QFileInfo targetDirectoryInfo(fileDirectory);

    QInstaller::createDirectoryWithParents(targetDirectoryInfo.absolutePath());
    setDefaultFilePermissions(resourcesPath, DefaultFilePermissions::Executable);
    setDefaultFilePermissions(targetDirectoryInfo.absolutePath(), DefaultFilePermissions::Executable);

    QFile file(targetDirectoryInfo.absolutePath() + QLatin1Char('/') + fileName);
    if (file.open(QIODevice::WriteOnly)) {
        setDefaultFilePermissions(file.fileName(), DefaultFilePermissions::NonExecutable);
        QDataStream out (&file);
        for (int i = 0; i < files.count(); ++i) {
            if (!installerBaseBinary.isEmpty() && files[i].startsWith(installerBaseBinary)) {
                // Do not write installerbase binary filename to extracted files. Installer binary
                // is maintenance tool program, the binary is removed elsewhere
                // when we do full uninstall.
                files.clear();
                break;
            }
            files[i] = replacePath(files.at(i), installDir, QLatin1String(scRelocatable));
        }
        if (!files.isEmpty())
            out << files;
        setValue(QLatin1String("files"), file.fileName());
        file.close();
    } else {
        qCWarning(QInstaller::lcInstallerInstallLog) << "Cannot open file for writing " << file.fileName() << ":" << file.errorString();
    }

    // TODO: Use backups for rollback, too? Doesn't work for uninstallation though.

    // delete all backups we can delete right now, remember the rest
    foreach (const Backup &i, m_backupFiles)
        deleteFileNowOrLater(i.second);

    if (!receiver.success()) {
        setError(UserDefinedError);
        setErrorString(receiver.errorString());
        return false;
    }
    return true;
}

bool ExtractArchiveOperation::undoOperation()
{
    Q_ASSERT(arguments().count() == 2);

    // For backward compatibility, check if "files" can be converted to QStringList.
    // If yes, files are listed in .dat instead of in a separate file.
    bool useStringListType(value(QLatin1String("files")).type() == QVariant::StringList);
    QString targetDir = arguments().at(1);
    if (packageManager())
        targetDir = packageManager()->value(scTargetDir);
    QStringList files;
    if (useStringListType) {
        files = value(QLatin1String("files")).toStringList();
    } else {
        if (!readDataFileContents(targetDir, &files))
            return false;
    }
    if (!files.isEmpty())
        startUndoProcess(files);
    if (!useStringListType)
        deleteDataFile(m_relocatedDataFileName);

    // Remove the installerResources directory if it is empty.
    QDir(targetDir).rmdir(QLatin1String("installerResources"));

    return true;
}

void ExtractArchiveOperation::startUndoProcess(const QStringList &files)
{
    WorkerThread *const thread = new WorkerThread(this, files);
    connect(thread, &WorkerThread::currentFileChanged, this,
        &ExtractArchiveOperation::outputTextChanged);
    connect(thread, &WorkerThread::progressChanged, this,
        &ExtractArchiveOperation::progressChanged);

    const QFileInfo archive(arguments().at(0));
    emit outputTextChanged(tr("Removing files extracted from \"%1\"").arg(archive.fileName()));

    QEventLoop loop;
    connect(thread, &QThread::finished, &loop, &QEventLoop::quit, Qt::QueuedConnection);
    thread->start();
    loop.exec();
    thread->deleteLater();
}

void ExtractArchiveOperation::deleteDataFile(const QString &fileName)
{
    if (fileName.isEmpty()) {
        qCWarning(QInstaller::lcInstallerInstallLog) << Q_FUNC_INFO << "data file name cannot be empty.";
        return;
    }
    QFile file(fileName);
    QFileInfo fileInfo(file);
    if (file.remove()) {
        QDir directory(fileInfo.absoluteDir());
        if (directory.exists() && directory.isEmpty())
            directory.rmdir(directory.path());
    } else {
        qCWarning(QInstaller::lcInstallerInstallLog) << "Cannot remove data file" << file.fileName();
    }
}

QString ExtractArchiveOperation::generateBackupName(const QString &fn)
{
    const QString bfn = fn + QLatin1String(".tmpUpdate");
    QString res = bfn;
    int i = 0;
    while (QFile::exists(res))
        res = bfn + QString::fromLatin1(".%1").arg(i++);
    return res;
}

bool ExtractArchiveOperation::prepareForFile(const QString &filename)
{
    if (!QFile::exists(filename))
        return true;

    FileGuardLocker locker(filename, FileGuard::globalObject());

    const QString backup = generateBackupName(filename);
    QFile f(filename);
    const bool renamed = f.rename(backup);
    if (f.exists() && !renamed) {
        qCritical("Cannot rename %s to %s: %s", qPrintable(filename), qPrintable(backup),
                  qPrintable(f.errorString()));
        return false;
    }
    m_backupFiles.append(qMakePair(filename, backup));
    return true;
}

bool ExtractArchiveOperation::testOperation()
{
    return true;
}

quint64 ExtractArchiveOperation::sizeHint()
{
    if (!checkArgumentCount(2))
        return UpdateOperation::sizeHint();

    if (hasValue(QLatin1String("sizeHint")))
        return value(QLatin1String("sizeHint")).toULongLong();

    const QString archivePath = arguments().at(0);
    const quint64 compressedSize = QFileInfo(archivePath).size();

    setValue(QLatin1String("sizeHint"), QString::number(compressedSize));

    // A rough estimate of how much time it takes to extract this archive. Other
    // affecting parameters are the archive format, compression filter and -level.
    return compressedSize;
}

bool ExtractArchiveOperation::readDataFileContents(QString &targetDir, QStringList *resultList)
{
    const QString filePath = value(QLatin1String("files")).toString();
    // Does not change target on non macOS platforms.
    if (QInstaller::isInBundle(targetDir, &targetDir))
        targetDir = QDir::cleanPath(targetDir + QLatin1String("/.."));
    m_relocatedDataFileName = replacePath(filePath, QLatin1String(scRelocatable), targetDir);
    QFile file(m_relocatedDataFileName);

    if (file.open(QIODevice::ReadOnly)) {
        QDataStream in(&file);
        in >> *resultList;
        for (int i = 0; i < resultList->count(); ++i)
            resultList->replace(i, replacePath(resultList->at(i),  QLatin1String(scRelocatable), targetDir));

    } else {
        // We should not be here. Either user has manually deleted the installer related
        // files or same component is installed several times.
        qCWarning(QInstaller::lcInstallerInstallLog) << "Cannot open file " << file.fileName() << " for reading:"
                << file.errorString() << ". Component is already uninstalled "
                << "or file is manually deleted.";
    }
    return true;
}

} // namespace QInstaller
