/**************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
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

#include "componentsortfilterproxymodel.h"

namespace QInstaller {

/*!
    \class QInstaller::ComponentSortFilterProxyModel
    \inmodule QtInstallerFramework
    \brief The ComponentSortFilterProxyModel provides support for sorting and
           filtering data passed between another model and a view.

           The class subclasses QSortFilterProxyModel. Compared to the base class,
           filters affect also child indexes in the base model, meaning if a
           certain row has a parent that is accepted by filter, it is also accepted.
           A distinction is made betweed directly and indirectly accepted indexes.
*/

/*!
    \enum ComponentSortFilterProxyModel::AcceptType

    This enum holds the possible values for filter acception type for model indexes.

    \value Direct
           Index was accepted directly by filter.
    \value Descendant
           Index is a descendant of an accepted index.
    \value Rejected
           Index was not accepted by filter.
*/

/*!
    Constructs object with \a parent.
*/
ComponentSortFilterProxyModel::ComponentSortFilterProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
}

/*!
    Returns a list of source model indexes that were accepted directly by the filter.
*/
QVector<QModelIndex> ComponentSortFilterProxyModel::directlyAcceptedIndexes() const
{
    QVector<QModelIndex> indexes;
    for (int i = 0; i < rowCount(); i++) {
        QModelIndex childIndex = index(i, 0, QModelIndex());
        findDirectlyAcceptedIndexes(childIndex, indexes);
    }
    return indexes;
}

/*!
    Returns \c true if the item in the row indicated by the given \a sourceRow and
    \a sourceParent should be included in the model; otherwise returns \c false.
*/
bool ComponentSortFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    return acceptsRow(sourceRow, sourceParent);
}

/*!
    Returns \c true if the item in the row indicated by the given \a sourceRow and
    \a sourceParent should be included in the model; otherwise returns \c false. The
    acception type can be retrieved with \a type.
*/
bool ComponentSortFilterProxyModel::acceptsRow(int sourceRow, const QModelIndex &sourceParent,
                                               AcceptType *type) const
{
    if (type)
        *type = AcceptType::Rejected;

    if (QSortFilterProxyModel::filterAcceptsRow(sourceRow, sourceParent)) {
        if (type)
            *type = AcceptType::Direct;
        return true;
    }

    if (!isRecursiveFilteringEnabled()) // filter not applied for children
        return false;

    if (!sourceParent.isValid()) // already root
        return false;

    int currentRow = sourceParent.row();
    QModelIndex currentParent = sourceParent.parent();
    // Ascend and check if any parent is accepted
    forever {
        if (QSortFilterProxyModel::filterAcceptsRow(currentRow, currentParent)) {
            if (type)
                *type = AcceptType::Descendant;
            return true;
        }
        if (!currentParent.isValid()) // we hit a root node
            break;

        currentRow = currentParent.row();
        currentParent = currentParent.parent();
    }
    return false;
}

/*!
    Finds directly accepted child \a indexes for parent index \a in. Returns \c true
    if at least one accepted index was found, \c false otherwise.
*/
bool ComponentSortFilterProxyModel::findDirectlyAcceptedIndexes(const QModelIndex &in, QVector<QModelIndex> &indexes) const
{
    bool found = false;
    for (int i = 0; i < rowCount(in); i++) {
        if (findDirectlyAcceptedIndexes(index(i, 0, in), indexes))
            found = true;
    }
    if (!hasChildren(in) || !found) { // No need to check current if any child matched
        AcceptType acceptType;
        const QModelIndex sourceIndex = mapToSource(in);
        acceptsRow(sourceIndex.row(), sourceIndex.parent(), &acceptType);
        if (acceptType == AcceptType::Direct) {
            indexes.append(in);
            found = true;
        }
    }
    return found;
}

} // namespace QInstaller
