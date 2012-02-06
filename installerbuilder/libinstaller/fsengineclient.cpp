/**************************************************************************
**
** This file is part of Installer Framework
**
** Copyright (c) 2011-2012 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (qt-info@nokia.com)
**
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
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** Other Usage
**
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
** If you have questions regarding the use of this file, please contact
** Nokia at qt-info@nokia.com.
**
**************************************************************************/

#include "fsengineclient.h"

#include "adminauthorization.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QMutex>
#include <QtCore/QProcess>
#include <QtCore/QThread>
#include <QtCore/QTimer>
#include <QtCore/QUuid>

#include <QtNetwork/QHostAddress>
#include <QtNetwork/QTcpSocket>


// -- StillAliveThread

/*!
    This thread convinces the watchdog in the running server that the client has not crashed yet.
*/
class StillAliveThread : public QThread
{
    Q_OBJECT
public:
    void run()
    {
        QTimer stillAliveTimer;
        connect(&stillAliveTimer, SIGNAL(timeout()), this, SLOT(stillAlive()));
        stillAliveTimer.start(1000);
        exec();
    }

public Q_SLOTS:
    void stillAlive()
    {
        if (!FSEngineClientHandler::instance().isServerRunning())
            return;

        // in case of the server not running, this will simply fail
        QTcpSocket socket;
        FSEngineClientHandler::instance().connect(&socket);
    }
};


// -- FSEngineClient

class FSEngineClient : public QAbstractFileEngine
{
public:
    FSEngineClient();
    ~FSEngineClient();

    bool atEnd() const;
    Iterator *beginEntryList(QDir::Filters filters, const QStringList &filterNames);
    bool caseSensitive() const;
    bool close();
    bool copy(const QString &newName);
    QStringList entryList(QDir::Filters filters, const QStringList &filterNames) const;
    QFile::FileError error() const;
    QString errorString() const;
    bool extension(Extension extension, const ExtensionOption *option = 0, ExtensionReturn *output = 0);
    FileFlags fileFlags(FileFlags type = FileInfoAll) const;
    QString fileName(FileName file = DefaultName) const;
    bool flush();
    int handle() const;
    bool isRelativePath() const;
    bool isSequential() const;
    bool link(const QString &newName);
    bool mkdir(const QString &dirName, bool createParentDirectories) const;
    bool open(QIODevice::OpenMode mode);
    QString owner(FileOwner owner) const;
    uint ownerId(FileOwner owner) const;
    qint64 pos() const;
    qint64 read(char *data, qint64 maxlen);
    qint64 readLine(char *data, qint64 maxlen);
    bool remove();
    bool rename(const QString &newName);
    bool rmdir(const QString &dirName, bool recurseParentDirectories) const;
    bool seek(qint64 offset);
    void setFileName(const QString &fileName);
    bool setPermissions(uint perms);
    bool setSize(qint64 size);
    qint64 size() const;
    bool supportsExtension(Extension extension) const;
    qint64 write(const char *data, qint64 len);

private:
    template<typename T> T returnWithType() const;
    template<typename T> T returnWithCastedType() const;

private:
    friend class FSEngineClientHandler;

    mutable QTcpSocket *socket;
    mutable QDataStream stream;
};

template<typename T> T FSEngineClient::returnWithType() const
{
    socket->flush();
    if (!socket->bytesAvailable())
        socket->waitForReadyRead();
    quint32 test;
    stream >> test;

    T result;
    stream >> result;
    return result;
}

template<typename T> T FSEngineClient::returnWithCastedType() const
{
    socket->flush();
    if (!socket->bytesAvailable())
       socket->waitForReadyRead();
    quint32 test;
    stream >> test;

    int result;
    stream >> result;
    return static_cast<T>(result);
}

/*!
 \internal
*/
class FSEngineClientIterator : public QAbstractFileEngineIterator
{
public:
    FSEngineClientIterator(QDir::Filters filters, const QStringList &nameFilters, const QStringList &files)
        : QAbstractFileEngineIterator(filters, nameFilters),
          entries(files),
          index(-1)
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
       if (!hasNext())
           return QString();
       ++index;
       return currentFilePath();
    }

    /*!
        \reimp
    */
    QString currentFileName() const
    {
        return entries.at(index);
    }

private:
    const QStringList entries;
    int index;
};

FSEngineClient::FSEngineClient()
    : socket(new QTcpSocket)
{
    FSEngineClientHandler::instance().connect(socket);
    stream.setDevice(socket);
    stream.setVersion(QDataStream::Qt_4_2);
}

