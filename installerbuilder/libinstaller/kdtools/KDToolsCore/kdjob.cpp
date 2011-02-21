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

#include "kdjob.h"

#include <QEventLoop>

class KDJob::Private {
    KDJob* const q;
public:
    explicit Private( KDJob* qq ) : q( qq ), error( KDJob::NoError ), errorString(), caps( KDJob::NoCapabilities ), autoDelete( true ), totalAmount( 100 ), processedAmount( 0 ) {}

    void delayedStart() {
        q->doStart();
        emit q->started( q );
    }

    void waitForSignal( const char* sig ) {
        QEventLoop loop;
        q->connect( q, sig, &loop, SLOT(quit()) );
        loop.exec();
    }

    int error;
    QString errorString;
    KDJob::Capabilities caps;
    bool autoDelete : 1;
    quint64 totalAmount;
    quint64 processedAmount;
};

KDJob::KDJob( QObject* parent ) : QObject( parent ), d( new Private( this ) ) {
}

KDJob::~KDJob() {
    //delete d;
}

bool KDJob::autoDelete() const {
    return d->autoDelete;
}

void KDJob::setAutoDelete( bool autoDelete ) {
    d->autoDelete = autoDelete;
}

int KDJob::error() const {
    return d->error;
}

QString KDJob::errorString() const {
    return d->errorString;
}

void KDJob::emitFinished() {
    emit finished( this );
    if ( d->autoDelete )
        deleteLater();
}

void KDJob::emitFinishedWithError( int error, const QString& errorString ) {
    d->error = error;
    d->errorString = errorString;
    emitFinished();
}

void KDJob::setError( int error ) {
    d->error = error;
}

void KDJob::setErrorString( const QString& errorString ) {
    d->errorString = errorString;
}

void KDJob::waitForStarted() {
    d->waitForSignal( SIGNAL(started(KDJob*)) );
}

void KDJob::waitForFinished() {
    d->waitForSignal( SIGNAL(finished(KDJob*)) );
}

KDJob::Capabilities KDJob::capabilities() const {
    return d->caps;
}

bool KDJob::hasCapability( Capability c ) const {
    return d->caps.testFlag( c );
}

void KDJob::setCapabilities( Capabilities c ) {
    d->caps = c;
}

void KDJob::start() {
    QMetaObject::invokeMethod( this, "delayedStart", Qt::QueuedConnection );
}

void KDJob::doCancel() {
}

void KDJob::cancel() {
    doCancel();
    setError( Canceled );
}

quint64 KDJob::totalAmount() const {
    return d->totalAmount;
}

quint64 KDJob::processedAmount() const {
    return d->processedAmount;
}

void KDJob::setTotalAmount( quint64 amount ) {
    if ( d->totalAmount == amount )
        return;
    d->totalAmount = amount;
    emit progress( this, d->processedAmount, d->totalAmount );
}

void KDJob::setProcessedAmount( quint64 amount ) {
    if ( d->processedAmount == amount )
        return;
    d->processedAmount = amount;
    emit progress( this, d->processedAmount, d->totalAmount );
}

#include "moc_kdjob.cpp"
