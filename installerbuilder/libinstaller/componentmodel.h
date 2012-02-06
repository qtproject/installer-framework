/**************************************************************************
**
** This file is part of Installer Framework
**
** Copyright (c) 2010-2012 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (qt-info@nokia.com)
**
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
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** Other Usage
**
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
** If you have questions regarding the use of this file, please contact
** Nokia at qt-info@nokia.com.
**
**************************************************************************/

#ifndef COMPONENTMODEL_H
#define COMPONENTMODEL_H

#include "qinstallerglobal.h"

#include <QtCore/QAbstractItemModel>
#include <QtCore/QList>
#include <QtCore/QSet>
#include <QtCore/QVector>

namespace QInstaller {

class Component;
class PackageManagerCore;

class INSTALLER_EXPORT ComponentModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    explicit ComponentModel(int columns, PackageManagerCore *core = 0);
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

    PackageManagerCore *packageManagerCore() const;

    bool defaultCheckState() const;
    bool hasCheckedComponents() const;
    QList<Component*> checkedComponents() const;

    QModelIndex indexFromComponentName(const QString &name) const;
    Component* componentFromIndex(const QModelIndex &index) const;

public Q_SLOTS:
    void selectAll();
    void deselectAll();
    void selectDefault();

Q_SIGNALS:
    void defaultCheckStateChanged(bool changed);
    void checkStateChanged(const QModelIndex &index);

private Q_SLOTS:
    void slotModelReset();
    void slotCheckStateChanged(const QModelIndex &index);

private:
    QSet<QString> select(Qt::CheckState state);
    void updateCache(const QModelIndex &parent) const;
    QModelIndexList collectComponents(const QModelIndex &parent) const;

private:
    PackageManagerCore *m_core;

    int m_rootIndex;
    QVector<QVariant> m_headerData;
    QSet<QString> m_initialCheckedSet;
    QSet<QString> m_currentCheckedSet;
    QList<Component*> m_rootComponentList;

    mutable QMap<QString, QPersistentModelIndex> m_indexByNameCache;
};

}   // namespace QInstaller

#endif // COMPONENTMODEL_H
