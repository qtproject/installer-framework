#pragma once

#include <QByteArray>
#include <QString>
#include <QList>

class OpenSslSignerVerifier
{
public:

    static QByteArray signEd25519(const QByteArray &data,
                                 const QByteArray &privateKeyPem,
                                 QString *errorMessage = nullptr);

    static bool verifyEd25519(const QByteArray &data,
                             const QByteArray &signature,
                            const QByteArray &publicKeyPem,
                            QString *errorMessage = nullptr);

    static bool verifyEd25519(const QByteArray &data,
                             const QByteArray &signature,
                            const QList<QByteArray> &publicKeyPemList,
                              QString *errorMessage = nullptr);
};
