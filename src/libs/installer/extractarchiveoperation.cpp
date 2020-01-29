/**************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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

#include <QEventLoop>
#include <QThreadPool>
#include <QFileInfo>
#include <QDataStream>

namespace QInstaller {

ExtractArchiveOperation::ExtractArchiveOperation(PackageManagerCore *core)
    : UpdateOperation(core)
{
    setName(QLatin1String("Extract"));
}

void ExtractArchiveOperation::backup()
{
    // we need to backup on the fly...
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

    if (PackageManagerCore *core = packageManager()) {
        connect(core, &PackageManagerCore::statusChanged, &callback, &Callback::statusChanged);
    }

    Runnable *runnable = new Runnable(archivePath, targetDir, &callback);
    connect(runnable, &Runnable::finished, &receiver, &Receiver::runnableFinished,
        Qt::QueuedConnection);

    QFileInfo fileInfo(archivePath);
    emit outputTextChanged(tr("Extracting \"%1\"").arg(fileInfo.fileName()));

    QEventLoop loop;
    connect(&receiver, &Receiver::finished, &loop, &QEventLoop::quit);
    if (QThreadPool::globalInstance()->tryStart(runnable)) {
        loop.exec();
    } else {
        // HACK: In case there is no availabe thread we should call it directly.
        runnable->run();
        receiver.runnableFinished(true, QString());
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

    QString fileDirectory = targetDir + QLatin1String("/installerResources/") +
            archivePath.section(QLatin1Char('/'), 1, 1, QString::SectionSkipEmpty) + QLatin1Char('/');
    QString archiveFileName = archivePath.section(QLatin1Char('/'), 2, 2, QString::SectionSkipEmpty);
    QFileInfo fileInfo2(archiveFileName);
    QString suffix = fileInfo2.suffix();
    archiveFileName.chop(suffix.length() + 1); // removes suffix (e.g. '.7z') from archive filename
    QString fileName = archiveFileName + QLatin1String(".txt");

    QFileInfo targetDirectoryInfo(fileDirectory);

    QDir dir(targetDirectoryInfo.absolutePath());
    if (!dir.exists()) {
        dir.mkpath(targetDirectoryInfo.absolutePath());
    }
    QFile file(targetDirectoryInfo.absolutePath() + QLatin1Char('/') + fileName);
    if (file.open(QIODevice::WriteOnly)) {
        setDefaultFilePermissions(file.fileName(), DefaultFilePermissions::NonExecutable);
        QDataStream out (&file);
        for (int i = 0; i < files.count(); ++i) {
            files[i] = replacePath(files.at(i), targetDir, QLatin1String(scRelocatable));
        }
        out << files;
        setValue(QLatin1String("files"), file.fileName());
        file.close();
    } else {
        qWarning() << "Cannot open file for writing " << file.fileName() << ":" << file.errorString();
    }

    // TODO: Use backups for rollback, too? Doesn't work for uninstallation though.

    // delete all backups we can delete right now, remember the rest
    foreach (const Backup &i, callback.backupFiles())
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
    QStringList files;
    if (useStringListType) {
        files = value(QLatin1String("files")).toStringList();
    } else {
        if (!readDataFileContents(targetDir, &files))
            return false;
    }
    startUndoProcess(files);
    if (!useStringListType)
        deleteDataFile(m_relocatedDataFileName);

    return true;
}

void ExtractArchiveOperation::startUndoProcess(const QStringList &files)
{
    WorkerThread *const thread = new WorkerThread(this, files);
    connect(thread, &WorkerThread::currentFileChanged, this,
        &ExtractArchiveOperation::outputTextChanged);
    connect(thread, &WorkerThread::progressChanged, this,
        &ExtractArchiveOperation::progressChanged);

    QEventLoop loop;
    connect(thread, &QThread::finished, &loop, &QEventLoop::quit, Qt::QueuedConnection);
    thread->start();
    loop.exec();
    thread->deleteLater();
}

void ExtractArchiveOperation::deleteDataFile(const QString &fileName)
{
    if (fileName.isEmpty()) {
        qWarning() << Q_FUNC_INFO << "data file name cannot be empty.";
        return;
    }
    QFile file(fileName);
    QFileInfo fileInfo(file);
    if (file.remove()) {
        QDir directory(fileInfo.absoluteDir());
        if (directory.exists() && directory.isEmpty())
            directory.rmdir(directory.path());
    } else {
        qWarning() << "Cannot remove data file" << file.fileName();
    }
}

bool ExtractArchiveOperation::testOperation()
{
    return true;
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
        qWarning() << "Cannot open file " << file.fileName() << " for reading:"
                << file.errorString() << ". Component is already uninstalled "
                << "or file is manually deleted.";
    }
    return true;
}

} // namespace QInstaller
