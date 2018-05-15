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

#include "uninstallercalculator.h"

#include "component.h"
#include "packagemanagercore.h"
#include "globals.h"

#include <QDebug>

namespace QInstaller {

UninstallerCalculator::UninstallerCalculator(const QList<Component *> &installedComponents)
    : m_installedComponents(installedComponents)
{
}

QSet<Component *> UninstallerCalculator::componentsToUninstall() const
{
    return m_componentsToUninstall;
}

void UninstallerCalculator::appendComponentToUninstall(Component *component)
{
    if (!component)
        return;

    if (!component->isInstalled())
        return;

    PackageManagerCore *core = component->packageManagerCore();
    // remove all already resolved dependees
    QSet<Component *> dependees = core->dependees(component).toSet()
            .subtract(m_componentsToUninstall);

    foreach (Component *dependee, dependees)
        appendComponentToUninstall(dependee);

    m_componentsToUninstall.insert(component);
}

void UninstallerCalculator::appendComponentsToUninstall(const QList<Component*> &components)
{
    foreach (Component *component, components)
        appendComponentToUninstall(component);

    QList<Component*> autoDependOnList;
    // All regular dependees are resolved. Now we are looking for auto depend on components.
    foreach (Component *component, m_installedComponents) {
        // If a components is installed and not yet scheduled for un-installation, check for auto depend.
        if (component->isInstalled() && !m_componentsToUninstall.contains(component)) {
            QStringList autoDependencies = PackageManagerCore::parseNames(component->autoDependencies());
            if (autoDependencies.isEmpty())
                continue;

            // This code needs to be enabled once the scripts use isInstalled, installationRequested and
            // uninstallationRequested...
            if (autoDependencies.first().compare(scScript, Qt::CaseInsensitive) == 0) {
                //QScriptValue valueFromScript;
                //try {
                //    valueFromScript = callScriptMethod(QLatin1String("isAutoDependOn"));
                //} catch (const Error &error) {
                //    // keep the component, should do no harm
                //    continue;
                //}

                //if (valueFromScript.isValid() && !valueFromScript.toBool())
                //    autoDependOnList.append(component);
                continue;
            }

            foreach (Component *c, m_installedComponents) {
                const QString replaces = c->value(scReplaces);
                const QStringList possibleNames = replaces.split(QInstaller::commaRegExp(),
                                                                 QString::SkipEmptyParts) << c->name();
                foreach (const QString &possibleName, possibleNames) {

                    Component *cc = PackageManagerCore::componentByName(possibleName, m_installedComponents);
                    if (cc && (cc->installAction() != ComponentModelHelper::AutodependUninstallation)) {
                        autoDependencies.removeAll(possibleName);

                    }
                }
            }

            // A component requested auto uninstallation, keep it to resolve their dependencies as well.
            if (!autoDependencies.isEmpty()) {
                autoDependOnList.append(component);
                component->setInstallAction(ComponentModelHelper::AutodependUninstallation);
            }
        }
    }

    if (!autoDependOnList.isEmpty())
        appendComponentsToUninstall(autoDependOnList);
}

} // namespace QInstaller