FSEngineClient::~FSEngineClient()
{
    if (QThread::currentThread() == socket->thread()) {
        socket->close();
        delete socket;
    } else {
        socket->deleteLater();
    }
}

/*!
    \reimp
*/
bool FSEngineClient::atEnd() const
{
    stream << QString::fromLatin1("QFSFileEngine::atEnd");
    return returnWithType<bool>();
}

/*!
    \reimp
*/
QAbstractFileEngine::Iterator* FSEngineClient::beginEntryList(QDir::Filters filters,
    const QStringList &filterNames)
{
    QStringList entries = entryList(filters, filterNames);
    entries.removeAll(QString());
    return new FSEngineClientIterator(filters, filterNames, entries);
}

/*!
    \reimp
*/
bool FSEngineClient::caseSensitive() const
{
    stream << QString::fromLatin1("QFSFileEngine::caseSensitive");
    return returnWithType<bool>();
}

/*!
    \reimp
*/
bool FSEngineClient::close()
{
    stream << QString::fromLatin1("QFSFileEngine::close");
    return returnWithType<bool>();
}

/*!
    \reimp
*/
bool FSEngineClient::copy(const QString &newName)
{
    stream << QString::fromLatin1("QFSFileEngine::copy");
    stream << newName;
    return returnWithType<bool>();
}

/*!
    \reimp
*/
QStringList FSEngineClient::entryList(QDir::Filters filters, const QStringList &filterNames) const
{
    stream << QString::fromLatin1("QFSFileEngine::entryList");
    stream << static_cast<int>(filters);
    stream << filterNames;
    return returnWithType<QStringList>();
}

/*!
    \reimp
*/
QFile::FileError FSEngineClient::error() const
{
    stream << QString::fromLatin1("QFSFileEngine::error");
    return returnWithCastedType<QFile::FileError>();
}

/*!
    \reimp
*/
QString FSEngineClient::errorString() const
{
    stream << QString::fromLatin1("QFSFileEngine::errorString");
    return returnWithType<QString>();
}

/*!
    \reimp
*/
bool FSEngineClient::extension(Extension extension, const ExtensionOption *option, ExtensionReturn *output)
{
    Q_UNUSED(extension)
    Q_UNUSED(option)
    Q_UNUSED(output)
    return false;
}

/*!
    \reimp
*/
QAbstractFileEngine::FileFlags FSEngineClient::fileFlags(FileFlags type) const
{
    stream << QString::fromLatin1("QFSFileEngine::fileFlags");
    stream << static_cast<int>(type);
    return returnWithCastedType<QAbstractFileEngine::FileFlags>();
}

/*!
    \reimp
*/
QString FSEngineClient::fileName(FileName file) const
{
    stream << QString::fromLatin1("QFSFileEngine::fileName");
    stream << static_cast<int>(file);
    return returnWithType<QString>();
}

/*!
    \reimp
*/
bool FSEngineClient::flush()
{
    stream << QString::fromLatin1("QFSFileEngine::flush");
    return returnWithType<bool>();
}

/*!
    \reimp
*/
int FSEngineClient::handle() const
{
    stream << QString::fromLatin1("QFSFileEngine::handle");
    return returnWithType<int>();
}

/*!
    \reimp
*/
bool FSEngineClient::isRelativePath() const
{
    stream << QString::fromLatin1("QFSFileEngine::isRelativePath");
    return returnWithType<bool>();
}

/*!
    \reimp
*/
bool FSEngineClient::isSequential() const
{
    stream << QString::fromLatin1("QFSFileEngine::isSequential");
    return returnWithType<bool>();
}

/*!
    \reimp
*/
bool FSEngineClient::link(const QString &newName)
{
    stream << QString::fromLatin1("QFSFileEngine::link");
    stream << newName;
    return returnWithType<bool>();
}

/*!
    \reimp
*/
bool FSEngineClient::mkdir(const QString &dirName, bool createParentDirectories) const
{
    stream << QString::fromLatin1("QFSFileEngine::mkdir");
    stream << dirName;
    stream << createParentDirectories;
    return returnWithType<bool>();
}

/*!
    \reimp
*/
bool FSEngineClient::open(QIODevice::OpenMode mode)
{
    stream << QString::fromLatin1("QFSFileEngine::open");
    stream << static_cast<int>(mode);
    return returnWithType<bool>();
}

/*!
    \reimp
*/
QString FSEngineClient::owner(FileOwner owner) const
{
    stream << QString::fromLatin1("QFSFileEngine::owner");
    stream << static_cast<int>(owner);
    return returnWithType<QString>();
}

