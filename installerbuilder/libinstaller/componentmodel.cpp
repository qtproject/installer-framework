/**************************************************************************
**
** This file is part of Qt SDK**
**
** Copyright (c) 2012 Nokia Corporation and/or its subsidiary(-ies).*
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

#include "component.h"
#include "packagemanagercore.h"

namespace QInstaller {

/*!
    \fn void defaultCheckStateChanged(bool changed)

    This signal is emitted whenever the default check state of a model is changed. The \a changed value
    indicates whether the model has it's initial checked state or some components changed it's checked state.
*/

/*!
    \fn void checkStateChanged(const QModelIndex &index)

    This signal is emitted whenever the default check state of a component is changed. The \a index value
    indicates the QModelIndex representation of the component as seen from the model.
*/


/*!
    Constructs an component model with the given number of \a columns and \a core as parent.
*/
ComponentModel::ComponentModel(int columns, PackageManagerCore *core)
    : QAbstractItemModel(core)
    , m_core(core)
    , m_rootIndex(0)
{
    m_headerData.insert(0, columns, QVariant());

    connect(this, SIGNAL(modelReset()), this, SLOT(slotModelReset()));
    connect(this, SIGNAL(checkStateChanged(QModelIndex)), this, SLOT(slotCheckStateChanged(QModelIndex)));
}

/*!
    Destroys the component model.
*/
ComponentModel::~ComponentModel()
{
}

/*!
    Returns the number of items under the given \a parent. When the parent is valid it means that rowCount is
    returning the number of items of parent.
*/
int ComponentModel::rowCount(const QModelIndex &parent) const
{
    if (Component *component = componentFromIndex(parent))
        return component->childCount();
    return m_rootComponentList.count();
}

/*!
    Returns the number of columns of the given \a parent.
*/
int ComponentModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return m_headerData.size();
}

/*!
    Returns the parent of the child item with the given \a parent. If the item has no parent, an invalid
    QModelIndex is returned.
*/
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

/*!
    Returns the index of the item in the model specified by the given \a row, \a column and \a parent index.
*/
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

/*!
    Returns the data stored under the given \a role for the item referred to by the \a index.

    \note An \bold invalid QVariant is returned if the given index is invalid. \bold Qt::CheckStateRole is
    only supported for the first column of the model. \bold Qt::EditRole, \bold Qt::DisplayRole and \bold
    Qt::ToolTipRole are specificly handled for columns greather than the first column and translate to \bold
    Qt::UserRole \bold + \bold index.column().

*/
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

/*!
    Sets the \a role data for the item at \a index to \a value. Returns true if successful; otherwise returns
    false. The dataChanged() signal is emitted if the data was successfully set. The checkStateChanged() and
    defaultCheckStateChanged() signal are emitted in addition if the check state of the item is set.
*/
bool ComponentModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid())
        return false;

    Component *component = componentFromIndex(index);
    if (!component)
        return false;

    component->setData(value, role);

    emit dataChanged(index, index);
    if (role == Qt::CheckStateRole) {
        emit checkStateChanged(index);
        foreach (Component* comp, m_rootComponentList) {
            comp->updateUncompressedSize();
        }
    }

    return true;
}

/*!
    Returns the data for the given \a role and \a section in the header with the specified \a orientation.
    An \bold invalid QVariant is returned if \a section is out of bounds, \a orientation is not Qt::Horizontal
    or \a role is anything else than Qt::DisplayRole.
*/
QVariant ComponentModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (section >= 0 && section < columnCount() && orientation == Qt::Horizontal && role == Qt::DisplayRole)
        return m_headerData.at(section);
    return QVariant();
}

