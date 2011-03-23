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
#include "qinstallercomponent_p.h"

#include "messageboxhandler.h"
#include "qinstaller.h"
#include "qinstallercomponent.h"

#include <QtGui/QApplication>
#include <QtGui/QDesktopServices>

namespace QInstaller {

QMap<const Component*, Qt::CheckState> ComponentPrivate::cachedCheckStates;

ComponentPrivate::ComponentPrivate(Installer* installer, Component* qq)
    : q(qq),
    m_flags(Qt::ItemIsEnabled | Qt::ItemIsSelectable| Qt::ItemIsUserCheckable),
    m_checkState(Qt::Unchecked),
    m_installer(installer),
    m_parent(0),
    m_offsetInInstaller(0),
    autoCreateOperations(true),
    operationsCreated(false),
    removeBeforeUpdate(true),
    isCheckedFromUpdater(false),
    m_newlyInstalled (false),
    operationsCreatedSuccessfully(true),
    minimumProgressOperation(0),
    m_licenseOperation(0)
{
}

void ComponentPrivate::init()
{
    // register translation stuff
    scriptEngine.installTranslatorFunctions();

    // register QMessageBox::StandardButton enum in the script connection
    registerMessageBox(&scriptEngine);

    // register QDesktopServices in the script connection
    QScriptValue desktopServices = scriptEngine.newArray();
    desktopServices.setProperty(QLatin1String("DesktopLocation"), scriptEngine.newVariant(static_cast< int >(QDesktopServices::DesktopLocation)));
    desktopServices.setProperty(QLatin1String("DocumentsLocation"), scriptEngine.newVariant(static_cast< int >(QDesktopServices::DocumentsLocation)));
    desktopServices.setProperty(QLatin1String("FontsLocation"), scriptEngine.newVariant(static_cast< int >(QDesktopServices::FontsLocation)));
    desktopServices.setProperty(QLatin1String("ApplicationsLocation"), scriptEngine.newVariant(static_cast< int >(QDesktopServices::ApplicationsLocation)));
    desktopServices.setProperty(QLatin1String("MusicLocation"), scriptEngine.newVariant(static_cast< int >(QDesktopServices::MusicLocation)));
    desktopServices.setProperty(QLatin1String("MoviesLocation"), scriptEngine.newVariant(static_cast< int >(QDesktopServices::MoviesLocation)));
    desktopServices.setProperty(QLatin1String("PicturesLocation"), scriptEngine.newVariant(static_cast< int >(QDesktopServices::PicturesLocation)));
    desktopServices.setProperty(QLatin1String("TempLocation"), scriptEngine.newVariant(static_cast< int >(QDesktopServices::TempLocation)));
    desktopServices.setProperty(QLatin1String("HomeLocation"), scriptEngine.newVariant(static_cast< int >(QDesktopServices::HomeLocation)));
    desktopServices.setProperty(QLatin1String("DataLocation"), scriptEngine.newVariant(static_cast< int >(QDesktopServices::DataLocation)));
    desktopServices.setProperty(QLatin1String("CacheLocation"), scriptEngine.newVariant(static_cast< int >(QDesktopServices::CacheLocation)));

    desktopServices.setProperty(QLatin1String("openUrl"), scriptEngine.newFunction(qDesktopServicesOpenUrl));
    desktopServices.setProperty(QLatin1String("displayName"), scriptEngine.newFunction(qDesktopServicesDisplayName));
    desktopServices.setProperty(QLatin1String("storageLocation"), scriptEngine.newFunction(qDesktopServicesStorageLocation));
    scriptEngine.globalObject().setProperty(QLatin1String("QDesktopServices"), desktopServices);

    // register ::WizardPage enum in the script connection
    QScriptValue qinstaller = scriptEngine.newArray();
    qinstaller.setProperty(QLatin1String("Introduction"), scriptEngine.newVariant(static_cast< int >(Installer::Introduction)));
    qinstaller.setProperty(QLatin1String("LicenseCheck"), scriptEngine.newVariant(static_cast< int >(Installer::LicenseCheck)));
    qinstaller.setProperty(QLatin1String("TargetDirectory"), scriptEngine.newVariant(static_cast< int >(Installer::TargetDirectory)));
    qinstaller.setProperty(QLatin1String("ComponentSelection"), scriptEngine.newVariant(static_cast< int >(Installer::ComponentSelection)));
    qinstaller.setProperty(QLatin1String("StartMenuSelection"), scriptEngine.newVariant(static_cast< int >(Installer::StartMenuSelection)));
    qinstaller.setProperty(QLatin1String("ReadyForInstallation"), scriptEngine.newVariant(static_cast< int >(Installer::ReadyForInstallation)));
    qinstaller.setProperty(QLatin1String("PerformInstallation"), scriptEngine.newVariant(static_cast< int >(Installer::PerformInstallation)));
    qinstaller.setProperty(QLatin1String("InstallationFinished"), scriptEngine.newVariant(static_cast< int >(Installer::InstallationFinished)));
    qinstaller.setProperty(QLatin1String("End"), scriptEngine.newVariant(static_cast< int >(Installer::End)));

    // register ::Status enum in the script connection
    qinstaller.setProperty(QLatin1String("InstallerSuccess"), scriptEngine.newVariant(static_cast< int >(Installer::Success)));
    qinstaller.setProperty(QLatin1String("InstallerSucceeded"), scriptEngine.newVariant(static_cast< int >(Installer::Success)));
    qinstaller.setProperty(QLatin1String("InstallerFailed"), scriptEngine.newVariant(static_cast< int >(Installer::Failure)));
    qinstaller.setProperty(QLatin1String("InstallerFailure"), scriptEngine.newVariant(static_cast< int >(Installer::Failure)));
    qinstaller.setProperty(QLatin1String("InstallerRunning"), scriptEngine.newVariant(static_cast< int >(Installer::Running)));
    qinstaller.setProperty(QLatin1String("InstallerCanceled"), scriptEngine.newVariant(static_cast< int >(Installer::Canceled)));
    qinstaller.setProperty(QLatin1String("InstallerCanceledByUser"), scriptEngine.newVariant(static_cast< int >(Installer::Canceled)));
    qinstaller.setProperty(QLatin1String("InstallerUnfinished"), scriptEngine.newVariant(static_cast< int >(Installer::Unfinished)));


    scriptEngine.globalObject().setProperty(QLatin1String("QInstaller"), qinstaller);
    scriptEngine.globalObject().setProperty(QLatin1String("component"), scriptEngine.newQObject(q));
    QScriptValue installerObject = scriptEngine.newQObject(m_installer);
    installerObject.setProperty(QLatin1String("componentByName"), scriptEngine.newFunction(qInstallerComponentByName, 1));
    scriptEngine.globalObject().setProperty(QLatin1String("installer"), installerObject);
}

void ComponentPrivate::setSelectedOnComponentList(const QList<Component*> &componentList,
    bool selected, RunModes runMode, int selectMode)
{
    foreach (Component *component, componentList) {
        if (!component->isSelected(runMode)) {
            // TODO: fix this or remove
            //component->setSelected(selected, runMode, selectMode);
        }
    }
}

}   // namespace QInstaller
