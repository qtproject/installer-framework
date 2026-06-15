#pragma once

#include <QByteArray>
#include <QString>
#include <QList>
#include "signatureverifier.h"

class ECDSAP256SignatureVerifier : public SignatureVerifier
{
public:
  virtual ~ECDSAP256SignatureVerifier() = default;
  using SignatureVerifier::verify;

protected:
  virtual bool verify(const QByteArray &data,
                      const QByteArray &signature,
                      const QByteArray &publicKeyPem,
                      QString *errorMessage = nullptr) override;

  virtual bool verify(const QByteArray &data,
                      const QByteArray &signature,
                      const QList<QByteArray> &publicKeyPemList,
                      QString *errorMessage = nullptr) override;
                      
  virtual EVP_PKEY *loadPublicKey(const QByteArray &publicKeyPem) const override;
};
