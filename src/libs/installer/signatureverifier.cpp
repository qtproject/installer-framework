#include "signatureverifier.h"

#include <openssl/err.h>
#include <openssl/evp.h>
#include <QFile>

void SignatureVerifier::setError(QString *errorMessage, const QString &message) const
{
    if (errorMessage) {
        *errorMessage = message;
    }
}

QString SignatureVerifier::readOpenSslError() const
{
    unsigned long errCode = ERR_get_error();
    if (errCode == 0) {
        return QStringLiteral("No OpenSSL error");
    }
    char errBuffer[256];
    ERR_error_string_n(errCode, errBuffer, sizeof(errBuffer));
    return QString::fromUtf8(errBuffer);
}

SignatureVerifier::VerificationResult SignatureVerifier::hardSha256(const QString& filePath, QByteArray &hash, QString *errorMessage) const
{
    unsigned char hashBuffer[EVP_MAX_MD_SIZE];
    unsigned int hashLength = 0;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(errorMessage, QStringLiteral("Failed to open file: %1").arg(file.errorString()));
        return VerificationResult::DataFileError;
    }

    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    if (!mdctx)
    {
        setError(errorMessage, QStringLiteral("EVP_MD_CTX_new failed"));
        return VerificationResult::CalculateHashError;
    }

    if (EVP_DigestInit_ex(mdctx, EVP_sha256(), nullptr) != 1) {
        EVP_MD_CTX_free(mdctx);
        setError(errorMessage, readOpenSslError());
        return VerificationResult::CalculateHashError;
    }

    constexpr qint64 kChunkSize = 100 *1024 * 1024; // 100 MB
    while (!file.atEnd()) {
        const QByteArray chunk = file.read(kChunkSize);
        if (chunk.isEmpty() && file.error() != QFile::NoError) {
            EVP_MD_CTX_free(mdctx);
            setError(errorMessage, QStringLiteral("Failed to read file: %1").arg(file.errorString()));
            return VerificationResult::CalculateHashError;
        }

        if (!chunk.isEmpty()
                && EVP_DigestUpdate(mdctx, chunk.constData(), chunk.size()) != 1) {
            EVP_MD_CTX_free(mdctx);
            setError(errorMessage, readOpenSslError());
            return VerificationResult::CalculateHashError;
        }
    }

    if (EVP_DigestFinal_ex(mdctx, hashBuffer, &hashLength) != 1) {
        EVP_MD_CTX_free(mdctx);
        setError(errorMessage, readOpenSslError());
        return VerificationResult::CalculateHashError;
    }

    EVP_MD_CTX_free(mdctx);
    hash = QByteArray(reinterpret_cast<char *>(hashBuffer), hashLength);
    return VerificationResult::Success;
}

bool SignatureVerifier::getSignatureData(const QString &filePath, QByteArray &data, QString *errorMessage) const
{
    QFile signatureFile(filePath);
    if (!signatureFile.open(QIODevice::ReadOnly)) {
        setError(errorMessage, QStringLiteral("Failed to open signature file: %1").arg(signatureFile.errorString()));
        return false;
    }
    data = signatureFile.readAll();
    return true;
}

SignatureVerifier::VerificationResult SignatureVerifier::getFileData(const QString &filePath, QByteArray &data, bool calculateHashFromFile, QString *errorMessage) const
{
    if (calculateHashFromFile) {
        SignatureVerifier::VerificationResult result = hardSha256(filePath, data, errorMessage);
        if (result != SignatureVerifier::VerificationResult::Success) {
            return result;
        }
    } else {
        QFile dataFile(filePath);
        if (!dataFile.open(QIODevice::ReadOnly)) {
            setError(errorMessage, QStringLiteral("Failed to open data file: %1").arg(dataFile.errorString()));
            return SignatureVerifier::VerificationResult::DataFileError;
        }
        data = dataFile.readAll();
    }
    return SignatureVerifier::VerificationResult::Success;
}

SignatureVerifier::VerificationResult SignatureVerifier::verify(const QString &filePath,
                                         const QString &signaturePath,
                                         const QByteArray &publicKeyPem,
                                         bool calculateHashFromFile,
                                         QString *errorMessage)
{
    QByteArray data;
    SignatureVerifier::VerificationResult result = getFileData(filePath, data, calculateHashFromFile, errorMessage);
    if (result != SignatureVerifier::VerificationResult::Success) {
        return result;
    }

    QByteArray signature;
    bool signatureDataSuccess = getSignatureData(signaturePath, signature, errorMessage);
    if (!signatureDataSuccess) {
        return SignatureVerifier::VerificationResult::SignatureFileError;
    }

    if (verify(data, signature, publicKeyPem, errorMessage)) {
        return SignatureVerifier::VerificationResult::Success;
    } else {
        return SignatureVerifier::VerificationResult::SignatureVerificationFailed;
    }
}

SignatureVerifier::VerificationResult SignatureVerifier::verify(const QString &filePath,
                                         const QString &signaturePath,
                                         const QList<QByteArray> &publicKeyPemList,
                                         bool calculateHashFromFile,
                                         QString *errorMessage)
{
    QByteArray data;
    SignatureVerifier::VerificationResult result = getFileData(filePath, data, calculateHashFromFile, errorMessage);
    if (result != SignatureVerifier::VerificationResult::Success) {
        return result;
    }

    QByteArray signature;
    bool signatureDataSuccess = getSignatureData(signaturePath, signature, errorMessage);
    if (!signatureDataSuccess) {
        return SignatureVerifier::VerificationResult::SignatureFileError;
    }

    if (verify(data, signature, publicKeyPemList, errorMessage)) {
        return SignatureVerifier::VerificationResult::Success;
    } else {
        return SignatureVerifier::VerificationResult::SignatureVerificationFailed;
    }
}
