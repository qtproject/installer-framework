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

#include "kdsysinfo.h"

#include <algorithm>

KDSysInfo::KDSysInfo()
{
}

KDSysInfo::~KDSysInfo()
{
}

#include <QDir>
#include <QDebug>

QDebug operator<<( QDebug dbg, KDSysInfo::OperatingSystemType type )
{
    switch( type )
    {
    case KDSysInfo::Linux:
        return dbg << "Linux";
    case KDSysInfo::MacOSX:
        return dbg << "Mac OS X";
    case KDSysInfo::Windows:
        return dbg << "Windows";
    default:
        return dbg << "<unknown>";
    }
}

QDebug operator<<( QDebug dbg, KDSysInfo::ArchitectureType type )
{
    switch( type )
    {
    case KDSysInfo::ARM:
        return dbg << "ARM";
    case KDSysInfo::Intel:
        return dbg << "Intel";
    case KDSysInfo::AMD64:
        return dbg << "AMD64";
    case KDSysInfo::IA64:
        return dbg << "IA64";
    case KDSysInfo::PowerPC:
        return dbg << "Power PC";
    case KDSysInfo::Motorola68k:
        return dbg << "Motorola 68k";
    default:
        return dbg << "<unknown>";
    }
}

QDebug operator<<( QDebug dbg, KDSysInfo::Volume volume )
{
    return dbg << "KDSysInfo::Volume(" << volume.path() << ")";
}

QPair< KDByteSize, KDByteSize > volumeSpace( const QString& volume );
QString volumeName( const QString& volume );

class KDSysInfo::Volume::Private : public QSharedData
{
public:
    QString p;
    QString name;
    KDByteSize size;
    QString fileSystemType;
    KDByteSize availableSpace;
};


KDSysInfo::Volume::Volume() 
    : d( new Private )
{
}

KDSysInfo::Volume::Volume( const Volume& other ) 
    : d( other.d )
{
}

KDSysInfo::Volume::~Volume()
{
}

void KDSysInfo::Volume::swap( KDSysInfo::Volume& other )
{
    std::swap( d, other.d );
}

KDSysInfo::Volume& KDSysInfo::Volume::operator=( const KDSysInfo::Volume& other )
{
    KDSysInfo::Volume tmp( other );
    swap( tmp );
    return *this;
}

void KDSysInfo::Volume::setPath( const QString& path )
{
    d->p = path;
}

bool KDSysInfo::Volume::operator == ( const Volume& other ) const
{
    return d->name == other.d->name && d->p == other.d->p;
}

void KDSysInfo::Volume::setName( const QString& name )
{
    d->name = name;
}

QString KDSysInfo::Volume::name() const
{
    return d->name;
}

QString KDSysInfo::Volume::path() const
{
    return d->p;
}

KDByteSize KDSysInfo::Volume::size() const
{
    return d->size;
}

void KDSysInfo::Volume::setSize( const KDByteSize& size )
{
    d->size = size;
}

QString KDSysInfo::Volume::fileSystemType() const
{
    return d->fileSystemType;
}

void KDSysInfo::Volume::setFileSystemType(const QString &type)
{
    d->fileSystemType = type;
}

KDByteSize KDSysInfo::Volume::availableSpace() const
{
    return d->availableSpace;
}

void KDSysInfo::Volume::setAvailableSpace( const KDByteSize& available )
{
    d->availableSpace = available;
}

struct PathLongerThan
{
    bool operator()( const KDSysInfo::Volume& lhs, const KDSysInfo::Volume& rhs ) const
    {
        return lhs.path().length() > rhs.path().length();
    }
};

KDSysInfo::Volume KDSysInfo::Volume::fromPath( const QString& path )
{
    QList< KDSysInfo::Volume > volumes = mountedVolumes();
    // sort by length to get the longest mount point (not just "/") first
    std::sort( volumes.begin(), volumes.end(), PathLongerThan() );
    for( QList< KDSysInfo::Volume >::const_iterator it = volumes.constBegin(); it != volumes.constEnd(); ++it )
    {
#ifdef Q_WS_WIN
        if( QDir::toNativeSeparators( path ).toLower().startsWith( it->path().toLower() ) )
#else
        if( QDir(path).canonicalPath().startsWith( it->path() ) )
#endif
            return *it;
    }
    return KDSysInfo::Volume();
}

QDebug operator<<( QDebug dbg, KDSysInfo::ProcessInfo process )
{
    return dbg << "KDSysInfo::ProcessInfo(" << process.id << ", " << process.name << ")";
}
