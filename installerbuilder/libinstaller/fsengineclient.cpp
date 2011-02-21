/**************************************************************************
**
** This file is part of Qt SDK**
**
** Copyright (c) 2011 Nokia Corporation and/or its subsidiary(-ies).*
**
** Contact:  Nokia Corporation qt-info@nokia.com**
**
** No Commercial Usage
**
** This file contains pre-release code and may not be distributed.
** You may use this file in accordance with the terms and conditions
** contained in the Technology Preview License Agreement accompanying
** this package.
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception version
** 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** If you are unsure which license is appropriate for your use, please contact
** (qt-info@nokia.com).
**
**************************************************************************/
#include "fsengineclient.h"

#undef QSettings
#undef QProcess

#include "adminauthorization.h"

#include <QAbstractFileEngineHandler>
#include <QCoreApplication>
#include <QFSFileEngine>
#include <QHostAddress>
#include <QTcpSocket>
#include <QLocalSocket>
#include <QDataStream>
#include <QProcess>
#include <QMutex>
#include <QUuid>
#include <QEventLoop>
#include <QThread>
#include <QTimer>
#include <QDebug>

#include <KDToolsCore/KDMetaMethodIterator>

/*!
 This thread convinces the watchdog in the running server that the client has
 not crashed yet.
 */
class StillAliveThread : public QThread
{
    Q_OBJECT
public:
    void run()
    {
        QTimer stillAliveTimer;
        connect( &stillAliveTimer, SIGNAL( timeout() ), this, SLOT( stillAlive() ) );
        stillAliveTimer.start( 1000 );
        exec();
    }

public Q_SLOTS:
    void stillAlive()
    {
        if( !FSEngineClientHandler::instance()->isServerRunning() )
            return;

        // in case of the server not running, this will simply fail
#ifdef FSENGINE_TCP
        QTcpSocket socket;
#else
        QLocalSocket socket;
#endif
        FSEngineClientHandler::instance()->connect( &socket );
    }
};

class FSEngineClient : public QAbstractFileEngine
{
public:
    FSEngineClient();
    ~FSEngineClient();

    bool atEnd() const;
    Iterator* beginEntryList( QDir::Filters filters, const QStringList& filterNames );
    bool caseSensitive() const;
    bool close();
    bool copy( const QString& newName );
    QStringList entryList( QDir::Filters filters, const QStringList& filterNames ) const;
    QFile::FileError error() const;
    QString errorString() const;
    bool extension( Extension extension, const ExtensionOption* option = 0, ExtensionReturn* output = 0 );
    FileFlags fileFlags( FileFlags type = FileInfoAll ) const;
    QString fileName( FileName file = DefaultName ) const;
    bool flush();
    int handle() const;
    bool isRelativePath() const;
    bool isSequential() const;
    bool link( const QString& newName );
    bool mkdir( const QString& dirName, bool createParentDirectories ) const;
    bool open( QIODevice::OpenMode mode );
    QString owner( FileOwner owner ) const;
    uint ownerId( FileOwner owner ) const;
    qint64 pos() const;
    qint64 read( char* data, qint64 maxlen );
    qint64 readLine( char* data, qint64 maxlen );
    bool remove();
    bool rename( const QString& newName );
    bool rmdir( const QString& dirName, bool recurseParentDirectories ) const;
    bool seek( qint64 offset );
    void setFileName( const QString& fileName );
    bool setPermissions( uint perms );
    bool setSize( qint64 size );
    qint64 size() const;
    bool supportsExtension( Extension extension ) const;
    qint64 write( const char* data, qint64 len );

private:
    friend class FSEngineClientHandler;
#ifdef FSENGINE_TCP
    mutable QTcpSocket* socket;
#else
    mutable QLocalSocket* socket;
#endif
    mutable QDataStream stream;
};



#define RETURN( T ) {          \
    socket->flush();            \
    if( !socket->bytesAvailable() ) \
        socket->waitForReadyRead(); \
    quint32 test;\
    stream >> test;\
    T result;                  \
    stream >> result;          \
    return result;             \
}

#define RETURN_VOID {          \
    socket->flush();            \
    if( !socket->bytesAvailable() ) \
       socket->waitForReadyRead(); \
    quint32 test;\
    stream >> test;\
    return;             \
}


#define RETURN_CASTED( T ) {   \
    socket->flush();            \
    if( !socket->bytesAvailable() ) \
       socket->waitForReadyRead(); \
    quint32 test;\
    stream >> test;\
    int result;                \
    stream >> result;          \
    return static_cast< T >( result ); \
}

#define RETURN_METHOD( T, NAME ) T FSEngineClient::NAME() const { \
stream << QString::fromLatin1( "QFSFileEngine::"#NAME ); \
RETURN( T );  \
}

#define ENUM_RETURN_METHOD( T, NAME ) T FSEngineClient::NAME() const { \
stream << QString::fromLatin1( "QFSFileEngine::"#NAME ); \
RETURN_CASTED( T ) \
}

/*!
 \internal
 */
class FSEngineClientIterator : public QAbstractFileEngineIterator
{
public:
    FSEngineClientIterator( QDir::Filters filters, const QStringList& nameFilters, const QStringList& files )
        : QAbstractFileEngineIterator( filters, nameFilters ), 
          entries( files ),
          index( -1 )
    {
    }

    /*!
     \reimp
     */
    bool hasNext() const
    {
        return index < entries.size() - 1;
    }

    /*!
     \reimp
     */
    QString next()
    {
       if( !hasNext() )
           return QString();
       ++index;
       return currentFilePath();
    }

    /*!
     \reimp
     */
    QString currentFileName() const
    {
        return entries.at( index );
    }

private:
    const QStringList entries;
    int index;
};

