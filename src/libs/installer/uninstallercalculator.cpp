/**************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
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

bool UninstallerCalculator::appendComponentToUninstall(Component *component)
{
    if (!component)
        return false;

    PackageManagerCore *core = component->packageManagerCore();
    // remove all already resolved dependees
    QSet<Component *> dependees = core->dependees(component).toSet()
            .subtract(m_componentsToUninstall);
    if (dependees.isEmpty()) {
        component->setCheckState(Qt::Unchecked);
        m_componentsToUninstall.insert(component);
        return true;
    }

    QSet<Component *> dependeesToResolve;
    foreach (Component *dependee, dependees) {
        if (dependee->isInstalled()) {
            // keep them as already resolved
            dependee->setCheckState(Qt::Unchecked);
            m_componentsToUninstall.insert(dependee);
            // gather possible dependees, keep them to resolve it later
            dependeesToResolve.unite(core->dependees(dependee).toSet());
        }
    }

    bool allResolved = true;
    foreach (Component *dependee, dependeesToResolve)
        allResolved &= appendComponentToUninstall(dependee);

    return allResolved;
}

bool UninstallerCalculator::appendComponentsToUninstall(const QList<Component*> &components)
{
    if (components.isEmpty()) {
        qDebug() << "components list is empty in" << Q_FUNC_INFO;
        return true;
    }

    bool allResolved = true;
    foreach (Component *component, components) {
        if (component->isInstalled()) {
            component->setCheckState(Qt::Unchecked);
            m_componentsToUninstall.insert(component);
            allResolved &= appendComponentToUninstall(component);
        }
    }

    QList<Component*> autoDependOnList;
    if (allResolved) {
        // All regular dependees are resolved. Now we are looking for auto depend on components.
        foreach (Component *component, m_installedComponents) {
            // If a components is installed and not yet scheduled for un-installation, check for auto depend.
            if (component->isInstalled() && !m_componentsToUninstall.contains(component)) {
                QStringList autoDependencies = component->autoDependencies();
                if (autoDependencies.isEmpty())
                    continue;

                // This code needs to be enabled once the scripts use isInstalled, installationRequested and
                // uninstallationRequested...
                if (autoDependencies.first().compare(QLatin1String("script"), Qt::CaseInsensitive) == 0) {
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
                    foreach (const QString &possibleName, possibleNames)
                        autoDependencies.removeAll(possibleName);
                }

                // A component requested auto installation, keep it to resolve their dependencies as well.
                if (!autoDependencies.isEmpty())
                    autoDependOnList.append(component);
            }
        }
    }

    if (!autoDependOnList.isEmpty())
        return appendComponentsToUninstall(autoDependOnList);
    return allResolved;
}


} // namespace QInstaller
