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

#ifndef KD_UPDATER_SIGNATUREVERIFICATIONRESULT_H
#define KD_UPDATER_SIGNATUREVERIFICATIONRESULT_H

#include "kdupdater.h"

#include <QtCore/QMetaType>
#include <QtCore/QSharedDataPointer>

QT_BEGIN_NAMESPACE
class QString;
QT_END_NAMESPACE

namespace KDUpdater {
    class KDTOOLS_EXPORT SignatureVerificationResult {
    public:
        enum Validity {
            ValidSignature=0,
            UnknownValidity,
            InvalidSignature,
            BadSignature
        };

        explicit SignatureVerificationResult( Validity validity = UnknownValidity );
        SignatureVerificationResult( const SignatureVerificationResult& other );
        ~SignatureVerificationResult();

        SignatureVerificationResult& operator=( const SignatureVerificationResult& other );
        bool operator==( const SignatureVerificationResult& other ) const;

        bool isValid() const;
        Validity validity() const;
        void setValidity( Validity validity );

        QString errorString() const;
        void setErrorString( const QString& errorString );

    private:
        class Private;
        QSharedDataPointer<Private> d;
    };
}

Q_DECLARE_METATYPE( KDUpdater::SignatureVerificationResult )

#endif // KD_UPDATER_SIGNATUREVERIFICATIONRESULT_H
