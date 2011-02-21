/**************************************************************************
**
** This file is part of Qt SDK**
**
** Copyright (c) 2011 Nokia Corporation and/or its subsidiary(-ies).*
**
** Contact:  Nokia Corporation qt-info@nokia.com**
**
** No Commercial Usage
**
** This file contains pre-release code and may not be distributed.
** You may use this file in accordance with the terms and conditions
** contained in the Technology Preview License Agreement accompanying
** this package.
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception version
** 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** If you are unsure which license is appropriate for your use, please contact
** (qt-info@nokia.com).
**
**************************************************************************/
#include "kdmmappedfileiodevice.h"

#include <cassert>

#include <QFile>

KDMMappedFileIODevice::KDMMappedFileIODevice( QFile* file, qint64 offset, qint64 length, QObject* parent )
  : QIODevice( parent )
  , m_file( file )
  , m_ownsFile( false )
  , m_offset( offset )
  , m_length( length )
  , m_data( 0 ) {
    assert( m_file );
}

KDMMappedFileIODevice::KDMMappedFileIODevice( const QString& filename, qint64 offset, qint64 length, QObject* parent )
  : QIODevice( parent )
  , m_file( new QFile( filename ) )
  , m_ownsFile( true )
  , m_offset( offset )
  , m_length( length )
  , m_data( 0 ) {
    if ( !open( QIODevice::ReadOnly ) )
        assert( !"Could not open device" );
}

KDMMappedFileIODevice::~KDMMappedFileIODevice() {
    assert( m_file );
    if ( m_ownsFile )
        delete m_file;
}

qint64 KDMMappedFileIODevice::readData( char* data, qint64 maxSize ) {
    ensureMapped();
    if ( maxSize < 0 )
        return 0;
    const qint64 actual = std::min( maxSize, m_length - pos() );
    memcpy( data, m_data + pos(), actual );
    return actual;
}

qint64 KDMMappedFileIODevice::writeData( const char* data, qint64 maxSize ) {
    Q_UNUSED( data )
    Q_UNUSED( maxSize )
    ensureMapped();
    assert( !"not implemented" );
    return -1;
}

bool KDMMappedFileIODevice::ensureMapped() {
    if ( m_data )
        return true;
    assert( m_file );
    m_data = m_file->map( m_offset, m_length );
    return m_data != false;
}

bool KDMMappedFileIODevice::open( QIODevice::OpenMode openMode ) {
    if ( openMode != QIODevice::ReadOnly )
        qWarning( "%s: Write not supported\n", Q_FUNC_INFO );
    if ( openMode == QIODevice::WriteOnly )
        return false;
    const bool opened = m_file->open( openMode );
    if ( opened )
        setOpenMode( openMode );
    return opened;
}

void KDMMappedFileIODevice::close() {
    if ( m_data )
        m_file->unmap( m_data );
    m_data = 0;
    m_file->close();
}

