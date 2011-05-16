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

#include "kdlockfile_p.h"

#include <QtCore/QCoreApplication>

#include <cerrno>

#include <sys/file.h>

KDLockFile::Private::~Private()
{
    unlock();
}

bool KDLockFile::Private::lock()
{
    if ( locked )
        return true;

    errorString.clear();
    errno = 0;
    handle = open( filename.toLatin1().constData(), O_CREAT | O_RDWR | O_NONBLOCK );
    if ( handle == -1 ) {
        errorString = QObject::tr("Could not create lock file %1: %2").arg( filename, QLatin1String( strerror( errno ) ) ); 
        return false;
    }
    const QString pid = QString::number( qApp->applicationPid() );
    const QByteArray data = pid.toLatin1();
    errno = 0;
    qint64 written = 0;
    while ( written < data.size() ) {
        const qint64 n = write( handle, data.constData() + written, data.size() - written );
        if ( n < 0 ) {
            errorString = QObject::tr("Could not write PID to lock file %1: %2").arg( filename, QLatin1String( strerror( errno ) ) );
            return false;
        }
        written += n;
    }
    errno = 0;
    locked = flock( handle, LOCK_NB | LOCK_EX ) != -1;
    if ( !locked )
        errorString = QObject::tr("Could not lock lock file %1: %2").arg( filename, QLatin1String( strerror( errno ) ) );
    return locked;
}

bool KDLockFile::Private::unlock()
{
    errorString.clear();
    if ( !locked )
        return true;
    errno = 0;
    locked = flock( handle, LOCK_UN | LOCK_NB ) == -1;
    if ( locked )
        errorString = QObject::tr("Could not unlock lock file %1: %2").arg( filename, QLatin1String( strerror( errno ) ) );
    return !locked;
}
