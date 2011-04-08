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
#include "componentmodel.h"

#include "qinstaller.h"
#include "qinstallercomponent.h"


namespace QInstaller {

ComponentModel::ComponentModel(int columns, Installer *parent)
    : QAbstractItemModel(parent)
    , m_installer(parent)
{
    m_headerData.insert(0, columns, QVariant());

    connect(this, SIGNAL(modelReset()), this, SLOT(slotModelReset()));
    connect(this, SIGNAL(checkStateChanged(QModelIndex)), this, SLOT(slotCheckStateChanged(QModelIndex)));
}

ComponentModel::~ComponentModel()
{
}

int ComponentModel::rowCount(const QModelIndex &parent) const
{
    if (Component *component = componentFromIndex(parent))
        return component->childCount();
    return m_rootComponentList.count();
}

int ComponentModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return m_headerData.size();
}

QModelIndex ComponentModel::parent(const QModelIndex &child) const
{
    if (!child.isValid())
        return QModelIndex();

    if (Component *childComponent = componentFromIndex(child)) {
        if (Component *parent = childComponent->parentComponent()) {
            if (!m_rootComponentList.contains(parent))
                return createIndex(parent->indexInParent(), 0, parent);
            return createIndex(child.row(), 0, parent);
        }
    }

    return QModelIndex();
}

QModelIndex ComponentModel::index(int row, int column, const QModelIndex &parent) const
{
    if (parent.isValid() && (row >= rowCount(parent) || column >= columnCount()))
         return QModelIndex();

    if (Component *parentComponent = componentFromIndex(parent)) {
        if (Component *childComponent = parentComponent->childAt(row))
            return createIndex(row, column, childComponent);
    } else if (row < m_rootComponentList.count()) {
        return createIndex(row, column, m_rootComponentList.at(row));
    }

    return QModelIndex();
}

QVariant ComponentModel::data(const QModelIndex &index, int role) const
{
    if (Component *component = componentFromIndex(index)) {
        if (index.column() > 0) {
            if (role == Qt::CheckStateRole)
                return QVariant();
            if (role == Qt::EditRole || role == Qt::DisplayRole || role == Qt::ToolTipRole)
                return component->data(Qt::UserRole + index.column());
        }
        return component->data(role);
    }
    return QVariant();
}

bool ComponentModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid())
        return false;

    Component *component = componentFromIndex(index);
    if (!component)
        return false;

    component->setData(value, role);

    emit dataChanged(index, index);
    if (role == Qt::CheckStateRole)
        emit checkStateChanged(index);

    return true;
}

QVariant ComponentModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Vertical || section < 0 || section >= columnCount())
        return QAbstractItemModel::headerData(section, orientation, role);
    return m_headerData.at(section);
}

bool ComponentModel::setHeaderData(int section, Qt::Orientation orientation, const QVariant &value,
    int role)
{
    if (orientation == Qt::Vertical || section < 0 || section >= columnCount())
        return QAbstractItemModel::setHeaderData(section, orientation, value, role);

    m_headerData.replace(section, value);
    emit headerDataChanged(orientation, section, section);

    return true;
}

Qt::ItemFlags ComponentModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;

    if (Component *component = componentFromIndex(index))
        return component->flags();

    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable;
}

void ComponentModel::setRootComponents(QList<Component*> rootComponents)
{
    beginResetModel();

    m_indexByNameCache.clear();
    m_rootComponentList.clear();
    m_initialCheckedList.clear();
    m_currentCheckedList.clear();

    m_rootComponentList = rootComponents;

    endResetModel();
}

void ComponentModel::appendRootComponents(QList<Component*> rootComponents)
{
    beginResetModel();

    m_indexByNameCache.clear();
    m_rootComponentList += rootComponents;

    endResetModel();
}

Installer* ComponentModel::installer() const
{
    return m_installer;
}

QModelIndex ComponentModel::indexFromComponentName(const QString &name) const
{
    if (m_indexByNameCache.isEmpty()) {
        for (int i = 0; i < m_rootComponentList.count(); ++i)
            updateCache(index(i, 0, QModelIndex()));
    }
    return m_indexByNameCache.value(name, QModelIndex());
}

Component* ComponentModel::componentFromIndex(const QModelIndex &index) const
{
    if (index.isValid())
        return static_cast<Component*>(index.internalPointer());
    return 0;
}

// -- public slots

void ComponentModel::selectAll()
{
    select(Qt::Checked);
}

