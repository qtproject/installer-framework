/**************************************************************************
**
** This file is part of Qt SDK**
**
** Copyright (c) 2011 Nokia Corporation and/or its subsidiary(-ies).*
**
** Contact:  Nokia Corporation qt-info@nokia.com**
**
** No Commercial Usage
**
** This file contains pre-release code and may not be distributed.
** You may use this file in accordance with the terms and conditions
** contained in the Technology Preview License Agreement accompanying
** this package.
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
** rights. These rights are described in the Nokia Qt LGPL Exception version
** 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** If you are unsure which license is appropriate for your use, please contact
** (qt-info@nokia.com).
**
**************************************************************************/
#include "installerbase_p.h"

#include <common/binaryformat.h>
#include <common/errors.h>
#include <common/fileutils.h>
#include <common/utils.h>
#include <lib7z_facade.h>
#include <qprocesswrapper.h>

#include <KDUpdater/FileDownloader>
#include <KDUpdater/FileDownloaderFactory>

#include <KDToolsCore/KDSaveFile>

#include <QtCore/QDir>
#include <QtCore/QDebug>
#include <QtCore/QTemporaryFile>
#include <QtCore/QUrl>

#include <QtGui/QMessageBox>

using namespace KDUpdater;
using namespace QInstaller;
using namespace QInstallerCreator;


// -- MyCoreApplication

MyCoreApplication::MyCoreApplication(int &argc, char **argv)
    : QCoreApplication(argc, argv)
{
}

// reimplemented from QCoreApplication so we can throw exceptions in scripts and slots
bool MyCoreApplication::notify(QObject *receiver, QEvent *event)
{
    try {
        return QCoreApplication::notify(receiver, event);
    } catch(std::exception &e) {
        qCritical() << "Exception thrown:" << e.what();
    }
    return false;
}


// -- MyApplication

MyApplication::MyApplication(int &argc, char **argv)
    : QApplication(argc, argv)
{
}

// reimplemented from QApplication so we can throw exceptions in scripts and slots
bool MyApplication::notify(QObject *receiver, QEvent *event)
{
    try {
        return QApplication::notify(receiver, event);
    } catch(std::exception &e) {
        qCritical() << "Exception thrown:" << e.what();
    }
    return false;
}


// -- InstallerBase

InstallerBase::InstallerBase(QObject *parent)
    : QObject(parent)
    , m_downloadFinished(false)
{
}

InstallerBase::~InstallerBase()
{
}

static bool supportedScheme(const QString &scheme)
{
    if (scheme == QLatin1String("http") || scheme == QLatin1String("ftp") || scheme == QLatin1String("file"))
        return true;
    return false;
}

