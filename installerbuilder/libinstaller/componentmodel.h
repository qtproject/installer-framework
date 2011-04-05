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
#ifndef COMPONENTMODEL_H
#define COMPONENTMODEL_H

#include "qinstallerglobal.h"

#include <QtCore/QAbstractItemModel>
#include <QtCore/QList>
#include <QtCore/QVector>

namespace QInstaller {
class Component;
class Installer;

class INSTALLER_EXPORT ComponentModel : public QAbstractItemModel
{
    Q_OBJECT
    typedef QMap<Component*, QPersistentModelIndex> ComponentModelIndexCache;

public:
    explicit ComponentModel(int columns, Installer *parent = 0);
    ~ComponentModel();

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

    Installer* installer() const;
    QModelIndex indexFromComponent(Component *component);
    Component* componentFromIndex(const QModelIndex &index) const;

public Q_SLOTS:
    void selectAll();
    void deselectAll();

Q_SIGNALS:
    void checkStateChanged(const QModelIndex &index);

private Q_SLOTS:
    void slotModelReset();
    void slotCheckStateChanged(const QModelIndex &index);

private:
    void updateCache(const QModelIndex &parent);
    QModelIndexList collectComponents(const QModelIndex &parent) const;

private:
    Installer *m_installer;

    QVector<QVariant> m_headerData;
    ComponentModelIndexCache m_cache;
    QList<Component*> m_rootComponentList;
};

}   // namespace QInstaller

#endif // COMPONENTMODEL_H
