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

#include "kdupdatercrypto.h"

#include <openssl/ssl.h>
#include <openssl/rsa.h>

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDebug>
#include <QStringList>
#include <QFile>
#include <QMutex>
#include <QLibrary>
#include <QDir>
#include <QTemporaryFile>

//BEGIN code taken from qsslsocket_openssl_symbols.cpp

#define DUMMYARG

#define CLEARFUNC(func) \
    _kd_##func = 0;

#define RESOLVEFUNC(func) \
    if (!(_kd_##func = _kd_PTR_##func(libs->resolve(#func)))) \
        qWarning("QSslSocket: cannot resolve "#func);
    
#define DEFINEFUNC(ret, func, arg, a, err, funcret) \
    typedef ret (*_kd_PTR_##func)(arg); \
    _kd_PTR_##func _kd_##func; \
    ret kd_##func(arg) { \
        Q_ASSERT(_kd_##func); \
        funcret _kd_##func(a); \
    }
    
#define DEFINEFUNC3(ret, func, arg1, a, arg2, b, arg3, c, err, funcret) \
    typedef ret (*_kd_PTR_##func)(arg1, arg2, arg3);            \
    _kd_PTR_##func _kd_##func;                        \
    ret kd_##func(arg1, arg2, arg3) { \
        Q_ASSERT(_kd_##func); \
        funcret _kd_##func(a, b, c); \
    }

#define DEFINEFUNC4(ret, func, arg1, a, arg2, b, arg3, c, arg4, d, err, funcret) \
    typedef ret (*_kd_PTR_##func)(arg1, arg2, arg3, arg4);               \
    _kd_PTR_##func _kd_##func;                                 \
    ret kd_##func(arg1, arg2, arg3, arg4) { \
         Q_ASSERT(_kd_##func); \
         funcret _kd_##func(a, b, c, d); \
    }

#define DEFINEFUNC5(ret, func, arg1, a, arg2, b, arg3, c, arg4, d, arg5, e, err, funcret) \
    typedef ret (*_kd_PTR_##func)(arg1, arg2, arg3, arg4, arg5);         \
    _kd_PTR_##func _kd_##func;                                 \
    ret kd_##func(arg1, arg2, arg3, arg4, arg5) { \
        Q_ASSERT(_kd_##func); \
        funcret _kd_##func(a, b, c, d, e); \
    }

#define DEFINEFUNC6(ret, func, arg1, a, arg2, b, arg3, c, arg4, d, arg5, e, arg6, f, err, funcret) \
    typedef ret (*_kd_PTR_##func)(arg1, arg2, arg3, arg4, arg5, arg6);            \
    _kd_PTR_##func _kd_##func;                        \
    ret kd_##func(arg1, arg2, arg3, arg4, arg5, arg6) { \
        Q_ASSERT(_kd_##func); \
        funcret _kd_##func(a, b, c, d, e, f); \
    }

    
# ifdef Q_OS_UNIX
static bool libGreaterThan(const QString &lhs, const QString &rhs)
{
    QStringList lhsparts = lhs.split(QLatin1Char('.'));
    QStringList rhsparts = rhs.split(QLatin1Char('.'));
    Q_ASSERT(lhsparts.count() > 1 && rhsparts.count() > 1);

    for (int i = 1; i < rhsparts.count(); ++i) {
        if (lhsparts.count() <= i)
            // left hand side is shorter, so it's less than rhs
            return false;

        bool ok = false;
        int b = 0;
        int a = lhsparts.at(i).toInt(&ok);
        if (ok)
            b = rhsparts.at(i).toInt(&ok);
        if (ok) {
            // both toInt succeeded
            if (a == b)
                continue;
            return a > b;
        } else {
            // compare as strings;
            if (lhsparts.at(i) == rhsparts.at(i))
                continue;
            return lhsparts.at(i) > rhsparts.at(i);
        }
    }

    // they compared strictly equally so far
    // lhs cannot be less than rhs
    return true;
}
static QStringList findAllLibSsl()
{
    QStringList paths;
#  ifdef Q_OS_DARWIN
    paths = QString::fromLatin1(qgetenv("DYLD_LIBRARY_PATH"))
            .split(QLatin1Char(':'), QString::SkipEmptyParts);
#  else
    paths = QString::fromLatin1(qgetenv("LD_LIBRARY_PATH"))
            .split(QLatin1Char(':'), QString::SkipEmptyParts);
#  endif
    paths << QLatin1String("/usr/lib") << QLatin1String("/usr/local/lib");

    QStringList foundSsls;
    Q_FOREACH (const QString &path, paths) {
        QDir dir = QDir(path);
        QStringList entryList = dir.entryList(QStringList() << QLatin1String("libssl.*"), QDir::Files);

        qSort(entryList.begin(), entryList.end(), libGreaterThan);
        Q_FOREACH (const QString &entry, entryList)
            foundSsls << path + QLatin1Char('/') + entry;
    }

    return foundSsls;
}
#endif

class KDLibraryLoader : public QObject
{
public:
    KDLibraryLoader()
    {
        const QMutexLocker ml( &mutex );
        if( tempDir.isEmpty() )
        {
            QTemporaryFile file( QDir::temp().absoluteFilePath( QString::fromLatin1( "templibsXXXXXX" ) ) );
            file.open();
            tempDir = file.fileName();
            file.close();
            file.remove();
        }
        QDir::temp().mkdir( QFileInfo( tempDir ).fileName() );
    }

    ~KDLibraryLoader()
    {
        const QMutexLocker ml( &mutex );
        for( QVector< QLibrary* >::const_iterator it = loadedLibraries.begin(); it != loadedLibraries.end(); ++it )
        {
            QLibrary* const lib = *it;
            lib->unload();
            delete lib;
        }

        for( QStringList::const_iterator it = temporaryFiles.begin(); it != temporaryFiles.end(); ++it )
        {
            QFile file( *it );
            file.setPermissions( file.permissions() | QFile::WriteOwner | QFile::WriteUser );
            file.remove();
        }

        QDir::temp().rmdir( QFileInfo( tempDir ).fileName() );
    }

    bool load( const QString& filename )
    {
        const QMutexLocker ml( &mutex );
        // does it work out of the box? great!
        QLibrary* const lib = new QLibrary;
        loadedLibraries.push_back( lib );
        lib->setFileName( filename );
        if( lib->load() )
            return true;

        // if this failed, we copy the filename to a different place
        // it might as well come from a resource
        const QString realFilename = QFileInfo( QDir( tempDir ), QFileInfo( filename ).fileName() ).absoluteFilePath();

        QFile::copy( filename, realFilename );
        temporaryFiles.push_back( realFilename );

        lib->setFileName( realFilename );
        if( lib->load() )
            return true;
    
        // if all fails... sorry, we can't perform magic. Seriously!
        QFile::remove( realFilename );
        temporaryFiles.pop_back();

        delete lib;
        loadedLibraries.pop_back();

        return false;
    }

    template< typename VERSION >
    bool load( const QString& filename, VERSION version )
    {
        const QMutexLocker ml( mutex );
        // does it work out of the box? great!
        QLibrary* const lib = new QLibrary;
        loadedLibraries.push_back( lib );
        lib->setFileNameAndVersion( filename, version );
        if( lib->load() )
            return true;

        // if this failed, we copy the filename to a different place
        // it might as well come from a resource
        const QString realFilename = QFileInfo( QDir( tempDir ), QFileInfo( filename ).fileName() ).absoluteFilePath();

        QFile::copy( filename, realFilename );
        temporaryFiles.push_back( realFilename );

        lib->setFileNameAndVersion( realFilename, version );
        if( lib->load() )
            return true;

        // if all fails... sorry, we can't perform magic. Seriously!
        QFile::remove( realFilename );
        temporaryFiles.pop_back();

        delete lib;
        loadedLibraries.pop_back();

        return false;

    }

    void* resolve( const char* symbol )
    {
        const QMutexLocker ml( &mutex );
        for( QVector< QLibrary* >::const_iterator it = loadedLibraries.begin(); it != loadedLibraries.end(); ++it )
        {
            QLibrary* const lib = *it;
            void* const ptr = lib->resolve( symbol );
            if( ptr != 0 )
                return ptr;
        }
        return 0;
    }

private:
    QVector< QLibrary* > loadedLibraries;
    QStringList temporaryFiles;

    static QMutex mutex;
    static QString tempDir;
};

QMutex KDLibraryLoader::mutex;
QString KDLibraryLoader::tempDir;


static KDLibraryLoader* loadOpenSsl()
{
    KDLibraryLoader* result = new KDLibraryLoader;
#ifdef Q_OS_WIN
    if( result->load( QLatin1String( ":/openssllibs/libeay32.dll" ) ) && result->load( QLatin1String( ":/openssllibs/libssl32.dll" ) ) )
        return result;

    delete result;
    return 0;

# elif defined(Q_OS_UNIX)
    // Try to find the libssl library on the system.
    //
    // Up until Qt 4.3, this only searched for the "ssl" library at version -1, that
    // is, libssl.so on most Unix systems.  However, the .so file isn't present in
    // user installations because it's considered a development file.
    //
    // The right thing to do is to load the library at the major version we know how
    // to work with: the SHLIB_VERSION_NUMBER version (macro defined in opensslv.h)
    //
    // However, OpenSSL is a well-known case of binary-compatibility breakage. To
    // avoid such problems, many system integrators and Linux distributions change
    // the soname of the binary, letting the full version number be the soname. So
    // we'll find libssl.so.0.9.7, libssl.so.0.9.8, etc. in the system. For that
    // reason, we will search a few common paths (see findAllLibSsl() above) in hopes
    // we find one that works.
    //
    // It is important, however, to try the canonical name and the unversioned name
    // without going through the loop. By not specifying a path, we let the system
    // dlopen(3) function determine it for us. This will include any DT_RUNPATH or
    // DT_RPATH tags on our library header as well as other system-specific search
    // paths. See the man page for dlopen(3) on your system for more information.

#ifdef SHLIB_VERSION_NUMBER
    // first attempt: the canonical name is libssl.so.<SHLIB_VERSION_NUMBER>
    
    if( result->load( QLatin1String( "ssl" ), QLatin1String( SHLIB_VERSION_NUMBER ) ) &&
        result->load( QLatin1String( "crypto" ), QLatin1String( SHLIB_VERSION_NUMBER ) ) ) {
        // libssl.so.<SHLIB_VERSION_NUMBER> and libcrypto.so.<SHLIB_VERSION_NUMBER> found
        return result;
    } else {
        delete result;
        result = new KDLibraryLoader;
    }
#endif

    // second attempt: find the development files libssl.so and libcrypto.so
    if( result->load( QLatin1String( "ssl" ), -1 ) &&
        result->load( QLatin1String( "crypto" ), -1 ) ) {
        // libssl.so.0 and libcrypto.so.0 found
        return result;
    } else {
        delete result;
        result = new KDLibraryLoader;
    }

    // third attempt: loop on the most common library paths and find libssl
    QStringList sslList = findAllLibSsl();
    Q_FOREACH (const QString &ssl, sslList) {
        QString crypto = ssl;
        crypto.replace(QLatin1String("ssl"), QLatin1String("crypto"));
        if( result->load( ssl, -1 ) &&
            result->load( crypto ), -1 ) {
            // libssl.so.0 and libcrypto.so.0 found
            return result;
        } else {
            delete result;
            result = new KDLibraryLoader;
        }
    }

    // failed to load anything
    delete result;
    return 0;

# else
    // not implemented for this platform yet
    return 0;
# endif
}

//END code taken from qsslsocket_openssl_symbols.cpp

class KDUpdaterCrypto::Private
{
    public:
        KDUpdaterCrypto *q;
        
        QByteArray m_privateKey;
        QByteArray m_privatePassword;
        RSA *m_private_key;
        BIO *m_private_key_bio;
        
        QByteArray m_publicKey;
        RSA *m_public_key;
        BIO *m_public_key_bio;

        KDLibraryLoader* libLoader;
       
        const PasswordProvider* passwordProvider;

        DEFINEFUNC(int, SSL_library_init, void, DUMMYARG, return -1, return)
        DEFINEFUNC(int, BIO_free, BIO *a, a, return 0, return)
        DEFINEFUNC(BIO *, BIO_new, BIO_METHOD *a, a, return 0, return)
        DEFINEFUNC(BIO_METHOD *, BIO_s_mem, void, DUMMYARG, return 0, return)
        DEFINEFUNC3(int, BIO_write, BIO *a, a, const void *b, b, int c, c, return -1, return)
        DEFINEFUNC(RSA*, RSA_new, void, DUMMYARG, return 0, return)
        DEFINEFUNC(void, RSA_free, RSA *a, a, return, DUMMYARG)
        DEFINEFUNC(int, RSA_size, RSA *a, a, return 0, return)
        DEFINEFUNC5(int, RSA_public_encrypt, int flen, flen, unsigned char *from, from, unsigned char *to, to, RSA *rsa, rsa, int padding, padding, return -1, return)
        DEFINEFUNC5(int, RSA_private_decrypt, int flen, flen, unsigned char *from, from, unsigned char *to, to, RSA *rsa, rsa, int padding, padding, return -1, return)
        DEFINEFUNC6(int, RSA_sign, int type, type, const unsigned char *m, m, unsigned int m_len, m_len, unsigned char *sigret, sigret, unsigned int *siglen, siglen, RSA *rsa, rsa, return 0, return)
        DEFINEFUNC6(int, RSA_verify, int type, type, const unsigned char *m, m, unsigned int m_len, m_len, unsigned char *sigret, sigret, unsigned int siglen, siglen, RSA *rsa, rsa, return 0, return)
        DEFINEFUNC4(RSA*, PEM_read_bio_RSAPrivateKey, BIO *bp, bp, RSA **x, x, pem_password_cb *cb, cb, void *u, u, return 0, return)
        DEFINEFUNC4(RSA*, PEM_read_bio_RSA_PUBKEY, BIO *bp, bp, RSA **x, x, pem_password_cb *cb, cb, void *u, u, return 0, return)

        explicit Private( KDUpdaterCrypto* q ) 
            : q( q ), 
              m_private_key( 0 ), 
              m_private_key_bio( 0 ), 
              m_public_key( 0 ), 
              m_public_key_bio( 0 ),
              libLoader( 0 ),
              passwordProvider( 0 )
        {
            CLEARFUNC(SSL_library_init)
            CLEARFUNC(BIO_free)
            CLEARFUNC(BIO_new)
            CLEARFUNC(BIO_s_mem)
            CLEARFUNC(BIO_write)
            CLEARFUNC(RSA_new)
            CLEARFUNC(RSA_free)
            CLEARFUNC(RSA_size)
            CLEARFUNC(PEM_read_bio_RSAPrivateKey)
            CLEARFUNC(PEM_read_bio_RSA_PUBKEY)
            CLEARFUNC(RSA_public_encrypt)
            CLEARFUNC(RSA_private_decrypt)
            CLEARFUNC(RSA_sign)
            CLEARFUNC(RSA_verify)
        }
        ~Private()
        { 
            finish();
            CLEARFUNC(SSL_library_init)
            CLEARFUNC(BIO_free)
            CLEARFUNC(BIO_new)
            CLEARFUNC(BIO_s_mem)
            CLEARFUNC(BIO_write)
            CLEARFUNC(RSA_new)
            CLEARFUNC(RSA_free)
            CLEARFUNC(RSA_size)
            CLEARFUNC(PEM_read_bio_RSAPrivateKey)
            CLEARFUNC(PEM_read_bio_RSA_PUBKEY)
            CLEARFUNC(RSA_public_encrypt)
            CLEARFUNC(RSA_private_decrypt)
            CLEARFUNC(RSA_sign)
            CLEARFUNC(RSA_verify)
            delete libLoader;
        }

        bool resolveOpenSslSymbols()
        {
            volatile bool symbolsResolved = false;
            volatile bool triedToResolveSymbols = false;
            //QMutexLocker locker(QMutexPool::globalInstanceGet((void *)&kd_SSL_library_init));
            if (symbolsResolved)
                return true;
            if (triedToResolveSymbols)
                return false;
            if( libLoader != 0 )
                return true;
            triedToResolveSymbols = true;

            KDLibraryLoader* const libs = loadOpenSsl();
            if( libs == 0 )
                // failed to load them
                return false;

            RESOLVEFUNC(SSL_library_init)
            RESOLVEFUNC(BIO_free)
            RESOLVEFUNC(BIO_new)
            RESOLVEFUNC(BIO_s_mem)
            RESOLVEFUNC(BIO_write)
            RESOLVEFUNC(RSA_new)
            RESOLVEFUNC(RSA_free)
            RESOLVEFUNC(RSA_size)
            RESOLVEFUNC(PEM_read_bio_RSAPrivateKey)
            RESOLVEFUNC(PEM_read_bio_RSA_PUBKEY)
            RESOLVEFUNC(RSA_public_encrypt)
            RESOLVEFUNC(RSA_private_decrypt)
            RESOLVEFUNC(RSA_sign)
            RESOLVEFUNC(RSA_verify)

            symbolsResolved = true;
            libLoader = libs;
            return true;
        }

        void init()
        {
            resolveOpenSslSymbols();
            const int inited = kd_SSL_library_init();
            Q_UNUSED( inited );
            Q_ASSERT(inited);
            //OpenSSL_add_all_algorithms();
        }

        void finish()
        {
            finishPrivateKey();
            finishPublicKey();
        }

        void finishPrivateKey()
        {
            if( m_private_key ) {
                kd_RSA_free(m_private_key);
                m_private_key = 0;
            }
            if( m_private_key_bio ) {
                kd_BIO_free(m_private_key_bio);
                m_private_key_bio = 0;
            }
        }
        
        void finishPublicKey()
        {
            if( m_public_key ) {
                kd_RSA_free(m_public_key);
                m_public_key = 0;
            }
            if( m_public_key_bio ) {
                kd_BIO_free(m_public_key_bio);
                m_public_key_bio = 0;
            }
        }

        static int password_callback(char *buf, int bufsiz, int verify, void *userdata)
        {
            Q_UNUSED( verify );
            //qDebug()<<"KDUpdaterCrypto::password_callback verify="<<verify;
            KDUpdaterCrypto *crypto = static_cast<KDUpdaterCrypto*>(userdata);
            const QByteArray password = crypto->d->passwordProvider == 0 ? crypto->privatePassword()
                                                                         : crypto->d->passwordProvider->password();
            int len = password.length();
            if (len <= 0)
                return 0;
            if (len > bufsiz)
                len = bufsiz;
            memcpy(buf, password.constData(), len);
            return len;
        }

        bool initPrivateKey()
        {
            finishPrivateKey();

            m_private_key_bio = kd_BIO_new(kd_BIO_s_mem());
            Q_ASSERT(m_private_key_bio);

            Q_ASSERT( ! m_privateKey.isNull());
            const int priv_bio_size = kd_BIO_write(m_private_key_bio, m_privateKey.constData(), m_privateKey.length());
            Q_UNUSED( priv_bio_size );
            Q_ASSERT(priv_bio_size >= 1);

            m_private_key = kd_RSA_new();
            Q_ASSERT(m_private_key);
            if( ! kd_PEM_read_bio_RSAPrivateKey(m_private_key_bio, &m_private_key, Private::password_callback, q)) {
                qWarning() << "PEM_read_bio_RSAPrivateKey failed";
                kd_RSA_free( m_private_key );
                m_private_key = 0;
                return false;
            }
            return true;
        }

        bool initPublicKey()
        {
            finishPublicKey();

            m_public_key_bio = kd_BIO_new(kd_BIO_s_mem());
            Q_ASSERT(m_public_key_bio);

            Q_ASSERT( ! m_publicKey.isNull());
            const int pub_bio_size = kd_BIO_write(m_public_key_bio, m_publicKey.constData(), m_publicKey.length());
            Q_UNUSED( pub_bio_size );
            Q_ASSERT(pub_bio_size >= 1);

            m_public_key = kd_RSA_new();
            Q_ASSERT(m_public_key);
            if( ! kd_PEM_read_bio_RSA_PUBKEY(m_public_key_bio, &m_public_key, password_callback, q)) {
                qWarning() << "PEM_read_bio_RSAPublicKey failed";
                kd_RSA_free( m_public_key );
                m_public_key = 0;
                return false;
            }
            return true;
        }
};

KDUpdaterCrypto::KDUpdaterCrypto()
    : d(new Private(this))
{
    d->init();
}

KDUpdaterCrypto::~KDUpdaterCrypto()
{
}

QByteArray KDUpdaterCrypto::privateKey() const
{
    return d->m_privateKey;
}

void KDUpdaterCrypto::setPrivateKey(const QByteArray &key)
{
    d->m_privateKey = key;
    d->finish();
}

QByteArray KDUpdaterCrypto::privatePassword() const
{
    return d->m_privatePassword;
}

void KDUpdaterCrypto::setPrivatePassword(const QByteArray &passwd)
{
    d->m_privatePassword = passwd;
    d->finish();
}

void KDUpdaterCrypto::setPrivatePasswordProvider( const PasswordProvider* provider )
{
    d->passwordProvider = provider;
}

QByteArray KDUpdaterCrypto::publicKey() const
{
    return d->m_publicKey;
}

void KDUpdaterCrypto::setPublicKey(const QByteArray &key)
{
    d->m_publicKey = key;
    d->finish();
}
 
QByteArray KDUpdaterCrypto::encrypt(const QByteArray &plaintext)
{
    if( !d->m_public_key && !d->initPublicKey() )
        return QByteArray();

    Q_ASSERT(d->m_public_key);
    unsigned char *encrypted = new unsigned char[ d->kd_RSA_size(d->m_public_key) ];
    const int encryptedlen = d->kd_RSA_public_encrypt(plaintext.length(), (unsigned char *) plaintext.constData(), encrypted, d->m_public_key, RSA_PKCS1_PADDING);
    const QByteArray ret = encryptedlen>=0 ? QByteArray( (const char*) encrypted, encryptedlen ) : QByteArray();
    Q_ASSERT(!ret.isNull());
    delete [] encrypted;
    return ret;
}

QByteArray KDUpdaterCrypto::decrypt(const QByteArray &encryptedtext)
{
    if( !d->m_private_key && !d->initPrivateKey() )
        return QByteArray();

    Q_ASSERT(d->m_private_key);
    unsigned char *decrypted = new unsigned char[ d->kd_RSA_size(d->m_private_key) ];
    const int decryptedlen = d->kd_RSA_private_decrypt(encryptedtext.length(), (unsigned char *) encryptedtext.constData(), decrypted, d->m_private_key, RSA_PKCS1_PADDING);
    const QByteArray ret = decryptedlen>=0 ? QByteArray( (const char*) decrypted, decryptedlen ) : QByteArray();
    delete [] decrypted;
    return ret;
}

bool KDUpdaterCrypto::verify(const QByteArray &data, const QByteArray &signature)
{
    if( !d->m_public_key && !d->initPublicKey() )
        return false;

    unsigned char *sigret = (unsigned char*) signature.constData();
    unsigned int siglen = signature.length();
    const int verifyresult = d->kd_RSA_verify(NID_sha1, (const unsigned char*) data.constData(), data.length(), sigret, siglen, d->m_public_key);
    return verifyresult == 1;
}

static QByteArray hash( QIODevice* dev, QCryptographicHash::Algorithm method )
{
    QByteArray buffer;
    buffer.resize( 8192 );
    QCryptographicHash h( method );
    while( !dev->atEnd() )
    {
        const int read = dev->read( buffer.data(), buffer.length() );
        h.addData( buffer.constData(), read );
    }
    return h.result();
}

bool KDUpdaterCrypto::verify( QIODevice* dev, const QByteArray& signature )
{
    const bool wasOpen = dev->isOpen();
    if( !wasOpen && !dev->open( QIODevice::ReadOnly ) )
    {
        qWarning() << "KDUpdaterCrypto::verify: could not open the device";
        return false;
    }

    const qint64 pos = dev->pos();
    const bool result = verify( hash( dev, QCryptographicHash::Sha1 ), signature );
    dev->seek( pos );
    if( !wasOpen )
        dev->close();
    return result;

}

bool KDUpdaterCrypto::verify( const QString& dataPath, const QByteArray& signature )
{
    QFile dFile( dataPath );
    return verify( &dFile, signature );
}

bool KDUpdaterCrypto::verify( const QString& dataPath, const QString& signaturePath )
{
    QFile sFile( signaturePath );
    if( !sFile.open( QIODevice::ReadOnly ) )
    {
        qWarning() << "KDUpdaterCrypto::veryfi: could not open the signature file";
        return false;
    }

    return verify( dataPath, sFile.readAll() );
}

QByteArray KDUpdaterCrypto::sign( QIODevice* dev )
{
    const bool wasOpen = dev->isOpen();
    if( !wasOpen && !dev->open( QIODevice::ReadOnly ) )
        return QByteArray();

    const qint64 pos = dev->pos();
    const QByteArray signature = sign( hash( dev, QCryptographicHash::Sha1 ) );
    dev->seek( pos );
    if( !wasOpen )
        dev->close();
    return signature;
}

QByteArray KDUpdaterCrypto::sign( const QString& path )
{
    QFile file( path );
    return sign( &file );
}

QByteArray KDUpdaterCrypto::sign(const QByteArray &data)
{
    if( !d->m_private_key && !d->initPrivateKey() )
        return QByteArray();

    const char *msg = data.constData();
    unsigned char *sigret = new unsigned char[ d->kd_RSA_size(d->m_private_key) ];
    unsigned int siglen;
    const int signresult = d->kd_RSA_sign(NID_sha1, (const unsigned char*) msg, strlen(msg), sigret, &siglen, d->m_private_key);
    const QByteArray ret = signresult == 1 ? QByteArray( (const char*) sigret, siglen) : QByteArray();
    delete [] sigret;
    return ret;
}
