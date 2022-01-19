/**************************************************************************
**
** Copyright (C) 2022 The Qt Company Ltd.
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

#ifndef COMPONENTSORTFILTERPROXYMODEL_H
#define COMPONENTSORTFILTERPROXYMODEL_H

#include "installer_global.h"

#include <QSortFilterProxyModel>

namespace QInstaller {

class INSTALLER_EXPORT ComponentSortFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    enum AcceptType {
        Direct,
        Descendant,
        Rejected
    };

    explicit ComponentSortFilterProxyModel(QObject *parent = nullptr);

    QVector<QModelIndex> directlyAcceptedIndexes() const;

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    bool acceptsRow(int sourceRow, const QModelIndex &sourceParent, AcceptType *type = nullptr) const;
    bool findDirectlyAcceptedIndexes(const QModelIndex &in, QVector<QModelIndex> &indexes) const;
};

} // namespace QInstaller

#endif // COMPONENTSORTFILTERPROXYMODEL_H
