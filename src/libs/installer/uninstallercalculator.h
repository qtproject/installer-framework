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
#ifndef UNINSTALLERCALCULATOR_H
#define UNINSTALLERCALCULATOR_H

#include "installer_global.h"
#include "qinstallerglobal.h"

#include <QHash>
#include <QList>
#include <QSet>
#include <QString>

namespace QInstaller {

class Component;
class PackageManagerCore;

class INSTALLER_EXPORT UninstallerCalculator
{
public:
    enum UninstallReasonType
    {
        Selected,         // "Deselected Component(s)"
        Replaced,         // "Component(s) replaced by other components"
        VirtualDependent, // "No dependencies to virtual component"
        Dependent,        // "Removed as dependency component is removed"
        AutoDependent     // "Removed as autodependency component is removed"
    };

    UninstallerCalculator(const QList<Component *> &installedComponents, PackageManagerCore *core,
                          const AutoDependencyHash &autoDependencyComponentHash,
                          const LocalDependencyHash &localDependencyComponentHash,
                          const QStringList &localVirtualComponents);

    QSet<Component*> componentsToUninstall() const;

    void appendComponentsToUninstall(const QList<Component*> &components, const bool reverse = false);
    void removeComponentsFromUnInstall(const QList<Component*> &components);
    void insertUninstallReason(Component *component,
                               const UninstallReasonType uninstallReason,
                               const QString &referencedComponentName = QString());
    QString uninstallReason(Component *component) const;
    UninstallerCalculator::UninstallReasonType uninstallReasonType(Component *c) const;
    QString uninstallReasonReferencedComponent(Component *component) const;
    bool isRequiredVirtualPackage(Component *component);
    void appendVirtualComponentsToUninstall(const bool reverse);

private:
    void appendComponentToUninstall(Component *component, const bool reverse);

    QList<Component *> m_installedComponents;
    QSet<Component *> m_componentsToUninstall;
    PackageManagerCore *m_core;
    QHash<QString, QPair<UninstallReasonType, QString> > m_toUninstallComponentIdReasonHash;
    AutoDependencyHash m_autoDependencyComponentHash;
    LocalDependencyHash m_localDependencyComponentHash;
    QStringList m_localVirtualComponents;
    QList<Component *> m_virtualComponentsForReverse;
};

}


#endif // UNINSTALLERCALCULATOR_H
