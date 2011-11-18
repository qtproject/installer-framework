/**************************************************************************
**
** This file is part of Qt SDK**
**
** Copyright (c) 2010 Nokia Corporation and/or its subsidiary(-ies).*
**
** Contact:  Nokia Corporation qt-info@nokia.com**
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

#ifndef CRYPTOSIGNATUREVERIFIER_H
#define CRYPTOSIGNATUREVERIFIER_H

#include <kdupdatercrypto.h>
#include <kdupdatersignatureverifier.h>
#include <kdupdatersignatureverificationresult.h>


class CryptoSignatureVerifier : public KDUpdater::SignatureVerifier
{
public:
    explicit CryptoSignatureVerifier(const QByteArray &publicKey)
        : m_publicKey(publicKey)
    {
    }

    SignatureVerifier* clone() const
    {
        return new CryptoSignatureVerifier(m_publicKey);
    }

    KDUpdater::SignatureVerificationResult verify(const QByteArray &data, const QByteArray &signature) const
    {
        KDUpdaterCrypto crypto;
        crypto.setPublicKey(m_publicKey);
        KDUpdater::SignatureVerificationResult r;
        r.setValidity(crypto.verify(data, signature) ? KDUpdater::SignatureVerificationResult::ValidSignature
            : KDUpdater::SignatureVerificationResult::BadSignature);
        r.setErrorString( QObject::tr("Bad signature"));
        return r;
    }

    QString type() const
    {
        return QLatin1String("CryptoSignature");
    }

private:
    const QByteArray m_publicKey;
};

#endif
