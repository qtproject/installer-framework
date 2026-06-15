#pragma once

#include <QByteArray>
#include <QString>
#include <QList>

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
  virtual ~SignatureVerifier() = default;
  VerificationResult verify(const QString &filePath,
                      const QString &signaturePath,
                      const QByteArray &publicKeyPem,
                      bool calculateHashFromFile = true,
                      QString *errorMessage = nullptr);
  VerificationResult verify(const QString &filePath,
                      const QString &signaturePath,
                      const QList<QByteArray> &publicKeyPemList,
                      bool calculateHashFromFile = true,
                      QString *errorMessage = nullptr);

protected:
  virtual bool verify(const QByteArray &data,
                      const QByteArray &signature,
                      const QByteArray &publicKeyPem,
                      QString *errorMessage = nullptr) = 0;

  virtual bool verify(const QByteArray &data,
                      const QByteArray &signature,
                      const QList<QByteArray> &publicKeyPemList,
                      QString *errorMessage = nullptr) = 0;

  virtual EVP_PKEY *loadPublicKey(const QByteArray &publicKeyPem) const = 0;

protected:
  QString readOpenSslError() const;

  VerificationResult hardSha256(const QString& filePath,
                                QByteArray &hash,
                                QString *errorMessage) const;
  bool getSignatureData(const QString &filePath,
                        QByteArray &data,
                        QString *errorMessage) const;
  VerificationResult getFileData(const QString &filePath,
                                 QByteArray &data,
                                 bool calculateHashFromFile,
                                 QString *errorMessage) const;
  void setError(QString *errorMessage, const QString &message) const;
};
