/**************************************************************************
**
** This file is part of Installer Framework
**
** Copyright (c) 2011-2012 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (qt-info@nokia.com)
**
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
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** Other Usage
**
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
** If you have questions regarding the use of this file, please contact
** Nokia at qt-info@nokia.com.
**
**************************************************************************/

#include "component_p.h"

#include "component.h"
#include "messageboxhandler.h"
#include "packagemanagercore.h"

#include <QtGui/QApplication>
#include <QtGui/QDesktopServices>


namespace QInstaller {

// -- ComponentPrivate

ComponentPrivate::ComponentPrivate(PackageManagerCore *core, Component *qq)
    : q(qq),
    m_core(core),
    m_parentComponent(0),
    m_licenseOperation(0),
    m_minimumProgressOperation(0),
    m_newlyInstalled (false),
    m_operationsCreated(false),
    m_removeBeforeUpdate(true),
    m_autoCreateOperations(true),
    m_operationsCreatedSuccessfully(true),
    m_updateIsAvailable(false),
    m_scriptEngine(0)
{
}

ComponentPrivate::~ComponentPrivate()
{
    // Before we can delete the added widgets, they need to be removed from the wizard first.
    QMap<QString, QPointer<QWidget> >::const_iterator it;
    foreach (const QString &widgetName, m_userInterfaces.keys()) {
        m_core->removeWizardPage(q, widgetName);
        m_core->removeWizardPageItem(q, widgetName);
    }

    // Use QPointer here instead of raw pointers. This is a requirement that needs to be met cause possible
    // Ui elements get added during component script run and might be destroyed by the package manager gui
    // before the actual component gets destroyed. Avoids a possible delete call on a dangling pointer.
    foreach (const QPointer<QWidget> widget, m_userInterfaces)
        delete widget.data();
}

QScriptEngine *ComponentPrivate::scriptEngine()
{
    if (m_scriptEngine != 0)
        return m_scriptEngine;


    m_scriptEngine = new QScriptEngine(q);

    // register translation stuff
    m_scriptEngine->installTranslatorFunctions();

    // register QMessageBox::StandardButton enum in the script connection
    registerMessageBox(m_scriptEngine);

    // register QDesktopServices in the script connection
    QScriptValue desktopServices = m_scriptEngine->newArray();
    setProperty(desktopServices, QLatin1String("DesktopLocation"), QDesktopServices::DesktopLocation);
    setProperty(desktopServices, QLatin1String("DocumentsLocation"), QDesktopServices::DocumentsLocation);
    setProperty(desktopServices, QLatin1String("FontsLocation"), QDesktopServices::FontsLocation);
    setProperty(desktopServices, QLatin1String("ApplicationsLocation"), QDesktopServices::ApplicationsLocation);
    setProperty(desktopServices, QLatin1String("MusicLocation"), QDesktopServices::MusicLocation);
    setProperty(desktopServices, QLatin1String("MoviesLocation"), QDesktopServices::MoviesLocation);
    setProperty(desktopServices, QLatin1String("PicturesLocation"), QDesktopServices::PicturesLocation);
    setProperty(desktopServices, QLatin1String("TempLocation"), QDesktopServices::TempLocation);
    setProperty(desktopServices, QLatin1String("HomeLocation"), QDesktopServices::HomeLocation);
    setProperty(desktopServices, QLatin1String("DataLocation"), QDesktopServices::DataLocation);
    setProperty(desktopServices, QLatin1String("CacheLocation"), QDesktopServices::CacheLocation);

    desktopServices.setProperty(QLatin1String("openUrl"), m_scriptEngine->newFunction(qDesktopServicesOpenUrl));
    desktopServices.setProperty(QLatin1String("displayName"),
        m_scriptEngine->newFunction(qDesktopServicesDisplayName));
    desktopServices.setProperty(QLatin1String("storageLocation"),
        m_scriptEngine->newFunction(qDesktopServicesStorageLocation));

    // register ::WizardPage enum in the script connection
    QScriptValue qinstaller = m_scriptEngine->newArray();
    setProperty(qinstaller, QLatin1String("Introduction"), PackageManagerCore::Introduction);
    setProperty(qinstaller, QLatin1String("LicenseCheck"), PackageManagerCore::LicenseCheck);
    setProperty(qinstaller, QLatin1String("TargetDirectory"), PackageManagerCore::TargetDirectory);
    setProperty(qinstaller, QLatin1String("ComponentSelection"), PackageManagerCore::ComponentSelection);
    setProperty(qinstaller, QLatin1String("StartMenuSelection"), PackageManagerCore::StartMenuSelection);
    setProperty(qinstaller, QLatin1String("ReadyForInstallation"), PackageManagerCore::ReadyForInstallation);
    setProperty(qinstaller, QLatin1String("PerformInstallation"), PackageManagerCore::PerformInstallation);
    setProperty(qinstaller, QLatin1String("InstallationFinished"), PackageManagerCore::InstallationFinished);
    setProperty(qinstaller, QLatin1String("End"), PackageManagerCore::End);

    // register ::Status enum in the script connection
    setProperty(qinstaller, QLatin1String("Success"), PackageManagerCore::Success);
    setProperty(qinstaller, QLatin1String("Failure"), PackageManagerCore::Failure);
    setProperty(qinstaller, QLatin1String("Running"), PackageManagerCore::Running);
    setProperty(qinstaller, QLatin1String("Canceled"), PackageManagerCore::Canceled);

    // maybe used by old scripts
    setProperty(qinstaller, QLatin1String("InstallerFailed"), PackageManagerCore::Failure);
    setProperty(qinstaller, QLatin1String("InstallerSucceeded"), PackageManagerCore::Success);
    setProperty(qinstaller, QLatin1String("InstallerUnfinished"), PackageManagerCore::Unfinished);
    setProperty(qinstaller, QLatin1String("InstallerCanceledByUser"), PackageManagerCore::Canceled);

    QScriptValue installerObject = m_scriptEngine->newQObject(m_core);
    installerObject.setProperty(QLatin1String("componentByName"), m_scriptEngine
        ->newFunction(qInstallerComponentByName, 1));

    m_scriptEngine->globalObject().setProperty(QLatin1String("QInstaller"), qinstaller);
    m_scriptEngine->globalObject().setProperty(QLatin1String("installer"), installerObject);
    m_scriptEngine->globalObject().setProperty(QLatin1String("QDesktopServices"), desktopServices);
    m_scriptEngine->globalObject().setProperty(QLatin1String("component"), m_scriptEngine->newQObject(q));

    return m_scriptEngine;
}

void ComponentPrivate::setProperty(QScriptValue &scriptValue, const QString &propertyName, int value)
{
    scriptValue.setProperty(propertyName, m_scriptEngine->newVariant(value));
}


// -- ComponentModelHelper

ComponentModelHelper::ComponentModelHelper()
{
    setCheckState(Qt::Unchecked);
    setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
}

/*!
    Returns the number of child components. Depending if virtual components are visible or not the count might
    differ from what one will get if calling Component::childComponents(...).count().
*/
int ComponentModelHelper::childCount() const
{
    if (m_componentPrivate->m_core->virtualComponentsVisible())
        return m_componentPrivate->m_allChildComponents.count();
    return m_componentPrivate->m_childComponents.count();
}

/*!
    Returns the index of this component as seen from it's parent.
*/
int ComponentModelHelper::indexInParent() const
{
    int index = 0;
    if (Component *parent = m_componentPrivate->m_parentComponent->parentComponent())
        index = parent->childComponents(false, AllMode).indexOf(m_componentPrivate->m_parentComponent);
    return (index >= 0 ? index : 0);
}

/*!
    Returns all children and whose children depending if virtual components are visible or not.
*/
QList<Component*> ComponentModelHelper::childs() const
{
    QList<Component*> *components = &m_componentPrivate->m_childComponents;
    if (m_componentPrivate->m_core->virtualComponentsVisible())
        components = &m_componentPrivate->m_allChildComponents;

    QList<Component*> result;
    foreach (Component *component, *components) {
        result.append(component);
        result += component->childs();
    }
    return result;
}

/*!
    Returns the component at index position in the list. Index must be a valid position in
    the list (i.e., index >= 0 && index < childCount()). Otherwise it returns 0.
*/
Component *ComponentModelHelper::childAt(int index) const
{
    if (index >= 0 && index < childCount()) {
        if (m_componentPrivate->m_core->virtualComponentsVisible())
            return m_componentPrivate->m_allChildComponents.value(index, 0);
        return m_componentPrivate->m_childComponents.value(index, 0);
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
        // make sure there's data for the check state role
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
    Returns the component's data for the given role, or an invalid QVariant if there is no data for role.
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
