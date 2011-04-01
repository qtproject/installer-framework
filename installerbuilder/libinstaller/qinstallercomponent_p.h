/**************************************************************************
**
** This file is part of Qt SDK**
**
** Copyright (c) 2011 Nokia Corporation and/or its subsidiary(-ies).*
**
** Contact:  Nokia Corporation qt-info@nokia.com**
**
** No Commercial Usage
**
** This file contains pre-release code and may not be distributed.
** You may use this file in accordance with the terms and conditions
** contained in the Technology Preview License Agreement accompanying
** this package.
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception version
** 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** If you are unsure which license is appropriate for your use, please contact
** (qt-info@nokia.com).
**
**************************************************************************/
#ifndef QINSTALLER_COMPONENT_P_H
#define QINSTALLER_COMPONENT_P_H

#include "qinstallerglobal.h"

#include <QtCore/QStringList>
#include <QtCore/QUrl>

#include <QtScript/QScriptEngine>

namespace KDUpdater {
    class UpdateOperation;
}

namespace QInstaller {
class Component;
class Installer;

class ComponentPrivate
{
    QInstaller::Component* const q;

public:
    explicit ComponentPrivate(Installer* installer, Component* qq);

    void init();
    void setSelectedOnComponentList(const QList<Component*> &componentList,
        bool selected, RunModes runMode, int selectMode);

    Qt::ItemFlags m_flags;
    Qt::CheckState m_checkState;

    static QMap<const Component*, Qt::CheckState> cachedCheckStates;

    Installer *m_installer;
    QHash<QString, QString> m_vars;
    QList<Component*> m_components;
    QList<Component*> m_allComponents;
    QList<Component*> m_virtualComponents;
    QList<KDUpdater::UpdateOperation* > operations;

    QList<QPair<QString, bool> > pathesForUninstallation;

    QMap<QString, QWidget*> userInterfaces;

    QUrl repositoryUrl;
    QString localTempPath;
    QStringList downloadableArchives;
    QStringList stopProcessForUpdateRequests;

    Component* m_parent;

    // filled before intaller runs
    qint64 m_offsetInInstaller;

    bool autoCreateOperations;
    bool operationsCreated;

    bool removeBeforeUpdate;

    bool isCheckedFromUpdater;

    bool m_newlyInstalled;

    bool operationsCreatedSuccessfully;

    QScriptEngine scriptEngine;
    QScriptValue scriptComponent;

    QHash<QString, bool> unexistingScriptMethods;

    KDUpdater::UpdateOperation* minimumProgressOperation;

    // < display name, < file name, file content > >
    QHash<QString, QPair<QString, QString> > m_licenses;
    KDUpdater::UpdateOperation *m_licenseOperation;
};


// -- ComponentModelHelper

class ComponentModelHelper
{
public:
    explicit ComponentModelHelper();
    ~ComponentModelHelper();

    int childCount() const;
    int indexInParent() const;
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

protected:
    void setPrivate(ComponentPrivate *componentPrivate);

private:
    void changeFlags(bool enable, Qt::ItemFlags itemFlags);

private:
    ComponentPrivate *m_componentPrivate;
};


}   // namespace QInstaller

#endif  // QINSTALLER_COMPONENT_P_H
