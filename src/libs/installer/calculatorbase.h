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

#ifndef CALCULATORBASE_H
#define CALCULATORBASE_H

#include "installer_global.h"

#include <QList>
#include <QString>
#include <QMetaEnum>

namespace QInstaller {

class Component;
class PackageManagerCore;

class INSTALLER_EXPORT CalculatorBase
{
public:
    enum class Resolution {
        Selected = 0,     // "Selected Component(s) without Dependencies" / "Deselected Component(s)"
        Replaced,         // "Component(s) replaced by other components"
        VirtualDependent, // "No dependencies to virtual component"
        Dependent,        // "Added as dependency for %1." / "Removed as dependency component is removed"
        Automatic,        // "Component(s) added as automatic dependencies" / "Removed as autodependency component is removed"
        Resolved,         // "Component(s) that have resolved Dependencies"
        Alias             // "Components added from selected alias"
    };

    CalculatorBase(PackageManagerCore *core);
    virtual ~CalculatorBase() = 0;

    virtual bool solve(const QList<Component *> &components) = 0;
    void insertResolution(Component *component, const Resolution resolutionType,
                          const QString &referencedComponent = QString());

    QList<Component *> resolvedComponents() const;
    virtual QString resolutionText(Component *component) const = 0;
    Resolution resolutionType(Component *component) const;

    QString error() const;

protected:
    virtual bool solveComponent(Component *component, const QString &version = QString()) = 0;
    QString referencedComponent(Component *component) const;

protected:
    PackageManagerCore *m_core;
    QString m_errorString;

    QList<Component *> m_resolvedComponents;
    QHash<QString, QPair<Resolution, QString> > m_componentNameResolutionHash;
};

} // namespace QInstaller

Q_DECLARE_METATYPE(QInstaller::CalculatorBase::Resolution)

#endif // CALCULATORBASE_H
