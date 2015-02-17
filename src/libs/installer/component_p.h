/**************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
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
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
**
** $QT_END_LICENSE$
**
**************************************************************************/

#ifndef COMPONENT_P_H
#define COMPONENT_P_H

#include "qinstallerglobal.h"

#include <QJSValue>
#include <QPointer>
#include <QStringList>
#include <QUrl>

namespace QInstaller {

class Component;
class PackageManagerCore;
class ScriptEngine;

class ComponentPrivate
{
    QInstaller::Component* const q;

public:
    explicit ComponentPrivate(PackageManagerCore *core, Component *qq);
    ~ComponentPrivate();

    ScriptEngine *scriptEngine() const;

    PackageManagerCore *m_core;
    Component *m_parentComponent;
    OperationList m_operations;
    Operation *m_licenseOperation;
    Operation *m_minimumProgressOperation;

    bool m_newlyInstalled;
    bool m_operationsCreated;
    bool m_autoCreateOperations;
    bool m_operationsCreatedSuccessfully;
    bool m_updateIsAvailable;

    QString m_componentName;
    QUrl m_repositoryUrl;
    QString m_localTempPath;
    QJSValue m_scriptContext;
    QHash<QString, QString> m_vars;
    QList<Component*> m_childComponents;
    QList<Component*> m_allChildComponents;
    QStringList m_downloadableArchives;
    QStringList m_stopProcessForUpdateRequests;
    QHash<QString, QPointer<QWidget> > m_userInterfaces;

    // < display name, < file name, file content > >
    QHash<QString, QPair<QString, QString> > m_licenses;
    QList<QPair<QString, bool> > m_pathsForUninstallation;
};


// -- ComponentModelHelper

class INSTALLER_EXPORT ComponentModelHelper
{
public:
    enum Roles {
        Action = Qt::UserRole + 1,
        LocalDisplayVersion,
        RemoteDisplayVersion,
        ReleaseDate,
        UncompressedSize
    };

    enum InstallAction {
        Install,
        Uninstall,
        KeepInstalled,
        KeepUninstalled
    };

    enum Column {
        NameColumn = 0,
        ActionColumn,
        InstalledVersionColumn,
        NewVersionColumn,
        ReleaseDateColumn,
        UncompressedSizeColumn,
        LastColumn
    };

    explicit ComponentModelHelper();

    int childCount() const;
    Component* childAt(int index) const;
    QList<Component*> childItems() const;

    bool isEnabled() const;
    void setEnabled(bool enabled);

    bool isTristate() const;
    void setTristate(bool tristate);

    bool isCheckable() const;
    void setCheckable(bool checkable);

    bool isSelectable() const;
    void setSelectable(bool selectable);

    InstallAction installAction() const;
    void setInstallAction(InstallAction action);

    Qt::ItemFlags flags() const;
    void setFlags(Qt::ItemFlags flags);

    Qt::CheckState checkState() const;
    void setCheckState(Qt::CheckState state);

    QVariant data(int role = Qt::UserRole + 1) const;
    void setData(const QVariant &value, int role = Qt::UserRole + 1);

protected:
    void setPrivate(ComponentPrivate *componentPrivate);

private:
    void changeFlags(bool enable, Qt::ItemFlags itemFlags);

private:
    QHash<int, QVariant> m_values;

    ComponentPrivate *m_componentPrivate;
};

} // namespace QInstaller

Q_DECLARE_METATYPE(QInstaller::ComponentModelHelper::InstallAction)

#endif  // COMPONENT_P_H
