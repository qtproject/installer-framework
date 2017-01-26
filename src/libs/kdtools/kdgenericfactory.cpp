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

#include "kdgenericfactory.h"

/*!
   \inmodule kdupdater
   \class KDGenericFactory
   \brief The KDGenericFactory class implements a template-based generic factory.

   KDGenericFactory is an implementation of the factory pattern. It can be used to produce
   instances of different classes having a common superclass \c T_Product. The user of the
   factory registers those producible classes in the factory by using an identifier
   \c T_Identifier. That identifier can then be used to produce as many instances of the
   registered product as the user wants.

   The advanced user can even choose the type of the class the factory is using to store its
   FactoryFunctions by passing a \c T_Map template parameter. It defaults to QHash.

   KDGenericFactory expects the storage container to be a template class accepting \c T_Identifier
   and \c FactoryFunction as parameters. Additionally it needs to provide:

    \table
        \header
            \li Method
            \li Description
        \row
            \li \c {const_iterator}
            \li A type that provides a random-access iterator that can read a const element and
                when dereferenced has type \c {const &FactoryFunction}.
        \row
            \li \c {insert(T_Identifier, FactoryFunction)}
            \li A function that inserts a new item with \c T_Identifier as key and \c FactoryFunction
                as value. If there is already an item with \c T_Identifier as key, that item's value
                must be replaced with \c FactoryFunction.
        \row
            \li \c {find(T_Identifier)}
            \li A function returning an iterator pointing to the item with the key \c T_Identifier.

        \row
            \li \c {end()}
            \li A function returning an iterator pointing to the imaginary item after the last item.

        \row
            \li \c {size()}
            \li A function returning the number of items.
        \row
            \li \c {remove(T_Identifier)}
            \li A function removing all items that have the key \c T_Identifier.
        \row
            \li \c {keys()}
            \li A function returning a list of all the keys \c T_Identifier in an arbitrary order.
    \endtable

   The only two class templates that currently match this concept are QHash and QMap. QMultiHash
   and QMultiMap do not work, because they violate the requirement on insert() above and std::map
   and std::unordered_map do not match, because they do not have keys() and, because a dereferenced
   iterator has type \c {std::pair<const T_Identifier, FactoryFunction>} instead of just
   \c FactoryFunction.

   The following example shows how the general use case of KDGenericFactory looks like:

    \code

    class Fruit
    {
    };

    class Apple : public Fruit
    {
    };

    class Pear : public Fruit
    {
    };

    int main()
    {
        // creates a common fruit "factory"
        KDGenericFactory< Fruit > fruitPlantation;
        // registers the product "Apple"
        fruitPlantation.registerProduct< Apple >( "Apple" );
        // registers the product "Pear"
        fruitPlantation.registerProduct< Pear >( "Pear" );

        // lets create some stuff - here comes our tasty apple:
        Fruit* myApple = fruitPlantation.create( "Apple" );

        // and a pear, please:
        Fruit* myPear = fruitPlantation.create( "Pear" );

        // ohh - that doesn't work, returns a null pointer:
        Fruit* myCherry = fruitPlantation.create( "Cherry" );
    }
    \endcode
*/

/*!
   \fn KDGenericFactory::~KDGenericFactory()

   Destroys the generic factory.
*/

/*!
    \typedef KDGenericFactory::FactoryFunction

    This typedef defines a factory function producing an object of type T_Product.
*/

/*!
    \typedef KDGenericFactory::FactoryFunctionWithArg

    This typedef defines a factory function producing an object of type T_Product
    with the arguments specified by \a arg.
*/

/*!
   \fn KDGenericFactory::registerProduct( const T_Identifier& name )

   Registers a product of the type T, identified by \a name in the factory.
   Any type with the same name gets unregistered.

   If a product was registered via this method, it will be created using its
   default constructor.
*/

/*!
   \fn KDGenericFactory::registerProductWithArg(const T_Identifier &name)

   Registers a product of the type T, identified by \a name, with arguments.
   Any type with the same name gets unregistered.

   If a product was registered via this method, it will be created using its
   default constructor.
*/

/*!
   \fn KDGenericFactory::create( const T_Identifier& name ) const

   Creates and returns a product of the type T identified by \a name.
   Ownership of the product is transferred to the caller.
*/

/*!
   \fn KDGenericFactory::createWithArg(const T_Identifier &name, const T_Argument &arg) const

   Creates and returns a product of the type T identified by \a name with the
   arguments specified by \a arg.
   Ownership of the product is transferred to the caller.
*/
