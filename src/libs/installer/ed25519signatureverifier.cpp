#include "ed25519signatureverifier.h"
#include <QList>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/x509.h>

quint32 readBigEndianUint32(const unsigned char *data)
{
    return (static_cast<quint32>(data[0]) << 24)
           | (static_cast<quint32>(data[1]) << 16)
           | (static_cast<quint32>(data[2]) << 8)
           | static_cast<quint32>(data[3]);
}

EVP_PKEY *parseOpenSshEd25519PublicKey(const QByteArray &publicKeyData)
{
    const QList<QByteArray> parts = publicKeyData.trimmed().split(' ');
    if (parts.size() < 2 || parts.at(0) != "ssh-ed25519") {
        return nullptr;
    }

    const QByteArray decoded = QByteArray::fromBase64(parts.at(1));
    if (decoded.size() < 4) {
        return nullptr;
    }

    const unsigned char *cursor = reinterpret_cast<const unsigned char *>(decoded.constData());
    int remaining = decoded.size();

    if (remaining < 4) {
        return nullptr;
    }
    const quint32 typeLength = readBigEndianUint32(cursor);
    cursor += 4;
    remaining -= 4;

    if (typeLength > static_cast<quint32>(remaining)) {
        return nullptr;
    }
    const QByteArray keyType(reinterpret_cast<const char *>(cursor), static_cast<int>(typeLength));
    cursor += typeLength;
    remaining -= static_cast<int>(typeLength);

    if (keyType != "ssh-ed25519" || remaining < 4) {
        return nullptr;
    }

    const quint32 keyLength = readBigEndianUint32(cursor);
    cursor += 4;
    remaining -= 4;

    if (keyLength != 32 || keyLength > static_cast<quint32>(remaining)) {
        return nullptr;
    }

    return EVP_PKEY_new_raw_public_key(
        EVP_PKEY_ED25519,
        nullptr,
        cursor,
        static_cast<size_t>(keyLength));
}

EVP_PKEY *ED25519SignatureVerifier::loadPublicKey(const QByteArray &publicKeyData) const
{
    // Try PEM first (BEGIN PUBLIC KEY / BEGIN OPENSSH PRIVATE KEY not applicable here).
    BIO *bio = BIO_new_mem_buf(publicKeyData.constData(), publicKeyData.size());
    if (bio) {
        EVP_PKEY *publicKey = PEM_read_bio_PUBKEY(bio, nullptr, nullptr, nullptr);
        BIO_free(bio);
        if (publicKey) {
            return publicKey;
        }
        ERR_clear_error();
    }

    // Try DER SubjectPublicKeyInfo.
    const unsigned char *derPtr = reinterpret_cast<const unsigned char *>(publicKeyData.constData());
    EVP_PKEY *publicKey = d2i_PUBKEY(nullptr, &derPtr, publicKeyData.size());
    if (publicKey) {
        return publicKey;
    }
    ERR_clear_error();

    // Try OpenSSH one-line public key (ssh-ed25519 AAAA...).
    return parseOpenSshEd25519PublicKey(publicKeyData);
}

bool ED25519SignatureVerifier::verify(const QByteArray &data,
                                         const QByteArray &signature,
                                         const QByteArray &publicKeyPem,
                                         QString *errorMessage)
{
    EVP_PKEY *publicKey = loadPublicKey(publicKeyPem);
    if (!publicKey) {
        setError(errorMessage, QStringLiteral("Failed to parse public key. Supported formats: PEM, DER, OpenSSH (ssh-ed25519)"));
        return false;
    }

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) {
        EVP_PKEY_free(publicKey);
        setError(errorMessage, QStringLiteral("EVP_MD_CTX_new failed"));
        return false;
    }

    if (EVP_DigestVerifyInit(ctx, nullptr, nullptr, nullptr, publicKey) != 1) {
        setError(errorMessage, QStringLiteral("OpenSSL verify init failed: %1").arg(readOpenSslError()));
        EVP_MD_CTX_free(ctx);
        EVP_PKEY_free(publicKey);
        return false;
    }

    const int verifyResult = EVP_DigestVerify(
        ctx,
        reinterpret_cast<const unsigned char *>(signature.constData()),
        static_cast<size_t>(signature.size()),
        reinterpret_cast<const unsigned char *>(data.constData()),
        static_cast<size_t>(data.size()));

    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(publicKey);

    if (verifyResult == 1) {
        return true;
    }

    if (verifyResult == 0) {
        setError(errorMessage, QStringLiteral("Signature verification failed"));
        return false;
    }

    setError(errorMessage, QStringLiteral("OpenSSL verify failed: %1").arg(readOpenSslError()));
    return false;
}

bool ED25519SignatureVerifier::verify(const QByteArray &data,
                                         const QByteArray &signature,
                                         const QList<QByteArray> &publicKeyPemList,
                                         QString *errorMessage)
{
    for (const QByteArray &publicKeyPem : publicKeyPemList) {
        if (verify(data, signature, publicKeyPem, errorMessage)) {
            return true;
        }
    }
    return false;
}

