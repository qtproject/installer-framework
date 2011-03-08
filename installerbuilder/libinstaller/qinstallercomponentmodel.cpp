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
#include "qinstallercomponentmodel.h"

#include "common/utils.h"
#include "qinstallercomponent.h"
#include "qinstallerglobal.h"

#include <QtScript/QScriptEngine>

#include <algorithm>

using namespace QInstaller;

struct ComponentIsVirtual
{
    bool operator() (const Component *comp) const
    {
        return comp->value(QLatin1String("Virtual"), QLatin1String("false")).toLower() == QLatin1String("true");
    }
};

struct ComponentPriorityLessThan
{
    bool operator() (const Component *lhs, const Component *rhs) const
    {
        const QLatin1String priority("SortingPriority");
        return lhs->value(priority).toInt() < rhs->value(priority).toInt();
    }
};

bool checkCompleteUninstallation(const Component *component)
{
    bool nonSelected = true;
    const QList<Component*> components = component->components(true);
    foreach (const Component *comp, components) {
        if (comp->isSelected())
            nonSelected = false;
        if (!checkCompleteUninstallation(comp))
            nonSelected = false;
    }
    return nonSelected;
}

bool checkWorkRequest(const Component *component)
{
    //first check the component itself
    QString componentName = component->name();
    QString wantedState = component->value(QLatin1String("WantedState"));
    QString currentState = component->value(QLatin1String("CurrentState"));
    if (!wantedState.isEmpty() && !currentState.isEmpty() && wantedState != currentState) {
        verbose() << QLatin1String("request work for ") << componentName
            << QString::fromLatin1(" because WantedState(%1) != CurrentState(%2)").arg(wantedState,
            currentState) << std::endl;
        return true;
    }

    //now checkWorkRequest for all childs
    const QList<Component*> components = component->components(true);
    foreach (const Component *component, components) {
        if (checkWorkRequest(component))
            return true;
    }
    return false;
}


// -- ComponentModel

ComponentModel::ComponentModel(Installer *installer, RunModes runMode)
    : QAbstractItemModel(installer)
    , m_runMode(runMode)
{
    if (runMode == InstallerMode) {
        connect(installer, SIGNAL(componentsAdded(QList<QInstaller::Component*>)), this,
            SLOT(addComponents(QList<QInstaller::Component*>)));
    } else {
        connect(installer, SIGNAL(updaterComponentsAdded(QList<QInstaller::Component*>)),
            this, SLOT(addComponents(QList<QInstaller::Component*>)));
    }

    connect(installer, SIGNAL(componentAdded(QInstaller::Component*)), this,
        SLOT(componentAdded(QInstaller::Component*)));
    connect(installer, SIGNAL(componentsAboutToBeCleared()), this, SLOT(clear()));
}

ComponentModel::~ComponentModel()
{
}

void ComponentModel::addComponents(QList<QInstaller::Component*> newComponents)
{
    beginResetModel();

    bool requestWork = false;
    foreach (Component *component, newComponents) {
        if (!m_components.contains(component))
            m_components.push_back(component);

        if (checkWorkRequest(component)) {
            requestWork = true;
            break;
        }
    }

    endResetModel();
    emit workRequested(requestWork);
}

void ComponentModel::clear()
{
    beginResetModel();

    m_components.clear();
    m_seenComponents.clear();

    endResetModel();
}

void ComponentModel::selectedChanged(bool checked)
{
    Q_UNUSED(checked)
    Component *comp = qobject_cast<Component*>(QObject::sender());
    Q_ASSERT(comp);

    bool requestWork = false;
    foreach (Component *component, comp->components(false, m_runMode)) {
        if (!m_seenComponents.contains(component)) {
            m_seenComponents.push_back(component);
            connect(component, SIGNAL(selectedChanged(bool)), this, SLOT(selectedChanged(bool)));
        }
    }

    foreach (const Component  *component, m_components) {
        if (checkWorkRequest(component)) {
            requestWork = true;
            break;
        }
    }

    emit workRequested(requestWork);
    emit dataChanged(index(0, 0), QModelIndex());
}

void ComponentModel::componentAdded(QInstaller::Component *comp)
{
    Q_UNUSED(comp)
    reset();
}

/*!
 \reimpl
*/
QModelIndex ComponentModel::index(int row, int column, const QModelIndex &parent) const
{
    if (row < 0 || row >= rowCount(parent))
        return QModelIndex();
    if (column < 0 || column >= columnCount(parent))
        return QModelIndex();

    QList<Component*> components = !parent.isValid() ? m_components
        : reinterpret_cast<Component*>(parent.internalPointer())->components(false, m_runMode);

    // don't count virtual components
    if (!virtualComponentsVisible() && m_runMode == InstallerMode) {
        components.erase(std::remove_if(components.begin(), components.end(), ComponentIsVirtual()),
            components.end());
    }

    // sort by priority
    std::sort(components.begin(), components.end(), ComponentPriorityLessThan());

    Component *const component = components[row];

    QModelIndex index = createIndex(row, column, component);

    if (!m_seenComponents.contains(component)) {
        connect(component, SIGNAL(selectedChanged(bool)), this, SLOT(selectedChanged(bool)));
        m_seenComponents.push_back(component);
    }

    return index;
}

