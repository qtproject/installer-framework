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

KDLockFile::Private::Private( const QString& filename_ )
    : filename( filename_ )
    , handle( 0 )
    , locked( false )
{
}

KDLockFile::KDLockFile( const QString& name )
    : d( new Private( name ) )
{
}

KDLockFile::~KDLockFile()
{
}

bool KDLockFile::lock()
{
    return d->lock();
}

QString KDLockFile::errorString() const
{
    return d->errorString;
}

bool KDLockFile::unlock()
{
    return d->unlock();
}


#ifdef KDTOOLSCORE_UNITTESTS

#include <KDUnitTest/Test>
#include <QDebug>
#include <QDir>

KDAB_UNITTEST_SIMPLE( KDLockFile, "kdcoretools" ) {
    {
        KDLockFile f( QLatin1String("/jlksdfdsfjkldsf-doesnotexist/file") );
        const bool locked = f.lock();
        assertFalse( locked );
        qDebug() << f.errorString();
        assertTrue( !f.errorString().isEmpty() );
        if ( !locked )
            assertTrue( f.unlock() );
    }
    {
        KDLockFile f( QDir::currentPath() + QLatin1String("/kdlockfile-test") );
        const bool locked = f.lock();
        assertTrue( locked );
        if ( !locked )
            qDebug() << f.errorString();
        assertEqual( locked, f.errorString().isEmpty() );
        const bool unlocked = f.unlock();
        assertTrue( unlocked );
        if ( !unlocked )
            qDebug() << f.errorString();
        assertEqual( unlocked, f.errorString().isEmpty() );
    }
}

#endif // KDTOOLSCORE_UNITTESTS
