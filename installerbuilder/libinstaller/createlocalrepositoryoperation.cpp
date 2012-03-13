/**************************************************************************
**
** This file is part of Installer Framework
**
** Copyright (c) 2012 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (qt-info@nokia.com)
**
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** Other Usage
**
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
** If you have questions regarding the use of this file, please contact
** Nokia at qt-info@nokia.com.
**
**************************************************************************/
#include "createlocalrepositoryoperation.h"

#include "binaryformat.h"
#include "errors.h"
#include "fileutils.h"
#include "copydirectoryoperation.h"
#include "lib7z_facade.h"
#include "packagemanagercore.h"

#include "kdupdaterupdateoperations.h"

#include <QtCore/QDir>
#include <QtCore/QDirIterator>

#include <cerrno>

namespace QInstaller {


// -- AutoHelper

class AutoHelper
{
public:
    AutoHelper(CreateLocalRepositoryOperation *op)
        : m_op(op)
    {
    }

    virtual ~AutoHelper()
    {
        m_op->emitFullProgress();
        m_op->setValue(QLatin1String("files"), m_files);
    }

    QStringList m_files;
    CreateLocalRepositoryOperation *m_op;
};


// statics

namespace Static {

static void fixPermissions(const QString &repoPath)
{
    QDirIterator it(repoPath, QDirIterator::Subdirectories);
    while (it.hasNext() && !it.next().isEmpty()) {
        if (!it.fileInfo().isFile())
            continue;

        if (!QFile::setPermissions(it.filePath(), QFile::ReadOwner | QFile::WriteOwner
            | QFile::ReadUser | QFile::WriteUser | QFile::ReadGroup | QFile::ReadOther)) {
                throw Error(CreateLocalRepositoryOperation::tr("Could not set file permissions %1!")
                    .arg(it.filePath()));
        }
    }
}

static void removeDirectory(const QString &path, AutoHelper *const helper)
{
    QInstaller::removeDirectory(path);
    QStringList files = helper->m_files.filter(path);
    foreach (const QString &file, files)
        helper->m_files.removeAll(file);
}

static void removeFiles(const QString &path, AutoHelper *const helper)
{
    const QFileInfoList entries = QDir(path).entryInfoList(QDir::AllEntries | QDir::Hidden);
    foreach (const QFileInfo &fi, entries) {
        if (fi.isSymLink() || fi.isFile()) {
            QFile f(fi.filePath());
            if (!f.remove())
                throw Error(QObject::tr("Could not remove file %1: %2").arg(f.fileName(), f.errorString()));
            helper->m_files.removeAll(f.fileName());
        }
    }
}

static QString createArchive(const QString repoPath, const QString &sourceDir, const QString &version
    , AutoHelper *const helper)
{
    const QString fileName = QString::fromLatin1("/%1meta.7z").arg(version);

    QFile archive(repoPath + fileName);
    QInstaller::openForWrite(&archive, archive.fileName());
    Lib7z::createArchive(&archive, QStringList() << sourceDir);
    removeFiles(sourceDir, helper); // cleanup the files we compressed
    if (!archive.rename(sourceDir + fileName)) {
        throw Error(CreateLocalRepositoryOperation::tr("Could not move file %1 to %2. Error: %3").arg(archive
            .fileName(), sourceDir + fileName, archive.errorString()));
    }
    return archive.fileName();
}

}   // namespace Statics


// -- CreateLocalRepositoryOperation

CreateLocalRepositoryOperation::CreateLocalRepositoryOperation()
{
    setName(QLatin1String("CreateLocalRepository"));
}

void CreateLocalRepositoryOperation::backup()
{
}

bool CreateLocalRepositoryOperation::performOperation()
{
    AutoHelper helper(this);
    emit progressChanged(0.0);

    const QStringList args = arguments();

    if (args.count() != 2) {
        emit progressChanged(1.0);
        setError(InvalidArguments);
        setErrorString(tr("Invalid arguments in %0: %1 arguments given, 2 expected.").arg(name())
            .arg(args.count()));
        return false;
    }

    QString repoPath;
    try {
        QString binaryPath = QFileInfo(args.at(0)).absoluteFilePath();
        // Note the "/" at the end, important to make copy directory operation behave well
        repoPath = QFileInfo(args.at(1)).absoluteFilePath() + QLatin1String("/repository/");

        // if we're running as installer and install into an existing target, remove possible previous repos
        PackageManagerCore *core = qVariantValue<PackageManagerCore*>(value(QLatin1String("installer")));
        if (core && core->isOfflineOnly() && QFile::exists(repoPath)) {
            Static::fixPermissions(repoPath);
            QInstaller::removeDirectory(repoPath);
        }

        // create the local repository target dir
        KDUpdater::MkdirOperation mkDirOp;
        mkDirOp.setArguments(QStringList() << repoPath);
        mkDirOp.backup();
        if (!mkDirOp.performOperation()) {
            setError(mkDirOp.error());
            setErrorString(mkDirOp.errorString());
            return false;
        }
        setValue(QLatin1String("createddir"), mkDirOp.value(QLatin1String("createddir")));

        // copy the whole meta data into local repository
        CopyDirectoryOperation copyDirOp;
        copyDirOp.setArguments(QStringList() << QLatin1String(":/metadata/") << repoPath);
        connect(&copyDirOp, SIGNAL(outputTextChanged(QString)), this, SIGNAL(outputTextChanged(QString)));

        const bool success = copyDirOp.performOperation();
        helper.m_files = copyDirOp.value(QLatin1String("files")).toStringList();
        if (!success) {
            setError(copyDirOp.error());
            setErrorString(copyDirOp.errorString());
            return false;
        }

        emit progressChanged(0.25);

        // we need to fix the folder and file permissions here, as copying from read only resource file
        // systems sets all permissions to a completely bogus value...
        Static::fixPermissions(repoPath);

        // open the updates xml file we previously copied
        QFile updatesXml(repoPath + QLatin1String("Updates.xml"));
        if (!updatesXml.exists() || !updatesXml.open(QIODevice::ReadOnly))
            throw QInstaller::Error(tr("Could not open file: %1").arg(updatesXml.fileName()));

        // read the content of the updates xml
        QString error;
        QDomDocument doc;
        if (!doc.setContent(&updatesXml, &error))
            throw QInstaller::Error(tr("Could not read: %1. Error: %2").arg(updatesXml.fileName(), error));

        // build for each available package a name - version mapping
        QHash<QString, QString> versionMap;
        const QDomElement root = doc.documentElement();
        const QDomNodeList rootChildNodes = root.childNodes();
        for (int i = 0; i < rootChildNodes.count(); ++i) {
            const QDomElement element = rootChildNodes.at(i).toElement();
            if (element.isNull())
                continue;

            QString name, version;
            if (element.tagName() == QLatin1String("PackageUpdate")) {
                const QDomNodeList elementChildNodes = element.childNodes();
                for (int j = 0; j < elementChildNodes.count(); ++j) {
                    const QDomElement e = elementChildNodes.at(j).toElement();
                    if (e.tagName() == QLatin1String("Name"))
                        name = e.text();
                    else if (e.tagName() == QLatin1String("Version"))
                        version = e.text();
                }
                versionMap.insert(name, version);
            }
        }

        emit progressChanged(0.50);

        QSharedPointer<QFile> file(new QFile(binaryPath));
        if (!file->open(QIODevice::ReadOnly)) {
            throw QInstaller::Error(tr("Could not open file: %1. Error: %2").arg(file->fileName(),
                file->errorString()));
        }

        // start to read the binary layout
        BinaryLayout bl = BinaryContent::readBinaryLayout(file.data(), findMagicCookie(file.data(),
            QInstaller::MagicCookie));

        // calculate the offset of the component index start inside the binary
        const qint64 resourceOffsetAndLengtSize = 2 * sizeof(qint64);
        const qint64 resourceSectionSize = resourceOffsetAndLengtSize * bl.resourceCount;
        file->seek(bl.endOfData - bl.indexSize - resourceSectionSize - resourceOffsetAndLengtSize);

        const qint64 dataBlockStart = bl.endOfData - bl.dataBlockSize;
        file->seek(retrieveInt64(file.data()) + dataBlockStart);
        QInstallerCreator::ComponentIndex componentIndex = QInstallerCreator::ComponentIndex::read(file,
            dataBlockStart);

        QDirIterator it(repoPath, QDirIterator::Subdirectories);
        while (it.hasNext() && !it.next().isEmpty()) {
            if (it.fileInfo().isDir()) {
                const QString fileName = it.fileName();
                const QString absoluteTargetPath = QDir(repoPath).absoluteFilePath(fileName);

                // zip the meta files that come with the offline installer
                if (versionMap.contains(fileName)) {
                    helper.m_files.prepend(Static::createArchive(repoPath, absoluteTargetPath,
                        versionMap.value(fileName), &helper));
                    versionMap.remove(fileName);
                    emit outputTextChanged(helper.m_files.first());
                }

                // copy the 7z files that are inside the component index into the target
                QInstallerCreator::Component c = componentIndex.componentByName(fileName.toUtf8());
                if (c.archives().count()) {
                    QVector<QSharedPointer<QInstallerCreator::Archive> > archives = c.archives();
                    foreach (const QSharedPointer<QInstallerCreator::Archive> &a, archives) {
                        if (!a->open(QIODevice::ReadOnly))
                            continue;

                        QFile target(absoluteTargetPath + QDir::separator() + QString::fromUtf8(a->name()));
                        QInstaller::openForWrite(&target, target.fileName());
                        QInstaller::blockingCopy(a.data(), &target, a->size());
                        helper.m_files.prepend(target.fileName());
                        emit outputTextChanged(helper.m_files.first());
                    }
                }
            }
        }

        emit progressChanged(0.75);

        QDir repo(repoPath);
        if (!versionMap.isEmpty()) {
            // offline installers might miss possible old components
            foreach (const QString &dir, versionMap.keys()) {
                const QString missingDir = repoPath + dir;
                if (!repo.mkpath(missingDir))
                    throw QInstaller::Error(tr("Could not create target dir: %1.").arg(missingDir));
                helper.m_files.prepend(Static::createArchive(repoPath, missingDir, versionMap.value(dir)
                    , &helper));
                emit outputTextChanged(helper.m_files.first());
            }
        }

        try {
            // remove these, if we fail it doesn't hurt
            Static::removeDirectory(QDir::cleanPath(repoPath + QLatin1String("/installer-config")),
                &helper);
            Static::removeDirectory(QDir::cleanPath(repoPath + QLatin1String("/config")), &helper);
            const QStringList files = repo.entryList(QStringList() << QLatin1String("*.qrc"), QDir::Files);
            foreach (const QString &file, files) {
                if (repo.remove(file))
                    helper.m_files.removeAll(QDir::cleanPath(repoPath + file));
            }
        } catch (...) {}
        setValue(QLatin1String("local-repo"), repoPath);
    } catch (const Lib7z::SevenZipException &e) {
        setError(UserDefinedError);
        setErrorString(e.message());
        return false;
    } catch (const QInstaller::Error &e) {
        setError(UserDefinedError);
        setErrorString(e.message());
        return false;
    } catch (...) {
        setError(UserDefinedError);
        setErrorString(tr("Unknown exception caught: %1.").arg(QLatin1String(Q_FUNC_INFO)));
        return false;
    }
    return true;
}

bool CreateLocalRepositoryOperation::undoOperation()
{
    Q_ASSERT(arguments().count() == 2);

    AutoHelper _(this);
    emit progressChanged(0.0);

    QDir dir;
    const QStringList files = value(QLatin1String("files")).toStringList();
    foreach (const QString &file, files) {
        emit outputTextChanged(tr("Removing file: %0").arg(file));
        if (!QFile::remove(file)) {
            setError(InvalidArguments);
            setErrorString(tr("Could not remove %0.").arg(file));
            return false;
        }
        dir.rmpath(QFileInfo(file).absolutePath());
    }
    setValue(QLatin1String("files"), QStringList());

    QDir createdDir = QDir(value(QLatin1String("createddir")).toString());
    if (createdDir == QDir::root() || !createdDir.exists())
        return true;

    QFile::remove(createdDir.path() + QLatin1String("/.DS_Store"));
    QFile::remove(createdDir.path() + QLatin1String("/Thumbs.db"));

    errno = 0;
    const bool result = QDir::root().rmdir(createdDir.path());
    if (!result) {
        setError(UserDefinedError, tr("Cannot remove directory %1: %2").arg(createdDir.path(),
             QLatin1String(strerror(errno))));
    }
    setValue(QLatin1String("files"), QStringList());

    return result;
}

bool CreateLocalRepositoryOperation::testOperation()
{
    return true;
}

Operation *CreateLocalRepositoryOperation::clone() const
{
    return new CreateLocalRepositoryOperation();
}

void CreateLocalRepositoryOperation::emitFullProgress()
{
    emit progressChanged(1.0);
}

}   // namespace QInstaller
