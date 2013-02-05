/**************************************************************************
**
** Copyright (C) 2012-2013 Digia Plc and/or its subsidiary(-ies).
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
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
**************************************************************************/

#ifndef COMPONENT_P_H
#define COMPONENT_P_H

#include "qinstallerglobal.h"

#include <QtCore/QPointer>
#include <QtCore/QStringList>
#include <QtCore/QUrl>

#include <QtScript/QScriptEngine>

namespace QInstaller {

class Component;
class PackageManagerCore;

class ComponentPrivate
{
    QInstaller::Component* const q;

public:
    explicit ComponentPrivate(PackageManagerCore *core, Component *qq);
    ~ComponentPrivate();

    QScriptEngine *scriptEngine();

    void setProperty(QScriptValue &scriptValue, const QString &propertyName, int value);

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
    QScriptValue m_scriptComponent;
    QHash<QString, QString> m_vars;
    QList<Component*> m_childComponents;
    QList<Component*> m_allChildComponents;
    QList<Component*> m_virtualChildComponents;
    QStringList m_downloadableArchives;
    QStringList m_stopProcessForUpdateRequests;
    QHash<QString, bool> m_unexistingScriptMethods;
    QHash<QString, QPointer<QWidget> > m_userInterfaces;

    // < display name, < file name, file content > >
    QHash<QString, QPair<QString, QString> > m_licenses;
    QList<QPair<QString, bool> > m_pathesForUninstallation;

private:
    QScriptValue getDesktopServices();

private:
    QScriptEngine* m_scriptEngine;
};


// -- ComponentModelHelper

class ComponentModelHelper
{
public:
    enum Roles {
        LocalDisplayVersion = Qt::UserRole + 1,
        RemoteDisplayVersion = LocalDisplayVersion + 1,
        UncompressedSize = RemoteDisplayVersion + 1
    };

    enum Column {
        NameColumn = 0,
        InstalledVersionColumn,
        NewVersionColumn,
        UncompressedSizeColumn
    };

    explicit ComponentModelHelper();

    int childCount() const;
    int indexInParent() const;

    QList<Component*> childs() const;
    Component* childAt(int index) const;

    bool isEnabled() const;
    void setEnabled(bool enabled);

    bool isTristate() const;
    void setTristate(bool tristate);

    bool isCheckable() const;
    void setCheckable(bool checkable);

    bool isSelectable() const;
    void setSelectable(bool selectable);

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

#endif  // COMPONENT_P_H
