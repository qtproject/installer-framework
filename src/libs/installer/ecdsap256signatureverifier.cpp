#include "ecdsap256signatureverifier.h"
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/err.h>

EVP_PKEY* ECDSAP256SignatureVerifier::loadPublicKey(const QByteArray &publicKeyPem) const
{
    BIO *bio = BIO_new_mem_buf(publicKeyPem.constData(), static_cast<int>(publicKeyPem.size()));
    if (!bio) {
        return nullptr;
    }

    EVP_PKEY *publicKey = PEM_read_bio_PUBKEY(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);

    if (!publicKey) {
        return nullptr;
    }

    if (EVP_PKEY_id(publicKey) != EVP_PKEY_EC) {
        EVP_PKEY_free(publicKey);
        return nullptr;
    }

     if (EVP_PKEY_get0_EC_KEY(publicKey) == nullptr) {
        EVP_PKEY_free(publicKey);
        return nullptr;
    }

     if (EVP_PKEY_bits(publicKey) != 256) {
        EVP_PKEY_free(publicKey);
        return nullptr;
    }
    return publicKey;
}

bool ECDSAP256SignatureVerifier::verify(const QByteArray &data,
                                         const QByteArray &signature,
                                         const QByteArray &publicKeyPem)
{
    EVP_PKEY *publicKey = loadPublicKey(publicKeyPem);
    if (!publicKey) {
        m_errorString = QStringLiteral("Failed to parse public key.");
        return false;
    }

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) {
        EVP_PKEY_free(publicKey);
        m_errorString = QStringLiteral("EVP_MD_CTX_new failed");
        return false;
    }

    if (EVP_DigestVerifyInit(ctx, nullptr, nullptr, nullptr, publicKey) != 1) {
        m_errorString = QStringLiteral("OpenSSL verify init failed: %1").arg(readOpenSslError());
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
    } else {
        m_errorString = QStringLiteral("Signature verification failed: %1").arg(readOpenSslError());
        return false;
    }
}

bool ECDSAP256SignatureVerifier::verify(const QByteArray &data,
                                         const QByteArray &signature,
                                         const QList<QByteArray> &publicKeyPemList)
{
    for (const QByteArray &publicKeyPem : publicKeyPemList) {
        if (verify(data, signature, publicKeyPem)) {
            return true;
        }
    }
    m_errorString = QStringLiteral("Signature verification failed with all provided public keys");
    return false;
}
