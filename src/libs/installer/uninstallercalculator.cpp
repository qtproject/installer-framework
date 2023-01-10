/**************************************************************************
**
** Copyright (C) 2023 The Qt Company Ltd.
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

namespace QInstaller {

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::UninstallerCalculator
    \internal
*/

UninstallerCalculator::UninstallerCalculator(PackageManagerCore *core
         , const AutoDependencyHash &autoDependencyComponentHash
         , const LocalDependencyHash &localDependencyComponentHash
         , const QStringList &localVirtualComponents)
    : m_core(core)
    , m_autoDependencyComponentHash(autoDependencyComponentHash)
    , m_localDependencyComponentHash(localDependencyComponentHash)
    , m_localVirtualComponents(localVirtualComponents)
{
}

QSet<Component *> UninstallerCalculator::componentsToUninstall() const
{
    return m_componentsToUninstall;
}

QString UninstallerCalculator::componentsToUninstallError() const
{
    return m_componentsToUninstallError;
}

bool UninstallerCalculator::appendComponentToUninstall(Component *component)
{
    if (!component)
        return true;

    if (!component->isInstalled())
        return true;

    if (m_localDependencyComponentHash.contains(component->name())) {
        const QStringList &dependencies = PackageManagerCore::parseNames(m_localDependencyComponentHash.value(component->name()));
        for (const QString &dependencyComponent : dependencies) {
            Component *depComponent = m_core->componentByName(dependencyComponent);
            if (!depComponent || !depComponent->isInstalled())
                continue;

            if (m_componentsToUninstall.contains(depComponent)
                    || m_core->orderedComponentsToInstall().contains(depComponent)) {
                // Component is already selected for uninstall or update
                continue;
            }

            if (depComponent->isEssential() || depComponent->forcedInstallation()) {
                const QString errorMessage = QCoreApplication::translate("InstallerCalculator",
                    "Impossible dependency resolution detected. Forced install component \"%1\" would be uninstalled "
                    "because its dependency \"%2\" is marked for uninstallation with reason: \"%3\".")
                        .arg(depComponent->name(), component->name(), uninstallReason(component));

                qCWarning(QInstaller::lcInstallerInstallLog).noquote() << errorMessage;
                m_componentsToUninstallError.append(errorMessage);
                return false;
            }
            // Resolve also all cascading dependencies
            if (!appendComponentToUninstall(depComponent))
                return false;

            insertUninstallReason(depComponent, UninstallerCalculator::Dependent, component->name());
        }
    }

    m_componentsToUninstall.insert(component);
    return true;
}

bool UninstallerCalculator::appendComponentsToUninstall(const QList<Component*> &components)
{
    foreach (Component *component, components) {
        if (!appendComponentToUninstall(component))
            return false;
    }

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
                if (!m_componentsToUninstall.contains(autoDepComponent)) {
                    insertUninstallReason(autoDepComponent, UninstallerCalculator::AutoDependent, component->name());
                    autoDepComponent->setInstallAction(ComponentModelHelper::AutodependUninstallation);
                    autoDependOnList.append(autoDepComponent);
                 }
            }
        }
    }

    if (!autoDependOnList.isEmpty())
        appendComponentsToUninstall(autoDependOnList);
    else
        appendVirtualComponentsToUninstall();

    return true;
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

void UninstallerCalculator::appendVirtualComponentsToUninstall()
{
    QList<Component*> unneededVirtualList;
    // Check for virtual components without dependees
    for (const QString &componentName : qAsConst(m_localVirtualComponents)) {
        Component *virtualComponent = m_core->componentByName(componentName, m_core->components(PackageManagerCore::ComponentType::All));
        if (!virtualComponent)
            continue;

        if (virtualComponent->isInstalled() && !m_componentsToUninstall.contains(virtualComponent)) {
           // Components with auto dependencies were handled in the previous step
           if (!virtualComponent->autoDependencies().isEmpty() || virtualComponent->forcedInstallation())
               continue;

           if (!isRequiredVirtualPackage(virtualComponent)) {
               unneededVirtualList.append(virtualComponent);
               m_virtualComponentsForReverse.append(virtualComponent);
               insertUninstallReason(virtualComponent, UninstallerCalculator::VirtualDependent);
           }
        }
    }

    if (!unneededVirtualList.isEmpty())
        appendComponentsToUninstall(unneededVirtualList);
}

bool UninstallerCalculator::isRequiredVirtualPackage(Component *component)
{
    const QStringList localInstallDependents = m_core->localDependenciesToComponent(component);
    for (const QString &dependent : localInstallDependents) {
        Component *comp = m_core->componentByName(dependent);
        if (!m_componentsToUninstall.contains(comp)) {
            return true;
        }
    }
    return m_core->isDependencyForRequestedComponent(component);
}
} // namespace QInstaller
