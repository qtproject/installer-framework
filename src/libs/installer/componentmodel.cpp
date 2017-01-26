/**************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
**************************************************************************/

#include "componentmodel.h"

#include "component.h"
#include "packagemanagercore.h"
#include <QIcon>

namespace QInstaller {

/*!
    \class QInstaller::ComponentModel
    \inmodule QtInstallerFramework
    \brief The ComponentModel class holds a data model for visual representation of available
        components to install.
*/

/*!
    \enum ComponentModel::ModelStateFlag

    This enum value holds the checked state of the components available for
    installation.

    \value AllChecked
           All components are checked.
    \value AllUnchecked
           No components are checked.
    \value DefaultChecked
           The components to be installed by default are checked.
    \value PartiallyChecked
           Some components are checked.
*/

/*!
    \fn void ComponentModel::checkStateChanged(const QModelIndex &index)

    This signal is emitted whenever the checked state of a component is changed. The \a index value
    indicates the QModelIndex representation of the component as seen from the model.
*/

/*!
    \fn void ComponentModel::checkStateChanged(QInstaller::ComponentModel::ModelState state)

    This signal is emitted whenever the checked state of a model is changed after all state
    calculations have taken place. The \a state is a combination of \c ModelStateFlag values
    indicating whether the model has its default checked state, all components are checked
    or unchecked, or some individual component's checked state has changed.
*/

class IconCache
{
public:
    IconCache() {
    }

    QIcon icon(ComponentModelHelper::InstallAction action) const {
        return m_icons.value(action);
    }
private:
    QMap<ComponentModelHelper::InstallAction, QIcon> m_icons;
};

Q_GLOBAL_STATIC(IconCache, iconCache)

/*!
    Constructs a component model with the given number of \a columns and \a core as parent.
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

    Returns a combination of flags that enables the item (Qt::ItemIsEnabled) and allows it to be
    selected (Qt::ItemIsSelectable) and to be checked (Qt::ItemIsUserCheckable).
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
    Returns the number of items under the given \a parent. When the parent index is invalid, the
    returned value is the root item count.
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
    Returns the parent item of the given \a child. If the item has no parent, an invalid
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
    Returns the index of the item in the model specified by the given \a row, \a column, and
    \a parent index.
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

    \note An \e invalid QVariant is returned if the given index is invalid.
    Qt::CheckStateRole is only supported for the first column of the model.
    Qt::EditRole, Qt::DisplayRole and Qt::ToolTipRole are specifically handled
    for columns greater than the first column and translate to
    \c {Qt::UserRole + index.column()}.

*/
QVariant ComponentModel::data(const QModelIndex &index, int role) const
{
    if (Component *component = componentFromIndex(index)) {
        if (index.column() > 0) {
            if (role == Qt::CheckStateRole)
                return QVariant();
            if (index.column() == ComponentModelHelper::ActionColumn) {
                if (role == Qt::DecorationRole)
                    return iconCache->icon(component->installAction());
                if (role == Qt::ToolTipRole) {
                    switch (component->installAction()) {
                    case ComponentModelHelper::Install:
                        return tr("Component is marked for installation.");
                    case ComponentModelHelper::Uninstall:
                        return tr("Component is marked for uninstallation.");
                    case ComponentModelHelper::KeepInstalled:
                        return tr("Component is installed.");
                    case ComponentModelHelper::KeepUninstalled:
                        return tr("Component is not installed.");
                    default:
                        return QString();
                    }
                }
                return QVariant();
            }
            if (role == Qt::EditRole || role == Qt::DisplayRole || role == Qt::ToolTipRole)
                return component->data(Qt::UserRole + index.column());
        }
        if (role == Qt::CheckStateRole) {
            if (!component->isCheckable())
                return QVariant();
            if (!component->autoDependencies().isEmpty())
                return QVariant();
        }
        return component->data(role);
    }
    return QVariant();
}

/*!
    Sets the \a role data for the item at \a index to \a value. Returns true if successful;
    otherwise returns false. The dataChanged() signal is emitted if the data was successfully set.
    The checkStateChanged() signals are emitted in addition if the checked state of the item is set.
*/
bool ComponentModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid())
        return false;

    Component *component = componentFromIndex(index);
    if (!component)
        return false;

    if (role == Qt::CheckStateRole) {
        if (index.column() != 0)
            return false;
        ComponentSet nodes = component->childItems().toSet();
        Qt::CheckState newValue = Qt::CheckState(value.toInt());
        if (newValue == Qt::PartiallyChecked) {
            const Qt::CheckState oldValue = component->checkState();
            newValue = (oldValue == Qt::Checked) ? Qt::Unchecked : Qt::Checked;
        }
        QSet<QModelIndex> changed = updateCheckedState(nodes << component, newValue);
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
    Returns the data for the given \a role and \a section in the header with the specified
    \a orientation.
    An \e invalid QVariant is returned if \a section is out of bounds,
    \a orientation is not Qt::Horizontal
    or \a role is anything else than Qt::DisplayRole.
*/
QVariant ComponentModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (section >= 0 && section < columnCount() && orientation == Qt::Horizontal && role == Qt::DisplayRole)
        return m_headerData.at(section);
    return QVariant();
}