/*!
    \reimp
*/
uint FSEngineClient::ownerId(FileOwner owner) const
{
    stream << QString::fromLatin1("QFSFileEngine::ownerId");
    stream << static_cast<int>(owner);
    return returnWithType<uint>();
}

/*!
    \reimp
*/
qint64 FSEngineClient::pos() const
{
    stream << QString::fromLatin1("QFSFileEngine::pos");
    return returnWithType<qint64>();
}

/*!
    \reimp
*/
qint64 FSEngineClient::read(char *data, qint64 maxlen)
{
    stream << QString::fromLatin1("QFSFileEngine::read");
    stream << maxlen;
    socket->flush();
    if (!socket->bytesAvailable())
        socket->waitForReadyRead();
    quint32 size;
    stream >> size;
    qint64 result;
    stream >> result;
    qint64 read = 0;
    while (read < result) {
        if (!socket->bytesAvailable())
            socket->waitForReadyRead();
        read += socket->read(data + read, result - read);
    }
    return result;
}

/*!
    \reimp
*/
qint64 FSEngineClient::readLine(char *data, qint64 maxlen)
{
    stream << QString::fromLatin1("QFSFileEngine::readLine");
    stream << maxlen;
    socket->flush();
    if (!socket->bytesAvailable())
        socket->waitForReadyRead();
    quint32 size;
    stream >> size;
    qint64 result;
    stream >> result;
    qint64 read = 0;
    while (read < result) {
        if (!socket->bytesAvailable())
            socket->waitForReadyRead();
        read += socket->read(data + read, result - read);
    }
    return result;
}

/*!
    \reimp
*/
bool FSEngineClient::remove()
{
    stream << QString::fromLatin1("QFSFileEngine::remove");
    return returnWithType<bool>();
}

/*!
    \reimp
*/
bool FSEngineClient::rename(const QString &newName)
{
    stream << QString::fromLatin1("QFSFileEngine::rename");
    stream << newName;
    return returnWithType<bool>();
}

/*!
    \reimp
*/
bool FSEngineClient::rmdir(const QString &dirName, bool recurseParentDirectories) const
{
    stream << QString::fromLatin1("QFSFileEngine::rmdir");
    stream << dirName;
    stream << recurseParentDirectories;
    return returnWithType<bool>();
}

/*!
    \reimp
*/
bool FSEngineClient::seek(qint64 offset)
{
    stream << QString::fromLatin1("QFSFileEngine::seek");
    stream << offset;
    return returnWithType<bool>();
}

/*!
    \reimp
*/
void FSEngineClient::setFileName(const QString &fileName)
{
    stream << QString::fromLatin1("QFSFileEngine::setFileName");
    stream << fileName;

    socket->flush();
    if (!socket->bytesAvailable())
       socket->waitForReadyRead();
    quint32 test;
    stream >> test;
}

/*!
    \reimp
*/
bool FSEngineClient::setPermissions(uint perms)
{
    stream << QString::fromLatin1("QFSFileEngine::setPermissions");
    stream << perms;
    return returnWithType<bool>();
}

/*!
    \reimp
*/
bool FSEngineClient::setSize(qint64 size)
{
    stream << QString::fromLatin1("QFSFileEngine::setSize");
    stream << size;
    return returnWithType<bool>();
}

/*!
    \reimp
*/
qint64 FSEngineClient::size() const
{
    stream << QString::fromLatin1("QFSFileEngine::size");
    return returnWithType<qint64>();
}

/*!
    \reimp
*/
bool FSEngineClient::supportsExtension(Extension extension) const
{
    stream << QString::fromLatin1("QFSFileEngine::supportsExtension");
    stream << static_cast<int>(extension);
    return returnWithType<bool>();
}

/*!
    \reimp
*/
qint64 FSEngineClient::write(const char *data, qint64 len)
{
    stream << QString::fromLatin1("QFSFileEngine::write");
    stream << len;
    qint64 written = 0;
    while (written < len) {
        written += socket->write(data, len - written);
        socket->waitForBytesWritten();
    }
    return returnWithType<qint64>();
}

class FSEngineClientHandler::Private
{
public:
    Private()
        : mutex(QMutex::Recursive),
          port(0),
          startServerAsAdmin(false),
          serverStarted(false),
          serverStarting(false),
          active(false),
          thread(new StillAliveThread)
    {
        thread->moveToThread(thread);
    }

    void maybeStartServer();
    void maybeStopServer();

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

    StillAliveThread *const thread;
};

/*!
    Creates a new FSEngineClientHandler with no connection.
*/
FSEngineClientHandler::FSEngineClientHandler()
    : d(new Private)
{
    //don't do this in the Private ctor as createUuid() accesses QFileEngine, which accesses this
    // half-constructed handler -> Crash (KDNDK-248)
    d->key = QUuid::createUuid().toString();
}

