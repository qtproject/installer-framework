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
#include "registerqtv23operation.h"

#include "constants.h"
#include "packagemanagercore.h"
#include "qtcreator_constants.h"
#include "persistentsettings.h"

#include <QString>
#include <QFileInfo>
#include <QDir>
#include <QSettings>
#include <QDebug>


using namespace QInstaller;

// TODO: move this to a general location it is used on some classes
static QString fromNativeSeparatorsAllOS(const QString &pathName)
{
    QString n = pathName;
    for (int i = 0; i < n.size(); ++i) {
        if (n.at(i) == QLatin1Char('\\'))
            n[i] = QLatin1Char('/');
    }
    return n;
}

static QString absoluteQmakePath(const QString &path)
{
    QString versionQmakePath = QDir(path).absolutePath();
    if (!versionQmakePath.endsWith(QLatin1String("qmake"))
         && !versionQmakePath.endsWith(QLatin1String("qmake.exe"))) {
#if defined (Q_OS_WIN)
        versionQmakePath.append(QLatin1String("/bin/qmake.exe"));
#elif defined(Q_OS_UNIX)
        versionQmakePath.append(QLatin1String("/bin/qmake"));
#endif
    }
    return fromNativeSeparatorsAllOS(versionQmakePath);
}

RegisterQtInCreatorV23Operation::RegisterQtInCreatorV23Operation()
{
    setName(QLatin1String("RegisterQtInCreatorV23"));
}

void RegisterQtInCreatorV23Operation::backup()
{
}

// Parameter List:
// Name - String displayed as name in Qt Creator
// qmake path - location of the qmake binary
// Type identifier - Desktop, Simulator, Symbian, ...
// SDK identifier - unique string to identify Qt version inside of the SDK (eg. desk473, simu11, ...)
// System Root Path
// sbs path
bool RegisterQtInCreatorV23Operation::performOperation()
{
    const QStringList args = arguments();

    if (args.count() < 4) {
        setError(InvalidArguments);
        setErrorString(tr("Invalid arguments in %0: %1 arguments given, minimum 4 expected.")
                        .arg(name()).arg(args.count()));
        return false;
    }

    PackageManagerCore *const core = qVariantValue<PackageManagerCore*>(value(QLatin1String("installer")));
    if (!core) {
        setError(UserDefinedError);
        setErrorString(tr("Needed installer object in \"%1\" operation is empty.").arg(name()));
        return false;
    }
    const QString &rootInstallPath = core->value(scTargetDir);
    if (rootInstallPath.isEmpty() || !QDir(rootInstallPath).exists()) {
        setError(UserDefinedError);
        setErrorString(tr("The given TargetDir %1 is not a valid/existing dir.").arg(rootInstallPath));
        return false;
    }

    const QString qtVersionsFileName = rootInstallPath
                                     + QLatin1String(QtVersionSettingsSuffixPath);
    int argCounter = 0;
    const QString &versionName = args.at(argCounter++);
    const QString &path = QDir::toNativeSeparators(args.value(argCounter++));
    const QString versionQmakePath = absoluteQmakePath(path);

    const QString &versionTypeIdentifier = args.at(argCounter++);
    const QString &versionSDKIdentifier = args.at(argCounter++);
    const QString &versionSystemRoot = fromNativeSeparatorsAllOS(args.value(argCounter++));
    const QString &versionSbsPath = fromNativeSeparatorsAllOS(args.value(argCounter++));

    ProjectExplorer::PersistentSettingsReader reader;
    int qtVersionCount = 0;
    QVariantMap map;
    if (reader.load(qtVersionsFileName)) {
        map = reader.restoreValues();
        qtVersionCount = map.value(QLatin1String("QtVersion.Count")).toInt();
        map.remove(QLatin1String("QtVersion.Count"));
        map.remove(QLatin1String("Version"));
    }

    ProjectExplorer::PersistentSettingsWriter writer;
    // Store old qt versions
    if (!map.isEmpty()) {
        for (int i = 0; i < qtVersionCount; ++i) {
            writer.saveValue(QString::fromLatin1("QtVersion.%1").arg(i)
                             , map[QLatin1String("QtVersion.") + QString::number(i)].toMap());
        }
        map.clear();
    }
    // Enter new version
    map.insert(QLatin1String("Id"), -1);
    map.insert(QLatin1String("Name"), versionName);
    map.insert(QLatin1String("QMakePath"), versionQmakePath);
    map.insert(QLatin1String("QtVersion.Type"),
               QLatin1String("Qt4ProjectManager.QtVersion.") + versionTypeIdentifier);
    map.insert(QLatin1String("isAutodetected"), true);
    map.insert(QLatin1String("autodetectionSource"),
               QLatin1String("SDK.") + versionSDKIdentifier);
    if (!versionSystemRoot.isEmpty())
        map.insert(QLatin1String("SystemRoot"), versionSystemRoot);
    if (!versionSbsPath.isEmpty())
        map.insert(QLatin1String("SBSv2Directory"), versionSbsPath);

    writer.saveValue(QLatin1String("QtVersion.") + QString::number(qtVersionCount), map);

    writer.saveValue(QLatin1String("Version"), 1);
    writer.saveValue(QLatin1String("QtVersion.Count"), qtVersionCount + 1);
    QDir().mkpath(QFileInfo(qtVersionsFileName).absolutePath());
    writer.save(qtVersionsFileName, QLatin1String("QtCreatorQtVersions"));

    return true;
}

