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

#ifndef KDTOOLS_KDUPDATERCRYPTO_H
#define KDTOOLS_KDUPDATERCRYPTO_H

#include "kdupdater.h"

QT_BEGIN_NAMESPACE
class QByteArray;
class QIODevice;
QT_END_NAMESPACE

/**
 * Class that provides cryptographic functionality like signing and verifying
 * or encrypting and decrypting content.
 */
class KDTOOLS_EXPORT KDUpdaterCrypto
{
public:
    class PasswordProvider
    {
    public:
        virtual ~PasswordProvider() {}
        virtual QByteArray password() const = 0;
    };

    KDUpdaterCrypto();
    virtual ~KDUpdaterCrypto();

    /**
     * The private key.
     */
    QByteArray privateKey() const;
    void setPrivateKey(const QByteArray &key);

    /**
     * The password for the private key.
     */
    QByteArray privatePassword() const;
    void setPrivatePassword(const QByteArray &passwd);

    void setPrivatePasswordProvider(const PasswordProvider *provider);

    /**
     * The public key.
     */
    QByteArray publicKey() const;
    void setPublicKey(const QByteArray &key);

    /**
     * Encrypt content using the public key.
     */
    QByteArray encrypt(const QByteArray &plaintext);

    /**
     * Decript encrypted content using the private key.
     */
    QByteArray decrypt(const QByteArray &encryptedtext);

    /**
     * Sign content with the private key.
     */
    QByteArray sign(const QByteArray &data);
    QByteArray sign(const QString &path);
    QByteArray sign(QIODevice *dev);

    /**
     * Verify signed content with the public key.
     */
    bool verify(const QByteArray &data, const QByteArray &signature);
    bool verify(const QString &dataPath, const QString &signaturePath);
    bool verify(const QString &dataPath, const QByteArray &signature);
    bool verify(QIODevice *dev, const QByteArray &signature);

private:
    class Private;
    Private *d;
};

#endif // KDTOOLS_KDUPDATERCRYPTO_H
