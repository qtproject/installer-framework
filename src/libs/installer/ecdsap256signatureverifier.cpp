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

    //create a new EVP_PKEY_CTX for verification
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(publicKey, nullptr);
    if (!ctx)
    {
        m_errorString = QStringLiteral("create PKEY_CTX failed: %1").arg(readOpenSslError());
        EVP_PKEY_free(publicKey);
        return false;
    }

    //init the context for verification
    if (EVP_PKEY_verify_init(ctx) != 1)
    {
        m_errorString = QStringLiteral("verify_init failed: %1").arg(readOpenSslError());
        EVP_PKEY_CTX_free(ctx);
        EVP_PKEY_free(publicKey);
        return false;
    }

    //set the signature digest type to SHA-256
    if (EVP_PKEY_CTX_set_signature_md(ctx, EVP_sha256()) != 1)
    {
        m_errorString = QStringLiteral("set_signature_md failed: %1").arg(readOpenSslError());
        EVP_PKEY_CTX_free(ctx);
        EVP_PKEY_free(publicKey);
        return false;
    }

    //perform the signature verification
    const int verifyResult = EVP_PKEY_verify(
        ctx,
        reinterpret_cast<const unsigned char*>(signature.constData()),
        (size_t)signature.size(),
        reinterpret_cast<const unsigned char*>(data.constData()),
        (size_t)data.size()
    );

    EVP_PKEY_CTX_free(ctx);
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
