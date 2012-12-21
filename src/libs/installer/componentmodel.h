/**************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
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

    void setRootComponents(QList<QInstaller::Component*> rootComponents);
    void appendRootComponents(QList<QInstaller::Component*> rootComponents);

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

    mutable QHash<QString, QPersistentModelIndex> m_indexByNameCache;
};

}   // namespace QInstaller

#endif // COMPONENTMODEL_H
