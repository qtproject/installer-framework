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

#include "installercalculator.h"

#include "component.h"
#include "packagemanagercore.h"
#include "settings.h"

#include <QDebug>

namespace QInstaller {

InstallerCalculator::InstallerCalculator(const QList<Component *> &allComponents)
    : m_allComponents(allComponents)
{
}

void InstallerCalculator::insertInstallReason(Component *component,
    InstallReasonType installReason, const QString &referencedComponentName)
{
    // keep the first reason
    if (m_toInstallComponentIdReasonHash.contains(component->name()))
        return;
    m_toInstallComponentIdReasonHash.insert(component->name(),
        qMakePair(installReason, referencedComponentName));
}

InstallerCalculator::InstallReasonType InstallerCalculator::installReasonType(Component *c) const
{
    return m_toInstallComponentIdReasonHash.value(c->name(),
        qMakePair(InstallerCalculator::Selected, QString())).first;
}

QString InstallerCalculator::installReasonReferencedComponent(Component *component) const
{
    return m_toInstallComponentIdReasonHash.value(component->name(),
        qMakePair(InstallerCalculator::Selected, QString())).second;
}

QString InstallerCalculator::installReason(Component *component) const
{
    InstallerCalculator::InstallReasonType reason = installReasonType(component);
    switch (reason) {
        case Automatic:
            return QCoreApplication::translate("InstallerCalculator",
                "Components added as automatic dependencies:");
        case Dependent:
            return QCoreApplication::translate("InstallerCalculator", "Components added as "
                "dependency for \"%1\":").arg(installReasonReferencedComponent(component));
        case Resolved:
            return QCoreApplication::translate("InstallerCalculator",
                "Components that have resolved dependencies:");
        case Selected:
            return QCoreApplication::translate("InstallerCalculator",
                "Selected components without dependencies:");
    }
    return QString();
}

QList<Component*> InstallerCalculator::orderedComponentsToInstall() const
{
    return m_orderedComponentsToInstall;
}

QString InstallerCalculator::componentsToInstallError() const
{
    return m_componentsToInstallError;
}

void InstallerCalculator::realAppendToInstallComponents(Component *component, const QString &version)
{
    if (!component->isInstalled(version) || component->updateRequested()) {
        m_orderedComponentsToInstall.append(component);
        m_toInstallComponentIds.insert(component->name());
    }
}

QString InstallerCalculator::recursionError(Component *component)
{
    return QCoreApplication::translate("InstallerCalculator", "Recursion detected, component \"%1\" "
        "already added with reason: \"%2\"").arg(component->name(), installReason(component));
}

bool InstallerCalculator::appendComponentsToInstall(const QList<Component *> &components)
{
    if (components.isEmpty())
        return true;

    QList<Component*> notAppendedComponents; // for example components with unresolved dependencies
    foreach (Component *component, components){
        if (m_toInstallComponentIds.contains(component->name())) {
            const QString errorMessage = recursionError(component);
            qWarning().noquote() << errorMessage;
            m_componentsToInstallError.append(errorMessage);
            Q_ASSERT_X(!m_toInstallComponentIds.contains(component->name()), Q_FUNC_INFO,
                qPrintable(errorMessage));
            return false;
        }

        if (component->dependencies().isEmpty())
            realAppendToInstallComponents(component);
        else
            notAppendedComponents.append(component);
    }

    foreach (Component *component, notAppendedComponents) {
        if (!appendComponentToInstall(component))
            return false;
    }

    QList<Component *> foundAutoDependOnList;
    // All regular dependencies are resolved. Now we are looking for auto depend on components.
    foreach (Component *component, m_allComponents) {
        // If a components is already installed or is scheduled for installation, no need to check
        // for auto depend installation.
        if ((!component->isInstalled() || component->updateRequested())
            && !m_toInstallComponentIds.contains(component->name())) {
                // If we figure out a component requests auto installation, keep it to resolve
                // their dependencies as well.
                if (component->isAutoDependOn(m_toInstallComponentIds)) {
                    foundAutoDependOnList.append(component);
                    insertInstallReason(component, InstallerCalculator::Automatic);
                }
        }
    }

    if (!foundAutoDependOnList.isEmpty())
        return appendComponentsToInstall(foundAutoDependOnList);
    return true;
}

bool InstallerCalculator::appendComponentToInstall(Component *component, const QString &version)
{
    QSet<QString> allDependencies = component->dependencies().toSet();
    QString requiredDependencyVersion = version;
    foreach (const QString &dependencyComponentName, allDependencies) {
        // PackageManagerCore::componentByName returns 0 if dependencyComponentName contains a
        // version which is not available
        Component *dependencyComponent =
            PackageManagerCore::componentByName(dependencyComponentName, m_allComponents);
        if (!dependencyComponent) {
            const QString errorMessage = QCoreApplication::translate("InstallerCalculator",
                "Cannot find missing dependency \"%1\" for \"%2\".").arg(dependencyComponentName,
                component->name());
            qWarning().noquote() << errorMessage;
            m_componentsToInstallError.append(errorMessage);
            if (component->packageManagerCore()->settings().allowUnstableComponents()) {
                component->setUnstable(PackageManagerCore::UnstableError::MissingDependency, errorMessage);
                continue;
            } else {
                return false;
            }
        }
        //Check if component requires higher version than what might be already installed
        bool isUpdateRequired = false;
        QString requiredName;
        QString requiredVersion;
        PackageManagerCore::parseNameAndVersion(dependencyComponentName, &requiredName, &requiredVersion);
        if (!requiredVersion.isEmpty() &&
                !dependencyComponent->value(scInstalledVersion).isEmpty()) {
            QRegExp compEx(QLatin1String("([<=>]+)(.*)"));
            const QString installedVersion = compEx.exactMatch(dependencyComponent->value(scInstalledVersion)) ?
                compEx.cap(2) : dependencyComponent->value(scInstalledVersion);

            requiredVersion = compEx.exactMatch(requiredVersion) ? compEx.cap(2) : requiredVersion;

            if (KDUpdater::compareVersion(requiredVersion, installedVersion) >= 1 ) {
                isUpdateRequired = true;
                requiredDependencyVersion = requiredVersion;
            }
        }
        //Check dependencies only if
        //- Dependency is not installed or update requested, nor newer version of dependency component required
        //- And dependency component is not already added for install
        //- And component is not already added for install, then dependencies are already resolved
        if (((!dependencyComponent->isInstalled() || dependencyComponent->updateRequested())
                || isUpdateRequired) && (!m_toInstallComponentIds.contains(dependencyComponent->name())
                && !m_toInstallComponentIds.contains(component->name()))) {
            if (m_visitedComponents.value(component).contains(dependencyComponent)) {
                const QString errorMessage = recursionError(component);
                qWarning().noquote() << errorMessage;
                m_componentsToInstallError = errorMessage;
                Q_ASSERT_X(!m_visitedComponents.value(component).contains(dependencyComponent),
                    Q_FUNC_INFO, qPrintable(errorMessage));
                return false;
            }
            m_visitedComponents[component].insert(dependencyComponent);

            // add needed dependency components to the next run
            insertInstallReason(dependencyComponent, InstallerCalculator::Dependent,
                component->name());

            if (!appendComponentToInstall(dependencyComponent, requiredDependencyVersion))
                return false;
        }
    }

    if (!m_toInstallComponentIds.contains(component->name())) {
        realAppendToInstallComponents(component, requiredDependencyVersion);
        insertInstallReason(component, InstallerCalculator::Resolved);
    }
    return true;
}

} // namespace QInstaller
