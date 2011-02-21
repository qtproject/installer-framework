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

#include "kdupdaterufcompresscommon_p.h"

#include <QCryptographicHash>
#include <QDataStream>

using namespace KDUpdater;

bool UFHeader::isValid() const
{
    return magic == QLatin1String( KD_UPDATER_UF_HEADER_MAGIC ) &&
           fileList.count() == permList.count() &&
           fileList.count() == isDirList.count();
}

void UFHeader::addToHash(QCryptographicHash& hash) const 
{
    QByteArray data;
    QDataStream stream( &data, QIODevice::WriteOnly );
    stream << *this;
    hash.addData(data);
}

UFEntry::UFEntry() 
    : permissions( 0 )
{
}

bool UFEntry::isValid() const
{
    return !fileName.isEmpty();
}

void UFEntry::addToHash(QCryptographicHash& hash) const 
{
    QByteArray data;
    QDataStream stream( &data, QIODevice::WriteOnly );
    stream.setVersion( QDataStream::Qt_4_2 );
    stream << *this;
    hash.addData(data);
}

namespace KDUpdater
{

QDataStream& operator<<( QDataStream& stream, const UFHeader& hdr )
{
    stream << hdr.magic;
    stream << hdr.fileList;
    stream << hdr.permList;
    stream << hdr.isDirList;
    return stream;
}

QDataStream& operator>>( QDataStream& stream, UFHeader& hdr )
{
    const QDataStream::Status oldStatus = stream.status();
    stream >> hdr.magic;
    if( stream.status() == QDataStream::Ok && hdr.magic != QLatin1String( KD_UPDATER_UF_HEADER_MAGIC ) )
        stream.setStatus( QDataStream::ReadCorruptData );
    
    if( stream.status() == QDataStream::Ok )
        stream >> hdr.fileList;

    if( stream.status() == QDataStream::Ok )
        stream >> hdr.permList;
    
    if( stream.status() == QDataStream::Ok )
        stream >> hdr.isDirList;
    
    if( stream.status() == QDataStream::Ok && ( hdr.fileList.count() != hdr.permList.count() || hdr.permList.count() != hdr.isDirList.count() ) )
        stream.setStatus( QDataStream::ReadCorruptData );
   
    if( stream.status() != QDataStream::Ok )
        hdr = UFHeader();

    if( oldStatus != QDataStream::Ok )
        stream.setStatus( oldStatus );

    return stream;
}

QDataStream& operator<<( QDataStream& stream, const UFEntry& entry ) 
{
    stream << entry.fileName;
    stream << entry.permissions;
    stream << entry.fileData;
    return stream;
}

QDataStream& operator>>( QDataStream& stream, UFEntry& entry ) 
{
    const QDataStream::Status oldStatus = stream.status();
    if( stream.status() == QDataStream::Ok )
        stream >> entry.fileName;
    if( stream.status() == QDataStream::Ok )
        stream >> entry.permissions;
    if( stream.status() == QDataStream::Ok )
        stream >> entry.fileData;

    if( stream.status() != QDataStream::Ok )
        entry = UFEntry();

    if( oldStatus != QDataStream::Ok )
        stream.setStatus( oldStatus );

    return stream;
}

}
