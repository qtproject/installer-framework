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
#include "registerqtoperation.h"

#include "component.h"
#include "packagemanagercore.h"
#include "qtcreator_constants.h"
#include "qtcreatorpersistentsettings.h"
#include "registertoolchainoperation.h"
#include "registerqtv2operation.h"

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QSettings>
#include <QtCore/QString>

using namespace QInstaller;


RegisterQtInCreatorOperation::RegisterQtInCreatorOperation()
{
    setName(QLatin1String("RegisterQtInCreator"));
}

void RegisterQtInCreatorOperation::backup()
{
}

bool RegisterQtInCreatorOperation::performOperation()
{
    const QStringList args = arguments();

    if (args.count() < 3) {
        setError(InvalidArguments);
        setErrorString(tr("Invalid arguments in %0: %1 arguments given, minimum 3 expected.").arg(name())
            .arg(args.count()));
        return false;
    }

    const QString &rootInstallPath = args.value(0); //for example "C:\\Nokia_SDK\\"
    const QString &versionName = args.value(1);
    const QString &path = args.value(2);
    QString mingwPath = args.value(3);
    QString s60SdkPath = args.value(4);
    QString gccePath = args.value(5);
    QString carbidePath = args.value(6);
    QString msvcPath = args.value(7);
    QString sbsPath = args.value(8);

//this is for creator 2.2
    PackageManagerCore *const core = qVariantValue<PackageManagerCore *>(value(QLatin1String("installer")));
    if (!core) {
        setError(UserDefinedError);
        setErrorString(tr("Needed installer object in \"%1\" operation is empty.").arg(name()));
        return false;
    }
    QString toolChainsXmlFilePath = rootInstallPath + QLatin1String(ToolChainSettingsSuffixPath);
    bool isCreator22 = false;
    //in case of the fake installer this component doesn't exist
    Component *creatorComponent =
        core->componentByName(QLatin1String("com.nokia.ndk.tools.qtcreator.application"));
    if (creatorComponent) {
        const QString creatorVersion = creatorComponent->value(scInstalledVersion);
        isCreator22 = PackageManagerCore::versionMatches(creatorVersion, QLatin1String("2.2"));
    }

    if (QFileInfo(toolChainsXmlFilePath).exists() || isCreator22) {
        QtCreatorPersistentSettings creatorToolChainSettings;

        if (!creatorToolChainSettings.init(toolChainsXmlFilePath)) {
            setError(UserDefinedError);
            setErrorString(tr("Can't read from tool chains xml file(%1) correctly.")
                .arg(toolChainsXmlFilePath));
            return false;
        }
        if (!mingwPath.isEmpty()) {
            RegisterToolChainOperation operation;
            operation.setValue(QLatin1String("installer"), QVariant::fromValue(core));
            operation.setArguments(QStringList()
                                   << QLatin1String("GccToolChain")
                                   << QLatin1String("ProjectExplorer.ToolChain.Mingw")
                                   << QLatin1String("Mingw as a GCC for Windows targets")
                                   << QLatin1String("x86-windows-msys-pe-32bit")
                                   << mingwPath + QLatin1String("\\bin\\g++.exe")
                                   << creatorToolChainSettings.abiToDebuggerHash().value(QLatin1String
                                        ("x86-windows-msys-pe-32bit"))
                                   );
            if (!operation.performOperation()) {
                setError(operation.error());
                setErrorString(operation.errorString());
                return false;
            }
        }
        if (!gccePath.isEmpty()) {
            RegisterToolChainOperation operation;
            operation.setValue(QLatin1String("installer"), QVariant::fromValue(core));
            operation.setArguments(QStringList()
                                   << QLatin1String("GccToolChain")
                                   << QLatin1String("Qt4ProjectManager.ToolChain.GCCE")
                                   << QLatin1String("GCCE 4 for Symbian targets")
                                   << QLatin1String("arm-symbian-device-elf-32bit")
                                   << gccePath + QLatin1String("\\bin\\arm-none-symbianelf-g++.exe")
                                   << creatorToolChainSettings.abiToDebuggerHash().value(QLatin1String(
                                        "arm-symbian-device-elf-32bit"))
                                   );
            if (!operation.performOperation()) {
                setError(operation.error());
                setErrorString(operation.errorString());
                return false;
            }
        }
        RegisterQtInCreatorV2Operation registerQtInCreatorV2Operation;
        registerQtInCreatorV2Operation.setValue(QLatin1String("installer"), QVariant::fromValue(core));
        registerQtInCreatorV2Operation.setArguments(QStringList() << versionName << path << s60SdkPath
            << sbsPath);
        if (!registerQtInCreatorV2Operation.performOperation()) {
            setError(registerQtInCreatorV2Operation.error());
            setErrorString(registerQtInCreatorV2Operation.errorString());
            return false;
        }
        return true;
    }
//END - this is for creator 2.2

    QSettings settings(rootInstallPath + QLatin1String(QtCreatorSettingsSuffixPath), QSettings::IniFormat);
    const QStringList oldNewQtVersions = settings.value(QLatin1String("NewQtVersions")).toString()
        .split(QLatin1String(";"));

    QString newVersions;
    //remove not existing Qt versions
    if (!oldNewQtVersions.isEmpty()) {
        foreach (const QString &qtVersion, oldNewQtVersions) {
            QStringList splitedQtConfiguration = qtVersion.split(QLatin1String("="));
            if (splitedQtConfiguration.count() > 1
                && splitedQtConfiguration.at(1).contains(QLatin1String("qmake"), Qt::CaseInsensitive)) {
                    QString qmakePath = splitedQtConfiguration.at(1);
                    if (QFile::exists(qmakePath))
                        newVersions.append(qtVersion + QLatin1String(";"));
            }
        }
    }
#if defined (Q_OS_WIN )
    QString addedVersion = versionName + QLatin1Char('=') + QDir(path)
        .absoluteFilePath(QLatin1String("bin/qmake.exe")).replace(QLatin1String("/"), QLatin1String("\\"));
#elif defined(Q_OS_UNIX )
    QString addedVersion = versionName + QLatin1Char('=') + QDir(path)
        .absoluteFilePath(QLatin1String("bin/qmake"));
#endif
    addedVersion += QLatin1Char('=') + mingwPath.replace(QLatin1String("/"), QLatin1String("\\"));
    addedVersion += QLatin1Char('=') + s60SdkPath.replace(QLatin1String("/"), QLatin1String("\\"));
    addedVersion += QLatin1Char('=') + gccePath.replace(QLatin1String("/"), QLatin1String("\\"));
    addedVersion += QLatin1Char('=') + carbidePath.replace(QLatin1String("/"), QLatin1String("\\"));
    addedVersion += QLatin1Char('=') + msvcPath.replace(QLatin1String("/"), QLatin1String("\\"));
    addedVersion += QLatin1Char('=') + sbsPath.replace(QLatin1String("/"), QLatin1String("\\"));
    newVersions += addedVersion;
    settings.setValue(QLatin1String("NewQtVersions"), newVersions);

    return true;
}

