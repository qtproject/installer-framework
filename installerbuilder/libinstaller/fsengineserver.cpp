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
#include "fsengineserver.h"

#include <QCoreApplication>
#include <QFSFileEngine>
#include <QLocalSocket>
#include <QDataStream>
#include <QProcess>
#include <QSettings>
#include <QTcpSocket>
#include <QThread>
#include <QVariant>

#ifdef FSENGINE_TCP
typedef int descriptor_t;
#else
typedef quintptr descriptor_t;
#endif

#ifdef Q_WS_WIN 
#include <windows.h>

// stolen from qprocess_win.cpp
static QString qt_create_commandline(const QString &program, const QStringList &arguments)
{
    QString args;
    if (!program.isEmpty()) {
        QString programName = program;
        if (!programName.startsWith(QLatin1Char('\"')) && !programName.endsWith(QLatin1Char('\"')) && programName.contains(QLatin1Char(' ')))
            programName = QLatin1Char('\"') + programName + QLatin1Char('\"');
        programName.replace(QLatin1Char('/'), QLatin1Char('\\'));

        // add the prgram as the first arg ... it works better
        args = programName + QLatin1Char(' ');
    }

    for (int i=0; i<arguments.size(); ++i) {
        QString tmp = arguments.at(i);
        // in the case of \" already being in the string the \ must also be escaped
        tmp.replace( QLatin1String("\\\""), QLatin1String("\\\\\"") );
        // escape a single " because the arguments will be parsed
        tmp.replace( QLatin1Char('\"'), QLatin1String("\\\"") );
        if (tmp.isEmpty() || tmp.contains(QLatin1Char(' ')) || tmp.contains(QLatin1Char('\t'))) {
            // The argument must not end with a \ since this would be interpreted
            // as escaping the quote -- rather put the \ behind the quote: e.g.
            // rather use "foo"\ than "foo\"
            QString endQuote(QLatin1Char('\"'));
            int i = tmp.length();
            while (i>0 && tmp.at(i-1) == QLatin1Char('\\')) {
                --i;
                endQuote += QLatin1Char('\\');
            }
            args += QLatin1String(" \"") + tmp.left(i) + endQuote;
        } else {
            args += QLatin1Char(' ') + tmp;
        }
    }
    return args;
}
#endif

bool startDetached( const QString& program, const QStringList& args, const QString& workingDirectory, qint64* pid )
{
#ifdef Q_WS_WIN
        const QString arguments = qt_create_commandline( program, args );
        
        PROCESS_INFORMATION pinfo;

        STARTUPINFOW startupInfo = { sizeof( STARTUPINFO ), 0, 0, 0,
                                     static_cast< ulong >( CW_USEDEFAULT ), static_cast< ulong >( CW_USEDEFAULT ),
                                     static_cast< ulong >( CW_USEDEFAULT ), static_cast< ulong >( CW_USEDEFAULT ),
                                     0, 0, 0, STARTF_USESHOWWINDOW, SW_HIDE, 0, 0, 0, 0, 0
                                   };
        const bool success = CreateProcess( 0, const_cast< wchar_t* >( static_cast< const wchar_t* >( arguments.utf16() ) ),
                                            0, 0, FALSE, CREATE_UNICODE_ENVIRONMENT | CREATE_NEW_CONSOLE,
                                            0, (wchar_t*)workingDirectory.utf16(),
                                            &startupInfo, &pinfo );
        if( success )
        {
            CloseHandle( pinfo.hThread );
            CloseHandle( pinfo.hProcess );
            if( pid )
                *pid = pinfo.dwProcessId;
        }
       
        return success;

#else
    return QProcess::startDetached( program, args, workingDirectory, pid );
#endif
}

class QProcessSignalReceiver : public QObject
{
    Q_OBJECT
public:
    QProcessSignalReceiver( QObject* parent = 0 )
        : QObject( parent )
    {
        connect( parent, SIGNAL( finished( int, QProcess::ExitStatus ) ),
                 this,   SLOT( processFinished( int, QProcess::ExitStatus ) ) );
        connect( parent, SIGNAL( error( QProcess::ProcessError ) ),
                 this,   SLOT( processError( QProcess::ProcessError ) ) );
        connect( parent, SIGNAL( readyRead() ),
                 this,   SLOT( processReadyRead() ) );
        connect( parent, SIGNAL( started() ),
                 this,   SLOT( processStarted() ) );
        connect( parent, SIGNAL( stateChanged( QProcess::ProcessState ) ),
                 this,   SLOT( processStateChanged( QProcess::ProcessState ) ) );
     }
    ~QProcessSignalReceiver()
    {
    }
    