/*!
 \reimpl
*/
QModelIndex ComponentModel::parent(const QModelIndex &index) const
{
    if (!index.isValid())
        return QModelIndex();

    const Component *const component = reinterpret_cast<Component*>(index.internalPointer());
    Component *const parentComponent = component->parentComponent(m_runMode);
    if (parentComponent == 0)
        return QModelIndex();

    QList<Component*> parentSiblings = parentComponent->parentComponent() == 0 ? m_components
        : parentComponent->parentComponent()->components(false, m_runMode);

    // don't count virtual components
    if (!virtualComponentsVisible() && m_runMode == InstallerMode) {
        parentSiblings.erase(std::remove_if(parentSiblings.begin(), parentSiblings.end(),
            ComponentIsVirtual()), parentSiblings.end());
    }

    // sort by priority
    std::sort(parentSiblings.begin(), parentSiblings.end(), ComponentPriorityLessThan());

    return createIndex(parentSiblings.indexOf(parentComponent), 0, parentComponent);
}

/*!
 \reimpl
*/
int ComponentModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 4;
}

/*!
 \reimpl
*/
int ComponentModel::rowCount(const QModelIndex &parent) const
{
    if (parent.column() > 0)
        return 0;

    QList<Component*> components = !parent.isValid() ? m_components
        : reinterpret_cast<Component*>(parent.internalPointer())->components(false, m_runMode);

    // don't count virtual components
    if (!virtualComponentsVisible() && m_runMode == InstallerMode) {
        components.erase(std::remove_if (components.begin(), components.end(), ComponentIsVirtual()),
            components.end());
    }
    return components.count();
}

/*!
 \reimpl
*/
Qt::ItemFlags ComponentModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags result = QAbstractItemModel::flags(index);
    if (!index.isValid())
        return result;

    const Component *const component = reinterpret_cast<Component*>(index.internalPointer());
    const bool forcedInstallation = QVariant(component->value(QLatin1String("ForcedInstallation"))).toBool();

    if (m_runMode == UpdaterMode || !forcedInstallation)
        result |= Qt::ItemIsUserCheckable;

    if (!component->isEnabled())
        result &= Qt::ItemIsDropEnabled;

    if (m_runMode == InstallerMode && forcedInstallation) {
        //now it should look like a disabled item
        result &= ~Qt::ItemIsEnabled;
    }

    return result;
}

/*!
 \reimpl
*/
bool ComponentModel::setData(const QModelIndex &index, const QVariant &data, int role)
{
    if (!index.isValid())
        return false;

    switch (role) {
    case Qt::CheckStateRole: {
        if (!(flags(index) & Qt::ItemIsUserCheckable))
            return false;

        Component *const component = reinterpret_cast<Component*>(index.internalPointer());
        component->setSelected(data.toInt() == Qt::Checked, m_runMode);

        bool requestWork = false;
        bool nonSelected = true;
        foreach (const Component *currentComponent, m_components) {
            if (checkWorkRequest(currentComponent))
                requestWork |= true;

            if (currentComponent->isSelected())
                nonSelected = false;

            if (!checkCompleteUninstallation(currentComponent))
                nonSelected = false;
        }

        if (m_runMode == InstallerMode) {
            Installer *installer = qobject_cast< Installer*> (QObject::parent());
            installer->setCompleteUninstallation(nonSelected);
        }

        emit workRequested(requestWork);
        return true;
    }

    default:
        return false;
    }

    return false;
}

