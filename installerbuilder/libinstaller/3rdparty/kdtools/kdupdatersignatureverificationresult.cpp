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

#include "kdupdatersignatureverificationresult.h"

#include <QSharedData>
#include <QString>

#include <algorithm>

using namespace KDUpdater;

class SignatureVerificationResult::Private : public QSharedData
{
public:
    Private() : QSharedData(), validity( SignatureVerificationResult::UnknownValidity )
    {
    }
    Private( const Private& other ) : QSharedData( other ), validity( other.validity ), errorString( other.errorString )  {
    }

    bool operator==( const Private& other ) const {
        return validity == other.validity && errorString == other.errorString;
    }

    SignatureVerificationResult::Validity validity;
    QString errorString;
};

SignatureVerificationResult::SignatureVerificationResult( Validity validity ) 
    : d( new Private ) 
{
    setValidity( validity );
}

SignatureVerificationResult::SignatureVerificationResult( const SignatureVerificationResult& other ) : d( other.d ) {
}

SignatureVerificationResult::~SignatureVerificationResult() {
}

SignatureVerificationResult& SignatureVerificationResult::operator=( const SignatureVerificationResult& other ) {
    SignatureVerificationResult copy( other );
    std::swap( d, copy.d );
    return *this;
}

bool SignatureVerificationResult::operator==( const SignatureVerificationResult& other ) const {
    return *d == *other.d;
}

bool SignatureVerificationResult::isValid() const {
    return d->validity == ValidSignature;
}

SignatureVerificationResult::Validity SignatureVerificationResult::validity() const {
    return d->validity;
}

void SignatureVerificationResult::setValidity( Validity validity ) {
    d->validity = validity;
}

QString SignatureVerificationResult::errorString() const {
    return d->errorString;
}

void SignatureVerificationResult::setErrorString( const QString& errorString ) {
    d->errorString = errorString;
}

