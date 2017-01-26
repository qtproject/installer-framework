/****************************************************************************
**
** Copyright (C) 2013 Klaralvdalens Datakonsult AB (KDAB)
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
****************************************************************************/

#ifndef KDTOOLS__KDGENERICFACTORY_H
#define KDTOOLS__KDGENERICFACTORY_H

#include <kdtoolsglobal.h>

#include <QtCore/QHash>

template <typename T_Product, typename T_Identifier = QString, typename T_Argument = QString>
class KDGenericFactory
{
public:
    virtual ~KDGenericFactory() {}

    typedef T_Product *(*FactoryFunction)();
    typedef T_Product *(*FactoryFunctionWithArg)(const T_Argument &arg);

    template <typename T>
    void registerProduct(const T_Identifier &name)
    {
#ifdef Q_CC_MSVC
        FactoryFunction function = &KDGenericFactory::create<T>;
#else // compile fix for old gcc
        FactoryFunction function = &create<T>;
#endif
        map.insert(name, function);
    }

    T_Product *create(const T_Identifier &name) const
    {
        const typename QHash<T_Identifier, FactoryFunction>::const_iterator it = map.find(name);
        if (it == map.end())
            return 0;
        return (*it)();
    }

    template <typename T>
    void registerProductWithArg(const T_Identifier &name)
    {
#ifdef Q_CC_MSVC
        FactoryFunctionWithArg function = &KDGenericFactory::create<T>;
#else // compile fix for old gcc
        FactoryFunctionWithArg function = &create<T>;
#endif
        map2.insert(name, function);
    }

    T_Product *createWithArg(const T_Identifier &name, const T_Argument &arg) const
    {
        const typename QHash<T_Identifier, FactoryFunctionWithArg>::const_iterator it = map2.find(name);
        if (it == map2.end())
            return 0;
        return (*it)(arg);
    }

private:
    template <typename T>
    static T_Product *create()
    {
        return new T;
    }

    template <typename T>
    static T_Product *create(const T_Argument &arg)
    {
        return new T(arg);
    }

    QHash<T_Identifier, FactoryFunction> map;
    QHash<T_Identifier, FactoryFunctionWithArg> map2;
};

#endif
