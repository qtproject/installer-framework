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

#include "genericfactory.h"

/*!
    \inmodule kdupdater
    \namespace KDUpdater
    \brief The KDUpdater classes provide functions to automatically detect
    updates to applications, to retrieve them from external repositories, and to
    install them.

    KDUpdater classes are a fork of KDAB's general
    \l{http://docs.kdab.com/kdtools/2.2.2/group__kdupdater.html}{KDUpdater module}.
*/

/*!
    \inmodule kdupdater
    \class GenericFactory
    \brief The GenericFactory class implements a template-based generic factory.

    GenericFactory is an implementation of the factory pattern. It can be used to produce
    instances of different classes having a common superclass \c BASE. The user of the factory
    registers those producible classes in the factory by using the identifier \c IDENTIFIER. That
    identifier can then be used to produce as many instances of the registered product as the
    user wants.

    One factory instance is able to produce instances of different types of DERIVED classes only
    when the constructor of DERIVED or the registered generator function have a common signature
    for all DERIVED classes. This signature is described by the declaration order of ARGUMENTS. It
    is referred to as SIGNATURE in the following paragraphs.

    If a class derived from BASE does not contain a SIGNATURE matching the registered one for the
    constructor or the generator function, it is not possible to create instances of it using one
    instance of GenericFactory subclass. In that case, more than one GenericFactory subclass
    and instance are needed.

    It is possible to register a subclass of BASE inside an instance of GenericFactory subclass
    using the registerProduct() function. At least one of the following conditions needs to be met:

    \list
        \li A global or static function with SIGNATURE exists.
        \li The DERIVED class has a constructor with SIGNATURE.
        \li The DERIVED class has a static function with SIGNATURE.
    \endlist

    To get a new instance of DERIVED, one needs to call the create() function. The value of
    IDENTIFIER determines the product's subclass registered in the factory, while the values
    of SIGNATURE are the actual arguments passed to the class constructor or the registered
    generator function.
*/

/*!
    \fn GenericFactory::GenericFactory()

    Creates the generic factory.
*/

/*!
    \fn GenericFactory::~GenericFactory()

    Destroys the generic factory.
*/

/*!
    \typedef GenericFactory::FactoryFunction

    This typedef defines a factory function producing an object of type BASE.
*/

/*!
    \fn void GenericFactory::registerProduct(const IDENTIFIER &id)

    Registers a type DERIVED, identified by \a id in the factory. Any type with the same id gets
    unregistered.
*/

/*!
    \overload
    \fn void GenericFactory::registerProduct(const IDENTIFIER &id, FactoryFunction func)

    Registers a function \a func that can create the type DERIVED, identified by \a id in the
    factory. Any type with the same id gets unregistered.
*/

/*!
    \fn bool GenericFactory::containsProduct(const IDENTIFIER &id) const

    Returns \c true if the factory contains a type with the \a id; otherwise returns false.
*/

/*!
    \fn BASE *GenericFactory::create(const IDENTIFIER &id, ARGUMENTS... args) const

    Creates and returns the type identified by \a id, but automatically upcasted to BASE. Ownership
    of the type is transferred to the caller.
*/