#ifdef FSENGINE_TCP
FSEngineClient::FSEngineClient()
    : socket( new QTcpSocket )
{
    FSEngineClientHandler::instance()->connect( socket );
    stream.setDevice( socket );
    stream.setVersion( QDataStream::Qt_4_2 );
}
#else
FSEngineClient::FSEngineClient()
    : socket( new QLocalSocket )
{
    FSEngineClientHandler::instance()->connect( socket );
    stream.setDevice( socket );
    stream.setVersion( QDataStream::Qt_4_2 );
}
#endif

FSEngineClient::~FSEngineClient()
{
    if( QThread::currentThread() == socket->thread() )
    {
        socket->close();
        delete socket;
    }
    else
    {
        socket->deleteLater();
    }
}

/*!
 \reimp
 */
RETURN_METHOD( bool, atEnd )

/*!
 \reimp
 */
QAbstractFileEngine::Iterator* FSEngineClient::beginEntryList( QDir::Filters filters, const QStringList& filterNames )
{
    QStringList entries = entryList( filters, filterNames );
    entries.removeAll( QString() );
    return new FSEngineClientIterator( filters, filterNames, entries );
}

/*!
 \reimp
 */
RETURN_METHOD( bool, caseSensitive )

/*!
 \reimp
 */
bool FSEngineClient::close()
{
    stream << QString::fromLatin1( "QFSFileEngine::close" );
    RETURN( bool )
}

/*!
 \reimp
 */
bool FSEngineClient::copy( const QString& newName )
{
    stream << QString::fromLatin1( "QFSFileEngine::copy" );
    stream << newName;
    RETURN( bool )
}

/*!
 \reimp
 */
QStringList FSEngineClient::entryList( QDir::Filters filters, const QStringList& filterNames ) const
{
    stream << QString::fromLatin1( "QFSFileEngine::entryList" );
    stream << static_cast< int >( filters );
    stream << filterNames;
    RETURN( QStringList )
}

/*!
 \reimp
 */
ENUM_RETURN_METHOD( QFile::FileError, error )

/*!
 \reimp
 */
RETURN_METHOD( QString, errorString )

/*!
 \reimp
 */
bool FSEngineClient::extension( Extension extension, const ExtensionOption* option, ExtensionReturn* output )
{
    Q_UNUSED( extension )
    Q_UNUSED( option )
    Q_UNUSED( output )
    return false;
}

/*!
 \reimp
 */
QAbstractFileEngine::FileFlags FSEngineClient::fileFlags( FileFlags type ) const
{
    stream << QString::fromLatin1( "QFSFileEngine::fileFlags" );
    stream << static_cast< int >( type );
    RETURN_CASTED( FileFlags )
}

/*!
 \reimp
 */
QString FSEngineClient::fileName( FileName file ) const
{
    stream << QString::fromLatin1( "QFSFileEngine::fileName" );
    stream << static_cast< int >( file );
    RETURN( QString )
}

/*!
 \reimp
 */
bool FSEngineClient::flush()
{
    stream << QString::fromLatin1( "QFSFileEngine::flush" );
    RETURN( bool )
}

/*!
 \reimp
 */
int FSEngineClient::handle() const
{
    stream << QString::fromLatin1( "QFSFileEngine::handle" );
    RETURN( int )
}

/*!
 \reimp
 */
RETURN_METHOD( bool, isRelativePath )

/*!
 \reimp
 */
RETURN_METHOD( bool, isSequential )

/*!
 \reimp
 */
bool FSEngineClient::link( const QString& newName )
{
    stream << QString::fromLatin1( "QFSFileEngine::link" );
    stream << newName;
    RETURN( bool )
}

/*!
 \reimp
 */
bool FSEngineClient::mkdir( const QString& dirName, bool createParentDirectories ) const
{
    stream << QString::fromLatin1( "QFSFileEngine::mkdir" );
    stream << dirName;
    stream << createParentDirectories;
    RETURN( bool )
}

/*!
 \reimp
 */
bool FSEngineClient::open( QIODevice::OpenMode mode )
{
    stream << QString::fromLatin1( "QFSFileEngine::open" );
    stream << static_cast< int >( mode );
    RETURN( bool )
}

/*!
 \reimp
 */
QString FSEngineClient::owner( FileOwner owner ) const
{
    stream << QString::fromLatin1( "QFSFileEngine::owner" );
    stream << static_cast< int >( owner );
    RETURN( QString )
}

/*!
 \reimp
 */
uint FSEngineClient::ownerId( FileOwner owner ) const
{
    stream << QString::fromLatin1( "QFSFileEngine::ownerId" );
    stream << static_cast< int >( owner );
    RETURN( uint )
}

/*!
 \reimp
 */
RETURN_METHOD( qint64, pos )

/*!
 \reimp
 */
qint64 FSEngineClient::read( char* data, qint64 maxlen )
{
    stream << QString::fromLatin1( "QFSFileEngine::read" );
    stream << maxlen;
    socket->flush();            
    if( !socket->bytesAvailable() )
        socket->waitForReadyRead();
    quint32 size;
    stream >> size;
    qint64 result;
    stream >> result;
    qint64 read = 0;
    while( read < result )
    {
        if( !socket->bytesAvailable() )
            socket->waitForReadyRead();
        read += socket->read( data + read, result - read );
    }
    return result;
}

/*!
 \reimp
 */
qint64 FSEngineClient::readLine( char* data, qint64 maxlen )
{
    stream << QString::fromLatin1( "QFSFileEngine::readLine" );
    stream << maxlen;
    socket->flush();            
    if( !socket->bytesAvailable() )
        socket->waitForReadyRead();
    quint32 size;
    stream >> size;
    qint64 result;
    stream >> result;
    qint64 read = 0;
    while( read < result )
    {
        if( !socket->bytesAvailable() )
            socket->waitForReadyRead();
        read += socket->read( data + read, result - read );
    }
    return result;
}

/*!
 \reimp
 */
