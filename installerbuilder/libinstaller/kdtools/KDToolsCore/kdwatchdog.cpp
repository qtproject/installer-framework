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

#include "kdwatchdog.h"

#include <QTimer>

/*!
 \internal
 */
class KDWatchdog::Private
{
public:
    Private()
        : active( true )
    {
    }

    bool active;
    QTimer timer;
};

/*!
 Creates a new KDWatchdog with \a parent.
 */
KDWatchdog::KDWatchdog( QObject* parent )
    : QObject( parent )
{
    d->timer.setSingleShot( true );
    connect( &d->timer, SIGNAL( timeout() ), this, SIGNAL( timeout() ) );
    setTimeoutInterval( 30000 );
    activate();
}

/*!
 Destroys the KDWatchdog.
 */
KDWatchdog::~KDWatchdog()
{
}

bool KDWatchdog::isActive() const
{
    return d->active;
}

int KDWatchdog::timeoutInterval() const
{
    return d->timer.interval();
}

void KDWatchdog::setTimeoutInterval( int interval )
{
    d->timer.setInterval( interval );
    resetTimeoutTimer();
}

void KDWatchdog::setActive( bool active )
{
    if( d->active == active )
        return;
    d->active = active;
    if( d->active )
        d->timer.start();
    else
        d->timer.stop();
}

/*!
 Activates the watchdog.
 */
void KDWatchdog::activate()
{
    setActive( true );
}

/*!
 Deactivates the watchdog.
 */
void KDWatchdog::deactivate()
{
    setActive( false );
}

/*!
 Resets the watchdog timer. This slot should be called whenever
 the watched event occures.
 If the watchdog has been stop()'ed, nothing happens.
 */
void KDWatchdog::resetTimeoutTimer()
{
    if( d->active )
        d->timer.start();
}

#include "moc_kdwatchdog.cpp"