/*!
    Sets the data for the given \a role and \a section in the header with the specified \a orientation to the
    \a value supplied. Returns true if the header's data was updated; otherwise returns false. The
    headerDataChanged() signal is emitted if the data was successfully set.

    \note Only \bold Qt::Horizontal orientation is supported.
*/
bool ComponentModel::setHeaderData(int section, Qt::Orientation orientation, const QVariant &value, int role)
{
    if (section >= 0 && section < columnCount() && orientation == Qt::Horizontal
        && (role == Qt::DisplayRole || role == Qt::EditRole)) {
            m_headerData.replace(section, value);
            emit headerDataChanged(orientation, section, section);
            return true;
    }
    return false;
}

/*!
    Returns the item flags for the given \a index.

    The class implementation returns a combination of flags that enables the item (Qt::ItemIsEnabled), allows
    it to be selected (Qt::ItemIsSelectable) and to be checked (Qt::ItemIsUserCheckable).
*/
Qt::ItemFlags ComponentModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;

    if (Component *component = componentFromIndex(index))
        return component->flags();

    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable;
}

/*!
    Set's the passed \a rootComponents to be list of currently shown components. The model is repopulated and
    the individual component checked state is used to show the check mark in front of the visual component
    representation. The modelAboutToBeReset() and modelReset() signals are emitted.
*/
void ComponentModel::setRootComponents(QList<Component*> rootComponents)
{
    beginResetModel();

    m_indexByNameCache.clear();
    m_rootComponentList.clear();
    m_initialCheckedSet.clear();
    m_currentCheckedSet.clear();

    m_rootIndex = 0;
    m_rootComponentList = rootComponents;

    endResetModel();
}

/*!
    Appends the passed \a rootComponents to the currently shown list of components. The model is repopulated
    and the individual component checked state is used to show the check mark in front of the visual component
    representation. Already changed check states on the previous model are preserved. The modelAboutToBeReset()
    and modelReset() signals are emitted.
*/
void ComponentModel::appendRootComponents(QList<Component*> rootComponents)
{
    beginResetModel();

    m_indexByNameCache.clear();

    m_rootIndex = m_rootComponentList.count() - 1;
    m_rootComponentList += rootComponents;

    endResetModel();
}

/*!
    Returns a pointer to the PackageManagerCore this model belongs to.
*/
PackageManagerCore *ComponentModel::packageManagerCore() const
{
    return m_core;
}

/*!
    Returns true if no changes to the components checked state have been done, otherwise returns false.
*/
bool ComponentModel::defaultCheckState() const
{
    return m_initialCheckedSet == m_currentCheckedSet;
}

/*!
    Returns true if this model has checked components, otherwise returns false.
*/
bool ComponentModel::hasCheckedComponents() const
{
    return !m_currentCheckedSet.isEmpty();
}

/*!
    Returns a list of checked components.
*/
QList<Component*> ComponentModel::checkedComponents() const
{
    QList<Component*> list;
    foreach (const QString &name, m_currentCheckedSet)
        list.append(componentFromIndex(indexFromComponentName(name)));
    return list;
}

/*!
    Translates between a given component \a name and it's associated QModelIndex. Returns the QModelIndex that
    represents the component or an invalid QModelIndex if the component does not exist in the model.
*/
QModelIndex ComponentModel::indexFromComponentName(const QString &name) const
{
    if (m_indexByNameCache.isEmpty()) {
        for (int i = 0; i < m_rootComponentList.count(); ++i)
            updateCache(index(i, 0, QModelIndex()));
    }
    return m_indexByNameCache.value(name, QModelIndex());
}

/*!
    Translates between a given QModelIndex \a index and it's associated Component. Returns the Component if
    the index is valid or 0 if an invalid QModelIndex is given.
*/
Component *ComponentModel::componentFromIndex(const QModelIndex &index) const
{
    if (index.isValid())
        return static_cast<Component*>(index.internalPointer());
    return 0;
}

// -- public slots

/*!
    Invoking this slot results in an checked state for every component the has a visual representation in the
    model. Note that components are not changed if they are not checkable. The checkStateChanged() and
    defaultCheckStateChanged() signal are emitted.
*/
void ComponentModel::selectAll()
{
    m_currentCheckedSet = m_currentCheckedSet.unite(select(Qt::Checked));
    emit defaultCheckStateChanged(m_initialCheckedSet != m_currentCheckedSet);
}