    QList< QVariant > receivedSignals;

private Q_SLOTS:
    void processError( QProcess::ProcessError error );
    void processFinished( int exitCode, QProcess::ExitStatus exitStatus );
    void processReadyRead();
    void processStarted();
    void processStateChanged( QProcess::ProcessState newState );

};

/*!
 \internal
 */
class FSEngineConnectionThread : public QThread
{
    Q_OBJECT
public:
    FSEngineConnectionThread( descriptor_t socketDescriptor, QObject* parent )
        : QThread( parent ),
          descriptor( socketDescriptor ),
          settings( 0 ),
          process( 0 ),
          signalReceiver( 0 )
    {
    }

    ~FSEngineConnectionThread()
    {
    }

protected:
    void run();
  
private:
    QByteArray handleCommand( const QString& command );

    QFSFileEngine engine;
    const descriptor_t descriptor;
    QDataStream receivedStream;
    QSettings* settings;
    
    QProcess* process;
    QProcessSignalReceiver* signalReceiver;
};

#ifdef FSENGINE_TCP
FSEngineServer::FSEngineServer( quint16 port, QObject* parent )
    : QTcpServer( parent )
{
    listen( QHostAddress::LocalHost, port );
    watchdog.setTimeoutInterval( 30000 );
    connect( &watchdog, SIGNAL( timeout() ), qApp, SLOT( quit() ) );
}

FSEngineServer::FSEngineServer( const QHostAddress& address, quint16 port, QObject* parent )
    : QTcpServer( parent )
{
    listen( address, port );
    watchdog.setTimeoutInterval( 30000 );
    connect( &watchdog, SIGNAL( timeout() ), qApp, SLOT( quit() ) );
}

#else
/*!
 Creates a new FSEngineServer with \a parent. The server will listen on \a socket.
 */
FSEngineServer::FSEngineServer( const QString& socket, QObject* parent )
    : QLocalServer( parent )
{
    removeServer( socket );
    listen( socket );
    QFile( socket ).setPermissions( static_cast< QFile::Permissions >( 0x6666 ) );
    watchdog.setTimeoutInterval( 30000 );
    connect( &watchdog, SIGNAL( timeout() ), qApp, SLOT( quit() ) );
}

#endif

/*!
 Destroys the FSEngineServer.
 */
FSEngineServer::~FSEngineServer()
{
    const QList< QThread* > threads = findChildren< QThread* >();
    for( QList< QThread* >::const_iterator it = threads.begin(); it != threads.end(); ++it )
        (*it)->wait();
}

#ifdef FSENGINE_TCP
/*!
 \reimp
 */
void FSEngineServer::incomingConnection( int socketDescriptor )
{   
    qApp->processEvents();
    QThread* const thread = new FSEngineConnectionThread( socketDescriptor, this );
    connect( thread, SIGNAL( finished() ), thread, SLOT( deleteLater() ) );
    thread->start();
    watchdog.resetTimeoutTimer();
}

#else

/*!
 \reimp
 */
void FSEngineServer::incomingConnection( quintptr socketDescriptor )
{   
    qApp->processEvents();
    QThread* const thread = new FSEngineConnectionThread( socketDescriptor, this );
    connect( thread, SIGNAL( finished() ), thread, SLOT( deleteLater() ) );
    thread->start();
    watchdog.resetTimeoutTimer();
}
#endif
   
/*!
 Sets the authorization key this server is asking the clients for to \a authorizationKey.
 */
void FSEngineServer::setAuthorizationKey( const QString& authorizationKey )
{
    key = authorizationKey;
}

QString FSEngineServer::authorizationKey() const
{
    return key;
}
    
void QProcessSignalReceiver::processError( QProcess::ProcessError error )
{
    receivedSignals.push_back( QLatin1String( "error" ) );
    receivedSignals.push_back( static_cast< int >( error ) );
}

