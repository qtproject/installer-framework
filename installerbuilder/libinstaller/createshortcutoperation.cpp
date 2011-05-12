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
//#include "kdupdaterupdateoperations.h"
#include <KDUpdater/kdupdaterapplication.h>
#include <KDUpdater/kdupdaterpackagesinfo.h>

#include "createshortcutoperation.h"
#include "common/errors.h"
#include "common/fileutils.h"
#include "common/utils.h"

#include <QDir>
#include <QFileInfo>
#include <QTemporaryFile>

#include <algorithm>
#include <cerrno>

#ifdef Q_WS_WIN
#include <windows.h>
#include <shlobj.h>
#endif

using namespace QInstaller;

static bool createLink(QString fileName, QString linkName, QString workingDir, QString arguments = QString())
{
#ifdef Q_WS_WIN
    bool ret = false;
    fileName = QDir::toNativeSeparators(fileName);
    linkName = QDir::toNativeSeparators(linkName);
    if (workingDir.isEmpty())
        workingDir = QFileInfo(fileName).absolutePath();
    workingDir = QDir::toNativeSeparators(workingDir);

    //### assume that they add .lnk

    IShellLink *psl;
    bool neededCoInit = false;

    HRESULT hres = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (void **)&psl);

    if (hres == CO_E_NOTINITIALIZED) { // COM was not initialized
        neededCoInit = true;
        CoInitialize(NULL);
        hres = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (void **)&psl);
    }

    if (SUCCEEDED(hres)) {
        hres = psl->SetPath((wchar_t *)fileName.utf16());
        if(SUCCEEDED(hres) && !arguments.isNull())
            hres = psl->SetArguments((wchar_t*)arguments.utf16());
        if (SUCCEEDED(hres)) {
            hres = psl->SetWorkingDirectory((wchar_t *)workingDir.utf16());
            if (SUCCEEDED(hres)) {
                IPersistFile *ppf;
                hres = psl->QueryInterface(IID_IPersistFile, (void **)&ppf);
                if (SUCCEEDED(hres)) {
                    hres = ppf->Save((wchar_t*)linkName.utf16(), TRUE);
                    if (SUCCEEDED(hres))
                        ret = true;
                    ppf->Release();
                }
            }
        }
        psl->Release();
    }

    if (neededCoInit)
        CoUninitialize();

    return ret;
#else
    Q_UNUSED(workingDir)
    Q_UNUSED(arguments)
    return QFile::link(fileName, linkName);
#endif
}

/*
TRANSLATOR QInstaller::CreateShortcutOperation
*/

CreateShortcutOperation::CreateShortcutOperation()
{
    setName(QLatin1String("CreateShortcut"));
}

CreateShortcutOperation::~CreateShortcutOperation()
{
    deleteFileNowOrLater(value(QLatin1String("backupOfExistingShortcut")).toString());
}

static bool isWorkingDirOption(const QString& s) {
    return s.startsWith(QLatin1String("workingDirectory="));
}

static QString getWorkingDir(QStringList& args) {
    QString workingDir;
    // if the args contain an option in the form "workingDirectory=...", find it and consume it
    QStringList::iterator wdiropt = std::find_if(args.begin(), args.end(), isWorkingDirOption);
    if (wdiropt != args.end()) {
        workingDir = wdiropt->mid(QString::fromLatin1("workingDirectory=").size());
        args.erase(wdiropt);
    }
    return workingDir;
}

void CreateShortcutOperation::backup()
{
    QStringList args = this->arguments();
    getWorkingDir(args); //consume workingDirectory= option

    const QString path = QDir::fromNativeSeparators(QFileInfo(args.at(1)).absolutePath());

    //verbose() << "dir to create shortcut in " << path << std::endl;

    QDir createdDir = QDir::root();

    // find out, which part of the path is the first one we actually need to create
    int end = 0;
    QStringList directoriesToCreate;
    while (true) {
        QString p = path.section(QLatin1String("/"), 0, ++end);
        createdDir = QDir(p);
        if (!createdDir.exists()) {
            directoriesToCreate.push_back(QDir::toNativeSeparators(createdDir.absolutePath()));
            verbose() << " backup created dir_pre " << QDir::toNativeSeparators(createdDir.absolutePath()) << std::endl;
            if (p == path)
                break;

        } else if(p == path) {
            // everything did already exist -> nothing to do for us (nothing to revert then, either)
            createdDir = QDir::root();
            break;
        }
    }
    verbose() << " backup created dir " << createdDir.absolutePath() << std::endl;

    setValue(QLatin1String("createddirs"), directoriesToCreate);

    //link creation context
    const QString linkLocation = arguments()[1];
    if (!QFile::exists(linkLocation))
        return;

    try {
        setValue(QLatin1String("backupOfExistingShortcut"), generateTemporaryFileName(linkLocation));
    } catch (const QInstaller::Error& e) {
        setErrorString(e.message());
        return;
    }

    QFile f(linkLocation);
    if (!f.copy(value(QLatin1String("backupOfExistingShortcut")).toString()))
        setErrorString(QObject::tr("Could not backup file %1: %2").arg(linkLocation, f.errorString()));
}

