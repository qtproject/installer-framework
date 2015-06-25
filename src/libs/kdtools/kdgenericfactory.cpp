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
   \fn KDGenericFactory::registerProduct(const T_Identifier &id)

   Registers a product of the type T, identified by \a id in the factory. Any type with the same id
   gets unregistered.
*/

/*!
    \fn bool KDGenericFactory::containsProduct(const T_Identifier &id) const

    Returns \c true if the factory contains a product with the \a id; otherwise returns false.
*/

/*!
   \fn KDGenericFactory::create(const T_Identifier &id) const

   Creates and returns a product of the type T identified by \a id. Ownership of the product is
   transferred to the caller.
*/
