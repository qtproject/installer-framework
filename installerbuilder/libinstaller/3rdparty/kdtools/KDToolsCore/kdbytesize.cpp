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
#include "kdbytesize.h"

#include <QStringList>
#include <QTextStream>

KDByteSize::KDByteSize( quint64 size )
    : m_size( size )
{
}

KDByteSize::~KDByteSize()
{
}

KDByteSize::operator quint64() const
{
    return m_size;
}

quint64 KDByteSize::size() const
{
    return m_size;
}

QString KDByteSize::toString() const
{
    static const QStringList units = QStringList() << QObject::tr( "B" )
                                                   << QObject::tr( "kB" )
                                                   << QObject::tr( "MB" )
                                                   << QObject::tr( "GB" )
                                                   << QObject::tr( "TB" );

    double s = m_size;
    quint64 factor = 1;
    int unit = 0;
    while( s >= 1024.0 && unit + 1 < units.count() )
    {
        ++unit;
        factor *= 1024;
        s = 1.0 * m_size / factor;
    }

    // only one digit after the decimal point is wanted
    s = qRound( s * 10 ) / 10.0;

    return QString::fromLatin1( "%L1 %2" ).arg( s, 0, 'g', 4 ).arg( units[ unit ] );
}
    
bool operator==( const KDByteSize& lhs, const KDByteSize& rhs )
{
    return lhs.size() == rhs.size();
}

bool operator<( const KDByteSize& lhs, const KDByteSize& rhs )
{
    return lhs.size() < rhs.size();
}

KDByteSize operator*( const KDByteSize& lhs, int rhs )
{
    return KDByteSize( lhs.size() * rhs );
}

#include <QDebug>

QDebug operator<<( QDebug dbg, const KDByteSize& size )
{
    return dbg << "KDByteSize(" << size.size() << ")";
}