bool CreateShortcutOperation::performOperation()
{
    QStringList args = this->arguments();
    const QString workingDir = getWorkingDir(args);

    if (args.count() != 2 && args.count() != 3) {
        setError(InvalidArguments);
        setErrorString(QObject::tr("Invalid arguments: %1 arguments given, 2 or 3 expected (optional: \"workingDirectory=...\").").arg(args.count()));
        return false;
    }

    const QString& linkTarget = args.at(0);
    const QString& linkLocation = args.at(1);
    const QString targetArguments = args.count() == 3 ? args[2] : QString();

    const QString dirName = QFileInfo(linkLocation).absolutePath();

    //verbose() << "dir to create shortcut in " << dirName << std::endl;

    errno = 0;

    const bool dirAlreadyExists = QDir(dirName).exists();
    const bool created = dirAlreadyExists || QDir::root().mkpath(dirName);

    if (!created) {
        setError(UserDefinedError);
        setErrorString(tr("Could not create folder %1: %2.").arg(QDir::toNativeSeparators(dirName), QLatin1String(strerror(errno))));
        return false;
    }

    TempDirDeleter deleter(dirName);

    if (dirAlreadyExists)
        deleter.releaseAll();

#if 0 // disabled for now, isDir() also returns true if the link exists and points to a folder, then removing it fails
    // link creation
    if (QFileInfo(linkLocation).isDir()) {
        errno = 0;
        if (!QDir().rmdir(linkLocation)) {
            setError(UserDefinedError);
            setErrorString(QObject::tr("Could not create link: failed to remove folder %1: %2").arg(QDir::toNativeSeparators(linkLocation), QLatin1String(strerror(errno))));
            return false;
        }
    }
#endif

    QString errorString;

    if (QFile::exists(linkLocation) && !deleteFileNowOrLater(linkLocation, &errorString)) {
        setError(UserDefinedError);
        setErrorString(QObject::tr("Failed to overwrite %1: %2").arg(QDir::toNativeSeparators(linkLocation),errorString));
        return false;
    }

    const bool linked = createLink(linkTarget, linkLocation, workingDir, targetArguments);
    if (!linked) {
        setError(UserDefinedError);
        setErrorString(tr("Could not create link %1: %2").arg(QDir::toNativeSeparators(linkLocation), qt_error_string()));
        return false;
    }
    deleter.releaseAll();
    return true;
}

bool CreateShortcutOperation::undoOperation()
{       
    const QString linkLocation = arguments()[ 1 ];
    const QStringList args = this->arguments();
    verbose() << " undo Shortcutoperation with arguments ";
    Q_FOREACH(const QString& val, args)
            verbose() << val << " ";
    verbose() << std::endl;

    // first remove the link
    if (!deleteFileNowOrLater(linkLocation)) {
        setErrorString(QObject::tr("Could not delete file %1").arg(linkLocation));
        return false;
    }
    verbose() << " link has been deleted " << std::endl;

    if (hasValue(QLatin1String("backupOfExistingShortcut"))) {
        const QString backupOfExistingShortcut = value(QLatin1String("backupOfExistingShortcut")).toString();
        const bool success = QFile::copy(backupOfExistingShortcut, linkLocation) && deleteFileNowOrLater(backupOfExistingShortcut);
        if (!success) {
            setErrorString(QObject::tr("Could not restore backup file into %1").arg(linkLocation));
            return success;
        }
    }
    verbose() << " got behin backup " << std::endl;

    const QStringList createdDirsPaths = value(QLatin1String("createddirs")).toStringList();
    if (createdDirsPaths.isEmpty()) //no dir to delete (QDir(createdDirPath) would return the current working directory -> never do that
        return true;

    const bool forceremoval = QVariant(value(QLatin1String("forceremoval"))).toBool();
    QListIterator<QString> it(createdDirsPaths);
    for (it.toBack(); it.hasPrevious();) {
        const QString createdDirPath = it.previous();
        const QDir createdDir = QDir(createdDirPath);
        verbose() << createdDir.absolutePath() << std::endl;
        if (createdDir == QDir::root())
            return true;
        QString errorString;
        if (forceremoval) {
            verbose() << " forced removal of path " << createdDir.path() << std::endl;
            try{
                removeDirectory(createdDir.path(), false);
            }catch(const QInstaller::Error e) {
                setError(UserDefinedError, e.message());
               return false;
            }
        } else {
            // even remove some hidden, OS-created files in there
#if defined Q_WS_MAC
            QFile::remove(createdDir.path() + QLatin1String("/.DS_Store"));
#elif defined Q_WS_WIN
            QFile::remove(createdDir.path() + QLatin1String("/Thumbs.db"));
#endif

            errno = 0;
            verbose() << " removal of path " << createdDir.path() << std::endl;
            const bool result = QDir::root().rmdir(createdDir.path());
            if (!result) {
                if (errorString.isEmpty())
                    setError(UserDefinedError, tr("Cannot remove directory %1: %2").arg(createdDir.path(), errorString));
                else
                    setError(UserDefinedError, tr("Cannot remove directory %1: %2").arg(createdDir.path(), QLatin1String(strerror(errno))));
                return result;
            }
        }
    }
    return true;
}

bool CreateShortcutOperation::testOperation()
{
    return true;
}

CreateShortcutOperation* CreateShortcutOperation::clone() const
{
    return new CreateShortcutOperation();
}
