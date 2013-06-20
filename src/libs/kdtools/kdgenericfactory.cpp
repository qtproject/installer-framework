/****************************************************************************
**
** Copyright (C) 2013 Klaralvdalens Datakonsult AB (KDAB)
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
****************************************************************************/

#include "kdgenericfactory.h"

/*!
   \class KDGenericFactory
   \ingroup core
   \brief Template based generic factory implementation
   \since_c 2.1

   (The exception safety of this class has not been evaluated yet.)

   KDGenericFactory is an implemention of of the factory pattern. It can be used to
   "produce" instances of different classes having a common superclass
   T_Product. The user of the
   factory registers those producable classes in the factory by using an identifier
   (T_Identifier, defaulting to QString). That identifer can then be used to
   produce as many instances of the registered product as he wants.

   The advanced user can even choose the type of the map the factory is using to store its
   FactoryFunctions by passing a T_Map template parameter. It defaults to QHash. KDGenericFactory
   expects it to be a template class accepting T_Identifier and FactoryFunction as parameters.
   Additionally it needs to provide:

   \li\link QHash::const_iterator a nested %const_iterator \endlink typedef for an iterator type that when dereferenced has type ((const) reference to) FactoryFunction (Qt convention),
   \li\link QHash::insert %insert( T_Identifier, FactoryFunction ) \endlink, which must overwrite any existing entries with the same identifier.
   \li\link QHash::find %find( T_Identifier ) \endlink,
   \li\link QHash::end %end() \endlink,
   \li\link QHash::size %size() \endlink,
   \li\link QHash::remove %remove( T_Identifier ) \endlink, and
   \li\link QHash::keys %keys ) \endlink, returning a QList<T_Identifier>.

   The only two class templates that currently match this concept are
   QHash and QMap. QMultiHash and QMulitMap do not work, since they
   violate the requirement on insert() above, and std::map and
   std::unordered_map do not match because they don't have keys() and
   because a dereferenced iterator has type
   std::pair<const T_Identifier,FactoryFunction>
   instead of just FactoryFunction.

   \section general-use General Use

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
   \fn KDGenericFactory::~KDGenericFactory

   Destructor.
*/

/*!
    \typedef KDGenericFactory::FactoryFunction

    This typedef defines a factory function producing an object of type T_Product.
*/

/*!
   \fn KDGenericFactory::registerProduct( const T_Identifier& name )

   Registers a product of type T, identified by \a name in the factory.
   Any type with the same name gets unregistered.

   If a product was registered via this method, it will be created using its
   default constructor.
*/

/*!
   \fn KDGenericFactory::unregisterProduct( const T_Identifier& name )

   Unregisters the previously registered product identified by \a name from the factory.
   If no such product is known, nothing is done.
*/

/*!
   \fn KDGenericFactory::productCount() const

   Returns the number of different products known to the factory.
*/

/*!
   \fn KDGenericFactory::availableProducts() const

   Returns the list of products known to the factory.
*/

/*!
   \fn KDGenericFactory::create( const T_Identifier& name ) const

   Creates and returns a product of the type identified by \a name.
   Ownership of the product is transferred to the caller.
*/

/*!
   \fn KDGenericFactory::registerProductionFunction( const T_Identifier& name, FactoryFunction create )

   Subclasses can use this method to register their own FactoryFunction \a create to create products of
   type T, identified by \a name. When a product is registered via this method, it will be created
   by calling create().
*/