Qt::CheckState QInstaller::componentCheckState(const Component *component, RunModes runMode)
{
    if (QVariant(component->value(QLatin1String("ForcedInstallation"))).toBool())
        return Qt::Checked;

    QString autoSelect = component->value(QLatin1String("AutoSelectOn"));
    // check the auto select expression
    if (!autoSelect.isEmpty()) {
        QScriptEngine engine;
        // TODO: check why we use the function from QInstaller instead of Component
        const QList<Component*> children = component->installer()->components(true, runMode);
        foreach (Component *child, children) {
            if (child == component)
                continue;

            QString name = child->name();
            name.replace(QLatin1String("."), QLatin1String("___"));
            engine.evaluate(QString::fromLatin1("%1 = %2;").arg(name,
                QVariant(child ->isSelected()).toString()));
        }

        autoSelect.replace(QLatin1String("."), QLatin1String("___"));
        if (engine.evaluate(autoSelect).toBool())
            return Qt::Checked;
    }

    // if one of our dependees is checked, we are checked
    const QList<Component*> dependees = component->dependees();
    foreach (Component *dependent, dependees) {
        if (dependent == component) {
            verbose() << "Infinite loop in dependencies detected, bailing out..." << std::endl;
            return Qt::Unchecked;
        }

        const Qt::CheckState state = dependent->checkState();
        if (state == Qt::Checked)
            return state;
    }
    
    // if the component has children then we need to check if all children are selected
    // to set the state to either checked if all children are selected or to unchecked
    // if no children are selected or otherwise to partially checked.
    // TODO: check why we use the function from Component instead of QInstaller
    QList<Component*> children = component->components(true, runMode);
    // don't count virtual components
    children.erase(std::remove_if(children.begin(), children.end(), ComponentIsVirtual()), children.end());

    bool foundChecked = false;
    bool foundUnchecked = false;
    if (!children.isEmpty()) {
        foreach (Component *child, children) {
            const Qt::CheckState state = child->checkState();
            foundChecked = foundChecked || state == Qt::Checked || state == Qt::PartiallyChecked;
            foundUnchecked = foundUnchecked || state == Qt::Unchecked;
        }

        if (foundChecked && foundUnchecked)
            return Qt::PartiallyChecked;

        if (foundChecked && ! foundUnchecked)
            return Qt::Checked;

        if (foundUnchecked && ! foundChecked)
            return Qt::Unchecked;
    }
    
    // explicitely selected
    if (component->value(QString::fromLatin1("WantedState")) == QString::fromLatin1("Installed"))
        return Qt::Checked;

    // explicitely unselected
    else if (component->value(QString::fromLatin1("WantedState")) == QString::fromLatin1("Uninstalled"))
        return Qt::Unchecked;
    
    // no decision made, use the predefined:
    const QString suggestedState = component->value(QString::fromLatin1("SuggestedState"));
    if (suggestedState == QString::fromLatin1("Installed"))
        return Qt::Checked;

    return Qt::Unchecked;
}

/*!
 \reimp
*/
QVariant ComponentModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();
        
    Component *const component = reinterpret_cast<Component*>(index.internalPointer());

    switch (index.column()) {
    case NameColumn:
        switch (role)
        {
        case Qt::EditRole:
        case Qt::DisplayRole:
            return component->displayName();
        case Qt::CheckStateRole:
            return component->checkState(m_runMode);
        case Qt::ToolTipRole: {
            return component->value(QString::fromLatin1("Description"))
                + QLatin1String("<br><br> Update Info: ") + component->value(QLatin1String ("UpdateText"));
        }
        case Qt::FontRole:{
            return component->value(QLatin1String("Virtual"),
                QLatin1String("false")).toLower() == QLatin1String("true") ? virtualComponentsFont() : QFont();
        }
        case ComponentRole:
            return qVariantFromValue(component);
        case IdRole:
            return component->name();
        default:
            return QVariant();
        }

    case VersionColumn:
        if (role == Qt::DisplayRole)
            return component->value(QLatin1String("Version"));
        break;

    case InstalledVersionColumn:
        if (role == Qt::DisplayRole)
            return component->value(QLatin1String("InstalledVersion"));
        break;

    case SizeColumn:
        if (role == Qt::DisplayRole) {
            double size = component->value(QLatin1String("UncompressedSize")).toDouble();
            if (size < 10000.0)
                return tr("%L1 Bytes").arg(size);
            size /= 1024.0;
            if (size < 10000.0)
                return tr("%L1 kB").arg(size, 0, 'f', 1);
            size /= 1024.0;
            if (size < 10000.0)
                return tr("%L1 MB").arg(size, 0, 'f', 1);
            size /= 1024.0;
            return tr("%L1 GB").arg(size, 0, 'f', 1);
        }
        break;
    }

    return QVariant();
}

/*!
 \reimp
*/
QVariant ComponentModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QAbstractItemModel::headerData(section, orientation, role);
    
    switch (section) {
    case NameColumn:
        return tr("Name");

    case InstalledVersionColumn:
        return tr("Installed Version");

    case VersionColumn:
        return tr("New Version");

    case SizeColumn:
        return tr("Size");

    default:
        return QAbstractItemModel::headerData(section, orientation, role);
    }
}

static bool s_virtualComponentsVisible = false;

bool ComponentModel::virtualComponentsVisible()
{
    return s_virtualComponentsVisible;
}

void ComponentModel::setVirtualComponentsVisible(bool visible)
{
    s_virtualComponentsVisible = visible;
}

static QFont s_virtualComponentsFont;

QFont ComponentModel::virtualComponentsFont()
{
    return s_virtualComponentsFont;
}

void ComponentModel::setVirtualComponentsFont(const QFont &f)
{
    s_virtualComponentsFont = f;
}

QModelIndex ComponentModel::findComponent(const QString &id) const
{
    QModelIndex idx = index(0, NameColumn);
    while (idx.isValid() && idx.data(IdRole).toString() != id) {
        if (rowCount(idx) > 0) {
            idx = idx.child(0, NameColumn);
            continue;
        }

        bool foundNext = false;
        while (!foundNext) {
            const int oldRow = idx.row();
            idx = idx.parent();
            const int rc = rowCount(idx);
            if (oldRow < rc - 1) {
                idx = index(oldRow + 1, NameColumn, idx);
                foundNext = true;
            } else if (!idx.isValid())
                return idx;
        }
    }
    return idx;
}
