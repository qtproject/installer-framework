/**************************************************************************
**
** This file is part of Installer Framework
**
** Copyright (c) 2012 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (info@qt.nokia.com)
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
** Nokia at info@qt.nokia.com.
**
**************************************************************************/
#include "installerbase_p.h"

#include <common/binaryformat.h>
#include <common/errors.h>
#include <common/fileutils.h>
#include <common/utils.h>
#include <lib7z_facade.h>
#include <qprocesswrapper.h>

#include <kdupdaterfiledownloader.h>
#include <kdupdaterfiledownloaderfactory.h>

#include <kdsavefile.h>

#include <QtCore/QDir>
#include <QtCore/QDebug>
#include <QtCore/QTemporaryFile>
#include <QtCore/QUrl>

#include <QtGui/QMessageBox>

#include <fstream>
#include <iomanip>
#include <iostream>

#ifdef Q_OS_WIN
#include <wincon.h>

#ifndef ENABLE_INSERT_MODE
#   define ENABLE_INSERT_MODE 0x0020
#endif

#ifndef ENABLE_QUICK_EDIT_MODE
#   define ENABLE_QUICK_EDIT_MODE 0x0040
#endif

#ifndef ENABLE_EXTENDED_FLAGS
#   define ENABLE_EXTENDED_FLAGS 0x0080
#endif
#endif

using namespace KDUpdater;
using namespace QInstaller;
using namespace QInstallerCreator;


// -- MyCoreApplication

MyCoreApplication::MyCoreApplication(int &argc, char **argv)
    : QCoreApplication(argc, argv)
{
}

// re-implemented from QCoreApplication so we can throw exceptions in scripts and slots
bool MyCoreApplication::notify(QObject *receiver, QEvent *event)
{
    try {
        return QCoreApplication::notify(receiver, event);
    } catch(std::exception &e) {
        qFatal("Exception thrown: %s", e.what());
    }
    return false;
}


// -- MyApplicationConsole

class MyApplicationConsole
{
public:
    MyApplicationConsole()
    {
#ifdef Q_OS_WIN
        AllocConsole();

        HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
        if (handle != INVALID_HANDLE_VALUE) {
            COORD largestConsoleWindowSize = GetLargestConsoleWindowSize(handle);
            largestConsoleWindowSize.X -= 3;
            largestConsoleWindowSize.Y = 5000;
            SetConsoleScreenBufferSize(handle, largestConsoleWindowSize);
        }

        handle = GetStdHandle(STD_INPUT_HANDLE);
        if (handle != INVALID_HANDLE_VALUE)
            SetConsoleMode(handle, ENABLE_INSERT_MODE | ENABLE_QUICK_EDIT_MODE | ENABLE_EXTENDED_FLAGS);

        m_oldCin = std::cin.rdbuf();
        m_newCin.open("CONIN$");
        std::cin.rdbuf(m_newCin.rdbuf());

        m_oldCout = std::cout.rdbuf();
        m_newCout.open("CONOUT$");
        std::cout.rdbuf(m_newCout.rdbuf());

        m_oldCerr = std::cerr.rdbuf();
        m_newCerr.open("CONOUT$");
        std::cerr.rdbuf(m_newCerr.rdbuf());
#endif
    }
    ~MyApplicationConsole()
    {
#ifdef Q_OS_WIN
        system("PAUSE");

        std::cin.rdbuf(m_oldCin);
        std::cerr.rdbuf(m_oldCerr);
        std::cout.rdbuf(m_oldCout);

        FreeConsole();
#endif
    }

private:
    std::ifstream m_newCin;
    std::ofstream m_newCout;
    std::ofstream m_newCerr;

    std::streambuf* m_oldCin;
    std::streambuf* m_oldCout;
    std::streambuf* m_oldCerr;
};


// -- MyApplication

MyApplication::MyApplication(int &argc, char **argv)
    : QApplication(argc, argv)
    , m_console(0)
{
}

MyApplication::~MyApplication()
{
    delete m_console;
}

void MyApplication::setVerbose()
{
    if (!m_console)
        m_console = new MyApplicationConsole;
}

