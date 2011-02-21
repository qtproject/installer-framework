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
#include "blockingbuffer.h"

#include <QByteArray>
#include <QMutex>
#include <QMutexLocker>
#include <QWaitCondition>

#include <cassert>

using namespace QInstaller;

class BlockingBuffer::Private {
public:
    explicit Private( int siz ) : size( siz ), readPos( 0 ), writePos( 0 ), closed( false ), mutex( QMutex::Recursive ) {
        data.resize( size );
    }

    int bytesFree() const {
        return size - bytesUsed();
    }

    int bytesUsed() const {
        return writePos >= readPos ? ( writePos - readPos ) : ( size - readPos + writePos );
    }

    const int size;
    QByteArray data;
    int readPos;
    int writePos;
    bool closed;
    QMutex mutex;
    QWaitCondition canWrite;
    QWaitCondition canRead;
};

BlockingBuffer::BlockingBuffer( int size ) : d( new Private( size ) ) {
}

BlockingBuffer::~BlockingBuffer() {
    delete d;
}

qint64 BlockingBuffer::read( char* buf, qint64 max ) {
    assert( max >= 0 );
    if ( max == 0 )
        return 0;
    const QMutexLocker lock( &d->mutex );
    while ( d->bytesUsed() == 0 && !d->closed )
        d->canRead.wait( &d->mutex );
    const int actual = qMin<int>( max, d->bytesUsed() );
    if ( actual == 0 ) // closed
        return 0;

    const int untilEnd = qMin( d->size - 1 - d->readPos, actual );
    memcpy( buf, d->data.constData() + d->readPos, untilEnd );
    d->readPos = ( d->readPos + untilEnd ) % d->size;
    const int left = actual - untilEnd;
    if ( left > 0 ) {
        memcpy( buf + untilEnd, d->data.constData(), left );
        d->readPos = left;
    }

    d->canWrite.wakeAll();
    return actual;
}

qint64 BlockingBuffer::write( const char* buf, qint64 max ) {
    assert( max >= 0 );
    if ( max == 0 )
        return 0;
    const QMutexLocker lock( &d->mutex );
    while ( d->bytesFree() == 0 && !d->closed )
        d->canWrite.wait( &d->mutex );
    if ( d->closed ) // closed, writing fails
        return -1;
    const int actual = qMin<int>( max, d->bytesFree() );
    assert( actual > 0 );
    const int untilEnd = qMin( d->size - 1 - d->readPos, actual );
    memcpy( d->data.data() + d->writePos, buf, untilEnd );
    d->writePos = ( d->writePos + untilEnd ) % d->size;
    const int left = actual - untilEnd;
    if ( left > 0 ) {
        memcpy( d->data.data(), buf + untilEnd, left );
        d->writePos = left;
    }

    d->canRead.wakeAll();
    return actual;
}

void BlockingBuffer::close() {
    const QMutexLocker lock( &d->mutex );
    d->closed = true;
}
