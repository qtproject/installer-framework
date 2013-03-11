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

#include "componentmodel.h"

#include "component.h"
#include "packagemanagercore.h"

namespace QInstaller {

/*!
    \fn void checkStateChanged(const QModelIndex &index)

    This signal is emitted whenever the check state of a component is changed. The \a index value indicates
    the QModelIndex representation of the component as seen from the model.
*/

/*!
    \fn void checkStateChanged(QInstaller::ComponentModel::ModelState state)

    This signal is emitted whenever the check state of a model is changed after all check state
    calculations have taken place. The \a state value indicates whether the model has its default checked
    state, all components are checked/ unchecked or some individual components checked state has changed.
*/


/*!
    Constructs an component model with the given number of \a columns and \a core as parent.
*/
ComponentModel::ComponentModel(int columns, PackageManagerCore *core)
    : QAbstractItemModel(core)
    , m_core(core)
    , m_modelState(DefaultChecked)
{
    m_headerData.insert(0, columns, QVariant());
    connect(this, SIGNAL(modelReset()), this, SLOT(slotModelReset()));
}

/*!
    Destroys the component model.
*/
ComponentModel::~ComponentModel()
{
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

    return Qt::ItemFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
}

/*!
    Returns the number of items under the given \a parent. When the parent is valid it means that rowCount
    is returning the number of items of parent.
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
    Returns the parent of the child item with the given \a child. If the item has no parent, an invalid
    QModelIndex is returned.
*/
QModelIndex ComponentModel::parent(const QModelIndex &child) const
{
    if (!child.isValid())
        return QModelIndex();

    if (Component *childComponent = componentFromIndex(child)) {
        if (Component *parent = childComponent->parentComponent())
            return indexFromComponentName(parent->name());
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
    Qt::ToolTipRole are specifically handled for columns greater than the first column and translate to \bold
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
    Sets the \a role data for the item at \a index to \a value. Returns true if successful;
    otherwise returns false. The dataChanged() signal is emitted if the data was successfully set.
    The checkStateChanged() signals are emitted in addition if the check state of the item is set.
*/
bool ComponentModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid())
        return false;

    Component *component = componentFromIndex(index);
    if (!component)
        return false;

    if (role == Qt::CheckStateRole) {
        ComponentSet nodes = component->childItems().toSet();
        QSet<QModelIndex> changed = updateCheckedState(nodes << component, Qt::CheckState(value.toInt()));
        foreach (const QModelIndex &index, changed) {
            emit dataChanged(index, index);
            emit checkStateChanged(index);
        }
        updateAndEmitModelState();     // update the internal state
    } else {
        component->setData(value, role);
        emit dataChanged(index, index);
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
    Returns a list of checked components.
*/
QSet<Component *> ComponentModel::checked() const
{
    return m_currentCheckedState[Qt::Checked];
}

/*!
    Returns a list of partially checked components.
*/
QSet<Component *> ComponentModel::partially() const
{
    return m_currentCheckedState[Qt::PartiallyChecked];
}

/*!
    Returns a list of unchecked components.
*/
QSet<Component *> ComponentModel::unchecked() const
{
    return m_currentCheckedState[Qt::Unchecked];
}

/*!
    Returns a list of components whose check state can't be changed. If package manager core is run with no
    forced installation argument, the list will always be empty.
*/
QSet<Component *> ComponentModel::uncheckable() const
{
    return m_uncheckable;
}

/*!
    Returns a pointer to the PackageManagerCore this model belongs to.
*/
PackageManagerCore *ComponentModel::core() const
{
    return m_core;
}

/*!
    Returns the current state check state of the model.
*/
ComponentModel::ModelState ComponentModel::checkedState() const
{
    return m_modelState;
}

/*!
    Translates between a given component \a name and its associated QModelIndex. Returns the QModelIndex
    that represents the component or an invalid QModelIndex if the component does not exist in the model.
*/
QModelIndex ComponentModel::indexFromComponentName(const QString &name) const
{
    if (m_indexByNameCache.isEmpty()) {
        for (int i = 0; i < m_rootComponentList.count(); ++i)
            collectComponents(m_rootComponentList.at(i), index(i, 0, QModelIndex()));
    }
    return m_indexByNameCache.value(name, QModelIndex());
}

/*!
    Translates between a given QModelIndex \a index and its associated Component. Returns the component if
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
    Sets the passed \a rootComponents to be the list of currently shown components.

    The model is repopulated and the individual component checked state is used to show the check mark in
    front of the visual component representation. The modelAboutToBeReset() and modelReset() signals are
    emitted.
*/
void ComponentModel::setRootComponents(QList<QInstaller::Component*> rootComponents)
{
    beginResetModel();

    m_uncheckable.clear();
    m_indexByNameCache.clear();
    m_initialCheckedState.clear();
    m_currentCheckedState.clear();
    m_modelState = DefaultChecked;

    // show virtual components only in case we run as updater or if the core engine is set to show them
    const bool showVirtuals = m_core->isUpdater() || m_core->virtualComponentsVisible();
    foreach (Component *const component, rootComponents) {
        if ((!showVirtuals) && component->isVirtual())
            continue;
        m_rootComponentList.append(component);
    }
    endResetModel();
}

/*!
    Sets the check state of every component in the model to be \a state.

    The ComponentModel::PartiallyChecked flag is ignored by this function. Note that components are not
    changed if they are not checkable. The dataChanged() and checkStateChanged() signals are emitted.
*/
void ComponentModel::setCheckedState(QInstaller::ComponentModel::ModelStateFlag state)
{
    QSet<QModelIndex> changed;
    switch (state) {
        case AllChecked:
            changed = updateCheckedState(m_currentCheckedState[Qt::Unchecked], Qt::Checked);
        break;
        case AllUnchecked:
            changed = updateCheckedState(m_currentCheckedState[Qt::Checked], Qt::Unchecked);
        break;
        case DefaultChecked:
            // record all changes, to be able to update the UI properly
            changed = updateCheckedState(m_currentCheckedState[Qt::Checked], Qt::Unchecked);
            changed += updateCheckedState(m_initialCheckedState[Qt::Checked], Qt::Checked);
        break;
        default:
            break;
    }

    if (changed.isEmpty())
        return;

    // notify about changes done to the model
    foreach (const QModelIndex &index, changed) {
        emit dataChanged(index, index);
        emit checkStateChanged(index);
    }
    updateAndEmitModelState();     // update the internal state
}


// -- private slots

void ComponentModel::slotModelReset()
{
    ComponentList components = m_rootComponentList;
    if (!m_core->isUpdater()) {
        foreach (Component *const component, m_rootComponentList)
            components += component->childItems();
    }

    ComponentSet checked;
    foreach (Component *const component, components) {
        if (component->checkState() == Qt::Checked)
            checked.insert(component);
    }

    updateCheckedState(checked, Qt::Checked);
    foreach (Component *const component, components) {
        if (!component->isCheckable())
            m_uncheckable.insert(component);
        m_initialCheckedState[component->checkState()].insert(component);
    }

    m_currentCheckedState = m_initialCheckedState;
    updateAndEmitModelState();     // update the internal state
}


// -- private

void ComponentModel::updateAndEmitModelState()
{
    m_modelState = ComponentModel::DefaultChecked;
    if (m_initialCheckedState != m_currentCheckedState)
        m_modelState = ComponentModel::PartiallyChecked;

    if (checked().count() == 0 && partially().count() == 0) {
        m_modelState |= ComponentModel::AllUnchecked;
        m_modelState &= ~ComponentModel::PartiallyChecked;
    }

    if (unchecked().count() == 0 && partially().count() == 0) {
        m_modelState |= ComponentModel::AllChecked;
        m_modelState &= ~ComponentModel::PartiallyChecked;
    }

    emit checkStateChanged(m_modelState);
}

void ComponentModel::collectComponents(Component *const component, const QModelIndex &parent) const
{
    m_indexByNameCache.insert(component->name(), parent);
    for (int i = 0; i < component->childCount(); ++i)
        collectComponents(component->childAt(i), index(i, 0, parent));
}

namespace ComponentModelPrivate {

struct NameGreaterThan
{
    bool operator() (const Component *lhs, const Component *rhs) const
    {
        return lhs->name() > rhs->name();
    }
};

static Qt::CheckState verifyPartiallyChecked(Component *component)
{
    int checked = 0;
    int unchecked = 0;

    const int count = component->childCount();
    for (int i = 0; i < count; ++i) {
        switch (component->childAt(i)->checkState()) {
            case Qt::Checked: {
                ++checked;
            }   break;
            case Qt::Unchecked: {
                ++unchecked;
            }   break;
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

}   // namespace ComponentModelPrivate

QSet<QModelIndex> ComponentModel::updateCheckedState(const ComponentSet &components, Qt::CheckState state)
{
    // get all parent nodes for the components we're going to update
    ComponentSet nodes = components;
    foreach (Component *const component, components) {
        if (Component *parent = component->parentComponent()) {
            nodes.insert(parent);
            while (parent->parentComponent() != 0) {
                parent = parent->parentComponent();
                nodes.insert(parent);
            }
        }
    }

    QSet<QModelIndex> changed;
    // sort the nodes, so we can start in descending order to check node and tri-state nodes properly
    ComponentList sortedNodes = nodes.toList();
    std::sort(sortedNodes.begin(), sortedNodes.end(), ComponentModelPrivate::NameGreaterThan());
    foreach (Component *const node, sortedNodes) {
        if (!node->isCheckable())
            continue;

        Qt::CheckState newState = state;
        const Qt::CheckState recentState = node->checkState();
        if (node->isTristate())
            newState = ComponentModelPrivate::verifyPartiallyChecked(node);
        if (recentState == newState)
            continue;

        node->setCheckState(newState);
        changed.insert(indexFromComponentName(node->name()));

        m_currentCheckedState[Qt::Checked].remove(node);
        m_currentCheckedState[Qt::Unchecked].remove(node);
        m_currentCheckedState[Qt::PartiallyChecked].remove(node);

        switch (newState) {
            case Qt::Checked:
                m_currentCheckedState[Qt::Checked].insert(node);
            break;
            case Qt::Unchecked:
                m_currentCheckedState[Qt::Unchecked].insert(node);
            break;
            case Qt::PartiallyChecked:
                m_currentCheckedState[Qt::PartiallyChecked].insert(node);
            break;
        }
    }
    return changed;
}

} // namespace QInstaller