bool FSEngineClient::remove() 
{
    stream << QString::fromLatin1( "QFSFileEngine::remove" );
    RETURN( bool )
}

/*!
 \reimp
 */
bool FSEngineClient::rename( const QString& newName )
{
    stream << QString::fromLatin1( "QFSFileEngine::rename" );
    stream << newName;
    RETURN( bool )
}

/*!
 \reimp
 */
bool FSEngineClient::rmdir( const QString& dirName, bool recurseParentDirectories ) const
{
    stream << QString::fromLatin1( "QFSFileEngine::rmdir" );
    stream << dirName;
    stream << recurseParentDirectories;
    RETURN( bool )
}

/*!
 \reimp
 */
bool FSEngineClient::seek( qint64 offset )
{
    stream << QString::fromLatin1( "QFSFileEngine::seek" );
    stream << offset;
    RETURN( bool )
}

/*!
 \reimp
 */
void FSEngineClient::setFileName( const QString& fileName )
{
    stream << QString::fromLatin1( "QFSFileEngine::setFileName" );
    stream << fileName;
    RETURN_VOID
} 

/*!
 \reimp
 */
bool FSEngineClient::setPermissions( uint perms )
{
    stream << QString::fromLatin1( "QFSFileEngine::setPermissions" );
    stream << perms;
    RETURN( bool )
}

/*!
 \reimp
 */
bool FSEngineClient::setSize( qint64 size )
{
    stream << QString::fromLatin1( "QFSFileEngine::setSize" );
    stream << size;
    RETURN( bool )
}

/*!
 \reimp
 */
RETURN_METHOD( qint64, size )

/*!
 \reimp
 */
bool FSEngineClient::supportsExtension( Extension extension ) const
{
    stream << QString::fromLatin1( "QFSFileEngine::supportsExtension" );
    stream << static_cast< int >( extension );
    RETURN( bool )
}

/*!
 \reimp
 */
qint64 FSEngineClient::write( const char* data, qint64 len )
{
    stream << QString::fromLatin1( "QFSFileEngine::write" );
    stream << len;
    qint64 written = 0;
    while( written < len )
    {
        written += socket->write( data, len - written );
        socket->waitForBytesWritten();
    }
    RETURN( qint64 )
}

class FSEngineClientHandler::Private
{
public:
    Private()
        : mutex( QMutex::Recursive ),
          port( 0 ),
          startServerAsAdmin( false ),
          serverStarted( false ),
          serverStarting( false ),
          active( false ),
          thread( new StillAliveThread )
    {
        thread->moveToThread( thread );
    }
    
    void maybeStartServer();
    void maybeStopServer();

    static FSEngineClientHandler* instance;

    QMutex mutex;
    QHostAddress address;
    quint16 port;
    QString socket;
    bool startServerAsAdmin;
    bool serverStarted;
    bool serverStarting;
    bool active;
    QString serverCommand;
    QStringList serverArguments;
    QString key;

    StillAliveThread* const thread;
};

FSEngineClientHandler* FSEngineClientHandler::Private::instance = 0;

/*!
 Creates a new FSEngineClientHandler with no connection.
 */
FSEngineClientHandler::FSEngineClientHandler()
    : d( new Private )
{
    //don't do this in the Private ctor as createUuid() accesses QFileEngine, which accesses this half-constructed handler -> Crash
    //KDNDK-248
    d->key = QUuid::createUuid().toString();
    Private::instance = this;
}

#ifdef FSENGINE_TCP
/*!
 Creates a new FSEngineClientHandler connection to \a port on address \a.
 */
FSEngineClientHandler::FSEngineClientHandler( quint16 port, const QHostAddress& a )
    : d( new Private )
{
    d->address = a;
    d->port = port;
    d->key = QUuid::createUuid().toString(); //moved from Private() ctor, see above
    Private::instance = this;
}

void FSEngineClientHandler::init( quint16 port, const QHostAddress& a )
{
    d->address = a;
    d->port = port;
    d->thread->start();
}
#else
/*!
 Creates a new FSEngineClientHandler connecting to \a socket.
 */
FSEngineClientHandler::FSEngineClientHandler( const QString& socket )
    : d( new Private )
{
    d->socket = socket;
    Private::instance = this;
}

void FSEngineClientHandler::init( const QString& socket )
{
    d->socket = socket;
    d->thread->start();
}
#endif

