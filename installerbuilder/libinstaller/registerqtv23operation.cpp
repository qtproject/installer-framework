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

namespace {
inline QString absoluteQmakePath(const QString &path)
{
    QString versionQmakePath = QDir(path).absolutePath();
    if ( !versionQmakePath.endsWith(QLatin1String("qmake"))
         && !versionQmakePath.endsWith(QLatin1String("qmake.exe")))
    {
#if defined ( Q_OS_WIN )
        versionQmakePath.append(QLatin1String("/bin/qmake.exe"));
#elif defined( Q_OS_UNIX )
        versionQmakePath.append(QLatin1String("/bin/qmake"));
#endif
    }
    return QDir::fromNativeSeparators(versionQmakePath);
}
}

RegisterQtInCreatorV23Operation::RegisterQtInCreatorV23Operation()
{
    setName(QLatin1String("RegisterQtInCreatorV23"));
}

RegisterQtInCreatorV23Operation::~RegisterQtInCreatorV23Operation()
{
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
                                     + QString::fromLatin1(QtVersionSettingsSuffixPath);
    int argCounter = 0;
    const QString &versionName = args.at(argCounter++);
    const QString &path = QDir::toNativeSeparators(args.value(argCounter++));
    const QString versionQmakePath = absoluteQmakePath(path);

    const QString &versionTypeIdentifier = args.at(argCounter++);
    const QString &versionSDKIdentifier = args.at(argCounter++);
    const QString &versionSystemRoot = QDir::fromNativeSeparators(args.value(argCounter++));
    const QString &versionSbsPath = QDir::fromNativeSeparators(args.value(argCounter++));

    ProjectExplorer::PersistentSettingsReader reader;
    int qtVersionCount = 0;
    QVariantMap map;
    if (reader.load(qtVersionsFileName)) {
        map = reader.restoreValues();
        qtVersionCount = map.value(QString::fromLatin1("QtVersion.Count")).toInt();
        map.remove(QString::fromLatin1("QtVersion.Count"));
        map.remove(QString::fromLatin1("Version"));
    }

    ProjectExplorer::PersistentSettingsWriter writer;
    // Store old qt versions
    if (!map.isEmpty()) {
        for (int i = 0; i < qtVersionCount; ++i) {
            writer.saveValue(QString::fromLatin1("QtVersion.%1").arg(i)
                             , map[QString::fromLatin1("QtVersion.") + QString::number(i)].toMap());
        }
        map.clear();
    }
    // Enter new version
    map.insert(QString::fromLatin1("Id"), -1);
    map.insert(QString::fromLatin1("Name"), versionName);
    map.insert(QString::fromLatin1("QMakePath"), versionQmakePath);
    map.insert(QString::fromLatin1("QtVersion.Type"),
               QString::fromLatin1("Qt4ProjectManager.QtVersion.") + versionTypeIdentifier);
    map.insert(QString::fromLatin1("isAutodetected"), true);
    map.insert(QString::fromLatin1("autodetectionSource"),
               QString::fromLatin1("SDK.") + versionSDKIdentifier);
    if (!versionSystemRoot.isEmpty())
        map.insert(QString::fromLatin1("SystemRoot"), versionSystemRoot);
    if (!versionSbsPath.isEmpty())
        map.insert(QString::fromLatin1("SBSv2Directory"), versionSbsPath);

    writer.saveValue(QString::fromLatin1("QtVersion.") + QString::number(qtVersionCount), map);

    writer.saveValue(QLatin1String("Version"), 1);
    writer.saveValue(QString::fromLatin1("QtVersion.Count"), qtVersionCount + 1);
    QDir().mkpath(QFileInfo(qtVersionsFileName).absolutePath());
    writer.save(qtVersionsFileName, QString::fromLatin1("QtCreatorQtVersions"));

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
                                     + QString::fromLatin1(QtVersionSettingsSuffixPath);

    ProjectExplorer::PersistentSettingsReader reader;
    // If no file, then it has been removed already
    if (!reader.load(qtVersionsFileName))
        return true;

    const QVariantMap map = reader.restoreValues();

    ProjectExplorer::PersistentSettingsWriter writer;
    const int qtVersionCount = map.value(QString::fromLatin1("QtVersion.Count")).toInt();

    int currentVersionIndex = 0;
    for (int i = 0; i < qtVersionCount; ++i) {
        QVariantMap versionMap = map[QString::fromLatin1("QtVersion.") + QString::number(i)].toMap();

        const QString path = QDir::toNativeSeparators(args.value(1));
        const QString versionQmakePath = absoluteQmakePath(path);

        if (versionMap[QString::fromLatin1("QMakePath")] == versionQmakePath)
            continue;
        writer.saveValue(QString::fromLatin1("QtVersion.%1").arg(currentVersionIndex++), versionMap);
    }

    writer.saveValue(QString::fromLatin1("QtVersion.Count"), currentVersionIndex);
    writer.saveValue(QString::fromLatin1("Version"), map[QString::fromLatin1("Version")].toInt());

    writer.save(qtVersionsFileName, QString::fromLatin1("QtCreatorQtVersions"));
    return true;
}

bool RegisterQtInCreatorV23Operation::testOperation()
{
    return true;
}

Operation* RegisterQtInCreatorV23Operation::clone() const
{
    return new RegisterQtInCreatorV23Operation();
}
