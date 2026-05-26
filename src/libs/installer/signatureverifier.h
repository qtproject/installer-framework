#pragma once

#include <QByteArray>
#include <QString>
#include <QList>

class SignatureVerifier
{
public:

    static QByteArray sign(const QByteArray &data,
                                 const QByteArray &privateKeyPem,
                                 QString *errorMessage = nullptr);

    static bool verify(const QByteArray &data,
                             const QByteArray &signature,
                            const QByteArray &publicKeyPem,
                            QString *errorMessage = nullptr);

    static bool verify(const QByteArray &data,
                             const QByteArray &signature,
                            const QList<QByteArray> &publicKeyPemList,
                              QString *errorMessage = nullptr);
};
