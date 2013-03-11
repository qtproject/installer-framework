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
#include "createshortcutoperation.h"

#include "fileutils.h"
#include "utils.h"

#include <QDebug>
#include <QDir>
#include <QSettings>

#include <cerrno>

using namespace QInstaller;

#ifdef Q_OS_WIN
#ifdef Q_CC_MINGW
# ifndef _WIN32_WINNT
#  define _WIN32_WINNT 0x0501
# endif
#endif
#include <windows.h>
#include <shlobj.h>

#ifndef PIDLIST_ABSOLUTE
typedef ITEMIDLIST *PIDLIST_ABSOLUTE;
#endif

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
#ifdef Q_OS_WIN
    bool success = QFile::link(fileName, linkName);

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
        ppf->Save((wchar_t*)QDir::toNativeSeparators(linkName).utf16(), true);
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

    return success;
#else
    Q_UNUSED(arguments)
    Q_UNUSED(workingDir)
    Q_UNUSED(fileName)
    Q_UNUSED(linkName)

    return true;
#endif
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
        setErrorString(tr("Invalid arguments in %0: %1 arguments given, %2 expected%3.")
            .arg(name()).arg(arguments().count()).arg(tr("2 or 3"), tr(" (optional: 'workingDirectory=...')")));
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
#if defined(Q_OS_WIN) && !defined(Q_CC_MINGW)
        char msg[128];
        if (strerror_s(msg, sizeof msg, errno) != 0) {
            setErrorString(tr("Could not create folder %1: %2.").arg(QDir::toNativeSeparators(linkPath),
                QString::fromLocal8Bit(msg)));
        }
#else
        setErrorString(tr("Could not create folder %1: %2.").arg(QDir::toNativeSeparators(linkPath),
            QString::fromLocal8Bit(strerror(errno))));
#endif
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
