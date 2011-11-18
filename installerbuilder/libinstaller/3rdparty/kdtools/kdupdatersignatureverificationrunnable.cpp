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

#include "kdupdatersignatureverificationrunnable.h"
#include "kdupdatersignatureverifier.h"
#include "kdupdatersignatureverificationresult.h"

#include <QByteArray>
#include <QIODevice>
#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QThreadPool>
#include <QVariant>
#include <QVector>

#include <cassert>

using namespace KDUpdater;

class Runnable::Private {
public:
    QVector<QObject*> receivers;
    QVector<QByteArray> methods;
};

Runnable::Runnable() : QRunnable(), d( new Private ) {
}

Runnable::~Runnable() {
    delete d;
}


void Runnable::addResultListener( QObject* receiver, const char* method ) {
    d->receivers.push_back( receiver );
    d->methods.push_back( QByteArray( method ) );
}

void Runnable::emitResult( const QGenericArgument& arg0,
                           const QGenericArgument& arg1,
                           const QGenericArgument& arg2,
                           const QGenericArgument& arg3,
                           const QGenericArgument& arg4,
                           const QGenericArgument& arg5,
                           const QGenericArgument& arg6,
                           const QGenericArgument& arg7,
                           const QGenericArgument& arg8,
                           const QGenericArgument& arg9 ) {
    assert( d->receivers.size() == d->methods.size() );
    for ( int i = 0; i < d->receivers.size(); ++i ) {
        QMetaObject::invokeMethod( d->receivers[i],
                                   d->methods[i].constData(),
                                   Qt::QueuedConnection,
                                   arg0,
                                   arg1,
                                   arg2,
                                   arg3,
                                   arg4,
                                   arg5,
                                   arg6,
                                   arg7,
                                   arg8,
                                   arg9 );
    }
}

class SignatureVerificationRunnable::Private {
public:
    Private() : verifier( 0 ) {}
    const SignatureVerifier* verifier;
    QPointer<QIODevice> device;
    QByteArray signature;
};

SignatureVerificationRunnable::SignatureVerificationRunnable() : Runnable(), d( new Private ) {
}

SignatureVerificationRunnable::~SignatureVerificationRunnable() {
    delete d;
}

const SignatureVerifier* SignatureVerificationRunnable::verifier() const {
    return d->verifier;
}

void SignatureVerificationRunnable::setVerifier( const SignatureVerifier* verifier ) {
    delete d->verifier;
    d->verifier = verifier ? verifier->clone() : 0;
}

QByteArray SignatureVerificationRunnable::signature() const {
    return d->signature;
}

void SignatureVerificationRunnable::setSignature( const QByteArray& sig ) {
    d->signature = sig;
}

QIODevice* SignatureVerificationRunnable::data() const {
    return d->device;
}

void SignatureVerificationRunnable::setData( QIODevice* device ) {
    d->device = device;
}


void SignatureVerificationRunnable::run() {
    QThreadPool::globalInstance()->releaseThread();
    const SignatureVerificationResult result = d->verifier->verify( d->device->readAll(), d->signature );
    QThreadPool::globalInstance()->reserveThread();
    delete d->verifier;
    delete d->device;
    emitResult( Q_ARG( KDUpdater::SignatureVerificationResult, result ) );
}


