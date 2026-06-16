#pragma once

#include <QByteArray>
#include <QString>
#include <QList>
#include <QSharedPointer>

struct evp_pkey_st;
typedef struct evp_pkey_st EVP_PKEY;

class SignatureVerifier
{
public:
  enum VerificationResult {
      Success,
      SignatureVerificationFailed,
      DataFileError,
      SignatureFileError,
      CalculateHashError,
  };

  enum class SignatureAlgorithm {
      Ed25519,
      ECDSA_P256,
  };

  static QSharedPointer<SignatureVerifier> createVerifier(SignatureAlgorithm algorithm);
  virtual ~SignatureVerifier() = default;
  VerificationResult verify(const QString &filePath,
                      const QString &signaturePath,
                      const QByteArray &publicKeyPem,
                      bool calculateHashFromFile = true);
  VerificationResult verify(const QString &filePath,
                      const QString &signaturePath,
                      const QList<QByteArray> &publicKeyPemList,
                      bool calculateHashFromFile = true);

  QString errorString() const;

protected:
  virtual bool verify(const QByteArray &data,
                      const QByteArray &signature,
                      const QByteArray &publicKeyPem) = 0;

  virtual bool verify(const QByteArray &data,
                      const QByteArray &signature,
                      const QList<QByteArray> &publicKeyPemList) = 0;

  virtual EVP_PKEY *loadPublicKey(const QByteArray &publicKeyPem) const = 0;

protected:
  QString readOpenSslError() const;

  VerificationResult calculateSha256(const QString& filePath,
                                QByteArray &hash) const;
  bool getSignatureData(const QString &filePath,
                        QByteArray &data) const;
  VerificationResult getFileData(const QString &filePath,
                                 QByteArray &data,
                                 bool calculateHashFromFile) const;

protected:
  mutable QString m_errorString;
};