#ifdef FSENGINE_TCP
bool FSEngineClientHandler::connect( QTcpSocket* socket )
{
    int tries = 3;
    while ( tries > 0 ) 
    {
        socket->connectToHost( d->address, d->port );
#else
bool FSEngineClientHandler::connect( QLocalSocket* socket )
{
    int tries = 5;
    while ( tries > 0 ) 
    {
        socket->connectToServer( d->socket );
#endif

        if( !socket->waitForConnected( 10000 ) )
        {
            if( static_cast< QAbstractSocket::SocketError >( socket->error() ) != QAbstractSocket::UnknownSocketError )
                --tries;
            qApp->processEvents();
            continue;
        }

        QDataStream stream( socket );
        stream << QString::fromLatin1( "authorize" );
        stream << d->key;
        socket->flush();
        return true;
    }
    return false;
}

/*!
 Destroys the FSEngineClientHandler. If the handler started a server instance, it gets shut down.
 */
FSEngineClientHandler::~FSEngineClientHandler()
{
    QMetaObject::invokeMethod( d->thread, "quit" );
    //d->maybeStopServer();
    delete d;
    Private::instance = 0;
}

/*!
 Returns a previously created FSEngineClientHandler instance.
 */
FSEngineClientHandler* FSEngineClientHandler::instance()
{
    if (!Private::instance) {
        Private::instance = new FSEngineClientHandler();
    }
    return Private::instance;
}

/*!
 Returns a created authorization key which is sent to the server when connecting via the "authorize" command
 after the server was started.
 */
QString FSEngineClientHandler::authorizationKey() const
{
    return d->key;
}

/*!
 Sets \a command as the command to be executed to startup the server. If \a startAsAdmin is set,
 it is executed with admin privilegies.
 */
void FSEngineClientHandler::setStartServerCommand( const QString& command, bool startAsAdmin )
{
    setStartServerCommand( command, QStringList(), startAsAdmin );
}

/*!
 Sets \a command as the command to be executed to startup the server. If \a startAsAdmin is set,
 it is executed with admin privilegies. A list of \a arguments is passed to the process.
 */
void FSEngineClientHandler::setStartServerCommand( const QString& command, const QStringList& arguments, bool startAsAdmin )
{
    d->maybeStopServer();

    d->startServerAsAdmin = startAsAdmin;
    d->serverCommand = command;
    d->serverArguments = arguments;
}

/*!
 \reimp
 */
QAbstractFileEngine* FSEngineClientHandler::create( const QString& fileName ) const
{
    if( d->serverStarting || !d->active )
        return 0;

    d->maybeStartServer();

    static QRegExp re( QLatin1String( "^[a-z0-9]*://.*$" ) );
    if( re.exactMatch( fileName ) )  // stuff like installer:// 7z:// and so on
        return 0;
    if( fileName.isEmpty() || fileName.startsWith( QLatin1String( ":" ) ) )
        return 0; // empty filename or Qt resource

    FSEngineClient* const client = new FSEngineClient;
    // authorize
    client->stream << QString::fromLatin1( "authorize" );
    client->stream << d->key;
    client->socket->flush();

    client->setFileName( fileName );
    return client;
}

/*!
 Sets the FSEngineClientHandler to \a active. I.e. to actually return FSEngineClients if asked for.
 */
void FSEngineClientHandler::setActive( bool active )
{
    d->active = active;
    if( active )
    {
        d->maybeStartServer();
        d->active = d->serverStarted;
    }
}

/*!
 Returns, wheter this FSEngineClientHandler is active or not.
 */
bool FSEngineClientHandler::isActive() const
{
    return d->active;
}

/*!
 Returns true, when the server already has been started.
 */
bool FSEngineClientHandler::isServerRunning() const
{
    return d->serverStarted;
}

/*!
 Starts the server if a command was set and it isn't already started.
 \internal
*/
void FSEngineClientHandler::Private::maybeStartServer()
{
    if( serverStarted || serverCommand.isEmpty() )
        return;

    const QMutexLocker ml( &mutex );
    if( serverStarted )
        return;

    serverStarting = true;

    if( startServerAsAdmin )
    {
        AdminAuthorization auth;
        serverStarted = auth.authorize() && auth.execute( 0, serverCommand, serverArguments );
    }
    else
    {
        serverStarted = QProcess::startDetached( serverCommand, serverArguments );
    }

    // now wait for the socket to arrive
#ifdef FSENGINE_TCP
    QTcpSocket s;
    while( serverStarting && serverStarted )
    {
        if( instance->connect( &s ) )
            serverStarting = false;
    }

#else
    QLocalSocket s;
    while( serverStarting && serverStarted )
    {
        if( instance->connect( &s ) )
            serverStarting = false;
    }
#endif

    serverStarting = false;
}

/*!
 Stops the server if it was started before.
 */
void FSEngineClientHandler::Private::maybeStopServer()
{
    if( !serverStarted )
        return;

    const QMutexLocker ml( &mutex );
    if( !serverStarted )
        return;

#ifdef FSENGINE_TCP
    QTcpSocket s;
#else
    QLocalSocket s;
#endif
    if( instance->connect( &s ) )
    {
        QDataStream stream( &s );
        stream.setVersion( QDataStream::Qt_4_2 );
        stream << QString::fromLatin1( "authorize" );
        stream << key;
        stream << QString::fromLatin1( "shutdown" );
        s.flush();
    }
    
    serverStarted = false;
}

class QSettingsWrapper::Private
{
public:
    Private( const QString& organization, const QString& application )
        : native( true ),  
          settings( organization, application ),
          socket( 0 )
    {
    }
    Private( QSettings::Scope scope, const QString& organization, const QString& application )
        : native( true ),
          settings( scope, organization, application ),
          socket( 0 )
    {
    }
    Private( QSettings::Format format, QSettings::Scope scope, const QString& organization, const QString& application )
        : native( format == QSettings::NativeFormat ),
          settings( format, scope, organization, application ),
          socket( 0 )
    {
    }
    Private( const QString& fileName, QSettings::Format format )
        : native( format == QSettings::NativeFormat ),
          fileName( fileName ),
          settings( fileName, format ),
          socket( 0 )
    {
    }
    Private()
        : native( true ),
          socket( 0 )
    {
    }

    bool createSocket()
    {
        if( !native || !FSEngineClientHandler::instance()->isActive() )
            return false;
        if( socket != 0 && socket->state() == static_cast< int >( QLocalSocket::ConnectedState ) )
            return true;
        if( socket != 0 )
            delete socket;
#ifdef FSENGINE_TCP
        socket = new QTcpSocket;
#else
        socket = new QLocalSocket;
#endif
        if( !FSEngineClientHandler::instance()->connect( socket ) )
            return false;
        stream.setDevice( socket );
        stream.setVersion( QDataStream::Qt_4_2 );

        stream << QString::fromLatin1( "createQSettings" );
        stream << this->fileName;
        socket->flush();
        stream.device()->waitForReadyRead( -1 );
        quint32 test;
        stream >> test;
        stream.device()->readAll();
        return true;
    }

    const bool native;
    const QString fileName;
    QSettings settings;
#ifdef FSENGINE_TCP
    mutable QTcpSocket* socket;
#else
    mutable QLocalSocket* socket;
#endif
    mutable QDataStream stream;
};

QSettingsWrapper::QSettingsWrapper( const QString& organization, const QString& application, QObject* parent )
    : QObject( parent ), 
     d( new Private( organization, application ) )
{
}

QSettingsWrapper::QSettingsWrapper( QSettingsWrapper::Scope scope, const QString& organization, const QString& application, QObject* parent )
    : QObject( parent ), 
     d( new Private( static_cast< QSettings::Scope >( scope ), organization, application ) )
{
}

QSettingsWrapper::QSettingsWrapper( QSettingsWrapper::Format format, QSettingsWrapper::Scope scope, const QString& organization, const QString& application, QObject* parent )
    : QObject( parent ), 
     d( new Private( static_cast< QSettings::Format >( format ), static_cast< QSettings::Scope >( scope ), organization, application ) )
{
}

QSettingsWrapper::QSettingsWrapper( const QString& fileName, QSettingsWrapper::Format format, QObject* parent )
    : QObject( parent ), 
     d( new Private( fileName, static_cast< QSettings::Format >( format ) ) )
{
}

QSettingsWrapper::QSettingsWrapper( QObject* parent )
    : QObject( parent ), 
     d( new Private )
{
}

QSettingsWrapper::~QSettingsWrapper()
{
    if( d->socket != 0 )
    {
        d->stream << QString::fromLatin1( "destroyQSettings" );
        d->socket->flush();
        quint32 result;
        d->stream >> result;

        if( QThread::currentThread() == d->socket->thread() )
        {
            d->socket->close();
            delete d->socket;
        }
        else
        {
            d->socket->deleteLater();
        }
    }
    delete d;
}

void callRemoteVoidMethod( QDataStream& stream, const QString& name )
{
    stream.device()->readAll();
    stream << name;
    stream.device()->waitForBytesWritten( -1 );
    if( !stream.device()->bytesAvailable() )
        stream.device()->waitForReadyRead( -1 );
    quint32 test;
    stream >> test;
    stream.device()->readAll();
    return;
}

template< typename T >
void callRemoteVoidMethod( QDataStream& stream, const QString& name, const T& param1 )
{
    stream.device()->readAll();
    stream << name;
    stream << param1;
    stream.device()->waitForBytesWritten( -1 );
    if( !stream.device()->bytesAvailable() )
        stream.device()->waitForReadyRead( -1 );
    quint32 test;
    stream >> test;
    stream.device()->readAll();
    return;
}

template< typename T1, typename T2 >
void callRemoteVoidMethod( QDataStream& stream, const QString& name, const T1& param1, const T2& param2 )
{
    stream.device()->readAll();
    stream << name;
    stream << param1;
    stream << param2;
    stream.device()->waitForBytesWritten( -1 );
    if( !stream.device()->bytesAvailable() )
        stream.device()->waitForReadyRead( -1 );
    quint32 test;
    stream >> test;
    stream.device()->readAll();
    return;
}

template< typename T1, typename T2, typename T3 >
void callRemoteVoidMethod( QDataStream& stream, const QString& name, const T1& param1, const T2& param2, const T3& param3 )
{
    stream.device()->readAll();
    stream << name;
    stream << param1;
    stream << param2;
    stream << param3;
    stream.device()->waitForBytesWritten( -1 );
    if( !stream.device()->bytesAvailable() )
        stream.device()->waitForReadyRead( -1 );
    quint32 test;
    stream >> test;
    stream.device()->readAll();
    return;
}

template< typename RESULT >
RESULT callRemoteMethod( QDataStream& stream, const QString& name )
{
    stream.device()->readAll();
    stream << name;
    stream.device()->waitForBytesWritten( -1 );
    if( !stream.device()->bytesAvailable() )
        stream.device()->waitForReadyRead( -1 );
    quint32 test;
    stream >> test;
    RESULT result;
    stream >> result;
    stream.device()->readAll();
    return result;
}

template< typename RESULT, typename T >
RESULT callRemoteMethod( QDataStream& stream, const QString& name, const T& param1 )
{
    stream.device()->readAll();
    stream << name;
    stream << param1;
    stream.device()->waitForBytesWritten( -1 );
    if( !stream.device()->bytesAvailable() )
        stream.device()->waitForReadyRead( -1 );
    quint32 test;
    stream >> test;
    RESULT result;
    stream >> result;
    stream.device()->readAll();
    return result;
}

template< typename RESULT, typename T1, typename T2 >
RESULT callRemoteMethod( QDataStream& stream, const QString& name, const T1& param1, const T2& param2 )
{
    stream.device()->readAll();
    stream << name;
    stream << param1;
    stream << param2;
    stream.device()->waitForBytesWritten( -1 );
    if( !stream.device()->bytesAvailable() )
        stream.device()->waitForReadyRead( -1 );
    quint32 test;
    stream >> test;
    RESULT result;
    stream >> result;
    stream.device()->readAll();
    return result;
}

template< typename RESULT, typename T1, typename T2, typename T3 >
RESULT callRemoteMethod( QDataStream& stream, const QString& name, const T1& param1, const T2& param2, const T3& param3 )
{
    stream.device()->readAll();
    stream << name;
    stream << param1;
    stream << param2;
    stream << param3;
    stream.device()->waitForBytesWritten( -1 );
    if( !stream.device()->bytesAvailable() )
        stream.device()->waitForReadyRead( -1 );
    quint32 test;
    stream >> test;
    RESULT result;
    stream >> result;
    stream.device()->readAll();
    return result;
}

static QDataStream& operator>>( QDataStream& stream, QSettingsWrapper::Status& status )
{
    int s;
    stream >> s;
    status = static_cast< QSettingsWrapper::Status >( s );
    return stream;
}

#define RETURN_NO_ARGS_CONST( RESULT, NAME ) \
RESULT QSettingsWrapper::NAME() const       \
{ \
    if( d->createSocket() ) \
        return callRemoteMethod< RESULT >( d->stream, QLatin1String( "QSettings::"#NAME ) ); \
    else \
        return static_cast< RESULT >( d->settings.NAME() ); \
}

#define RETURN_ONE_ARG( RESULT, NAME, TYPE1 ) \
RESULT QSettingsWrapper::NAME( TYPE1 param1 ) \
{ \
    if( d->createSocket() ) \
        return callRemoteMethod< RESULT >( d->stream, QLatin1String( "QSettings::"#NAME ), param1 ); \
    else \
        return d->settings.NAME( param1 ); \
}

#define RETURN_ONE_ARG_CONST( RESULT, NAME, TYPE1 ) \
RESULT QSettingsWrapper::NAME( TYPE1 param1 ) const \
{ \
    if( d->createSocket() ) \
        return callRemoteMethod< RESULT >( d->stream, QLatin1String( "QSettings::"#NAME ), param1 ); \
    else \
        return d->settings.NAME( param1 ); \
}

#define RETURN_TWO_ARGS_CONST( RESULT, NAME, TYPE1, TYPE2 ) \
RESULT QSettingsWrapper::NAME( TYPE1 param1, TYPE2 param2 ) const \
{ \
    if( d->createSocket() ) \
        return callRemoteMethod< RESULT >( d->stream, QLatin1String( "QSettings::"#NAME ), param1, param2 ); \
    else \
        return d->settings.NAME( param1, param2 ); \
}

#define VOID_NO_ARGS( NAME ) \
void QSettingsWrapper::NAME() \
{ \
    if( d->createSocket() ) \
        callRemoteVoidMethod( d->stream, QLatin1String( "QSettings::"#NAME ) ); \
    else \
        d->settings.NAME(); \
}

#define VOID_ONE_ARG( NAME, TYPE1 ) \
void QSettingsWrapper::NAME( TYPE1 param1 ) \
{ \
    if( d->createSocket() ) \
        callRemoteVoidMethod( d->stream, QLatin1String( "QSettings::"#NAME ), param1 ); \
    else \
        d->settings.NAME( param1 ); \
}

#define VOID_TWO_ARGS( NAME, TYPE1, TYPE2 ) \
void QSettingsWrapper::NAME( TYPE1 param1, TYPE2 param2 ) \
{ \
    if( d->createSocket() ) \
        callRemoteVoidMethod( d->stream, QLatin1String( "QSettings::"#NAME ), param1, param2 ); \
    else \
        d->settings.NAME( param1, param2 ); \
}

RETURN_NO_ARGS_CONST( QStringList, allKeys )
RETURN_NO_ARGS_CONST( QString, applicationName )
VOID_ONE_ARG( beginGroup, const QString& )
RETURN_ONE_ARG( int, beginReadArray, const QString& )
VOID_TWO_ARGS( beginWriteArray, const QString&, int )
RETURN_NO_ARGS_CONST( QStringList, childGroups )
RETURN_NO_ARGS_CONST( QStringList, childKeys )
VOID_NO_ARGS( clear )
RETURN_ONE_ARG_CONST( bool, contains, const QString& )
VOID_NO_ARGS( endArray )
VOID_NO_ARGS( endGroup )
RETURN_NO_ARGS_CONST( bool, fallbacksEnabled )
RETURN_NO_ARGS_CONST( QString, fileName )

QSettingsWrapper::Format QSettingsWrapper::format() const
{
    return static_cast< QSettingsWrapper::Format >( d->settings.format() );
}

RETURN_NO_ARGS_CONST( QString, group )

QTextCodec* QSettingsWrapper::iniCodec() const
{
    return d->settings.iniCodec();
}

RETURN_NO_ARGS_CONST( bool, isWritable )
RETURN_NO_ARGS_CONST( QString, organizationName )
VOID_ONE_ARG( remove, const QString& )

QSettingsWrapper::Scope QSettingsWrapper::scope() const
{
    return static_cast< QSettingsWrapper::Scope >( d->settings.scope() );
}

VOID_ONE_ARG( setArrayIndex, int )
VOID_ONE_ARG( setFallbacksEnabled, bool )

void QSettingsWrapper::setIniCodec( QTextCodec* codec )
{
    d->settings.setIniCodec( codec );
}

void QSettingsWrapper::setIniCodec( const char* codecName )
{
    d->settings.setIniCodec( codecName );
}

VOID_TWO_ARGS( setValue, const QString&, const QVariant& )
RETURN_NO_ARGS_CONST( QSettingsWrapper::Status, status );
VOID_NO_ARGS( sync )
RETURN_TWO_ARGS_CONST( QVariant, value, const QString&, const QVariant& )

class QProcessWrapper::Private
{
public:
    Private( QProcessWrapper* qq )
        : q( qq ),
          ignoreTimer( false ),
          socket( 0 )
    {
    }

    bool createSocket()
    {
        if( !FSEngineClientHandler::instance()->isActive() )
            return false;
        if( socket != 0 && socket->state() == static_cast< int >( QLocalSocket::ConnectedState ) )
            return true;
        if( socket != 0 )
            delete socket;
#ifdef FSENGINE_TCP
        socket = new QTcpSocket;
#else
        socket = new QLocalSocket;
#endif
        if( !FSEngineClientHandler::instance()->connect( socket ) )
            return false;
        stream.setDevice( socket );
        stream.setVersion( QDataStream::Qt_4_2 );

        stream << QString::fromLatin1( "createQProcess" );
        socket->flush();
        stream.device()->waitForReadyRead( -1 );
        quint32 test;
        stream >> test;
        stream.device()->readAll();

        q->startTimer( 250 );

        return true;
    }

    class TimerBlocker
    {
    public:
        explicit TimerBlocker( const QProcessWrapper* wrapper )
            : w( const_cast< QProcessWrapper* >( wrapper ) )
        {
            w->d->ignoreTimer = true;
        }
        ~TimerBlocker()
        {
            w->d->ignoreTimer = false;
        }

    private:
        QProcessWrapper* const w;
    };

private:
    QProcessWrapper* const q;

public:
    bool ignoreTimer;

    QProcess process;
#ifdef FSENGINE_TCP
    mutable QTcpSocket* socket;
#else
    mutable QLocalSocket* socket;
#endif
    mutable QDataStream stream;
};

QProcessWrapper::QProcessWrapper( QObject* parent )
    : QObject( parent ),
      d( new Private( this ) )
{
    KDMetaMethodIterator it( QProcess::staticMetaObject, KDMetaMethodIterator::Signal, KDMetaMethodIterator::IgnoreQObjectMethods );
    while( it.hasNext() )
    {
        it.next();
        connect( &d->process, it.connectableSignature(), this, it.connectableSignature() );
    }
}

QProcessWrapper::~QProcessWrapper()
{
    if( d->socket != 0 )
    {
        d->stream << QString::fromLatin1( "destroyQProcess" );
        d->socket->flush();
        quint32 result;
        d->stream >> result;

        if( QThread::currentThread() == d->socket->thread() )
        {
            d->socket->close();
            delete d->socket;
        }
        else
        {
            d->socket->deleteLater();
        }
    }
    delete d;
}

void QProcessWrapper::timerEvent( QTimerEvent* event )
{
    Q_UNUSED( event )

    if( d->ignoreTimer )
        return;

    QList< QVariant > receivedSignals;
    
    {
        const Private::TimerBlocker blocker( this );

        d->stream << QString::fromLatin1( "getQProcessSignals" );
        d->socket->flush();
        d->stream.device()->waitForReadyRead( -1 );
        quint32 test;
        d->stream >> test;
        d->stream >> receivedSignals;
        d->stream.device()->readAll();
    }

    while( !receivedSignals.isEmpty() )
    {
        const QString name = receivedSignals.takeFirst().toString();
        if( name == QLatin1String( "started" ) )
        {
            emit started();
        }
        else if( name == QLatin1String( "readyRead" ) )
        {
            emit readyRead();
        }
         else if( name == QLatin1String( "stateChanged" ) )
        {
            const QProcess::ProcessState newState = static_cast< QProcess::ProcessState >( receivedSignals.takeFirst().toInt() );
            emit stateChanged( newState );
        }
        else if( name == QLatin1String( "finished" ) )
        {
            const int exitCode = receivedSignals.takeFirst().toInt();
            const QProcess::ExitStatus exitStatus = static_cast< QProcess::ExitStatus >( receivedSignals.takeFirst().toInt() );
            emit finished( exitCode );
            emit finished( exitCode, exitStatus );
        }
    }
}

static QDataStream& operator>>( QDataStream& stream, QProcessWrapper::ProcessState& state )
{
    int s;
    stream >> s;
    state = static_cast< QProcessWrapper::ProcessState >( s );
    return stream;
}

static QDataStream& operator>>( QDataStream& stream, QProcessWrapper::ExitStatus& status )
{
    int s;
    stream >> s;
    status = static_cast< QProcessWrapper::ExitStatus >( s );
    return stream;
}

static QDataStream& operator>>( QDataStream& stream, QProcessWrapper::ProcessChannelMode& status )
{
    int s;
    stream >> s;
    status = static_cast< QProcessWrapper::ProcessChannelMode >( s );
    return stream;
}

static QDataStream& operator>>( QDataStream& stream, QProcessWrapper::ProcessChannel& status )
{
    int s;
    stream >> s;
    status = static_cast< QProcessWrapper::ProcessChannel >( s );
    return stream;
}

#undef RETURN_NO_ARGS_CONST
#define RETURN_NO_ARGS_CONST( RESULT, NAME ) \
RESULT QProcessWrapper::NAME() const       \
{ \
    const Private::TimerBlocker blocker( this );\
    if( d->createSocket() ) \
        return callRemoteMethod< RESULT >( d->stream, QLatin1String( "QProcess::"#NAME ) ); \
    else \
        return static_cast< RESULT >( d->process.NAME() ); \
}

#define RETURN_NO_ARGS( RESULT, NAME ) \
RESULT QProcessWrapper::NAME()       \
{ \
    const Private::TimerBlocker blocker( this );\
    if( d->createSocket() ) \
        return callRemoteMethod< RESULT >( d->stream, QLatin1String( "QProcess::"#NAME ) ); \
    else \
        return d->process.NAME(); \
}

#undef RETURN_ONE_ARG
#define RETURN_ONE_ARG( RESULT, NAME, TYPE1 ) \
RESULT QProcessWrapper::NAME( TYPE1 param1 ) \
{ \
    const Private::TimerBlocker blocker( this );\
    if( d->createSocket() ) \
        return callRemoteMethod< RESULT >( d->stream, QLatin1String( "QProcess::"#NAME ), param1 ); \
    else \
        return d->process.NAME( param1 ); \
}

#undef RETURN_ONE_ARG_CONST
#define RETURN_ONE_ARG_CONST( RESULT, NAME, TYPE1 ) \
RESULT QProcessWrapper::NAME( TYPE1 param1 ) const \
{ \
    const Private::TimerBlocker blocker( this );\
    if( d->createSocket() ) \
        return callRemoteMethod< RESULT >( d->stream, QLatin1String( "QProcess::"#NAME ), param1 ); \
    else \
        return d->process.NAME( param1 ); \
}

#undef RETURN_TWO_ARGS_CONST
#define RETURN_TWO_ARGS_CONST( RESULT, NAME, TYPE1, TYPE2 ) \
RESULT QProcessWrapper::NAME( TYPE1 param1, TYPE2 param2 ) const \
{ \
    const Private::TimerBlocker blocker( this );\
    if( d->createSocket() ) \
        return callRemoteMethod< RESULT >( d->stream, QLatin1String( "QProcess::"#NAME ), param1, param2 ); \
    else \
        return d->process.NAME( param1, param2 ); \
}

#undef VOID_NO_ARGS
#define VOID_NO_ARGS( NAME ) \
void QProcessWrapper::NAME() \
{ \
    qDebug() << Q_FUNC_INFO; \
    const Private::TimerBlocker blocker( this );\
    if( d->createSocket() ) \
        callRemoteVoidMethod( d->stream, QLatin1String( "QProcess::"#NAME ) ); \
    else \
        d->process.NAME(); \
}

#undef VOID_ONE_ARG
#define VOID_ONE_ARG( NAME, TYPE1 ) \
void QProcessWrapper::NAME( TYPE1 param1 ) \
{ \
    qDebug() << Q_FUNC_INFO; \
    const Private::TimerBlocker blocker( this );\
    if( d->createSocket() ) \
        callRemoteVoidMethod( d->stream, QLatin1String( "QProcess::"#NAME ), param1 ); \
    else \
        d->process.NAME( param1 ); \
}

#undef VOID_TWO_ARGS
#define VOID_TWO_ARGS( NAME, TYPE1, TYPE2 ) \
void QProcessWrapper::NAME( TYPE1 param1, TYPE2 param2 ) \
{ \
    qDebug() << Q_FUNC_INFO; \
    const Private::TimerBlocker blocker( this );\
    if( d->createSocket() ) \
        callRemoteVoidMethod( d->stream, QLatin1String( "QProcess::"#NAME ), param1, param2 ); \
    else \
        d->process.NAME( param1, param2 ); \
}

#define VOID_THREE_ARGS( NAME, TYPE1, TYPE2, TYPE3 ) \
void QProcessWrapper::NAME( TYPE1 param1, TYPE2 param2, TYPE3 param3 ) \
{ \
    qDebug() << Q_FUNC_INFO; \
    const Private::TimerBlocker blocker( this );\
    if( d->createSocket() ) \
        callRemoteVoidMethod( d->stream, QLatin1String( "QProcess::"#NAME ), param1, param2, param3 ); \
    else \
        d->process.NAME( param1, param2, param3 ); \
}

VOID_NO_ARGS( closeWriteChannel );
RETURN_NO_ARGS_CONST( int, exitCode );
RETURN_NO_ARGS_CONST( QProcessWrapper::ExitStatus, exitStatus );
VOID_NO_ARGS( kill )
RETURN_NO_ARGS( QByteArray, readAll );
RETURN_NO_ARGS( QByteArray, readAllStandardOutput );
VOID_THREE_ARGS( start, const QString&, const QStringList&, QIODevice::OpenMode )
VOID_ONE_ARG( start, const QString& )

bool startDetached( const QString& program, const QStringList& args, const QString& workingDirectory, qint64* pid );

bool QProcessWrapper::startDetached( const QString& program, const QStringList& arguments, const QString& workingDirectory, qint64* pid )
{
    QProcessWrapper w;
    if( w.d->createSocket() )
    {
        const QPair< bool, qint64 > result = callRemoteMethod< QPair< bool, qint64 > >( w.d->stream, QLatin1String( "QProcess::startDetached" ), program, arguments, workingDirectory );
        if( pid != 0 )
            *pid = result.second;
        return result.first;
    }
    else
    {
        return ::startDetached( program, arguments, workingDirectory, pid );
    }
}

bool QProcessWrapper::startDetached( const QString& program, const QStringList& arguments )
{
    return startDetached( program, arguments, QDir::currentPath() );
}

bool QProcessWrapper::startDetached( const QString& program )
{
    return startDetached( program, QStringList() );
}

void QProcessWrapper::setProcessChannelMode( QProcessWrapper::ProcessChannelMode mode )
{ \
    const Private::TimerBlocker blocker( this );\
    if( d->createSocket() ) \
        callRemoteVoidMethod( d->stream, QLatin1String( "QProcess::setProcessChannelMode" ), static_cast<QProcess::ProcessChannelMode>( mode ) ); \
    else \
        d->process.setProcessChannelMode( static_cast<QProcess::ProcessChannelMode>( mode ) ); \
}

/*!
 Cancels the process. This methods tries to terminate the process
 gracefully by calling QProcess::terminate. After 10 seconds, the process gets killed.
 */
void QProcessWrapper::cancel()
{
    if( state() == QProcessWrapper::Running )
        terminate();
    if( !waitForFinished( 10000 ) )
        kill();
}

void QProcessWrapper::setReadChannel( QProcessWrapper::ProcessChannel chan )
{ \
    const Private::TimerBlocker blocker( this );\
    if( d->createSocket() ) \
        callRemoteVoidMethod( d->stream, QLatin1String( "QProcess::setReadChannel" ), static_cast<QProcess::ProcessChannel>( chan ) ); \
    else \
        d->process.setReadChannel( static_cast<QProcess::ProcessChannel>( chan ) ); \
}

RETURN_NO_ARGS_CONST( QProcessWrapper::ProcessState, state )
VOID_NO_ARGS( terminate )
RETURN_ONE_ARG( bool, waitForFinished, int )
RETURN_ONE_ARG( bool, waitForStarted, int )
RETURN_NO_ARGS_CONST( QProcessWrapper::ProcessChannel, readChannel )
RETURN_NO_ARGS_CONST( QProcessWrapper::ProcessChannelMode, processChannelMode )
RETURN_NO_ARGS_CONST( QString, workingDirectory )
RETURN_ONE_ARG( qint64, write, const QByteArray& )
VOID_ONE_ARG( setEnvironment, const QStringList& )
#ifdef Q_OS_WIN
VOID_ONE_ARG( setNativeArguments, const QString& )
#endif
VOID_ONE_ARG( setWorkingDirectory, const QString& )

#include "moc_fsengineclient.cpp"
#include "fsengineclient.moc"
