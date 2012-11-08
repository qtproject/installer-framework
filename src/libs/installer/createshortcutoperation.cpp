/**************************************************************************
**
** This file is part of Installer Framework
**
** Copyright (c) 2011-2012 Nokia Corporation and/or its subsidiary(-ies).
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
#include "createshortcutoperation.h"

#include "fileutils.h"
#include "utils.h"

#include <QDebug>
#include <QDir>
#include <QSettings>

#include <cerrno>

using namespace QInstaller;

#ifdef Q_OS_WIN
#include <windows.h>
#include <shlobj.h>

struct DeCoInitializer
{
    DeCoInitializer()
        : neededCoInit(CoInitialize(NULL) == S_OK)
    {
    }
    ~DeCoInitializer()
    {
        if (neededCoInit)
            CoUninitialize();
    }
    bool neededCoInit;
};
#endif

struct StartsWithWorkingDirectory
{
    bool operator()(const QString &s)
    {
        return s.startsWith(QLatin1String("workingDirectory="));
    }
};

static QString parentDirectory(const QString &current)
{
    return current.mid(0, current.lastIndexOf(QLatin1Char('/')));
}

static QString takeWorkingDirArgument(QStringList &args)
{
    // if the arguments contain an option in the form "workingDirectory=...", find it and consume it
    QStringList::iterator wdiropt = std::find_if (args.begin(), args.end(), StartsWithWorkingDirectory());
    if (wdiropt == args.end())
        return QString();

    const QString workingDir = wdiropt->mid(QString::fromLatin1("workingDirectory=").size());
    args.erase(wdiropt);
    return workingDir;
}

static bool createLink(const QString &fileName, const QString &linkName, QString workingDir,
    QString arguments = QString())
{
    bool success = QFile::link(fileName, linkName);
#ifdef Q_OS_WIN
    if (!success)
        return success;

    if (workingDir.isEmpty())
        workingDir = QFileInfo(fileName).absolutePath();
    workingDir = QDir::toNativeSeparators(workingDir);

    // CoInitialize cleanup object
    DeCoInitializer _;

    IShellLink *psl = NULL;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (LPVOID*)&psl)))
        return success;

    // TODO: implement this server side, since there's not Qt equivalent to set working dir and arguments
    psl->SetPath((wchar_t *)QDir::toNativeSeparators(fileName).utf16());
    psl->SetWorkingDirectory((wchar_t *)workingDir.utf16());
    if (!arguments.isNull())
        psl->SetArguments((wchar_t*)arguments.utf16());

    IPersistFile *ppf = NULL;
    if (SUCCEEDED(psl->QueryInterface(IID_IPersistFile, (void **)&ppf))) {
        ppf->Save((wchar_t*)QDir::toNativeSeparators(linkName).utf16(), TRUE);
        ppf->Release();
    }
    psl->Release();

    PIDLIST_ABSOLUTE pidl;  // Force start menu cache update
    if (SUCCEEDED(SHGetFolderLocation(0, CSIDL_STARTMENU, 0, 0, &pidl))) {
        SHChangeNotify(SHCNE_UPDATEDIR, SHCNF_IDLIST, pidl, 0);
        CoTaskMemFree(pidl);
    }
    if (SUCCEEDED(SHGetFolderLocation(0, CSIDL_COMMON_STARTMENU, 0, 0, &pidl))) {
        SHChangeNotify(SHCNE_UPDATEDIR, SHCNF_IDLIST, pidl, 0);
        CoTaskMemFree(pidl);
    }
#else
    Q_UNUSED(arguments)
    Q_UNUSED(workingDir)
#endif
    return success;
}


// -- CreateShortcutOperation

CreateShortcutOperation::CreateShortcutOperation()
{
    setName(QLatin1String("CreateShortcut"));
}

void CreateShortcutOperation::backup()
{
    QDir linkPath(QFileInfo(arguments().at(1)).absolutePath());

    QStringList directoriesToCreate;
    while (!linkPath.exists() && linkPath != QDir::root()) {
        const QString absolutePath = linkPath.absolutePath();
        directoriesToCreate.append(absolutePath);
        linkPath = parentDirectory(absolutePath);
    }

    setValue(QLatin1String("createddirs"), directoriesToCreate);
}

bool CreateShortcutOperation::performOperation()
{
    QStringList args = arguments();
    const QString workingDir = takeWorkingDirArgument(args);

    if (args.count() != 2 && args.count() != 3) {
        setError(InvalidArguments);
        setErrorString(QObject::tr("Invalid arguments: %1 arguments given, 2 or 3 expected (optional: "
            "\"workingDirectory=...\").").arg(args.count()));
        return false;
    }

    const QString linkTarget = args.at(0);
    const QString linkLocation = args.at(1);
    const QString targetArguments = args.value(2); //used value because it could be not existing

    const QString linkPath = QFileInfo(linkLocation).absolutePath();

    const bool linkPathAlreadyExists = QDir(linkPath).exists();
    const bool created = linkPathAlreadyExists || QDir::root().mkpath(linkPath);

    if (!created) {
        setError(UserDefinedError);
        setErrorString(tr("Could not create folder %1: %2.").arg(QDir::toNativeSeparators(linkPath),
            QLatin1String(strerror(errno))));
        return false;
    }

    //remove a possible existing older one
    QString errorString;
    if (QFile::exists(linkLocation) && !deleteFileNowOrLater(linkLocation, &errorString)) {
        setError(UserDefinedError);
        setErrorString(QObject::tr("Failed to overwrite %1: %2").arg(QDir::toNativeSeparators(linkLocation),
            errorString));
        return false;
    }

    const bool linked = createLink(linkTarget, linkLocation, workingDir, targetArguments);
    if (!linked) {
        setError(UserDefinedError);
        setErrorString(tr("Could not create link %1: %2").arg(QDir::toNativeSeparators(linkLocation),
            qt_error_string()));
        return false;
    }
    return true;
}

bool CreateShortcutOperation::undoOperation()
{
    const QString &linkLocation = arguments().at(1);
    if (!deleteFileNowOrLater(linkLocation) )
        qDebug() << "Can't delete:" << linkLocation;

    QDir dir;   // remove all directories we created
    const QStringList directoriesToDelete = value(QLatin1String("createddirs")).toStringList();
    foreach (const QString &directory, directoriesToDelete) {
        QInstaller::removeSystemGeneratedFiles(directory);
        if (!dir.rmdir(directory))
            break;
    }

#ifdef Q_OS_WIN
    // special case on windows, multiple installations might leave empty folder inside the start menu
    QSettings user(QLatin1String("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\"
        "Explorer\\User Shell Folders"), QSettings::NativeFormat);
    QSettings system(QLatin1String("HKEY_LOCAL_MACHINE\\Software\\Microsoft\\Windows\\CurrentVersion\\"
        "Explorer\\Shell Folders"), QSettings::NativeFormat);

    const QString userStartMenu = QDir::fromNativeSeparators(replaceWindowsEnvironmentVariables(user
        .value(QLatin1String("Programs"), QString()).toString())).toLower();
    const QString systemStartMenu = QDir::fromNativeSeparators(system.value(QLatin1String("Common Programs"))
        .toString()).toLower();

    // try to remove every empty folder until we fail
    QString linkPath = QFileInfo(linkLocation).absolutePath().toLower();
    if (linkPath.startsWith(userStartMenu) || linkPath.startsWith(systemStartMenu)) {
        QInstaller::removeSystemGeneratedFiles(linkPath);
        while (QDir().rmdir(linkPath)) {
            linkPath = parentDirectory(linkPath);
            QInstaller::removeSystemGeneratedFiles(linkPath);
        }
    }
#endif
    return true;
}

bool CreateShortcutOperation::testOperation()
{
    return true;
}

Operation *CreateShortcutOperation::clone() const
{
    return new CreateShortcutOperation();
}