void QProcessSignalReceiver::processFinished( int exitCode, QProcess::ExitStatus exitStatus )
{
    receivedSignals.push_back( QLatin1String( "finished" ) );
    receivedSignals.push_back( exitCode );
    receivedSignals.push_back( static_cast< int >( exitStatus ) );
}

void QProcessSignalReceiver::processStarted()
{
    receivedSignals.push_back( QLatin1String( "started" ) );
}

void QProcessSignalReceiver::processReadyRead()
{
    receivedSignals.push_back( QLatin1String( "readyRead" ) );
}
     
void QProcessSignalReceiver::processStateChanged( QProcess::ProcessState newState )
{
    receivedSignals.push_back( QLatin1String( "stateChanged" ) );
    receivedSignals.push_back( static_cast< int >( newState ) );
}

/*!
 \reimp
 */
void FSEngineConnectionThread::run()
{
#ifdef FSENGINE_TCP
    QTcpSocket socket;
#else
    QLocalSocket socket;
#endif
    socket.setSocketDescriptor( descriptor );

    receivedStream.setDevice( &socket );
    receivedStream.setVersion( QDataStream::Qt_4_2 );
    
    bool authorized = false;

    while( static_cast< QLocalSocket::LocalSocketState >( socket.state() ) == QLocalSocket::ConnectedState )
    {
        if( !socket.bytesAvailable() && !socket.waitForReadyRead( 250 ) )
            continue;

        QString command;
        receivedStream >> command;

        if( authorized && command == QLatin1String( "shutdown" ) )
        {
            // this is a graceful shutdown
            socket.close();
            parent()->deleteLater();
            return;
        }
        else if( command == QLatin1String( "authorize" ) )
        {
            QString k;
            receivedStream >> k;
            if( k != dynamic_cast< FSEngineServer* >( parent() )->authorizationKey() )
            {
                // this is closing the connection... auth failed
                socket.close();
                return;
            }
            authorized = true;
        }
        else if( authorized )
        {
            if( command.isEmpty() )
                continue;
            const QByteArray result = handleCommand( command );
            receivedStream << static_cast< quint32 >( result.size() );
            if( !result.isEmpty() )
                receivedStream.writeRawData( result.data(), result.size() );
        }
        else
        {
            // authorization failed, connection not wanted
            socket.close();
            return;
        }
    }
}

static QDataStream& operator<<( QDataStream& stream, const QSettings::Status& status )
{
    return stream << static_cast< int >( status );
}

/*!
 Handles \a command and returns a QByteArray which has the result streamed into it.
 */
