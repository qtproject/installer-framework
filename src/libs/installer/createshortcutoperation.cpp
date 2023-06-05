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
#include "createshortcutoperation.h"

#include "fileutils.h"
#include "utils.h"
#include "globals.h"

#include <QDebug>
#include <QDir>
#include <QSettings>

#include <cerrno>

using namespace QInstaller;

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::CreateShortcutOperation
    \internal
*/

#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <shlobj.h>
#include <Intshcut.h>

#ifndef PIDLIST_ABSOLUTE
typedef ITEMIDLIST *PIDLIST_ABSOLUTE;
#endif

struct DeCoInitializer
{
    DeCoInitializer()
        : neededCoInit(CoInitialize(nullptr) == S_OK)
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

struct StartsWith
{
    StartsWith(const QString &searchTerm)
        : m_searchTerm(searchTerm) {}

    bool operator()(const QString &searchString)
    {
        return searchString.startsWith(m_searchTerm);
    }

    QString m_searchTerm;
};

static QString parentDirectory(const QString &current)
{
    return current.mid(0, current.lastIndexOf(QLatin1Char('/')));
}

static QString takeArgument(const QString &argument, QStringList *arguments)
{
    // if the arguments contain an option in the form "argument=...", find it and consume it
    QStringList::iterator it = std::find_if(arguments->begin(), arguments->end(), StartsWith(argument));
    if (it == arguments->end())
        return QString();

    const QString value = it->mid(argument.size());
    arguments->erase(it);
    return value;
}

static bool createLink(const QString &fileName, const QString &linkName, QString &workingDir,
    const QString &arguments = QString(), const QString &iconPath = QString(),
    const QString &iconId = QString(), const QString &description = QString())
{
#ifdef Q_OS_WIN
    // CoInitialize cleanup object
    DeCoInitializer _;

    IUnknown *iunkn = nullptr;

    if (fileName.toLower().startsWith(QLatin1String("http:"))
        || fileName.toLower().startsWith(QLatin1String("https:"))
        || fileName.toLower().startsWith(QLatin1String("ftp:"))) {
        IUniformResourceLocator *iurl = nullptr;
        if (FAILED(CoCreateInstance(CLSID_InternetShortcut, nullptr, CLSCTX_INPROC_SERVER,
                                    IID_IUniformResourceLocator, (LPVOID*)&iurl))) {
            return false;
        }

        if (FAILED(iurl->SetURL((wchar_t *)fileName.utf16(), IURL_SETURL_FL_GUESS_PROTOCOL))) {
            iurl->Release();
            return false;
        }
        iunkn = iurl;
    } else {
        bool success = QFile::link(fileName, linkName);

        if (!success) {
            return success;
        }

        if (workingDir.isEmpty())
            workingDir = QFileInfo(fileName).absolutePath();
        workingDir = QDir::toNativeSeparators(workingDir);

        IShellLink *psl = NULL;
        if (FAILED(CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                                    IID_IShellLink, (LPVOID*)&psl))) {
            return success;
        }

        // TODO: implement this server side, since there's not Qt equivalent to set working dir and arguments
        psl->SetPath((wchar_t *)QDir::toNativeSeparators(fileName).utf16());
        psl->SetWorkingDirectory((wchar_t *)workingDir.utf16());
        if (!arguments.isNull())
            psl->SetArguments((wchar_t*)arguments.utf16());
        if (!iconPath.isNull())
            psl->SetIconLocation((wchar_t*)(iconPath.utf16()), iconId.toInt());
        if (!description.isNull())
            psl->SetDescription((wchar_t*)(description.utf16()));
        iunkn = psl;
    }

    IPersistFile *ppf = NULL;
    if (SUCCEEDED(iunkn->QueryInterface(IID_IPersistFile, (void **)&ppf))) {
        ppf->Save((wchar_t*)QDir::toNativeSeparators(linkName).utf16(), true);
        ppf->Release();
    }
    iunkn->Release();

    PIDLIST_ABSOLUTE pidl;  // Force start menu cache update
    if (SUCCEEDED(SHGetFolderLocation(0, CSIDL_STARTMENU, 0, 0, &pidl))) {
        SHChangeNotify(SHCNE_UPDATEDIR, SHCNF_IDLIST, pidl, 0);
        CoTaskMemFree(pidl);
    }
    if (SUCCEEDED(SHGetFolderLocation(0, CSIDL_COMMON_STARTMENU, 0, 0, &pidl))) {
        SHChangeNotify(SHCNE_UPDATEDIR, SHCNF_IDLIST, pidl, 0);
        CoTaskMemFree(pidl);
    }

