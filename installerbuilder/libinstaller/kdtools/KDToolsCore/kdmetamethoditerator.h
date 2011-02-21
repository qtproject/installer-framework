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

#ifndef __KDTOOLSCORE_KDMETAMETHODITERATOR_H__
#define __KDTOOLSCORE_KDMETAMETHODITERATOR_H__

#include <KDToolsCore/pimpl_ptr.h>

#include <QtCore/QMetaMethod>

class KDTOOLSCORE_EXPORT KDMetaMethodIterator
{
public:
    enum AccessType
    {
        Private   = 1 << QMetaMethod::Private,
        Protected = 1 << QMetaMethod::Protected,
        Public    = 1 << QMetaMethod::Public,
        AllAccessTypes = Private | Protected | Public
    };
    Q_DECLARE_FLAGS( AccessTypes, AccessType )

    enum MethodType
    {
        Method      = 0x1,
        Signal      = 0x2, 
        Slot        = 0x4, 
        Constructor = 0x8,
        AllMethodTypes = Method | Signal | Slot | Constructor
    };
    Q_DECLARE_FLAGS( MethodTypes, MethodType )

    enum IteratorFlag
    {
        NoFlags                 = 0x00,
        IgnoreQObjectMethods    = 0x01,
        IgnoreSuperClassMethods = 0x02
    };
    Q_DECLARE_FLAGS( IteratorFlags, IteratorFlag )

    explicit KDMetaMethodIterator( const QMetaObject& metaObject, MethodTypes types = AllMethodTypes, IteratorFlags flags = NoFlags );
    KDMetaMethodIterator( const QMetaObject& metaObject, MethodTypes types, AccessType access, IteratorFlags flags = NoFlags );
    KDMetaMethodIterator( const QMetaObject& metaObject, AccessType access, IteratorFlags flags = NoFlags );

    explicit KDMetaMethodIterator( const QMetaObject* metaObject, MethodTypes types = AllMethodTypes, IteratorFlags flags = NoFlags );
    KDMetaMethodIterator( const QMetaObject* metaObject, MethodTypes types, AccessType access, IteratorFlags flags = NoFlags );
    KDMetaMethodIterator( const QMetaObject* metaObject, AccessType access, IteratorFlags flags = NoFlags );

    explicit KDMetaMethodIterator( const QObject* object, MethodTypes types = AllMethodTypes, IteratorFlags flags = NoFlags );
    KDMetaMethodIterator( const QObject* object, MethodTypes types, AccessType access, IteratorFlags flags = NoFlags );
    KDMetaMethodIterator( const QObject* object, AccessType access, IteratorFlags flags = NoFlags );
    
    ~KDMetaMethodIterator();

    bool hasNext() const;
    QMetaMethod next();

    const char* connectableSignature() const;

private:
    class Priv;
    kdtools::pimpl_ptr< Priv > d;
};


#endif // __KDTOOLSCORE_KDMETAMETHODITERATOR_H__
