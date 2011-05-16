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

#include "kdlockfile.h"
#include "kdlockfile_p.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QFileInfo>

KDLockFile::Private::~Private()
{
    unlock();    
}

bool KDLockFile::Private::lock()
{
    const QFileInfo fi( filename );
    handle = CreateFile( filename.toStdWString().data(),
                         GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ,
                         NULL, fi.exists() ? OPEN_EXISTING : CREATE_NEW,
                         FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, NULL );

    if ( !handle )
        return false;
    QString pid = QString::number( qApp->applicationPid() );
    QByteArray data = pid.toLatin1();
    DWORD  bytesWritten;
    const bool wrotePid  = WriteFile( handle, data.data(), data.size(), &bytesWritten, NULL );
    if ( !wrotePid )
        return false;
    FlushFileBuffers( handle );

    const bool locked = LockFile( handle, 0, 0, fi.size(), 0 );

    this->locked = locked;
    return locked;
}

bool KDLockFile::Private::unlock()
{
    const QFileInfo fi( filename );
    if ( locked )
    {
        const bool success = UnlockFile( handle, 0, 0, 0, fi.size() );
        this->locked = !success;
        CloseHandle( handle );
        return success;
    }
    return true;
}
