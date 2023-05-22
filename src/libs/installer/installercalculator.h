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
#ifndef INSTALLERCALCULATOR_H
#define INSTALLERCALCULATOR_H

#include "installer_global.h"
#include "qinstallerglobal.h"
#include "calculatorbase.h"

#include <QHash>
#include <QList>
#include <QSet>
#include <QString>

namespace QInstaller {

class Component;
class ComponentAlias;
class PackageManagerCore;

class INSTALLER_EXPORT InstallerCalculator : public CalculatorBase
{
public:
    InstallerCalculator(PackageManagerCore *core, const AutoDependencyHash &autoDependencyComponentHash);
    ~InstallerCalculator();

    bool solve();
    bool solve(const QList<Component *> &components) override;
    bool solve(const QList<ComponentAlias *> &aliases);

    QString resolutionText(Component *component) const override;

private:
    bool solveComponent(Component *component, const QString &version = QString()) override;
    bool solveAlias(ComponentAlias *alias);

    void addComponentForInstall(Component *component, const QString &version = QString());
    bool addComponentsFromAlias(ComponentAlias *alias);
    QSet<Component *> autodependencyComponents();
    QString recursionError(Component *component) const;

    bool updateCheckState(Component *component, Qt::CheckState state);

private:
    QHash<Component*, QSet<Component*> > m_visitedComponents;
    QList<const Component*> m_componentsForAutodepencencyCheck;
    QSet<QString> m_toInstallComponentIds; //for faster lookups
    QSet<QString> m_toInstallComponentAliases;
    //Helper hash for quicker search for autodependency components
    AutoDependencyHash m_autoDependencyComponentHash;
};

} // namespace QInstaller

#endif // INSTALLERCALCULATOR_H
