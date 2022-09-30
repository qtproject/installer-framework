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
#ifndef INSTALLERCALCULATOR_H
#define INSTALLERCALCULATOR_H

#include "installer_global.h"
#include "qinstallerglobal.h"

#include <QHash>
#include <QList>
#include <QSet>
#include <QString>

namespace QInstaller {

class Component;

class INSTALLER_EXPORT InstallerCalculator
{
public:
    InstallerCalculator(PackageManagerCore *core, const AutoDependencyHash &autoDependencyComponentHash);

    enum InstallReasonType
    {
        Selected,   // "Selected Component(s) without Dependencies"
        Automatic, // "Component(s) added as automatic dependencies"
        Dependent, // "Added as dependency for %1."
        Resolved  // "Component(s) that have resolved Dependencies"
    };

    InstallReasonType installReasonType(const Component *c) const;
    QString installReason(const Component *component) const;
    QList<Component*> orderedComponentsToInstall() const;
    QString componentsToInstallError() const;

    bool appendComponentsToInstall(const QList<Component*> &components, bool modelReset = false, const bool revertFromInstall = false);
    bool removeComponentsFromInstall(const QList<Component*> &components);

private:
    QString installReasonReferencedComponent(const Component *component) const;
    void insertInstallReason(const Component *component,
                             const InstallReasonType installReason,
                             const QString &referencedComponentName = QString(),
                             const bool revertFromInstall = false);
    void realAppendToInstallComponents(Component *component, const QString &version, const bool revertFromInstall);
    bool appendComponentToInstall(Component *component, const QString &version, const bool revertFromInstall);
    QString recursionError(const Component *component) const;
    QSet<Component *> autodependencyComponents(const bool revertFromInstall);
    void calculateComponentDependencyReferences(const QString dependencyComponentName, const Component *component);

private:
    PackageManagerCore *m_core;
    QHash<Component*, QSet<Component*> > m_visitedComponents;
    QList<const Component*> m_componentsForAutodepencencyCheck;
    //for faster lookups.
    QSet<QString> m_toInstallComponentIds;
    QHash<QString, QStringList> m_referenceCount;
    QString m_componentsToInstallError;
    //calculate installation order variables
    QList<Component*> m_orderedComponentsToInstall;
    //we can't use this reason hash as component id hash, because some reasons are ready before
    //the component is added
    QHash<QString, QPair<InstallReasonType, QString> > m_toInstallComponentIdReasonHash;
    //Helper hash for quicker search for autodependency components
    AutoDependencyHash m_autoDependencyComponentHash;
};

}


#endif // INSTALLERCALCULATOR_H