int InstallerBase::replaceMaintenanceToolBinary(QStringList arguments)
{
    QInstaller::setVerbose(arguments.contains(QLatin1String("--verbose")));

    arguments.removeAll(QLatin1String("--verbose"));
    arguments.removeAll(QLatin1String("--update-installerbase"));

    QUrl url = arguments.value(1);
    if (!supportedScheme(url.scheme()) && QFileInfo(url.toString()).exists())
        url = QLatin1String("file:///") + url.toString();
    m_downloader.reset(FileDownloaderFactory::instance().create(url.scheme()));
    if (m_downloader.isNull()) {
        verbose() << tr("Scheme not supported: %1 (%2)").arg(url.scheme(), url.toString()) << std::endl;
        return EXIT_FAILURE;
    }
    m_downloader->setUrl(url);
    m_downloader->setAutoRemoveDownloadedFile(true);

    QString target = QDir::tempPath() + QLatin1String("/") + QFileInfo(arguments.at(1)).fileName();
    if (supportedScheme(url.scheme()))
        m_downloader->setDownloadedFileName(target);

    connect(m_downloader.data(), SIGNAL(downloadStarted()), this, SLOT(downloadStarted()));
    connect(m_downloader.data(), SIGNAL(downloadCanceled()), this, SLOT(downloadFinished()));
    connect(m_downloader.data(), SIGNAL(downloadCompleted()), this, SLOT(downloadFinished()));
    connect(m_downloader.data(), SIGNAL(downloadAborted(QString)), this, SLOT(downloadAborted(QString)));

    m_downloader->download();

    while (true) {
        QCoreApplication::processEvents();
        if (m_downloadFinished)
            break;
    }

    if (!m_downloader->isDownloaded()) {
        verbose() << tr("Could not download file %s: . Error: %s.").arg(m_downloader->url().toString(),
            m_downloader->errorString()) << std::endl;
        return EXIT_FAILURE;
    }

    if (Lib7z::isSupportedArchive(target)) {
        QFile archive(target);
        if (archive.open(QIODevice::ReadOnly)) {
            try {
                Lib7z::extractArchive(&archive, QDir::tempPath());
                if (!archive.remove())
                    verbose() << tr("Could not delete file %s: %s.").arg(target, archive.errorString());
            } catch (const Lib7z::SevenZipException& e) {
                verbose() << tr("Error while extracting %1: %2.").arg(target, e.message());
                return EXIT_FAILURE;
            } catch (...) {
                verbose() << tr("Unknown exception caught while extracting %1.").arg(target);
                return EXIT_FAILURE;
            }
        } else {
            verbose() << tr("Could not open %1 for reading: %2.").arg(target, archive.errorString());
            return EXIT_FAILURE;
        }
#ifndef Q_OS_WIN
        target = QDir::tempPath() + QLatin1String("/.tempSDKMaintenanceTool");
#else
        target = QDir::tempPath() + QLatin1String("/temp/SDKMaintenanceToolBase.exe");
#endif
    }

    try {
        QFile installerBase(target);
        QInstaller::openForRead(&installerBase, installerBase.fileName());
        writeMaintenanceBinary(arguments.value(0), &installerBase, installerBase.size());
        deferredRename(arguments.value(0) + QLatin1String(".new"), arguments.value(0));
    } catch (const QInstaller::Error &error) {
        verbose() << error.message() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

/* static*/
void InstallerBase::showVersion(int &argc, char **argv, const QString &version)
{
#ifdef Q_OS_WIN
    MyApplication app(argc, argv);
    QMessageBox::information(0, tr("Version"), version);
#else
    Q_UNUSED(argc) Q_UNUSED(argv)
    fprintf(stdout, "%s\n", qPrintable(version));
#endif
}


// -- private slots

void InstallerBase::downloadStarted()
{
    m_downloadFinished = false;
    verbose() << tr("Download started! Source: ") << m_downloader->url().toString() << tr(", Target: ")
        << m_downloader->downloadedFileName() << std::endl;
}

void InstallerBase::downloadFinished()
{
    m_downloadFinished = true;
    verbose() << tr("Download finished! Source: ") << m_downloader->url().toString() << tr(", Target: ")
        << m_downloader->downloadedFileName() << std::endl;
}

void InstallerBase::downloadProgress(double progress)
{
    verbose() << tr("Progress: ") << progress << std::endl;
}

void InstallerBase::downloadAborted(const QString &error)
{
    m_downloadFinished = true;
    verbose() << tr("Download aborted! Source: ") << m_downloader->url().toString() << tr(", Target: ")
        << m_downloader->downloadedFileName() << tr(", Error: ") << error << std::endl;
}


// -- private

void InstallerBase::deferredRename(const QString &oldName, const QString &newName)
{
#ifdef Q_OS_WIN
    QTemporaryFile vbScript(QDir::temp().absoluteFilePath(QLatin1String("deferredrenameXXXXXX.vbs")));
    {
        openForWrite(&vbScript, vbScript.fileName());
        vbScript.setAutoRemove(false);

        QTextStream batch(&vbScript);
        batch << "Set fso = WScript.CreateObject(\"Scripting.FileSystemObject\")\n";
        batch << "Set tmp = WScript.CreateObject(\"WScript.Shell\")\n";
        batch << QString::fromLatin1("file = \"%1\"\n").arg(QDir::toNativeSeparators(newName));
        batch << QString::fromLatin1("backup = \"%1.bak\"\n").arg(QDir::toNativeSeparators(newName));
        batch << "on error resume next\n";

        batch << "while fso.FileExists(file)\n";
        batch << "    fso.MoveFile file, backup\n";
        batch << "    WScript.Sleep(1000)\n";
        batch << "wend\n";
        batch << QString::fromLatin1("fso.MoveFile \"%1\", file\n").arg(QDir::toNativeSeparators(oldName));
        batch << "fso.DeleteFile(WScript.ScriptFullName)\n";
    }

    QProcessWrapper::startDetached(QLatin1String("cscript"), QStringList() << QLatin1String("//Nologo")
        << QDir::toNativeSeparators(vbScript.fileName()));
#else
    QFile::rename(newName, newName + QLatin1String(".bak"));
    QFile::rename(oldName, newName);
#endif
}

void InstallerBase::writeMaintenanceBinary(const QString &target, QFile *const source, qint64 size)
{
    KDSaveFile out(target + QLatin1String(".new"));
    QInstaller::openForWrite(&out, out.fileName()); // throws an exception in case of error

    if (!source->seek(0)) {
        throw QInstaller::Error(QObject::tr("Failed to seek in file %1. Reason: %2.").arg(source->fileName(),
            source->errorString()));
    }

    QInstaller::appendData(&out, source, size);
    QInstaller::appendInt64(&out, 0);   // resource count
    QInstaller::appendInt64(&out, 4 * sizeof(qint64));   // data block size
    QInstaller::appendInt64(&out, QInstaller::MagicUninstallerMarker);
    QInstaller::appendInt64(&out, QInstaller::MagicCookie);

    out.setPermissions(out.permissions() | QFile::WriteUser | QFile::ReadGroup | QFile::ReadOther
        | QFile::ExeOther | QFile::ExeGroup | QFile::ExeUser);

    if (!out.commit(KDSaveFile::OverwriteExistingFile)) {
        throw QInstaller::Error(QString::fromLatin1("Could not write new maintenance-tool to %1. Reason: %2.")
            .arg(out.fileName(), out.errorString()));
    }
}