QByteArray FSEngineConnectionThread::handleCommand( const QString& command )
{
    QByteArray block;
    QDataStream returnStream( &block, QIODevice::WriteOnly );
    returnStream.setVersion( QDataStream::Qt_4_2 );
        
    // first, QSettings handling
    if( command == QLatin1String( "createQSettings" ) )
    {
        QString fileName;
        receivedStream >> fileName;
        settings = new QSettings( fileName, QSettings::NativeFormat );
    }
    else if( command == QLatin1String( "destroyQSettings" ) )
    {
        delete settings;
        settings = 0;
    }
    else if( command == QLatin1String( "QSettings::allKeys" ) )
    {
        returnStream << settings->allKeys();
    }
    else if( command == QLatin1String( "QSettings::beginGroup" ) )
    {
        QString prefix;
        receivedStream >> prefix;
        settings->beginGroup( prefix );
    }
    else if( command == QLatin1String( "QSettings::beginReadArray" ) )
    {
        QString prefix;
        int size;
        receivedStream >> prefix;
        receivedStream >> size;
        settings->beginWriteArray( prefix, size );
    }
    else if( command == QLatin1String( "QSettings::beginWriteArray" ) )
    {
        QString prefix;
        receivedStream >> prefix;
        returnStream << settings->beginReadArray( prefix );
    }
    else if( command == QLatin1String( "QSettings::childGroups" ) )
    {
        returnStream << settings->childGroups();
    }
    else if( command == QLatin1String( "QSettings::childKeys" ) )
    {
        returnStream << settings->childKeys();
    }
    else if( command == QLatin1String( "QSettings::clear" ) )
    {
        settings->clear();
    }
    else if( command == QLatin1String( "QSettings::contains" ) )
    {
        QString key;
        receivedStream >> key;
        returnStream << settings->contains( key );
    }
    else if( command == QLatin1String( "QSettings::endArray" ) )
    {
        settings->endArray();
    }
    else if( command == QLatin1String( "QSettings::endGroup" ) )
    {
        settings->endGroup();
    }
    else if( command == QLatin1String( "QSettings::fallbacksEnabled" ) )
    {
        returnStream << settings->fallbacksEnabled();
    }
    else if( command == QLatin1String( "QSettings::fileName" ) )
    {
        returnStream << settings->fileName();
    }
    else if( command == QLatin1String( "QSettings::group" ) )
    {
        returnStream << settings->group();
    }
    else if( command == QLatin1String( "QSettings::isWritable" ) )
    {
        returnStream << settings->isWritable();
    }
    else if( command == QLatin1String( "QSettings::remove" ) )
    {
        QString key;
        receivedStream >> key;
        settings->remove( key );
    }
    else if( command == QLatin1String( "QSettings::setArrayIndex" ) )
    {
        int i;
        receivedStream >> i;
        settings->setArrayIndex( i );
    }
    else if( command == QLatin1String( "QSettings::setFallbacksEnabled" ) )
    {
        bool b;
        receivedStream >> b;
        settings->setFallbacksEnabled( b );
    }
    else if( command == QLatin1String( "QSettings::status" ) )
    {
        returnStream << settings->status();
    }
    else if( command == QLatin1String( "QSettings::sync" ) )
    {
        settings->sync();
    }
    else if( command == QLatin1String( "QSettings::setValue" ) )
    {
        QString key;
        QVariant value;
        receivedStream >> key;
        receivedStream >> value;
        settings->setValue( key, value );
    }
    else if( command == QLatin1String( "QSettings::value" ) )
    {
        QString key;
        QVariant defaultValue;
        receivedStream >> key;
        receivedStream >> defaultValue;
        returnStream << settings->value( key, defaultValue );
    }

    // from here, QProcess handling
    else if( command == QLatin1String( "createQProcess" ) )
    {
        process = new QProcess;
        signalReceiver = new QProcessSignalReceiver( process );
   }
    else if( command == QLatin1String( "destroyQProcess" ) )
    {
        signalReceiver->receivedSignals.clear();
        process->deleteLater();
        process = 0;
    }
    else if( command == QLatin1String( "getQProcessSignals" ) )
    {
        returnStream << signalReceiver->receivedSignals;
        signalReceiver->receivedSignals.clear();
        qApp->processEvents();
    }
    else if( command == QLatin1String( "QProcess::closeWriteChannel" ) )
    {
        process->closeWriteChannel();
    }
    else if( command == QLatin1String( "QProcess::exitCode" ) )
    {
        returnStream << process->exitCode();
    }
    else if( command == QLatin1String( "QProcess::exitStatus" ) )
    {
        returnStream << static_cast< int >( process->exitStatus() );
    }
    else if( command == QLatin1String( "QProcess::kill" ) )
    {
        process->kill();
    }
    else if( command == QLatin1String( "QProcess::readAll" ) )
    {
        returnStream << process->readAll();
    }
    else if( command == QLatin1String( "QProcess::readAllStandardOutput" ) )
    {
        returnStream << process->readAllStandardOutput();
    }
    else if( command == QLatin1String( "QProcess::startDetached" ) )
    {
        QString program;
        QStringList arguments;
        QString workingDirectory;
        receivedStream >> program;
        receivedStream >> arguments;
        receivedStream >> workingDirectory;
        qint64 pid;
        const bool result = startDetached( program, arguments, workingDirectory, &pid );
        returnStream << qMakePair< bool, qint64 >( result, pid );
    }
    else if( command == QLatin1String( "QProcess::setWorkingDirectory" ) )
    {
        QString dir;
        receivedStream >> dir;
        process->setWorkingDirectory( dir );
    }
    else if( command == QLatin1String( "QProcess::setEnvironment" ) )
    {
        QStringList env;
        receivedStream >> env;
        process->setEnvironment( env );
    }
    else if( command == QLatin1String( "QProcess::start" ) )
    {
        QString program;
        QStringList arguments;
        int mode;
        receivedStream >> program;
        receivedStream >> arguments;
        receivedStream >> mode;
        process->start( program, arguments, static_cast< QIODevice::OpenMode >( mode ) );
    }
    else if( command == QLatin1String( "QProcess::state" ) )
    {
        returnStream << static_cast< int >( process->state() );
    }
    else if( command == QLatin1String( "QProcess::terminate" ) )
    {
        process->terminate();
    }
    else if( command == QLatin1String( "QProcess::waitForFinished" ) )
    {
        int msecs;
        receivedStream >> msecs;
        returnStream << process->waitForFinished( msecs );
    }
    else if( command == QLatin1String( "QProcess::waitForStarted" ) )
    {
        int msecs;
        receivedStream >> msecs;
        returnStream << process->waitForStarted( msecs );
    }
    else if( command == QLatin1String( "QProcess::workingDirectory" ) )
    {
        returnStream << process->workingDirectory();
    }
    else if( command == QLatin1String( "QProcess::write" ) )
    {
        QByteArray byteArray;
        receivedStream >> byteArray;
        returnStream << process->write( byteArray );
    }
    else if( command == QLatin1String( "QProcess::readChannel" ) )
    {
        returnStream << static_cast< int >( process->readChannel() );
    }
    else if( command == QLatin1String( "QProcess::setReadChannel" ) )
    {
        int processChannel;
        receivedStream >> processChannel;
        process->setReadChannel( static_cast<QProcess::ProcessChannel>(processChannel) );
    }
    else if( command == QLatin1String( "QProcess::write" ) )
    {
        QByteArray byteArray;
        receivedStream >> byteArray;
        returnStream << process->write( byteArray );
    }

    // from here, QFSEngine handling
    else if( command == QLatin1String( "QFSFileEngine::atEnd" ) )
    {
        returnStream << engine.atEnd();
    }
    else if( command == QLatin1String( "QFSFileEngine::caseSensitive" ) )
    {
        returnStream << engine.caseSensitive();
    }
    else if( command == QLatin1String( "QFSFileEngine::close" ) )
    {
        returnStream << engine.close();
    }
    else if( command == QLatin1String( "QFSFileEngine::copy" ) )
    {
        QString newName;
        receivedStream >> newName;
        returnStream << engine.copy( newName );
    }
    else if( command == QLatin1String( "QFSFileEngine::entryList" ) )
    {
        int filters;
        QStringList filterNames;
        receivedStream >> filters;
        receivedStream >> filterNames;
        returnStream << engine.entryList( static_cast< QDir::Filters >( filters ), filterNames );
    }
    else if( command == QLatin1String( "QFSFileEngine::error" ) )
    {
        returnStream << static_cast< int >( engine.error() );
    }
    else if( command == QLatin1String( "QFSFileEngine::errorString" ) )
    {
        returnStream << engine.errorString();
    }
    // extension
    else if( command == QLatin1String( "QFSFileEngine::fileFlags" ) )
    {
        int flags;
        receivedStream >> flags;
        returnStream << static_cast< int >( engine.fileFlags( static_cast< QAbstractFileEngine::FileFlags >( flags ) ) );
    }
    else if( command == QLatin1String( "QFSFileEngine::fileName" ) )
    {
        int file;
        receivedStream >> file;
        returnStream << engine.fileName( static_cast< QAbstractFileEngine::FileName >( file ) );
    }
    else if( command == QLatin1String( "QFSFileEngine::flush" ) )
    {
        returnStream << engine.flush();
    }
    else if( command == QLatin1String( "QFSFileEngine::handle" ) )
    {
        returnStream << engine.handle();
    }
    else if( command == QLatin1String( "QFSFileEngine::isRelativePath" ) )
    {
        returnStream << engine.isRelativePath();
    }
    else if( command == QLatin1String( "QFSFileEngine::isSequential" ) )
    {
        returnStream << engine.isSequential();
    }
    else if( command == QLatin1String( "QFSFileEngine::link" ) )
    {
        QString newName;
        receivedStream >> newName;
        returnStream << engine.link( newName );
    }
    else if( command == QLatin1String( "QFSFileEngine::mkdir" ) )
    {
        QString dirName;
        bool createParentDirectories;
        receivedStream >> dirName;
        receivedStream >> createParentDirectories;
        returnStream << engine.mkdir( dirName, createParentDirectories );
    }
    else if( command == QLatin1String( "QFSFileEngine::open" ) )
    {
        int openMode;
        receivedStream >> openMode;
        returnStream << engine.open( static_cast< QIODevice::OpenMode >( openMode ) );
    }
    else if( command == QLatin1String( "QFSFileEngine::owner" ) )
    {
        int owner;
        receivedStream >> owner;
        returnStream << engine.owner( static_cast< QAbstractFileEngine::FileOwner >( owner ) );
    }
    else if( command == QLatin1String( "QFSFileEngine::ownerId" ) )
    {
        int owner;
        receivedStream >> owner;
        returnStream << engine.ownerId( static_cast< QAbstractFileEngine::FileOwner >( owner ) );
    }
    else if( command == QLatin1String( "QFSFileEngine::pos" ) )
    {
        returnStream << engine.pos();
    }
    else if( command == QLatin1String( "QFSFileEngine::read" ) )
    {
        qint64 maxlen;
        receivedStream >> maxlen;
        QByteArray ba( maxlen, '\0' );
        const qint64 result = engine.read( ba.data(), maxlen );
        returnStream << result;
        int written = 0;
        while( written < result )
            written += returnStream.writeRawData( ba.data() + written, result - written );
    }
    else if( command == QLatin1String( "QFSFileEngine::readLine" ) )
    {
        qint64 maxlen;
        receivedStream >> maxlen;
        QByteArray ba( maxlen, '\0' );
        const qint64 result = engine.readLine( ba.data(), maxlen );
        returnStream << result;
        int written = 0;
        while( written < result )
            written += returnStream.writeRawData( ba.data() + written, result - written );
    }
    else if( command == QLatin1String( "QFSFileEngine::remove" ) )
    {
        returnStream << engine.remove();
    }
    else if( command == QLatin1String( "QFSFileEngine::rename" ) )
    {
        QString newName;
        receivedStream >> newName;
        returnStream << engine.rename( newName );
    }
    else if( command == QLatin1String( "QFSFileEngine::rmdir" ) )
    {
        QString dirName;
        bool recurseParentDirectories;
        receivedStream >> dirName;
        receivedStream >> recurseParentDirectories;
        returnStream << engine.rmdir( dirName, recurseParentDirectories );
    }
    else if( command == QLatin1String( "QFSFileEngine::seek" ) )
    {
        quint64 offset;
        receivedStream >> offset;
        returnStream << engine.seek( offset );
    }
    else if( command == QLatin1String( "QFSFileEngine::setFileName" ) )
    {
        QString fileName;
        receivedStream >> fileName;
        engine.setFileName( fileName );
    }
    else if( command == QLatin1String( "QFSFileEngine::setPermissions" ) )
    {
        uint perms;
        receivedStream >> perms;
        returnStream << engine.setPermissions( perms );
    }
    else if( command == QLatin1String( "QFSFileEngine::setSize" ) )
    {
        qint64 size;
        receivedStream >> size;
        returnStream << engine.setSize( size );
    }
    else if( command == QLatin1String( "QFSFileEngine::size" ) )
    {
        returnStream << engine.size();
    }
    else if( command == QLatin1String( "QFSFileEngine::supportsExtension" ) )
    {
        int extension;
        receivedStream >> extension;
        returnStream << false;//engine.supportsExtension( static_cast< QAbstractFileEngine::Extension >( extension ) );
    }
    else if( command == QLatin1String( "QFSFileEngine::write" ) )
    {
        qint64 length;
        receivedStream >> length;
        qint64 read = 0;
        qint64 written = 0;
        QByteArray buffer( 65536, '\0' );
        while( read < length )
        {
            if( !receivedStream.device()->bytesAvailable() )
                receivedStream.device()->waitForReadyRead( -1 );
            const qint64 r = receivedStream.readRawData( buffer.data(), qMin( length - read, static_cast< qint64 >( buffer.length() ) ) );
            read += r;
            qint64 w = 0;
            while( w < r )
                w += engine.write( buffer.data(), r );
            written += r;
        }

        returnStream << written;
    }
    else if( !command.isEmpty() )
    {
        qDebug() << "unknown command:" << command;
        // unknown command...
    }

    return block;
}

#include "fsengineserver.moc"
#include "moc_fsengineserver.cpp"
