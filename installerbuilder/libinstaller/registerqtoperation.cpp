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
#include "registerqtoperation.h"

#include "component.h"
#include "qtcreator_constants.h"
#include "qinstaller.h"
#include "registertoolchainoperation.h"
#include "registerqtv2operation.h"
#include "qtcreatorpersistentsettings.h"

#include <QString>
#include <QFileInfo>
#include <QDir>
#include <QSettings>
#include <QDebug>

using namespace QInstaller;


RegisterQtInCreatorOperation::RegisterQtInCreatorOperation()
{
    setName(QLatin1String("RegisterQtInCreator"));
}

RegisterQtInCreatorOperation::~RegisterQtInCreatorOperation()
{
}

void RegisterQtInCreatorOperation::backup()
{
}

bool RegisterQtInCreatorOperation::performOperation()
{
    const QStringList args = arguments();

    if( args.count() < 3) {
        setError(InvalidArguments);
        setErrorString(tr("Invalid arguments in %0: %1 arguments given, minimum 3 expected.")
                        .arg(name()).arg( args.count()));
        return false;
    }

    const QString &rootInstallPath = args.at(0); //for example "C:\\Nokia_SDK\\"
    const QString &versionName = args.at(1);
    const QString &path = args.at(2);
    QString mingwPath;
    QString s60SdkPath;
    QString gccePath;
    QString carbidePath;
    QString msvcPath;
    QString sbsPath;
    if (args.count() >= 4)
        mingwPath = args.at(3);
    if (args.count() >= 5)
        s60SdkPath = args.at(4);
    if (arguments().count() >= 6)
        gccePath = args.at(5);
    if (args.count() >= 7)
        carbidePath = args.at(6);
    if (args.count() >= 8)
        msvcPath = args.at(7);
    if (args.count() >= 9)
        sbsPath = args.at(8);

//this is for creator 2.2
    QInstaller::Installer* const installer = qVariantValue<Installer*>(value(QLatin1String(
        "installer")));
    if (!installer) {
        setError(UserDefinedError);
        setErrorString(tr("Needed installer object in \"%1\" operation is empty.").arg(name()));
        return false;
    }
    QString toolChainsXmlFilePath = rootInstallPath + QLatin1String(ToolChainSettingsSuffixPath);
    bool isCreator22 = false;
    //in case of the fake installer this component doesn't exist
    Component* creatorComponent = installer->componentByName(
        QLatin1String("com.nokia.ndk.tools.qtcreator.application"));
    if (creatorComponent) {
        QString creatorVersion = creatorComponent->value(QLatin1String("InstalledVersion"));
        isCreator22 = Installer::versionMatches(creatorVersion, QLatin1String("2.2"));
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
            operation.setValue(QLatin1String("installer"), QVariant::fromValue(installer));
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
            operation.setValue(QLatin1String("installer"), QVariant::fromValue(installer));
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
        registerQtInCreatorV2Operation.setValue(QLatin1String("installer"), QVariant::fromValue(installer));
        registerQtInCreatorV2Operation.setArguments(QStringList() << versionName << path
            << s60SdkPath << sbsPath);
        if (!registerQtInCreatorV2Operation.performOperation()) {
            setError(registerQtInCreatorV2Operation.error());
            setErrorString(registerQtInCreatorV2Operation.errorString());
            return false;
        }
        return true;
    }
//END - this is for creator 2.2

    QSettings settings(rootInstallPath + QLatin1String(QtCreatorSettingsSuffixPath),
                        QSettings::IniFormat);

    QString newVersions;
    QStringList oldNewQtVersions = settings.value(QLatin1String("NewQtVersions")
                                                  ).toString().split(QLatin1String(";"));

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
#if defined ( Q_OS_WIN )
    QString addedVersion = versionName + QLatin1Char('=') +
                           QDir(path).absoluteFilePath(QLatin1String("bin/qmake.exe")).replace(QLatin1String("/"), QLatin1String("\\"));
#elif defined( Q_OS_UNIX )
    QString addedVersion = versionName + QLatin1Char('=') +
                           QDir(path).absoluteFilePath(QLatin1String("bin/qmake"));
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
    const QStringList args = arguments();
    const QString &rootInstallPath = args.at(0); //for example "C:\\Nokia_SDK\\"
    const QString &versionName = args.at(1);
    Q_UNUSED(versionName)
    const QString &path = args.at(2);
    Q_UNUSED(path)
    QString mingwPath;
    QString s60SdkPath;
    QString gccePath;
    QString carbidePath;
    QString msvcPath;
    QString sbsPath;
    if (args.count() >= 4)
        mingwPath = args.at(3);
    if (args.count() >= 5)
        s60SdkPath = args.at(4);
    if (arguments().count() >= 6)
        gccePath = args.at(5);
    if (args.count() >= 7)
        carbidePath = args.at(6);
    if (args.count() >= 8)
        msvcPath = args.at(7);
    if (args.count() >= 9)
        sbsPath = args.at(8);

    QSettings settings(rootInstallPath + QLatin1String(QtCreatorSettingsSuffixPath),
                        QSettings::IniFormat);

    QString newVersions;
    QStringList oldNewQtVersions = settings.value(QLatin1String("NewQtVersions")
                                                  ).toString().split(QLatin1String(";"));

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

KDUpdater::UpdateOperation* RegisterQtInCreatorOperation::clone() const
{
    return new RegisterQtInCreatorOperation();
}
