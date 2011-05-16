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

#include "kdversion.h"

#include <QDebug>

KDVersion::KDVersion()
{
}

KDVersion::~KDVersion()
{
}

bool KDVersion::isNull() const
{
    return parts.isEmpty();
}

QString KDVersion::toString() const
{
    return parts.join( QChar::fromLatin1( '.' ) );
}

KDVersion KDVersion::fromString( const QString& string )
{
    KDVersion result;
    result.parts = string.split( QChar::fromLatin1( '.' ) );
    return result;
}

bool operator<( const KDVersion& lhs, const KDVersion& rhs )
{
    for( int i = 0; i < lhs.parts.count(); ++i )
    {
        if( i == rhs.parts.count() )
            return false;

        const QString& l = lhs.parts[ i ];
        const QString& r = lhs.parts[ i ];

        bool okl = false;
        bool okr = false;
        const int li = l.toInt( &okl );
        const int ri = r.toInt( &okr );

        if( okl && okr )
        {
            if( li < ri )
                return true;
        }
        else if( QString::localeAwareCompare( l, r ) < 0 )
        {
            return true;
        }
    }
    return true;
}

bool operator==( const KDVersion& lhs, const KDVersion& rhs )
{
    return lhs.parts == rhs.parts;
}

QDebug operator<<( QDebug debug, const KDVersion& version )
{
    return debug << "KDVersion(" << version.toString().toLatin1().data() << ")";
    return debug;
}
