/**************************************************************************
**
** Copyright (C) 2022 The Qt Company Ltd.
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

#include "uninstallercalculator.h"

#include "component.h"
#include "packagemanagercore.h"
#include "globals.h"

#include <QDebug>

namespace QInstaller {

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::UninstallerCalculator
    \internal
*/

UninstallerCalculator::UninstallerCalculator(const QList<Component *> &installedComponents
         , PackageManagerCore *core
         , const QHash<QString, QStringList> &autoDependencyComponentHash
         , const QHash<QString, QStringList> &dependencyComponentHash
         , const QStringList &localVirtualComponents)
    : m_installedComponents(installedComponents)
    , m_core(core)
    , m_autoDependencyComponentHash(autoDependencyComponentHash)
    , m_dependencyComponentHash(dependencyComponentHash)
    , m_localVirtualComponents(localVirtualComponents)
{
}

QSet<Component *> UninstallerCalculator::componentsToUninstall() const
{
    return m_componentsToUninstall;
}

void UninstallerCalculator::appendComponentToUninstall(Component *component, const bool reverse)
{
    if (!component)
        return;

    if (!component->isInstalled())
        return;

    if (m_dependencyComponentHash.contains(component->name())) {
        const QStringList &dependencies = PackageManagerCore::parseNames(m_dependencyComponentHash.value(component->name()));
        for (const QString &dependencyComponent : dependencies) {
            Component *depComponent = m_core->componentByName(dependencyComponent);
             if (depComponent && depComponent->isInstalled() && !m_componentsToUninstall.contains(depComponent)) {
                 appendComponentToUninstall(depComponent, reverse);
                 insertUninstallReason(depComponent, UninstallerCalculator::Dependent, component->name());
             } else if (reverse) {
                 appendComponentToUninstall(depComponent, true);
             }
        }
    }
    if (reverse) {
        m_componentsToUninstall.remove(component);
    } else {
        m_componentsToUninstall.insert(component);
    }
}

void UninstallerCalculator::appendComponentsToUninstall(const QList<Component*> &components, const bool reverse)
{
    if (components.isEmpty())
        return;
    foreach (Component *component, components)
        appendComponentToUninstall(component, reverse);
    QList<Component*> autoDependOnList;
    // All regular dependees are resolved. Now we are looking for auto depend on components.
    for (Component *component : components) {
        // If a components is installed and not yet scheduled for un-installation, check for auto depend.
        if (!m_autoDependencyComponentHash.contains(component->name()))
            continue;
        const QStringList autoDependencies = PackageManagerCore::parseNames(m_autoDependencyComponentHash.value(component->name()));
        for (const QString &autoDependencyComponent : autoDependencies) {
            Component *autoDepComponent = m_core->componentByName(autoDependencyComponent);
            if (autoDepComponent && autoDepComponent->isInstalled()) {
                // A component requested auto uninstallation, keep it to resolve their dependencies as well.
                if (reverse) {
                    autoDependOnList.append(autoDepComponent);
                } else if (!m_componentsToUninstall.contains(autoDepComponent)) {
                    insertUninstallReason(autoDepComponent, UninstallerCalculator::AutoDependent, component->name());
                    autoDepComponent->setInstallAction(ComponentModelHelper::AutodependUninstallation);
                    autoDependOnList.append(autoDepComponent);
                 }
            }
        }
    }
    if (!autoDependOnList.isEmpty())
        appendComponentsToUninstall(autoDependOnList, reverse);
    else
        appendVirtualComponentsToUninstall(reverse);
}

void UninstallerCalculator::removeComponentsFromUnInstall(const QList<Component*> &components)
{
    appendComponentsToUninstall(components, true);
}

void UninstallerCalculator::insertUninstallReason(Component *component, const UninstallReasonType uninstallReason,
                                                  const QString &referencedComponentName)
{
    // keep the first reason
    if (m_toUninstallComponentIdReasonHash.contains(component->name()))
        return;
    m_toUninstallComponentIdReasonHash.insert(component->name(),
        qMakePair(uninstallReason, referencedComponentName));
}

QString UninstallerCalculator::uninstallReason(Component *component) const
{
    UninstallerCalculator::UninstallReasonType reason = uninstallReasonType(component);
    switch (reason) {
        case Selected:
            return QCoreApplication::translate("UninstallerCalculator",
                "Deselected Components:");
        case Replaced:
            return QCoreApplication::translate("UninstallerCalculator", "Components replaced "
                "by \"%1\":").arg(uninstallReasonReferencedComponent(component));
        case VirtualDependent:
            return QCoreApplication::translate("UninstallerCalculator",
                "Removing virtual components without existing dependencies:");
        case Dependent:
            return QCoreApplication::translate("UninstallerCalculator", "Components "
                "dependency \"%1\" removed:").arg(uninstallReasonReferencedComponent(component));
        case AutoDependent:
            return QCoreApplication::translate("UninstallerCalculator", "Components "
                "autodependency \"%1\" removed:").arg(uninstallReasonReferencedComponent(component));
    }
    return QString();
}

UninstallerCalculator::UninstallReasonType UninstallerCalculator::uninstallReasonType(Component *c) const
{
    return m_toUninstallComponentIdReasonHash.value(c->name()).first;
}

QString UninstallerCalculator::uninstallReasonReferencedComponent(Component *component) const
{
    return m_toUninstallComponentIdReasonHash.value(component->name()).second;
}

void UninstallerCalculator::appendVirtualComponentsToUninstall(const bool reverse)
{
    QList<Component*> unneededVirtualList;

    // Check for virtual components without dependees
    if (reverse) {
        for (Component *reverseFromUninstall : qAsConst(m_virtualComponentsForReverse)) {
            if (m_componentsToUninstall.contains(reverseFromUninstall)) {
                bool required = false;
                // Check if installed or about to be updated -packages are dependant on the package
                const QList<Component*> installDependants = m_core->installDependants(reverseFromUninstall);
                for (Component *dependant : installDependants) {
                    if (dependant->isInstalled() && !m_componentsToUninstall.contains(dependant)) {
                        required = true;
                        break;
                    }
                }
                if (required) {
                    unneededVirtualList.append(reverseFromUninstall);
                }
            }
        }
    } else {
        for (const QString &componentName : qAsConst(m_localVirtualComponents)) {
            Component *virtualComponent = m_core->componentByName(componentName);
            if (virtualComponent->isInstalled() && !m_componentsToUninstall.contains(virtualComponent)) {
               // Components with auto dependencies were handled in the previous step
               if (!virtualComponent->autoDependencies().isEmpty() || virtualComponent->forcedInstallation())
                   continue;

               bool required = false;
               // Check if installed or about to be updated -packages are dependant on the package
               const QList<Component*> installDependants = m_core->installDependants(virtualComponent);
               for (Component *dependant : installDependants) {
                   if (dependant->isInstalled() && !m_componentsToUninstall.contains(dependant)) {
                       required = true;
                       break;
                   }
               }
               if (!required) {
                   unneededVirtualList.append(virtualComponent);
                   m_virtualComponentsForReverse.append(virtualComponent);
                   insertUninstallReason(virtualComponent, UninstallerCalculator::VirtualDependent);
               }
            }
        }
    }

    if (!unneededVirtualList.isEmpty())
        appendComponentsToUninstall(unneededVirtualList, reverse);
}

} // namespace QInstaller
