/**************************************************************************
**
** This file is part of Qt SDK**
**
** Copyright (c) 2012 Nokia Corporation and/or its subsidiary(-ies).*
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
#include "createdesktopentryoperation.h"
#include "common/errors.h"
#include "common/fileutils.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QProcess>
#if QT_VERSION >= 0x040600
#include <QProcessEnvironment>
#endif

using namespace QInstaller;

QString CreateDesktopEntryOperation::absoluteFileName()
{
    const QString filename = arguments().first();

    // give filename is already absolute
    if (QFileInfo(filename).isAbsolute())
        return filename;

    // we're not searching for the first time, let's re-use the old value
    if (hasValue(QLatin1String("directory")))
        return QDir(value(QLatin1String("directory")).toString()).absoluteFilePath(filename);

    const QProcessEnvironment env;
    QStringList XDG_DATA_DIRS = env.value(QLatin1String("XDG_DATA_DIRS")).split(QLatin1Char(':'),
        QString::SkipEmptyParts);
    QStringList XDG_DATA_HOME = env.value(QLatin1String("XDG_DATA_HOME")).split(QLatin1Char(':'),
        QString::SkipEmptyParts);

    XDG_DATA_DIRS.push_back(QLatin1String("/usr/share")); // default path
    XDG_DATA_HOME.push_back(QDir::home().absoluteFilePath(QLatin1String(".local/share"))); // default path

    const QStringList directories = XDG_DATA_DIRS + XDG_DATA_HOME;
    QString directory;
    for (QStringList::const_iterator it = directories.begin(); it != directories.end(); ++it) {
        if (it->isEmpty())
            continue;

        directory = QDir(*it).absoluteFilePath(QLatin1String("applications"));
        QDir dir(directory);

        // let's see wheter this dir exists or we're able to create it
        if (!dir.exists() && !QDir().mkpath(directory))
            continue;

        // we just try wheter we're able to open the file in ReadWrite
        QFile file(QDir(directory).absoluteFilePath(filename));
        const bool existed = file.exists();
        if (!file.open(QIODevice::ReadWrite))
            continue;
        file.close();
        if (!existed)
            file.remove();
        break;
    }

    if (!QDir(directory).exists())
        QDir().mkpath(directory);

    setValue(QLatin1String("directory"), directory);

    return QDir(directory).absoluteFilePath(filename);
}

/*
TRANSLATOR QInstaller::CreateDesktopEntryOperation
*/

CreateDesktopEntryOperation::CreateDesktopEntryOperation()
{
    setName(QLatin1String("CreateDesktopEntry"));
}

CreateDesktopEntryOperation::~CreateDesktopEntryOperation()
{
    deleteFileNowOrLater(value(QLatin1String("backupOfExistingDesktopEntry")).toString());
}

void CreateDesktopEntryOperation::backup()
{
    const QString filename = absoluteFileName();
    if (!QFile::exists(filename))
        return;

    try {
        setValue(QLatin1String("backupOfExistingDesktopEntry"), generateTemporaryFileName(filename));
    } catch (const QInstaller::Error &e) {
        setErrorString(e.message());
        return;
    }

    if (!QFile::copy(filename, value(QLatin1String("backupOfExistingDesktopEntry")).toString()))
        setErrorString(QObject::tr("Could not backup file %1").arg(filename));
}

bool CreateDesktopEntryOperation::performOperation()
{
    const QStringList args = arguments();
    if (args.count() != 2) {
        setError(InvalidArguments);
        setErrorString(tr("Invalid arguments in %0: %1 arguments given, 2 expected.").arg(name()).arg(args
            .count()));
        return false;
    }

    const QString filename = absoluteFileName();
    const QString &values = args[1];

    if (QFile::exists(filename) && !deleteFileNowOrLater(filename)) {
        setError(UserDefinedError);
        setErrorString(tr("Failed to overwrite %1").arg(filename));
        return false;
    }

    QFile file(filename);
    if(!file.open(QIODevice::WriteOnly)) {
        setError(UserDefinedError);
        setErrorString(tr("Could not write Desktop Entry at %1").arg(filename));
        return false;
    }

    QFile::setPermissions(filename, QFile::ReadOwner | QFile::WriteOwner | QFile::ReadUser | QFile::ReadGroup
        | QFile::ReadOther);

    QTextStream stream(&file);
    stream.setCodec("UTF-8");
    stream << QLatin1String("[Desktop Entry]") << endl;
    stream << QLatin1String("Encoding=UTF-8") << endl;

    // Type=Application\nExec=qtcreator\nPath=...
    const QStringList pairs = values.split(QLatin1Char('\n'));
    for (QStringList::const_iterator it = pairs.begin(); it != pairs.end(); ++it)
        stream << *it << endl;

    return true;
}

bool CreateDesktopEntryOperation::undoOperation()
{
    const QString filename = absoluteFileName();

    // first remove the link
    if (!deleteFileNowOrLater(filename)) {
        setErrorString(QObject::tr("Could not delete file %1").arg(filename));
        return false;
    }

    if (!hasValue(QLatin1String("backupOfExistingDesktopEntry")))
        return true;

    const QString backupOfExistingDesktopEntry = value(QLatin1String("backupOfExistingDesktopEntry")).toString();
    const bool success = QFile::copy(backupOfExistingDesktopEntry, filename)
        && deleteFileNowOrLater(backupOfExistingDesktopEntry);
    if (!success)
        setErrorString(QObject::tr("Could not restore backup file into %1").arg(filename));

    return success;
}

bool CreateDesktopEntryOperation::testOperation()
{
    return true;
}

Operation *CreateDesktopEntryOperation::clone() const
{
    return new CreateDesktopEntryOperation();
}
