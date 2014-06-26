/**************************************************************************
**
** Copyright (C) 2012-2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
**************************************************************************/
#include "installerbase_p.h"
#include "console.h"

#include <binaryformat.h>
#include <errors.h>
#include <fileio.h>
#include <init.h>
#include <lib7z_facade.h>
#include <qprocesswrapper.h>
#include <utils.h>

#include <kdupdaterfiledownloader.h>
#include <kdupdaterfiledownloaderfactory.h>

#include <QtCore/QDir>
#include <QtCore/QDebug>
#include <QtCore/QTemporaryFile>
#include <QtCore/QUrl>

#include <QMessageBox>

#include <iomanip>
#include <iostream>

using namespace KDUpdater;
using namespace QInstaller;
using namespace QInstallerCreator;


// -- InstallerBase

InstallerBase::InstallerBase(QObject *parent)
    : QObject(parent)
    , m_downloadFinished(false)
{
}

InstallerBase::~InstallerBase()
{
}

int InstallerBase::replaceMaintenanceToolBinary(QStringList arguments)
{
    QInstaller::init();
    QInstaller::setVerbose(arguments.contains(QLatin1String("--verbose"))
                           || arguments.contains(QLatin1String("-v")));

    arguments.removeAll(QLatin1String("--verbose"));
    arguments.removeAll(QLatin1String("-v"));
    arguments.removeAll(QLatin1String("--update-installerbase"));

    const QUrl url = QUrl::fromUserInput(arguments.value(1));
    m_downloader.reset(FileDownloaderFactory::instance().create(url.scheme(), 0));
    if (m_downloader.isNull()) {
        qDebug() << QString::fromLatin1("Scheme not supported: %1 (%2)").arg(url.scheme(),
            url.toString());
        return EXIT_FAILURE;
    }
    m_downloader->setUrl(url);
    m_downloader->setAutoRemoveDownloadedFile(false);
    qDebug() << QString::fromLatin1("Downloading file '%1'.").arg(url.toString());

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
        qDebug() << QString::fromLatin1("Could not download file %1: . Error: %2.").arg(
            m_downloader->url().toString(), m_downloader->errorString());
        return EXIT_FAILURE;
    }

    QString newInstallerBasePath = m_downloader->downloadedFileName();
    if (Lib7z::isSupportedArchive(newInstallerBasePath)) {
        QFile archive(newInstallerBasePath);
        if (archive.open(QIODevice::ReadOnly)) {
            try {
                Lib7z::extractArchive(&archive, QDir::tempPath());
                const QVector<Lib7z::File> files = Lib7z::listArchive(&archive);
                newInstallerBasePath = QDir::tempPath() + QLatin1Char('/') + files.value(0).path;

                if (!archive.remove()) {
                    qDebug() << QString::fromLatin1("Could not delete file %1: %2").arg(
                        newInstallerBasePath, archive.errorString());
                }
            } catch (const Lib7z::SevenZipException& e) {
                qDebug() << QString::fromLatin1("Error while extracting '%1': %2")
                    .arg(newInstallerBasePath, e.message());
                return EXIT_FAILURE;
            } catch (...) {
                qDebug() << QString::fromLatin1("Unknown exception caught while extracting %1.")
                    .arg(newInstallerBasePath);
                return EXIT_FAILURE;
            }
        } else {
            qDebug() << QString::fromLatin1("Could not open %1 for reading: %2.")
                .arg(newInstallerBasePath, archive.errorString());
            return EXIT_FAILURE;
        }
    }

    qDebug() << QString::fromLatin1("New installer base path '%1'.").arg(newInstallerBasePath);

    try {
        {
            QFile installerBaseNew(newInstallerBasePath);
            QInstaller::openForAppend(&installerBaseNew);

            installerBaseNew.seek(installerBaseNew.size());
            QInstaller::appendInt64(&installerBaseNew, 0);   // resource count
            QInstaller::appendInt64(&installerBaseNew, 4 * sizeof(qint64));   // data block size
            QInstaller::appendInt64(&installerBaseNew, QInstaller::MagicUninstallerMarker);
            QInstaller::appendInt64(&installerBaseNew, QInstaller::MagicCookie);

            QFile installerBaseOld(arguments.value(0));
            installerBaseNew.setPermissions(installerBaseOld.permissions());
        }
        deferredRename(newInstallerBasePath, arguments.value(0));
    } catch (const QInstaller::Error &error) {
        qDebug() << error.message();
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

/* static*/
void InstallerBase::showUsage()
{
#define WIDTH1 46
#define WIDTH2 40
    Console c;
    std::cout << "Usage: SDKMaintenanceTool [OPTIONS]" << std::endl << std::endl;

    std::cout << "User:"<<std::endl;
    std::cout << std::setw(WIDTH1) << std::setiosflags(std::ios::left) << "  --help" << std::setw(WIDTH2)
        << "Show commandline usage" << std::endl;
    std::cout << std::setw(WIDTH1) << std::setiosflags(std::ios::left) << "  --version" << std::setw(WIDTH2)
        << "Show current version" << std::endl;
    std::cout << std::setw(WIDTH1) << std::setiosflags(std::ios::left) << "  --checkupdates" << std::setw(WIDTH2)
        << "Check for updates and return an XML file describing" << std::endl
        << std::setw(WIDTH1) << " " << std::setw(WIDTH2) << "the available updates" << std::endl;
    std::cout << std::setw(WIDTH1) << std::setiosflags(std::ios::left) << "  --updater" << std::setw(WIDTH2)
        << "Start in updater mode." << std::endl;
    std::cout << std::setw(WIDTH1) << std::setiosflags(std::ios::left) << "  --manage-packages" << std::setw(WIDTH2)
        << "Start in packagemanager mode." << std::endl;
    std::cout << std::setw(WIDTH1) << std::setiosflags(std::ios::left) << "  --proxy" << std::setw(WIDTH2)
        << "Set system proxy on Win and Mac." << std::endl
        << std::setw(WIDTH1) << " " << std::setw(WIDTH2) << "This option has no effect on Linux." << std::endl;
    std::cout << std::setw(WIDTH1) << std::setiosflags(std::ios::left) << "  --verbose" << std::setw(WIDTH2)
        << "Show debug output on the console" << std::endl;
    std::cout << std::setw(WIDTH1) << std::setiosflags(std::ios::left) << "  --create-offline-repository"
        << std::setw(WIDTH2) << "Offline installer only: Create a local repository inside the" << std::endl
        << std::setw(WIDTH1) << " " << std::setw(WIDTH2) << "installation directory based on the offline" << std::endl
        << std::setw(WIDTH1) << " " << std::setw(WIDTH2) << "installer's content." << std::endl;

    std::cout << "\nDeveloper:"<< std::endl;
    std::cout << std::setw(WIDTH1) << std::setiosflags(std::ios::left)
        << "  --runoperation [OPERATION] [arguments...]" << std::setw(WIDTH2)
        << "Perform an operation with a list of arguments" << std::endl;
    std::cout << std::setw(WIDTH1) << std::setiosflags(std::ios::left)
        << "  --undooperation [OPERATION] [arguments...]" << std::setw(WIDTH2)
        << "Undo an operation with a list of arguments" <<std::endl;
    std::cout << std::setw(WIDTH1) << std::setiosflags(std::ios::left)
        << "  --script [scriptName]" << std::setw(WIDTH2) << "Execute a script" << std::endl;
    std::cout << std::setw(WIDTH1) << std::setiosflags(std::ios::left) << "  --no-force-installations"
        << std::setw(WIDTH2) << "Enable deselection of forced components" << std::endl;
    std::cout << std::setw(WIDTH1) << std::setiosflags(std::ios::left) << "  --addRepository [URI]"
        << std::setw(WIDTH2) << "Add a local or remote repo to the list of user defined repos." << std::endl;
    std::cout << std::setw(WIDTH1) << std::setiosflags(std::ios::left) << "  --addTempRepository [URI]"
        << std::setw(WIDTH2) << "Add a local or remote repo to the list of temporary available" << std::endl
        << std::setw(WIDTH1) << " " << std::setw(WIDTH2) << "repos." << std::endl;
    std::cout << std::setw(WIDTH1) << std::setiosflags(std::ios::left) << "  --setTempRepository [URI]"
        << std::setw(WIDTH2) << "Set a local or remote repo as tmp repo, it is the only one" << std::endl
        << std::setw(WIDTH1) << " " << std::setw(WIDTH2) << "used during fetch." << std::endl;
    std::cout << std::setw(WIDTH1) << std::setiosflags(std::ios::left) << " " << std::setw(WIDTH2) << "Note: URI "
        "must be prefixed with the protocol, i.e. file:///" << std::endl
        << std::setw(WIDTH1) << " " << std::setw(WIDTH2) << "http:// or ftp://. It can consist of multiple" << std::endl
        << std::setw(WIDTH1) << " " << std::setw(WIDTH2) << "addresses separated by comma only." << std::endl;
    std::cout << std::setw(WIDTH1) << std::setiosflags(std::ios::left) << "  --show-virtual-components"
        << std::setw(WIDTH2) << "Show virtual components in package manager and updater" << std::endl;
    std::cout << std::setw(WIDTH1) << std::setiosflags(std::ios::left)
        << "  --binarydatafile [binary_data_file]" << std::setw(WIDTH2) << "Use the binary data of "
        "another installer or maintenance tool." << std::endl;
    std::cout << std::setw(WIDTH1) << std::setiosflags(std::ios::left)
        << "  --update-installerbase [new_installerbase]" << std::setw(WIDTH2)
        << "Patch a full installer with a new installer base" << std::endl;
    std::cout << std::setw(WIDTH1) << std::setiosflags(std::ios::left) << "  --dump-binary-data -i [PATH] -o [PATH]"
        << std::setw(WIDTH2) << "Dumps the binary content into specified output path (offline" << std::endl
        << std::setw(WIDTH1) << " " << std::setw(WIDTH2) << "installer only)." << std::endl
        << std::setw(WIDTH1) << " " << std::setw(WIDTH2) << "Input path pointing to binary data file, if omitted" << std::endl
        << std::setw(WIDTH1) << " " << std::setw(WIDTH2) << "the current application is used as input." << std::endl;
}

/* static*/
void InstallerBase::showVersion(const QString &version)
{
    Console c;
    std::cout << qPrintable(version) << std::endl;
}


// -- private slots

void InstallerBase::downloadStarted()
{
    m_downloadFinished = false;
    qDebug() << QString::fromLatin1("Download started! Source: %1, Target: %2").arg(
        m_downloader->url().toString(), m_downloader->downloadedFileName());
}

void InstallerBase::downloadFinished()
{
    m_downloadFinished = true;
    qDebug() << QString::fromLatin1("Download finished! Source: %1, Target: %2").arg(
        m_downloader->url().toString(), m_downloader->downloadedFileName());
}

void InstallerBase::downloadProgress(double progress)
{
    qDebug() << "Progress: " << progress;
}

void InstallerBase::downloadAborted(const QString &error)
{
    m_downloadFinished = true;
    qDebug() << QString::fromLatin1("Download aborted! Source: %1, Target: %2, Error: %3").arg(
        m_downloader->url().toString(), m_downloader->downloadedFileName(), error);
}


// -- private

void InstallerBase::deferredRename(const QString &oldName, const QString &newName)
{
#ifdef Q_OS_WIN
    QTemporaryFile vbScript(QDir::temp().absoluteFilePath(QLatin1String("deferredrenameXXXXXX.vbs")));
    {
        QInstaller::openForWrite(&vbScript);
        vbScript.setAutoRemove(false);

        QTextStream batch(&vbScript);
        batch << "Set fso = WScript.CreateObject(\"Scripting.FileSystemObject\")\n";
        batch << "Set tmp = WScript.CreateObject(\"WScript.Shell\")\n";
        batch << QString::fromLatin1("file = \"%1\"\n").arg(QDir::toNativeSeparators(newName));
        batch << QString::fromLatin1("backup = \"%1.bak\"\n").arg(QDir::toNativeSeparators(newName));
        batch << "on error resume next\n";

        batch << "while fso.FileExists(backup)\n";
        batch << "    fso.DeleteFile(backup)\n";
        batch << "    WScript.Sleep(1000)\n";
        batch << "wend\n";

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
    QFile::remove(newName + QLatin1String(".bak"));
    QFile::rename(newName, newName + QLatin1String(".bak"));
    QFile::rename(oldName, newName);
#endif
}