// re-implemented from QApplication so we can throw exceptions in scripts and slots
bool MyApplication::notify(QObject *receiver, QEvent *event)
{
    try {
        return QApplication::notify(receiver, event);
    } catch(std::exception &e) {
        qFatal("Exception thrown: %s", e.what());
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
        qDebug() << QString::fromLatin1("Scheme not supported: %1 (%2)").arg(url.scheme(), url.toString());
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
        qDebug() << QString::fromLatin1("Could not download file %1: . Error: %2.").arg(
            m_downloader->url().toString(), m_downloader->errorString());
        return EXIT_FAILURE;
    }

    if (Lib7z::isSupportedArchive(target)) {
        QFile archive(target);
        if (archive.open(QIODevice::ReadOnly)) {
            try {
                Lib7z::extractArchive(&archive, QDir::tempPath());
                if (!archive.remove()) {
                    qDebug() << QString::fromLatin1("Could not delete file %1: %2.").arg(
                        target, archive.errorString());
                }
            } catch (const Lib7z::SevenZipException& e) {
                qDebug() << QString::fromLatin1("Error while extracting %1: %2.").arg(target, e.message());
                return EXIT_FAILURE;
            } catch (...) {
                qDebug() << QString::fromLatin1("Unknown exception caught while extracting %1.").arg(target);
                return EXIT_FAILURE;
            }
        } else {
            qDebug() << QString::fromLatin1("Could not open %1 for reading: %2.").arg(
                target, archive.errorString());
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
        qDebug() << error.message();
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

/* static*/
void InstallerBase::showUsage()
{
    MyApplicationConsole c;
    std::cout << "Usage: SDKMaintenanceTool [OPTIONS]" << std::endl << std::endl;

    std::cout << "User:"<<std::endl;
    std::cout << std::setw(55) << std::setiosflags(std::ios::left) << "  --help" << std::setw(40)
        << "Show commandline usage" << std::endl;
    std::cout << std::setw(55) << std::setiosflags(std::ios::left) << "  --version" << std::setw(40)
        << "Show current version" << std::endl;
    std::cout << std::setw(55) << std::setiosflags(std::ios::left) << "  --checkupdates" << std::setw(40)
        << "Check for updates and return an XML file of the available updates" << std::endl;
    std::cout << std::setw(55) << std::setiosflags(std::ios::left) << "  --proxy" << std::setw(40)
        << "Set system proxy on Win and Mac. This option has no effect on Linux." << std::endl;
    std::cout << std::setw(55) << std::setiosflags(std::ios::left) << "  --verbose" << std::setw(40)
        << "Show debug output on the console" << std::endl;

    std::cout << "Developer:"<< std::endl;
    std::cout << std::setw(55) << std::setiosflags(std::ios::left)
        << "  --runoperation [operationName] [arguments...]" << std::setw(40)
        << "Perform an operation with a list of arguments" << std::endl;
    std::cout << std::setw(55) << std::setiosflags(std::ios::left)
        << "  --undooperation [operationName] [arguments...]" << std::setw(40)
        << "Undo an operation with a list of arguments" <<std::endl;
    std::cout << std::setw(55) << std::setiosflags(std::ios::left)
        << "  --script [scriptName]" << std::setw(40) << "Execute a script" << std::endl;
    std::cout << std::setw(55) << std::setiosflags(std::ios::left) << "  --no-force-installations"
        << std::setw(40) << "Enable deselection of forced components" << std::endl;
    std::cout << std::setw(55) << std::setiosflags(std::ios::left) << "  --addTempRepository [URI]"
        << std::setw(40) << "Add a local or remote repo to the list of available repos." << std::endl;
    std::cout << std::setw(55) << std::setiosflags(std::ios::left) << "  --setTempRepository [URI]"
        << std::setw(40) << "Set the update URL to an arbitrary local or remote URI. URI must be prefixed "
        "with the protocol, i.e. file:/// or http://" << std::endl;
    std::cout << std::setw(55) << std::setiosflags(std::ios::left) << "  --show-virtual-components"
        << std::setw(40) << "Show virtual components in package manager and updater" << std::endl;
    std::cout << std::setw(55) << std::setiosflags(std::ios::left)
        << "  --update-installerbase [path/to/new/installerbase]" << std::setw(40)
        << "Patch a full installer with a new installer base" << std::endl;
}

/* static*/
void InstallerBase::showVersion(const QString &version)
{
    MyApplicationConsole c;
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