    return true;
#else
    Q_UNUSED(arguments)
    Q_UNUSED(workingDir)
    Q_UNUSED(fileName)
    Q_UNUSED(linkName)
    Q_UNUSED(iconPath)
    Q_UNUSED(iconId)
    Q_UNUSED(description)
    return true;
#endif
}


// -- CreateShortcutOperation

CreateShortcutOperation::CreateShortcutOperation(PackageManagerCore *core)
    : UpdateOperation(core)
    , m_optionalArgumentsRead(false)
{
    setName(QLatin1String("CreateShortcut"));
}

void CreateShortcutOperation::backup()
{
    ensureOptionalArgumentsRead();

    QDir linkPath(QFileInfo(arguments().at(1)).absolutePath());

    QStringList directoriesToCreate;
    while (!linkPath.exists() && linkPath != QDir::root()) {
        const QString absolutePath = linkPath.absolutePath();
        directoriesToCreate.append(absolutePath);
        linkPath = parentDirectory(absolutePath);
    }

    setValue(QLatin1String("createddirs"), directoriesToCreate);
}

void CreateShortcutOperation::ensureOptionalArgumentsRead()
{
    if (m_optionalArgumentsRead)
        return;

    m_optionalArgumentsRead = true;

    QStringList args = arguments();

    m_iconId = takeArgument(QString::fromLatin1("iconId="), &args);
    m_iconPath = takeArgument(QString::fromLatin1("iconPath="), &args);
    m_workingDir = takeArgument(QString::fromLatin1("workingDirectory="), &args);
    m_description = takeArgument(QString::fromLatin1("description="), &args);

    setArguments(args);
}

bool CreateShortcutOperation::performOperation()
{
    ensureOptionalArgumentsRead();

    if (!checkArgumentCount(2, 3, tr("<target> <link location> [target arguments] "
                                     "[\"workingDirectory=...\"] [\"iconPath=...\"] [\"iconId=...\"] "
                                     "[\"description=...\"]"))) {
        return false;
    }

    QStringList args = arguments();

    const QString linkTarget = args.at(0);
    const QString linkLocation = args.at(1);
    const QString targetArguments = args.value(2);  // value() used since it's optional

    const QString linkPath = QFileInfo(linkLocation).absolutePath().trimmed();
    const bool created = QDir(linkPath).exists() || QDir::root().mkpath(linkPath);

    if (!created) {
        setError(UserDefinedError);
#if defined(Q_OS_WIN) && !defined(Q_CC_MINGW)
        char msg[128];
        if (strerror_s(msg, sizeof msg, errno) != 0) {
            setErrorString(tr("Cannot create directory \"%1\": %2").arg(QDir::toNativeSeparators(linkPath),
                QString::fromLocal8Bit(msg)));
        }
#else
        setErrorString(tr("Cannot create directory \"%1\": %2").arg(QDir::toNativeSeparators(linkPath),
            QString::fromLocal8Bit(strerror(errno))));
#endif
        return false;
    }

    //remove a possible existing older one
    QString errorString;
    if (QFile::exists(linkLocation) && !deleteFileNowOrLater(linkLocation, &errorString)) {
        setError(UserDefinedError);
        setErrorString(tr("Failed to overwrite \"%1\": %2").arg(QDir::toNativeSeparators(linkLocation),
            errorString));
        return false;
    }

    const bool linked = createLink(linkTarget, linkLocation, m_workingDir, targetArguments, m_iconPath, m_iconId,
                                   m_description);
    if (!linked) {
        setError(UserDefinedError);
        setErrorString(tr("Cannot create link \"%1\": %2").arg(QDir::toNativeSeparators(linkLocation),
            qt_error_string()));
        return false;
    }
    return true;
}

bool CreateShortcutOperation::undoOperation()
{
    ensureOptionalArgumentsRead();

    const QString &linkLocation = arguments().at(1);
    if (!deleteFileNowOrLater(linkLocation) )
        qCWarning(QInstaller::lcInstallerInstallLog) << "Cannot delete:" << linkLocation;

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
