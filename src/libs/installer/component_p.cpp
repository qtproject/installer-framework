/**************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
**************************************************************************/

#include "component_p.h"

#include "component.h"
#include "packagemanagercore.h"

#include <QWidget>

namespace QInstaller {


// -- ComponentPrivate

ComponentPrivate::ComponentPrivate(PackageManagerCore *core, Component *qq)
    : q(qq)
    , m_core(core)
    , m_parentComponent(nullptr)
    , m_licenseOperation(nullptr)
    , m_minimumProgressOperation(nullptr)
    , m_newlyInstalled (false)
    , m_operationsCreated(false)
    , m_autoCreateOperations(true)
    , m_operationsCreatedSuccessfully(true)
    , m_updateIsAvailable(false)
{
}

ComponentPrivate::~ComponentPrivate()
{
    // Before we can delete the added widgets, they need to be removed from the wizard first.
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

ScriptEngine *ComponentPrivate::scriptEngine() const
{
    return m_core->componentScriptEngine();
}

// -- ComponentModelHelper

ComponentModelHelper::ComponentModelHelper()
{
    setCheckState(Qt::Unchecked);
    setFlags(Qt::ItemFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable));
}

/*!
    Returns the number of child components. Depending if virtual components are visible or not,
    the count might differ from what one will get if calling Component::childComponents(...).count().
*/
int ComponentModelHelper::childCount() const
{
    if (m_componentPrivate->m_core->virtualComponentsVisible())
        return m_componentPrivate->m_allChildComponents.count();
    return m_componentPrivate->m_childComponents.count();
}

/*!
    Returns the component at index position in the list. Index must be a valid position in
    the list (i.e., index >= 0 && index < childCount()). Otherwise it returns 0.
*/
Component *ComponentModelHelper::childAt(int index) const
{
    if (index < 0 && index >= childCount())
        return nullptr;

    if (m_componentPrivate->m_core->virtualComponentsVisible())
        return m_componentPrivate->m_allChildComponents.value(index, nullptr);
    return m_componentPrivate->m_childComponents.value(index, nullptr);
}

/*!
    Returns all descendants of this component depending if virtual components are visible or not.
*/
QList<Component*> ComponentModelHelper::childItems() const
{
    QList<Component*> *components = &m_componentPrivate->m_childComponents;
    if (m_componentPrivate->m_core->virtualComponentsVisible())
        components = &m_componentPrivate->m_allChildComponents;

    QList<Component*> result;
    foreach (Component *const component, *components) {
        result.append(component);
        result += component->childItems();
    }
    return result;
}

/*!
    Determines if the installation status of the component can be changed. The default value is true.
*/
bool ComponentModelHelper::isEnabled() const
{
    return (flags() & Qt::ItemIsEnabled) != 0;
}

/*!
    Enables or disables the ability to change the installation status of the components.
*/
void ComponentModelHelper::setEnabled(bool enabled)
{
    changeFlags(enabled, Qt::ItemIsEnabled);
}

/*!
    Returns whether the component is tri-state; that is, if it's checkable with three separate states.
    The default value is false.
*/
bool ComponentModelHelper::isTristate() const
{
    return (flags() & Qt::ItemIsTristate) != 0;
}

/*!
    Sets whether the component is tri-state. If tri-state is true, the component is checkable with three
    separate states; otherwise, the component is checkable with two states.

    Note: this also requires that the component is checkable. \sa isCheckable()
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
    Returns whether the component is expanded by default. The default value is \c false.
*/
bool ComponentModelHelper::isExpandedByDefault() const
{
    return data(ComponentModelHelper::ExpandedByDefault).value<bool>();
}

/*!
    Sets whether the component is expanded by default. The default value is \c false.
*/
void ComponentModelHelper::setExpandedByDefault(bool expandedByDefault)
{
    setData(QVariant::fromValue<bool>(expandedByDefault), ComponentModelHelper::ExpandedByDefault);
}

ComponentModelHelper::InstallAction ComponentModelHelper::installAction() const
{
    return data(ComponentModelHelper::Action).value<ComponentModelHelper::InstallAction>();
}

void ComponentModelHelper::setInstallAction(ComponentModelHelper::InstallAction action)
{
    setData(QVariant::fromValue<ComponentModelHelper::InstallAction>(action), ComponentModelHelper::Action);
}

/*!
    Returns the item flags for the component. The item flags determine how the user can interact with the
    component.
*/
Qt::ItemFlags ComponentModelHelper::flags() const
{
    QVariant variant = data(Qt::UserRole - 1);
    if (!variant.isValid())
        return Qt::ItemFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
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
