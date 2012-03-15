/****************************************************************************
** Copyright (C) 2001-2010 Klaralvdalens Datakonsult AB.  All rights reserved.
**
** This file is part of the KD Tools library.
**
** Licensees holding valid commercial KD Tools licenses may use this file in
** accordance with the KD Tools Commercial License Agreement provided with
** the Software.
**
**
** This file may be distributed and/or modified under the terms of the
** GNU Lesser General Public License version 2 and version 3 as published by the
** Free Software Foundation and appearing in the file LICENSE.LGPL included.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
** Contact info@kdab.com if any conditions of this licensing are not
** clear to you.
**
**********************************************************************/

#ifndef KDTOOLS__KDGENERICFACTORY_H
#define KDTOOLS__KDGENERICFACTORY_H

#include <kdtoolsglobal.h>

#include <QtCore/QHash>

template <typename T_Product, typename T_Identifier = QString>
class KDGenericFactory
{
public:
    virtual ~KDGenericFactory() {}

    typedef T_Product *(*FactoryFunction)();

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

private:
    template <typename T>
    static T_Product *create()
    {
        return new T;
    }

    QHash<T_Identifier, FactoryFunction> map;
};

#endif