void ComponentModel::deselectAll()
{
    select(Qt::Unchecked);
}

void ComponentModel::selectDefault()
{
    select(Qt::Unchecked);
    foreach (const QString &name, m_initialCheckedList)
        setData(indexFromComponentName(name), Qt::Checked, Qt::CheckStateRole);
}

// -- private slots

void ComponentModel::slotModelReset()
{
    foreach (Component *component, m_rootComponentList) {
        foreach (Component *child, component->childs()) {
            // TODO: this might be wrong in case of append component, we really need a initialCheckState()
            if (child->checkState() == Qt::Checked)
                m_initialCheckedList.insert(child->name());
        }
    }
    m_currentCheckedList += m_initialCheckedList;

    for (int i = 0; i < m_rootComponentList.count(); ++i)
        slotCheckStateChanged(index(i, 0, QModelIndex()));
}

static Qt::CheckState verifyPartiallyChecked(Component *component)
{
    int checked = 0;
    int unchecked = 0;
    const int count = component->childCount();
    for (int i = 0; i < count; ++i) {
        switch (component->childAt(i)->checkState()) {
            case Qt::Checked: {
                ++checked;
            }    break;

            case Qt::Unchecked: {
                ++unchecked;
            }    break;

            default:
                break;
        }
    }

    if (checked == count)
        return Qt::Checked;

    if (unchecked == count)
        return Qt::Unchecked;

    return Qt::PartiallyChecked;
}

void ComponentModel::slotCheckStateChanged(const QModelIndex &index)
{
    Component *component = componentFromIndex(index);
    if (!component)
        return;

    if (component->checkState() == Qt::Checked)
        m_currentCheckedList.insert(component->name());
    else if (component->checkState() == Qt::Unchecked)
        m_currentCheckedList.remove(component->name());
    emit defaultCheckStateChanged(m_initialCheckedList != m_currentCheckedList);

    const Qt::CheckState state = component->checkState();
    if (component->isTristate()) {
        if (state == Qt::PartiallyChecked) {
            component->setCheckState(verifyPartiallyChecked(component));
            return;
        }

        QModelIndexList notCheckable;
        foreach (Component *child, component->childs()) {
            const QModelIndex &idx = indexFromComponentName(child->name());
            if (child->isCheckable()) {
                if (child->checkState() != state)
                    setData(idx, state, Qt::CheckStateRole);
            } else {
                notCheckable.append(idx);
            }
        }

        if (state == Qt::Unchecked && !notCheckable.isEmpty()) {
            foreach (const QModelIndex &idx, notCheckable)
                setData(idx, idx.data(Qt::CheckStateRole), Qt::CheckStateRole);
        }
    } else {
        QList<Component*> parents;
        while (0 != component->parentComponent()) {
            parents.append(component->parentComponent());
            component = parents.last();
        }

        foreach (Component *parent, parents) {
            if (parent->isCheckable()) {
                const QModelIndex &idx = indexFromComponentName(parent->name());
                if (parent->checkState() == Qt::PartiallyChecked) {
                    setData(idx, verifyPartiallyChecked(parent), Qt::CheckStateRole);
                } else {
                    setData(idx, Qt::PartiallyChecked, Qt::CheckStateRole);
                }
            }
        }
    }
}

// -- private

void ComponentModel::select(Qt::CheckState state)
{
    for (int i = 0; i < m_rootComponentList.count(); ++i) {
        const QList<Component*> &children = m_rootComponentList.at(i)->childs();
        foreach (Component *child, children) {
            if (child->isCheckable())
                child->setCheckState(state);
        }
        setData(index(i, 0, QModelIndex()), state, Qt::CheckStateRole);
    }
}

void ComponentModel::updateCache(const QModelIndex &parent) const
{
    const QModelIndexList &list = collectComponents(parent);
    foreach (const QModelIndex &index, list) {
        if (Component *component = componentFromIndex(index))
            m_indexByNameCache.insert(component->name(), index);
    }
    m_indexByNameCache.insert((static_cast<Component*> (parent.internalPointer()))->name(), parent);
}

QModelIndexList ComponentModel::collectComponents(const QModelIndex &parent) const
{
    QModelIndexList list;
    for (int i = 0; i < rowCount(parent) ; ++i) {
        const QModelIndex &next = index(i, 0, parent);
        if (Component *component = componentFromIndex(next)) {
            if (component->childCount() > 0)
                list += collectComponents(next);
        }
        list.append(next);
    }
    return list;
}

}   // namespace QInstaller
