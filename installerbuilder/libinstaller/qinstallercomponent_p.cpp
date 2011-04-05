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


// -- ComponentModelHelper

ComponentModelHelper::ComponentModelHelper()
{
    setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
}

ComponentModelHelper::~ComponentModelHelper()
{
}

/*!
    Returns the number of child components.
*/
int ComponentModelHelper::childCount() const
{
    if (m_componentPrivate->m_installer->virtualComponentsVisible())
        return m_componentPrivate->m_allComponents.count();
    return m_componentPrivate->m_components.count();
}

/*!
    Returns the index of this component as seen from it's parent.
*/
int ComponentModelHelper::indexInParent() const
{
    if (Component *parent = m_componentPrivate->m_parent->parentComponent())
        return parent->childComponents().indexOf(m_componentPrivate->m_parent);
    return 0;
}

/*!
    Returns all children and whose children depending if virtual components are visible or not.
*/
QList<Component*> ComponentModelHelper::childs() const
{
    QList<Component*> *components = &m_componentPrivate->m_components;
    if (m_componentPrivate->m_installer->virtualComponentsVisible())
        components = &m_componentPrivate->m_allComponents;

    QList<Component*> result;
    foreach (Component *component, *components) {
        result.append(component);
        result += component->childComponents(true);
    }
    return result;
}

/*!
    Returns the component at index position in the list. Index must be a valid position in
    the list (i.e., index >= 0 && index < childCount()). Otherwise it returns 0.
*/
Component* ComponentModelHelper::childAt(int index) const
{
    if (index >= 0 && index < childCount()) {
        if (m_componentPrivate->m_installer->virtualComponentsVisible())
            return m_componentPrivate->m_allComponents.value(index, 0);
        return m_componentPrivate->m_components.value(index, 0);
    }
    return 0;
}

/*!
    Determines if the components installations status can be changed. The default value is true.
*/
bool ComponentModelHelper::isEnabled() const
{
    return (flags() & Qt::ItemIsEnabled) != 0;
}

/*!
    Enables oder disables ability to change the components installations status.
*/
void ComponentModelHelper::setEnabled(bool enabled)
{
    changeFlags(enabled, Qt::ItemIsEnabled);
}

/*!
    Returns whether the component is tristate; that is, if it's checkable with three separate states.
    The default value is false.
*/
bool ComponentModelHelper::isTristate() const
{
    return (flags() & Qt::ItemIsTristate) != 0;
}

/*!
    Sets whether the component is tristate. If tristate is true, the component is checkable with three
    separate states; otherwise, the component is checkable with two states.

    (Note that this also requires that the component is checkable; see isCheckable().)
*/
void ComponentModelHelper::setTristate(bool tristate)
{
    changeFlags(tristate, Qt::ItemIsTristate);
}

/*!
    Returns whether the component is user-checkable. The default value is true.
*/
bool ComponentModelHelper::isCheckable() const
{
    return (flags() & Qt::ItemIsUserCheckable) != 0;
}

/*!
    Sets whether the component is user-checkable. If checkable is true, the component can be checked by the
    user; otherwise, the user cannot check the component. The delegate will render a checkable component
    with a check box next to the component's text.
*/
void ComponentModelHelper::setCheckable(bool checkable)
{
    if (checkable && !isCheckable()) {
        // make sure there's data for the checkstate role
        if (!data(Qt::CheckStateRole).isValid())
            setData(Qt::Unchecked, Qt::CheckStateRole);
    }
    changeFlags(checkable, Qt::ItemIsUserCheckable);
}

/*!
    Returns whether the component is selectable by the user. The default value is true.
*/
bool ComponentModelHelper::isSelectable() const
{
    return (flags() & Qt::ItemIsSelectable) != 0;
}

/*!
    Sets whether the component is selectable. If selectable is true, the component can be selected by the
    user; otherwise, the user cannot select the component.
*/
void ComponentModelHelper::setSelectable(bool selectable)
{
    changeFlags(selectable, Qt::ItemIsSelectable);
}

/*!
    Returns the item flags for the component. The item flags determine how the user can interact with the
    component.
*/
Qt::ItemFlags ComponentModelHelper::flags() const
{
    QVariant variant = data(Qt::UserRole - 1);
    if (!variant.isValid())
        return (Qt::ItemIsEnabled | Qt::ItemIsSelectable| Qt::ItemIsUserCheckable);
    return Qt::ItemFlags(variant.toInt());
}

/*!
    Sets the item flags for the component to flags. The item flags determine how the user can interact with
    the component. This is often used to disable an component.
*/
void ComponentModelHelper::setFlags(Qt::ItemFlags flags)
{
    setData(int(flags), Qt::UserRole - 1);
}

/*!
    Returns the checked state of the component.
*/
Qt::CheckState ComponentModelHelper::checkState() const
{
    return Qt::CheckState(qvariant_cast<int>(data(Qt::CheckStateRole)));
}

/*!
    Sets the check state of the component to be state.
*/
void ComponentModelHelper::setCheckState(Qt::CheckState state)
{
    setData(state, Qt::CheckStateRole);
}

/*!
    Returns the components's data for the given role, or an invalid QVariant if there is no data for role.
*/
QVariant ComponentModelHelper::data(int role) const
{
    return m_values.value((role == Qt::EditRole ? Qt::DisplayRole : role), QVariant());
}

/*!
    Sets the component's data for the given role to the specified value.
*/
void ComponentModelHelper::setData(const QVariant &value, int role)
{
    m_values.insert((role == Qt::EditRole ? Qt::DisplayRole : role), value);
}

// -- protected

void ComponentModelHelper::setPrivate(ComponentPrivate *componentPrivate)
{
    m_componentPrivate = componentPrivate;
}

// -- private

void ComponentModelHelper::changeFlags(bool enable, Qt::ItemFlags itemFlags)
{
    setFlags(enable ? flags() |= itemFlags : flags() &= ~itemFlags);
}

}   // namespace QInstaller