/*!
    Sets the data for the given \a role and \a section in the header with the specified
    \a orientation to the \a value supplied. Returns \c true if the header's data was updated;
    otherwise returns \c false. The headerDataChanged() signal is emitted if the data was
    successfully set.

    \note Only Qt::Horizontal orientation is supported.
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
    Returns a list of components whose checked state cannot be changed. If package manager
    core is run with no forced installation argument, the list will always be empty.
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
    Returns the current checked state of the model.
*/
ComponentModel::ModelState ComponentModel::checkedState() const
{
    return m_modelState;
}

/*!
    Translates between a given component \a name and its associated QModelIndex. Returns the
    QModelIndex that represents the component or an invalid QModelIndex if the component does
    not exist in the model.
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
    Translates between a given QModelIndex \a index and its associated Component.
    Returns the component if the index is valid or \c 0 if an invalid
    QModelIndex is given.
*/
Component *ComponentModel::componentFromIndex(const QModelIndex &index) const
{
    if (index.isValid())
        return static_cast<Component*>(index.internalPointer());
    return 0;
}


// -- public slots

/*!
    Sets \a rootComponents to be the list of currently shown components.

    The model is repopulated and the individual component's checked state is used to show the check
    mark in front of the visual component representation. The modelAboutToBeReset() and
    modelReset() signals are emitted.
*/
void ComponentModel::setRootComponents(QList<QInstaller::Component*> rootComponents)
{
    beginResetModel();

    m_uncheckable.clear();
    m_indexByNameCache.clear();
    m_rootComponentList.clear();
    m_modelState = DefaultChecked;

    // Initialize these with an empty set for every possible state, cause we compare the hashes later in
    // updateAndEmitModelState(). The comparison than might lead to wrong results if one of the checked
    // states is absent initially.
    m_initialCheckedState[Qt::Checked] = ComponentSet();
    m_initialCheckedState[Qt::Unchecked] = ComponentSet();
    m_initialCheckedState[Qt::PartiallyChecked] = ComponentSet();
    m_currentCheckedState = m_initialCheckedState;  // both should be equal

    // show virtual components only in case we run as updater or if the core engine is set to show them
    const bool showVirtuals = m_core->isUpdater() || m_core->virtualComponentsVisible();
    foreach (Component *const component, rootComponents) {
        connect(component, SIGNAL(virtualStateChanged()), this, SLOT(onVirtualStateChanged()));
        if ((!showVirtuals) && component->isVirtual())
            continue;
        m_rootComponentList.append(component);
    }
    endResetModel();
}

/*!
    Sets the checked state of every component in the model to be \a state.

    The ComponentModel::PartiallyChecked flag is ignored by this function. Note that components
    are not changed if they are not checkable. The dataChanged() and checkStateChanged() signals
    are emitted.
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
        connect(component, SIGNAL(virtualStateChanged()), this, SLOT(onVirtualStateChanged()));
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

void ComponentModel::onVirtualStateChanged()
{
    // If the virtual state of a component changes, force a reset of the component model.
    setRootComponents(m_core->components(PackageManagerCore::ComponentType::Root));
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

    foreach (const Component *component, m_rootComponentList) {
        emit dataChanged(indexFromComponentName(component->name()),
                         indexFromComponentName(component->name()));
        QList<Component *> children = component->childItems();
        foreach (const Component *child, children)
            emit dataChanged(indexFromComponentName(child->name()),
                             indexFromComponentName(child->name()));
    }
}

void ComponentModel::collectComponents(Component *const component, const QModelIndex &parent) const
{
    m_indexByNameCache.insert(component->name(), parent);
    for (int i = 0; i < component->childCount(); ++i)
        collectComponents(component->childAt(i), index(i, 0, parent));
}

namespace ComponentModelPrivate {

static Qt::CheckState verifyPartiallyChecked(Component *component)
{
    bool anyChecked = false;
    bool anyUnchecked = false;

    const int count = component->childCount();
    for (int i = 0; i < count; ++i) {
        switch (component->childAt(i)->checkState()) {
            case Qt::Checked: {
                anyChecked = true;
            }   break;
            case Qt::Unchecked: {
                anyUnchecked = true;
            }   break;
            default:
                return Qt::PartiallyChecked;
        }
        if (anyChecked && anyUnchecked)
            return Qt::PartiallyChecked;
    }

    if (anyChecked)
        return Qt::Checked;

    if (anyUnchecked)
        return Qt::Unchecked;

    return Qt::PartiallyChecked; // never hit here
}

}   // namespace ComponentModelPrivate

QSet<QModelIndex> ComponentModel::updateCheckedState(const ComponentSet &components, Qt::CheckState state)
{
    // get all parent nodes for the components we're going to update
    QMap<QString, Component *> sortedNodesMap;
    foreach (Component *component, components) {
        while (component && !sortedNodesMap.values(component->name()).contains(component)) {
            sortedNodesMap.insertMulti(component->name(), component);
            component = component->parentComponent();
        }
    }

    QSet<QModelIndex> changed;
    const ComponentList sortedNodes = sortedNodesMap.values();
    // we can start in descending order to check node and tri-state nodes properly
    for (int i = sortedNodes.count(); i > 0; i--) {
        Component * const node = sortedNodes.at(i - 1);
        if (!node->isCheckable() || !node->isEnabled() || !node->autoDependencies().isEmpty())
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
