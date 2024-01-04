/**************************************************************************
**
** Copyright (C) 2024 The Qt Company Ltd.
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

#ifndef COMPONENTMODEL_H
#define COMPONENTMODEL_H

#include "qinstallerglobal.h"

#include "component.h"

#include <QtCore/QAbstractItemModel>
#include <QtCore/QList>
#include <QtCore/QSet>
#include <QtCore/QVector>

namespace QInstaller {

class PackageManagerCore;

class INSTALLER_EXPORT ComponentModel : public QAbstractItemModel
{
    Q_OBJECT
    typedef QSet<Component *> ComponentSet;
    typedef QList<Component *> ComponentList;

public:
    enum ModelStateFlag {
        Empty = -0x01,
        AllChecked = 0x01,
        AllUnchecked = 0x02,
        DefaultChecked = 0x04,
        PartiallyChecked = 0x08
    };
    Q_DECLARE_FLAGS(ModelState, ModelStateFlag);

    explicit ComponentModel(int columns, PackageManagerCore *core = 0);
    ~ComponentModel();

    Qt::ItemFlags flags(const QModelIndex &index) const override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    QModelIndex parent(const QModelIndex &child) const override;
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;

    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    bool setHeaderData(int section, Qt::Orientation orientation, const QVariant &value,
        int role = Qt::EditRole) override;

    QSet<Component *> checked() const;
    QSet<Component *> partially() const;
    QSet<Component *> unchecked() const;
    QSet<Component *> uncheckable() const;
    bool componentsSelected() const;

    PackageManagerCore *core() const;
    ComponentModel::ModelState checkedState() const;

    QModelIndex indexFromComponentName(const QString &name) const;
    Component* componentFromIndex(const QModelIndex &index) const;

public Q_SLOTS:
    void reset(QList<Component *> rootComponents = QList<Component *>());
    void setCheckedState(QInstaller::ComponentModel::ModelStateFlag state);

Q_SIGNALS:
    void checkStateChanged(QInstaller::ComponentModel::ModelState state);

private Q_SLOTS:
    void onVirtualStateChanged();

private:
    void postModelReset();
    void updateAndEmitModelState();
    void collectComponents(Component *const component, const QModelIndex &parent) const;
    QSet<QModelIndex> updateCheckedState(const ComponentSet &components, const Qt::CheckState state);

private:
    PackageManagerCore *m_core;

    ModelState m_modelState;
    ComponentSet m_uncheckable;
    QVector<QVariant> m_headerData;
    ComponentList m_rootComponentList;

    QHash<Qt::CheckState, ComponentSet> m_initialCheckedState;
    QHash<Qt::CheckState, ComponentSet> m_currentCheckedState;
    mutable QHash<QString, QPersistentModelIndex> m_indexByNameCache;
};
Q_DECLARE_OPERATORS_FOR_FLAGS(ComponentModel::ModelState);

}   // namespace QInstaller

Q_DECLARE_METATYPE(QInstaller::ComponentModel::ModelState);
Q_DECLARE_METATYPE(QInstaller::ComponentModel::ModelStateFlag);

#endif // COMPONENTMODEL_H
