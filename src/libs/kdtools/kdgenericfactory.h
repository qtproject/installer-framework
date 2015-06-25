/****************************************************************************
**
** Copyright (C) 2013 Klaralvdalens Datakonsult AB (KDAB)
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef KDTOOLS__KDGENERICFACTORY_H
#define KDTOOLS__KDGENERICFACTORY_H

#include <kdtoolsglobal.h>

#include <QtCore/QHash>

template <typename T_Product, typename T_Identifier = QString, typename... T_Argument>
class KDGenericFactory
{
public:
    virtual ~KDGenericFactory() {}

    typedef T_Product *(*FactoryFunction)(T_Argument...);

    template <typename T>
    void registerProduct(const T_Identifier &id)
    {
        m_hash.insert(id, &KDGenericFactory::create<T>);
    }

    bool containsProduct(const T_Identifier &id) const
    {
        return m_hash.contains(id);
    }

    T_Product *create(const T_Identifier &id, T_Argument... args) const
    {
        const auto it = m_hash.constFind(id);
        if (it == m_hash.constEnd())
            return 0;
        return (*it)(args...);
    }

protected:
    KDGenericFactory() = default;

private:
    template <typename T>
    static T_Product *create(T_Argument... args)
    {
        return new T(args...);
    }

    KDGenericFactory(const KDGenericFactory &) = delete;
    KDGenericFactory &operator=(const KDGenericFactory &) = delete;

private:
    QHash<T_Identifier, FactoryFunction> m_hash;
};

#endif