//works with creator 2.1 and 2.2
bool RegisterQtInCreatorOperation::undoOperation()
{
    const QString &rootInstallPath = arguments().value(0); //for example "C:\\Nokia_SDK\\"
    QSettings settings(rootInstallPath + QLatin1String(QtCreatorSettingsSuffixPath), QSettings::IniFormat);
    const QStringList oldNewQtVersions = settings.value(QLatin1String("NewQtVersions")).toString()
        .split(QLatin1String(";"));

    QString newVersions;
    //remove not existing Qt versions, the current to remove Qt version has an already removed qmake
    if (!oldNewQtVersions.isEmpty()) {
        foreach (const QString &qtVersion, oldNewQtVersions) {
            QStringList splitedQtConfiguration = qtVersion.split(QLatin1String("="));
            if (splitedQtConfiguration.count() > 1
                && splitedQtConfiguration.at(1).contains(QLatin1String("qmake"), Qt::CaseInsensitive)) {
                    QString qmakePath = splitedQtConfiguration.at(1);
                    if (QFile::exists(qmakePath))
                        newVersions.append(qtVersion + QLatin1String(";"));
            }
        }
    }
    settings.setValue(QLatin1String("NewQtVersions"), newVersions);
    return true;
}

bool RegisterQtInCreatorOperation::testOperation()
{
    return true;
}

Operation *RegisterQtInCreatorOperation::clone() const
{
    return new RegisterQtInCreatorOperation();
}
