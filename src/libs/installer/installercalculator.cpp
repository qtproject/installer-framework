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

#include "installercalculator.h"

#include "component.h"
#include "packagemanagercore.h"
#include "settings.h"
#include <globals.h>

namespace QInstaller {

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::InstallerCalculator
    \internal
*/

InstallerCalculator::InstallerCalculator(PackageManagerCore *core, const AutoDependencyHash &autoDependencyComponentHash)
    : m_core(core)
    , m_autoDependencyComponentHash(autoDependencyComponentHash)
{
}

InstallerCalculator::InstallReasonType InstallerCalculator::installReasonType(const Component *c) const
{
    return m_toInstallComponentIdReasonHash.value(c->name(),
        qMakePair(InstallerCalculator::Selected, QString())).first;
}

QString InstallerCalculator::installReason(const Component *component) const
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

bool InstallerCalculator::appendComponentsToInstall(const QList<Component *> &components)
{
    if (components.isEmpty())
        return true;

    QList<Component*> notAppendedComponents; // for example components with unresolved dependencies
    for (Component *component : qAsConst(components)){
        if (!component)
            continue;
        if (m_toInstallComponentIds.contains(component->name())) {
            const QString errorMessage = recursionError(component);
            qCWarning(QInstaller::lcInstallerInstallLog).noquote() << errorMessage;
            m_componentsToInstallError.append(errorMessage);
            Q_ASSERT_X(!m_toInstallComponentIds.contains(component->name()), Q_FUNC_INFO,
                qPrintable(errorMessage));
            return false;
        }

        if (component->currentDependencies().isEmpty())
            realAppendToInstallComponents(component);
        else
            notAppendedComponents.append(component);
    }

    for (Component *component : qAsConst(notAppendedComponents)) {
        if (!appendComponentToInstall(component))
            return false;
    }

    // All regular dependencies are resolved. Now we are looking for auto depend on components.
    QSet<Component *> foundAutoDependOnList = autodependencyComponents();

    if (!foundAutoDependOnList.isEmpty())
        return appendComponentsToInstall(foundAutoDependOnList.values());
    return true;
}

QString InstallerCalculator::installReasonReferencedComponent(const Component *component) const
{
    return m_toInstallComponentIdReasonHash.value(component->name(),
        qMakePair(InstallerCalculator::Selected, QString())).second;
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

void InstallerCalculator::realAppendToInstallComponents(Component *component, const QString &version)
{
    if (!m_componentsForAutodepencencyCheck.contains(component))
        m_componentsForAutodepencencyCheck.append(component);

    if (!component->isInstalled(version) || (m_core->isUpdater() && component->isUpdateAvailable())) {
        m_orderedComponentsToInstall.append(component);
        m_toInstallComponentIds.insert(component->name());
    }
}

QString InstallerCalculator::recursionError(Component *component) const
{
    return QCoreApplication::translate("InstallerCalculator", "Recursion detected, component \"%1\" "
        "already added with reason: \"%2\"").arg(component->name(), installReason(component));
}

bool InstallerCalculator::appendComponentToInstall(Component *component, const QString &version)
{
    const QStringList dependenciesList = component->currentDependencies();
    QString requiredDependencyVersion = version;
    for (const QString &dependencyComponentName : dependenciesList) {
        // PackageManagerCore::componentByName returns 0 if dependencyComponentName contains a
        // version which is not available
        Component *dependencyComponent = m_core->componentByName(dependencyComponentName);
        if (!dependencyComponent) {
            const QString errorMessage = QCoreApplication::translate("InstallerCalculator",
                "Cannot find missing dependency \"%1\" for \"%2\".").arg(dependencyComponentName,
                component->name());
            qCWarning(QInstaller::lcInstallerInstallLog).noquote() << errorMessage;
            m_componentsToInstallError.append(errorMessage);
            if (component->packageManagerCore()->settings().allowUnstableComponents()) {
                component->setUnstable(Component::UnstableError::MissingDependency, errorMessage);
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
                qCWarning(QInstaller::lcInstallerInstallLog).noquote() << errorMessage;
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

QSet<Component *> InstallerCalculator::autodependencyComponents()
{
    // All regular dependencies are resolved. Now we are looking for auto depend on components.
    // m_componentsForAutodepencencyCheck is a list of recently calculated components to be installed
    // (normal components and regular dependencies components), and we check possible installable auto
    // dependency components based on that list.
    QSet<Component *> foundAutoDependOnList;
    for (const Component *component : qAsConst(m_componentsForAutodepencencyCheck)) {
        if (!m_autoDependencyComponentHash.contains(component->name())
                || (m_core->isUpdater() && !component->updateRequested()))
            continue;
        for (const QString& autoDependency : m_autoDependencyComponentHash.value(component->name())) {
            // If a components is already installed or is scheduled for installation, no need to check
            // for auto depend installation.
            if (m_toInstallComponentIds.contains(autoDependency)) {
                continue;
            }
            Component *autoDependComponent = m_core->componentByName(autoDependency);
            if (!autoDependComponent)
                continue;
            if ((!autoDependComponent->isInstalled()
                 || (m_core->isUpdater() && autoDependComponent->isUpdateAvailable()))
                && !m_toInstallComponentIds.contains(autoDependComponent->name())) {
                // One of the components autodependons is requested for install, check if there
                // are other autodependencies as well
                if (autoDependComponent->isAutoDependOn(m_toInstallComponentIds)) {
                    foundAutoDependOnList.insert(autoDependComponent);
                    insertInstallReason(autoDependComponent, InstallerCalculator::Automatic);
                }
            }
        }
    }
    m_componentsForAutodepencencyCheck.clear();
    return foundAutoDependOnList;
}

} // namespace QInstaller
