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
#include "registerqtv2operation.h"

#include "constants.h"
#include "packagemanagercore.h"
#include "qtcreator_constants.h"

#include <QString>
#include <QFileInfo>
#include <QDir>
#include <QSettings>
#include <QDebug>

using namespace QInstaller;

RegisterQtInCreatorV2Operation::RegisterQtInCreatorV2Operation()
{
    setName(QLatin1String("RegisterQtInCreatorV2"));
}

void RegisterQtInCreatorV2Operation::backup()
{
}

//version name; qmake path; system root path; sbs path
bool RegisterQtInCreatorV2Operation::performOperation()
{
    const QStringList args = arguments();

    if (args.count() < 2) {
        setError(InvalidArguments);
        setErrorString( tr("Invalid arguments in %0: %1 arguments given, minimum 2 expected.")
                        .arg(name()).arg( args.count() ) );
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

    int argCounter = 0;
    const QString &versionName = args.value(argCounter++);
    const QString &path = QDir::toNativeSeparators(args.value(argCounter++));
    QString qmakePath = QDir(path).absolutePath();
    if ( !qmakePath.endsWith(QLatin1String("qmake"))
         && !qmakePath.endsWith(QLatin1String("qmake.exe")))
    {
#if defined ( Q_OS_WIN )
        qmakePath.append(QLatin1String("/bin/qmake.exe"));
#elif defined( Q_OS_UNIX )
        qmakePath.append(QLatin1String("/bin/qmake"));
#endif
    }
    qmakePath = QDir::toNativeSeparators(qmakePath);

    const QString &systemRoot = QDir::toNativeSeparators(args.value(argCounter++)); //Symbian SDK root for example
    const QString &sbsPath = QDir::toNativeSeparators(args.value(argCounter++));

    QSettings settings(rootInstallPath + QLatin1String(QtCreatorSettingsSuffixPath),
                        QSettings::IniFormat);

    QString newVersions;
    QStringList oldNewQtVersions = settings.value(QLatin1String("NewQtVersions")
        ).toString().split(QLatin1String(";"));

    //remove not existing Qt versions and the current new one(because its added after this)
    if (!oldNewQtVersions.isEmpty()) {
        foreach (const QString &qtVersion, oldNewQtVersions) {
            QStringList splitedQtConfiguration = qtVersion.split(QLatin1String("="));
            if (splitedQtConfiguration.value(1).contains(QLatin1String("qmake"),
                    Qt::CaseInsensitive)) {
                QString foundVersionName = splitedQtConfiguration.at(0);
                QString foundQmakePath = splitedQtConfiguration.at(1);
                    if (QFileInfo(qmakePath) != QFileInfo(foundQmakePath) && versionName != foundVersionName
                            && QFile::exists(foundQmakePath)) {
                        newVersions.append(qtVersion + QLatin1String(";"));
                    }
            }
        }
    }

    QString addedVersion = versionName;

    addedVersion += QLatin1Char('=') + qmakePath;
    addedVersion += QLatin1Char('=') + systemRoot;
    addedVersion += QLatin1Char('=') + sbsPath;
    newVersions += addedVersion;
    settings.setValue(QLatin1String("NewQtVersions"), newVersions);

    return true;
}

bool RegisterQtInCreatorV2Operation::undoOperation()
{
    const QStringList args = arguments();

    if (args.count() < 2) {
        setError(InvalidArguments);
        setErrorString( tr("Invalid arguments in %0: %1 arguments given, minimum 2 expected.")
                        .arg(name()).arg( args.count() ) );
        return false;
    }

    PackageManagerCore *const core = qVariantValue<PackageManagerCore*>(value(QLatin1String("installer")));
    if (!core) {
        setError(UserDefinedError);
        setErrorString(tr("Needed installer object in \"%1\" operation is empty.").arg(name()));
        return false;
    }
    const QString &rootInstallPath = core->value(scTargetDir);

    int argCounter = 0;
    const QString &versionName = args.value(argCounter++);
    const QString &path = QDir::toNativeSeparators(args.value(argCounter++));
    QString qmakePath = QDir(path).absolutePath();
    if (!qmakePath.endsWith(QLatin1String("qmake"))
         || !qmakePath.endsWith(QLatin1String("qmake.exe")))
    {
#if defined ( Q_OS_WIN )
        qmakePath.append(QLatin1String("/bin/qmake.exe"));
#elif defined( Q_OS_UNIX )
        qmakePath.append(QLatin1String("/bin/qmake"));
#endif
    }
    qmakePath = QDir::toNativeSeparators(qmakePath);

    QSettings settings(rootInstallPath + QLatin1String(QtCreatorSettingsSuffixPath),
                        QSettings::IniFormat);

    QString newVersions;
    QStringList oldNewQtVersions = settings.value(QLatin1String("NewQtVersions")
        ).toString().split(QLatin1String(";"));

    //remove the removed Qt version from "NewQtVersions" setting
    if (!oldNewQtVersions.isEmpty()) {
        foreach (const QString &qtVersion, oldNewQtVersions) {
            QStringList splitedQtConfiguration = qtVersion.split(QLatin1String("="));
            if (splitedQtConfiguration.value(1).contains(QLatin1String("qmake"),
                    Qt::CaseInsensitive)) {
                QString foundVersionName = splitedQtConfiguration.at(0);
                QString foundQmakePath = splitedQtConfiguration.at(1);
                    if (QFileInfo(qmakePath) != QFileInfo(foundQmakePath) &&versionName != foundVersionName
                            && QFile::exists(foundQmakePath)) {
                        newVersions.append(qtVersion + QLatin1String(";"));
                    }
            }
        }
    }
    settings.setValue(QLatin1String("NewQtVersions"), newVersions);
    return true;
}

bool RegisterQtInCreatorV2Operation::testOperation()
{
    return true;
}

Operation *RegisterQtInCreatorV2Operation::clone() const
{
    return new RegisterQtInCreatorV2Operation();
}