/*!
    Invoking this slot results in an unchecked state for every component the has a visual representation in
    the model. Note that components are not changed if they are not checkable. The checkStateChanged() and
    defaultCheckStateChanged() signal are emitted.
*/
void ComponentModel::deselectAll()
{
    m_currentCheckedSet = m_currentCheckedSet.subtract(select(Qt::Unchecked));
    emit defaultCheckStateChanged(m_initialCheckedSet != m_currentCheckedSet);
}

/*!
    Invoking this slot results in an checked state for every component the has a visual representation in the
    model when the model was setup during setRootComponents() or appendRootComponents(). Note that components
    are not changed it they are not checkable. The checkStateChanged() and defaultCheckStateChanged() signal
    are emitted.
*/
void ComponentModel::selectDefault()
{
    m_currentCheckedSet = m_currentCheckedSet.subtract(select(Qt::Unchecked));
    foreach (const QString &name, m_initialCheckedSet)
        setData(indexFromComponentName(name), Qt::Checked, Qt::CheckStateRole);
    emit defaultCheckStateChanged(m_initialCheckedSet != m_currentCheckedSet);
}

// -- private slots

void ComponentModel::slotModelReset()
{
    QList<QInstaller::Component*> components = m_rootComponentList;
    if (m_core->runMode() == QInstaller::AllMode) {
        for (int i = m_rootIndex; i < m_rootComponentList.count(); ++i)
            components.append(m_rootComponentList.at(i)->childs());
    }

    foreach (Component *child, components) {
        if (child->checkState() == Qt::Checked && !child->isTristate())
            m_initialCheckedSet.insert(child->name());
    }
    m_currentCheckedSet += m_initialCheckedSet;

    if (m_core->runMode() == QInstaller::AllMode)
        select(Qt::Unchecked);

    foreach (const QString &name, m_currentCheckedSet)
        setData(indexFromComponentName(name), Qt::Checked, Qt::CheckStateRole);

}

static Qt::CheckState verifyPartiallyChecked(Component *component)
{
    int checked = 0;
    int unchecked = 0;
    int virtualChilds = 0;

    const int count = component->childCount();
    for (int i = 0; i < count; ++i) {
        Component *const child = component->childAt(i);
        if (!child->isVirtual()) {
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
        } else {
            ++virtualChilds;
        }
    }

    if ((checked + virtualChilds) == count)
        return Qt::Checked;

    if ((unchecked + virtualChilds) == count)
        return Qt::Unchecked;

    return Qt::PartiallyChecked;
}

void ComponentModel::slotCheckStateChanged(const QModelIndex &index)
{
    Component *component = componentFromIndex(index);
    if (!component)
        return;

    if (component->checkState() == Qt::Checked && !component->isTristate())
        m_currentCheckedSet.insert(component->name());
    else if (component->checkState() == Qt::Unchecked && !component->isTristate())
        m_currentCheckedSet.remove(component->name());
    emit defaultCheckStateChanged(m_initialCheckedSet != m_currentCheckedSet);

    if (component->isVirtual())
        return;

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
                if (child->checkState() != state && !child->isVirtual())
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

QSet<QString> ComponentModel::select(Qt::CheckState state)
{
    QSet<QString> changed;
    for (int i = 0; i < m_rootComponentList.count(); ++i) {
        QSet<QString> tmp;
        QList<Component*> children = m_rootComponentList.at(i)->childs();
        children.prepend(m_rootComponentList.at(i));    // we need to take the root item into account as well
        foreach (Component *child, children) {
            if (child->isCheckable() && !child->isTristate() && child->checkState() != state) {
                tmp.insert(child->name());
                child->setCheckState(state);
            }
        }
        if (!tmp.isEmpty()) {
            changed += tmp;
            setData(index(i, 0, QModelIndex()), state, Qt::CheckStateRole);
        }
    }
    return changed;
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

} // namespace QInstaller