bool RegisterQtInCreatorV23Operation::undoOperation()
{
    const QStringList args = arguments();

    if (args.count() < 4) {
        setError(InvalidArguments);
        setErrorString(tr("Invalid arguments in %0: %1 arguments given, minimum 4 expected.")
                        .arg(name()).arg(args.count()));
        return false;
    }

    PackageManagerCore *const core = qVariantValue<PackageManagerCore*>(value(QLatin1String("installer")));
    if (!core) {
        setError(UserDefinedError);
        setErrorString(tr("Needed installer object in \"%1\" operation is empty.").arg(name()));
        return false;
    }
    const QString &rootInstallPath = core->value(scTargetDir);

    const QString qtVersionsFileName = rootInstallPath
                                     + QLatin1String(QtVersionSettingsSuffixPath);

    ProjectExplorer::PersistentSettingsReader reader;
    // If no file, then it has been removed already
    if (!reader.load(qtVersionsFileName))
        return true;

    const QVariantMap map = reader.restoreValues();

    ProjectExplorer::PersistentSettingsWriter writer;
    const int qtVersionCount = map.value(QLatin1String("QtVersion.Count")).toInt();

    int currentVersionIndex = 0;
    for (int i = 0; i < qtVersionCount; ++i) {
        QVariantMap versionMap = map[QLatin1String("QtVersion.") + QString::number(i)].toMap();

        const QString path = QDir::toNativeSeparators(args.value(1));
        const QString versionQmakePath = absoluteQmakePath(path);

        //use absoluteQmakePath function to normalize the path string, for example //
        const QString existingQtQMakePath = absoluteQmakePath(
            versionMap[QLatin1String("QMakePath")].toString());
        if (existingQtQMakePath == versionQmakePath)
            continue;
        writer.saveValue(QString::fromLatin1("QtVersion.%1").arg(currentVersionIndex++), versionMap);
    }

    writer.saveValue(QLatin1String("QtVersion.Count"), currentVersionIndex);
    writer.saveValue(QLatin1String("Version"), map[QLatin1String("Version")].toInt());

    writer.save(qtVersionsFileName, QLatin1String("QtCreatorQtVersions"));
    return true;
}

bool RegisterQtInCreatorV23Operation::testOperation()
{
    return true;
}

Operation *RegisterQtInCreatorV23Operation::clone() const
{
    return new RegisterQtInCreatorV23Operation();
}
