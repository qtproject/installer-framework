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

#include "kdrunoncechecker.h"
#include "kdlockfile.h"
#include "kdsysinfo.h"

#include <QtCore/QList>
#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>

#include <algorithm>

class KDRunOnceChecker::Private
{
public:
    Private( const QString& filename );
    ~Private();
    KDLockFile m_lockfile;
    bool m_hasLock;
};

KDRunOnceChecker::Private::Private( const QString& filename )
    : m_lockfile( filename )
    , m_hasLock( false )
{

}

KDRunOnceChecker::Private::~Private()
{

}

KDRunOnceChecker::KDRunOnceChecker( const QString& filename )
    :d( new Private( filename ) )
{

}

KDRunOnceChecker::~KDRunOnceChecker()
{

}

class ProcessnameEquals
{
public:
    ProcessnameEquals( const QString& name ): m_name( name ){}
    bool operator()( const KDSysInfo::ProcessInfo& info )
    {
#ifndef Q_WS_WIN
        if( info.name == m_name )
            return true;
        const QFileInfo fi( info.name );
        if( fi.fileName() == m_name || fi.baseName() == m_name )
            return true;
        return false;
#else
        if( info.name.toLower() == m_name.toLower() )
            return true;
        if( info.name.toLower() == QDir::toNativeSeparators(m_name.toLower()) )
            return true;
        const QFileInfo fi( info.name );
        if( fi.fileName().toLower() == m_name.toLower() || fi.baseName().toLower() == m_name.toLower() )
            return true;
        return info.name == m_name;
#endif
    }

private:
    QString m_name;
};

bool KDRunOnceChecker::isRunning( Dependencies depends )
{
    bool running = false;
    switch ( depends )
    {
    case( Lockfile ):
        {
            const bool locked = d->m_hasLock || d->m_lockfile.lock();
            if ( locked )
                d->m_hasLock = true;
            running = running || ! locked;
        }
        break;
    case( ProcessList ):
        {
            const QList< KDSysInfo::ProcessInfo > allProcesses = KDSysInfo::runningProcesses();
            const QString appName = qApp->applicationFilePath();
            //QList< KDSysInfo::ProcessInfo >::const_iterator it = std::find_if( allProcesses.constBegin(), allProcesses.constEnd(), ProcessnameEquals( appName ) );
            const int count = std::count_if( allProcesses.constBegin(), allProcesses.constEnd(), ProcessnameEquals( appName ) );
            running = running || /*it != allProcesses.constEnd()*/count > 1;
        }
        break;
    case( BOTH ):
        {
            const QList< KDSysInfo::ProcessInfo > allProcesses = KDSysInfo::runningProcesses();
            const QString appName = qApp->applicationFilePath();
            //QList< KDSysInfo::ProcessInfo >::const_iterator it = std::find_if( allProcesses.constBegin(), allProcesses.constEnd(), ProcessnameEquals( appName ) );
            const int count = std::count_if( allProcesses.constBegin(), allProcesses.constEnd(), ProcessnameEquals( appName ) );
            const bool locked = d->m_hasLock || d->m_lockfile.lock();
            if ( locked )
                d->m_hasLock = true;
            running = running || ( /*it != allProcesses.constEnd()*/count > 1 && !locked );

        }
        break;
    }

    return running;
}