void FSEngineClientHandler::enableTestMode()
{
    d->key = QLatin1String("testAuthorizationKey");
    d->serverStarted = true;
}

void FSEngineClientHandler::init(quint16 port, const QHostAddress &a)
{
    d->address = a;
    d->port = port;
    d->thread->start();
}

bool FSEngineClientHandler::connect(QTcpSocket *socket)
{
    int tries = 3;
    while (tries > 0) {
        socket->connectToHost(d->address, d->port);
        if (!socket->waitForConnected(10000)) {
            if (static_cast<QAbstractSocket::SocketError>(socket->error()) != QAbstractSocket::UnknownSocketError)
                --tries;
            qApp->processEvents();
            continue;
        }

        QDataStream stream(socket);
        stream << QString::fromLatin1("authorize");
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
    QMetaObject::invokeMethod(d->thread, "quit");
    //d->maybeStopServer();
    delete d;
}

/*!
    Returns a previously created FSEngineClientHandler instance.
*/
FSEngineClientHandler &FSEngineClientHandler::instance()
{
    static FSEngineClientHandler instance;
    return instance;
}

/*!
    Returns a created authorization key which is sent to the server when connecting via the "authorize"
    command after the server was started.
*/
QString FSEngineClientHandler::authorizationKey() const
{
    return d->key;
}

/*!
    Sets \a command as the command to be executed to startup the server. If \a startAsAdmin is set,
    it is executed with admin privilegies.
*/
void FSEngineClientHandler::setStartServerCommand(const QString &command, bool startAsAdmin)
{
    setStartServerCommand(command, QStringList(), startAsAdmin);
}

/*!
    Sets \a command as the command to be executed to startup the server. If \a startAsAdmin is set, it is
    executed with admin privilegies. A list of \a arguments is passed to the process.
*/
void FSEngineClientHandler::setStartServerCommand(const QString &command, const QStringList &arguments,
    bool startAsAdmin)
{
    d->maybeStopServer();

    d->startServerAsAdmin = startAsAdmin;
    d->serverCommand = command;
    d->serverArguments = arguments;
}

/*!
    \reimp
*/
QAbstractFileEngine* FSEngineClientHandler::create(const QString &fileName) const
{
    if (d->serverStarting || !d->active)
        return 0;

    d->maybeStartServer();

    static QRegExp re(QLatin1String("^[a-z0-9]*://.*$"));
    if (re.exactMatch(fileName))  // stuff like installer:// 7z:// and so on
        return 0;

    if (fileName.isEmpty() || fileName.startsWith(QLatin1String(":")))
        return 0; // empty filename or Qt resource

    FSEngineClient *const client = new FSEngineClient;
    // authorize
    client->stream << QString::fromLatin1("authorize");
    client->stream << d->key;
    client->socket->flush();

    client->setFileName(fileName);
    return client;
}

/*!
    Sets the FSEngineClientHandler to \a active. I.e. to actually return FSEngineClients if asked for.
*/
void FSEngineClientHandler::setActive(bool active)
{
    d->active = active;
    if (active) {
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
    \internal
    Starts the server if a command was set and it isn't already started.
*/
void FSEngineClientHandler::Private::maybeStartServer()
{
    if (serverStarted || serverCommand.isEmpty())
        return;

    const QMutexLocker ml(&mutex);
    if (serverStarted)
        return;

    serverStarting = true;

    if (startServerAsAdmin) {
        AdminAuthorization auth;
        serverStarted = auth.authorize() && auth.execute(0, serverCommand, serverArguments);
    } else {
        serverStarted = QProcess::startDetached(serverCommand, serverArguments);
    }

    // now wait for the socket to arrive
    QTcpSocket s;
    while (serverStarting && serverStarted) {
        if (FSEngineClientHandler::instance().connect(&s))
            serverStarting = false;
    }
    serverStarting = false;
}

/*!
    Stops the server if it was started before.
*/
void FSEngineClientHandler::Private::maybeStopServer()
{
    if (!serverStarted)
        return;

    const QMutexLocker ml(&mutex);
    if (!serverStarted)
        return;

    QTcpSocket s;
    if (FSEngineClientHandler::instance().connect(&s)) {
        QDataStream stream(&s);
        stream.setVersion(QDataStream::Qt_4_2);
        stream << QString::fromLatin1("authorize");
        stream << key;
        stream << QString::fromLatin1("shutdown");
        s.flush();
    }
    serverStarted = false;
}

#include "fsengineclient.moc"
