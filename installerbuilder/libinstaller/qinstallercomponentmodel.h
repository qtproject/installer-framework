/**************************************************************************
**
** This file is part of Qt SDK**
**
** Copyright (c) 2010 Nokia Corporation and/or its subsidiary(-ies).*
**
** Contact:  Nokia Corporation qt-info@nokia.com**
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
#ifndef QINSTALLER_COMPONENTMODEL_H
#define QINSTALLER_COMPONENTMODEL_H

#include "installer_global.h"

#include <QtCore/QAbstractItemModel>
#include <QtCore/QList>

namespace QInstaller {
class Component;
class Installer;

typedef QMap<Component*, QPersistentModelIndex> ComponentModelIndexCache;

class INSTALLER_EXPORT InstallerComponentModel : public QAbstractItemModel
{
    Q_OBJECT

public:

    enum Column {
        NameColumn = 0,
        InstalledVersionColumn,
        VersionColumn,
        SizeColumn
    };

    explicit InstallerComponentModel(int columns, Installer *parent = 0);
    ~InstallerComponentModel();

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;

    QModelIndex parent(const QModelIndex &child) const;
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole);

    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    bool setHeaderData(int section, Qt::Orientation orientation, const QVariant &value,
        int role = Qt::EditRole);

    Qt::ItemFlags flags(const QModelIndex &index) const;

    void setRootComponents(QList<Component*> rootComponents);
    void appendRootComponents(QList<Component*> rootComponents);

    QModelIndex indexFromComponent(Component *component) const;
    Component* componentFromIndex(const QModelIndex &index) const;

    static QFont virtualComponentsFont();
    static void setVirtualComponentsFont(const QFont &font);

private:
    void setupCache(const QModelIndex &parent);
    QModelIndexList collectComponents(const QModelIndex &parent) const;

private:
    int m_columns;
    ComponentModelIndexCache m_cache;

    Installer *m_installer;
    Component *m_headerComponent;
};

class INSTALLER_EXPORT ComponentModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    enum Role {
        ComponentRole = Qt::UserRole,
        IdRole
    };

    enum Column {
        NameColumn = 0,
        InstalledVersionColumn,
        VersionColumn,
        SizeColumn
    };

    explicit ComponentModel(Installer *parent, RunModes runMode = AllMode);
    ~ComponentModel();

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
    QModelIndex parent(const QModelIndex &index) const;

    int columnCount(const QModelIndex &parent = QModelIndex()) const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const;

    Qt::ItemFlags flags(const QModelIndex &index) const;

    bool setData(const QModelIndex &index, const QVariant &data, int role = Qt::CheckStateRole);
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

    static bool virtualComponentsVisible();
    static void setVirtualComponentsVisible(bool visible);

    static QFont virtualComponentsFont();
    static void setVirtualComponentsFont(const QFont &f);

    QModelIndex findComponent(const QString &id) const;

public Q_SLOTS:
    void clear();
    void setRunMode(RunModes runMode);
    void addRootComponents(QList<QInstaller::Component*> components);

Q_SIGNALS:
    void workRequested(bool value);

private Q_SLOTS:
    void selectedChanged(bool checked);
    void componentAdded(QInstaller::Component* comp);

private:
    RunModes m_runMode;
    QList<Component*> m_components;
    mutable QList<Component*> m_seenComponents;
};

}   // namespace QInstaller

#endif // QINSTALLER_COMPONENTMODEL_H
