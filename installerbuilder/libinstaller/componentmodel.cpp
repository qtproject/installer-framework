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
    , m_rootComponent(new Component(0))
{
    m_headerData.insert(0, columns, QVariant());

    connect(this, SIGNAL(modelReset()), this, SLOT(slotModelReset()));
    connect(this, SIGNAL(checkStateChanged(QModelIndex)), this, SLOT(slotCheckStateChanged(QModelIndex)));
}

ComponentModel::~ComponentModel()
{
    for (int i = m_rootComponent->childCount() - 1; i >= 0; --i)
        m_rootComponent->removeComponent(m_rootComponent->childAt(i));
    delete m_rootComponent;
}

int ComponentModel::rowCount(const QModelIndex &parent) const
{
    if (Component *component = componentFromIndex(parent))
        return component->childCount();
    return 0;
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
             if (parent != m_rootComponent)
                 return createIndex(parent->indexInParent(), 0, parent);
         }
     }
     return QModelIndex();
}

QModelIndex ComponentModel::index(int row, int column, const QModelIndex &parent) const
{
    if (parent.isValid() && column >= columnCount())
         return QModelIndex();

    if (Component *parentComponent = componentFromIndex(parent)) {
        if (Component *childComponent = parentComponent->childAt(row))
            return createIndex(row, column, childComponent);
    }
    return QModelIndex();
}

QVariant ComponentModel::data(const QModelIndex &index, int role) const
{
    if (Component *component = componentFromIndex(index)) {
        if (index.column() > 0) {
            if (role == Qt::CheckStateRole)
                return QVariant();
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
    if (!component || component == m_rootComponent)
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

    m_cache.clear();

    for (int i = m_rootComponent->childCount() - 1; i >= 0; --i)
        m_rootComponent->removeComponent(m_rootComponent->childAt(i));

    foreach (Component *component, rootComponents)
        m_rootComponent->appendComponent(component);

    endResetModel();
}

void ComponentModel::appendRootComponents(QList<Component*> rootComponents)
{
    beginResetModel();

    m_cache.clear();

    foreach (Component *component, rootComponents)
        m_rootComponent->appendComponent(component);

    endResetModel();
}

Installer* ComponentModel::installer() const
{
    return m_installer;
}

QModelIndex ComponentModel::indexFromComponent(Component *component)
{
    if (m_cache.isEmpty()) {
        for (int i = 0; i < m_rootComponent->childCount(); ++i) {
            const QModelIndex &root = index(i, 0, QModelIndex());
            updateCache(root);
            m_cache.insert(static_cast<Component*> (root.internalPointer()), root);
        }
    }
    return m_cache.value(component, QModelIndex());
}

Component* ComponentModel::componentFromIndex(const QModelIndex &index) const
{
    if (index.isValid())
         return static_cast<Component*>(index.internalPointer());
     return m_rootComponent;
}

// -- public slots

void ComponentModel::selectAll()
{
    for (int i = 0; i < m_rootComponent->childCount(); ++i)
        setData(index(i, 0, QModelIndex()), Qt::Checked, Qt::CheckStateRole);
}

void ComponentModel::deselectAll()
{
    for (int i = 0; i < m_rootComponent->childCount(); ++i)
        setData(index(i, 0, QModelIndex()), Qt::Unchecked, Qt::CheckStateRole);
}

// -- private slots

void ComponentModel::slotModelReset()
{
    for (int i = 0; i < m_rootComponent->childCount(); ++i)
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
    if (!component || component == m_rootComponent)
        return;

    const Qt::CheckState state = component->checkState();
    if (component->isTristate()) {
        if (state == Qt::PartiallyChecked) {
            component->setCheckState(verifyPartiallyChecked(component));
            return;
        }

        bool notCheckable = false;
        const QList<Component*> &children = component->childs();
        foreach (Component *child, children) {
            if (child->isCheckable()) {
                const QModelIndex &idx = indexFromComponent(child);
                setData(idx, state, Qt::CheckStateRole);
            } else {
                notCheckable = true;
            }
        }

        if (state == Qt::Unchecked && notCheckable)
            setData(index, Qt::PartiallyChecked, Qt::CheckStateRole);
    } else {
        QList<Component*> parents;
        Component *tmp = component;
        while (m_rootComponent != tmp->parentComponent()) {
            parents.append(tmp->parentComponent());
            tmp = parents.last();
        }

        foreach (Component *parent, parents) {
            if (parent->isCheckable()) {
                Qt::CheckState pState = parent->checkState();
                const QModelIndex &idx = indexFromComponent(parent);
                if (pState == Qt::Checked) {
                    if (state == Qt::Unchecked)
                        setData(idx, Qt::PartiallyChecked, Qt::CheckStateRole);
                } else if (pState == Qt::Unchecked) {
                    if (state == Qt::Checked)
                        setData(idx, Qt::PartiallyChecked, Qt::CheckStateRole);
                } else if (pState == Qt::PartiallyChecked) {
                    setData(idx, verifyPartiallyChecked(parent), Qt::CheckStateRole);
                }
            }
        }
    }
}

// -- private

void ComponentModel::updateCache(const QModelIndex &parent)
{
    const QModelIndexList &list = collectComponents(parent);
    foreach (const QModelIndex &index, list)
        m_cache.insert(componentFromIndex(index), index);
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
