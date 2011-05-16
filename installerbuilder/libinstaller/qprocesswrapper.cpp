/**************************************************************************
**
** This file is part of Qt SDK**
**
** Copyright (c) 2010 Nokia Corporation and/or its subsidiary(-ies).*
**
** Contact:  Nokia Corporation qt-info@nokia.com**
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
#include "qprocesswrapper.h"
#include "fsengineclient.h"

#include <KDToolsCore/KDMetaMethodIterator>

#include <QtCore/QThread>

#include <QtNetwork/QLocalSocket>
#include <QtNetwork/QTcpSocket>

template<typename T>
QDataStream &operator>>(QDataStream &stream, T &state)
{
    int s;
    stream >> s;
    state = static_cast<T> (s);
    return stream;
}

void callRemoteVoidMethod(QDataStream &stream, const QString &name, const QString &foo = QString())
{
    Q_UNUSED(foo)
    stream.device()->readAll();
    stream << name;
    stream.device()->waitForBytesWritten(-1);
    if (!stream.device()->bytesAvailable())
        stream.device()->waitForReadyRead(-1);
    quint32 test;
    stream >> test;
    stream.device()->readAll();
    return;
}

template<typename T>
void callRemoteVoidMethod(QDataStream & stream, const QString &name, const T &param1)
{
    stream.device()->readAll();
    stream << name;
    stream << param1;
    stream.device()->waitForBytesWritten(-1);
    if (!stream.device()->bytesAvailable())
        stream.device()->waitForReadyRead(-1);
    quint32 test;
    stream >> test;
    stream.device()->readAll();
    return;
}

template<typename T1, typename T2>
void callRemoteVoidMethod(QDataStream &stream, const QString &name, const T1 &param1, const T2 &param2)
{
    stream.device()->readAll();
    stream << name;
    stream << param1;
    stream << param2;
    stream.device()->waitForBytesWritten(-1);
    if (!stream.device()->bytesAvailable())
        stream.device()->waitForReadyRead(-1);
    quint32 test;
    stream >> test;
    stream.device()->readAll();
    return;
}

template<typename T1, typename T2, typename T3>
void callRemoteVoidMethod(QDataStream &stream, const QString &name, const T1 &param1, const T2 &param2,
    const T3 & param3)
{
    stream.device()->readAll();
    stream << name;
    stream << param1;
    stream << param2;
    stream << param3;
    stream.device()->waitForBytesWritten(-1);
    if (!stream.device()->bytesAvailable())
        stream.device()->waitForReadyRead(-1);
    quint32 test;
    stream >> test;
    stream.device()->readAll();
    return;
}

template<typename RESULT>
RESULT callRemoteMethod(QDataStream &stream, const QString &name)
{
    stream.device()->readAll();
    stream << name;
    stream.device()->waitForBytesWritten(-1);
    if (!stream.device()->bytesAvailable())
        stream.device()->waitForReadyRead(-1);
    quint32 test;
    stream >> test;
    RESULT result;
    stream >> result;
    stream.device()->readAll();
    return result;
}

template<typename RESULT, typename T>
RESULT callRemoteMethod(QDataStream &stream, const QString &name, const T &param1)
{
    stream.device()->readAll();
    stream << name;
    stream << param1;
    stream.device()->waitForBytesWritten(-1);
    if (!stream.device()->bytesAvailable())
        stream.device()->waitForReadyRead(-1);
    quint32 test;
    stream >> test;
    RESULT result;
    stream >> result;
    stream.device()->readAll();
    return result;
}

template<typename RESULT, typename T1, typename T2>
RESULT callRemoteMethod(QDataStream &stream, const QString &name, const T1 & param1, const T2 &param2)
{
    stream.device()->readAll();
    stream << name;
    stream << param1;
    stream << param2;
    stream.device()->waitForBytesWritten(-1);
    if (!stream.device()->bytesAvailable())
        stream.device()->waitForReadyRead(-1);
    quint32 test;
    stream >> test;
    RESULT result;
    stream >> result;
    stream.device()->readAll();
    return result;
}

template<typename RESULT, typename T1, typename T2, typename T3>
RESULT callRemoteMethod(QDataStream &stream, const QString &name, const T1 &param1, const T2 & param2,
    const T3 &param3)
{
    stream.device()->readAll();
    stream << name;
    stream << param1;
    stream << param2;
    stream << param3;
    stream.device()->waitForBytesWritten(-1);
    if (!stream.device()->bytesAvailable())
        stream.device()->waitForReadyRead(-1);
    quint32 test;
    stream >> test;
    RESULT result;
    stream >> result;
    stream.device()->readAll();
    return result;
}


// -- QProcessWrapper::Private

class QProcessWrapper::Private
{
public:
    Private(QProcessWrapper *qq)
        : q(qq),
          ignoreTimer(false),
          socket(0) { }

    bool createSocket()
    {
        if (!FSEngineClientHandler::instance()->isActive())
            return false;
        if (socket != 0 && socket->state() == static_cast< int >(QLocalSocket::ConnectedState))
            return true;
        if (socket != 0)
            delete socket;
#ifdef FSENGINE_TCP
        socket = new QTcpSocket;
#else
        socket = new QLocalSocket;
#endif
        if (!FSEngineClientHandler::instance()->connect(socket))
            return false;
        stream.setDevice(socket);
        stream.setVersion(QDataStream::Qt_4_2);

        stream << QString::fromLatin1("createQProcess");
        socket->flush();
        stream.device()->waitForReadyRead(-1);
        quint32 test;
        stream >> test;
        stream.device()->readAll();

        q->startTimer(250);

        return true;
    }

    class TimerBlocker
    {
    public:
        explicit TimerBlocker(const QProcessWrapper *wrapper)
            : w(const_cast< QProcessWrapper*> (wrapper))
        {
            w->d->ignoreTimer = true;
        }

        ~TimerBlocker()
        {
            w->d->ignoreTimer = false;
        }

    private:
        QProcessWrapper *const w;
    };

private:
    QProcessWrapper *const q;

public:
    bool ignoreTimer;

    QProcess process;
#ifdef FSENGINE_TCP
    mutable QTcpSocket *socket;
#else
    mutable QLocalSocket *socket;
#endif
    mutable QDataStream stream;
};


// -- QProcessWrapper

QProcessWrapper::QProcessWrapper(QObject *parent)
    : QObject(parent),
      d(new Private(this))
{
    KDMetaMethodIterator it(QProcess::staticMetaObject, KDMetaMethodIterator::Signal,
        KDMetaMethodIterator::IgnoreQObjectMethods);
    while (it.hasNext()) {
        it.next();
        connect(&d->process, it.connectableSignature(), this, it.connectableSignature());
    }
}

QProcessWrapper::~QProcessWrapper()
{
    if (d->socket != 0) {
        d->stream << QString::fromLatin1("destroyQProcess");
        d->socket->flush();
        quint32 result;
        d->stream >> result;

        if (QThread::currentThread() == d->socket->thread()) {
            d->socket->close();
            delete d->socket;
        } else {
            d->socket->deleteLater();
        }
    }
    delete d;
}

void QProcessWrapper::timerEvent(QTimerEvent *event)
{
    Q_UNUSED(event)

    if (d->ignoreTimer)
        return;

    QList<QVariant> receivedSignals;
    {
        const Private::TimerBlocker blocker(this);

        d->stream << QString::fromLatin1("getQProcessSignals");
        d->socket->flush();
        d->stream.device()->waitForReadyRead(-1);
        quint32 test;
        d->stream >> test;
        d->stream >> receivedSignals;
        d->stream.device()->readAll();
    }

    while (!receivedSignals.isEmpty()) {
        const QString name = receivedSignals.takeFirst().toString();
        if (name == QLatin1String("started")) {
            emit started();
        } else if (name == QLatin1String("readyRead")) {
            emit readyRead();
        } else if (name == QLatin1String("stateChanged")) {
            const QProcess::ProcessState newState =
                static_cast<QProcess::ProcessState> (receivedSignals.takeFirst().toInt());
            emit stateChanged(newState);
        } else if (name == QLatin1String("finished")) {
            const int exitCode = receivedSignals.takeFirst().toInt();
            const QProcess::ExitStatus exitStatus =
                static_cast<QProcess::ExitStatus> (receivedSignals.takeFirst().toInt());
            emit finished(exitCode);
            emit finished(exitCode, exitStatus);
        }
    }
}

bool startDetached(const QString &program, const QStringList &args, const QString &workingDirectory,
    qint64 *pid);

bool QProcessWrapper::startDetached(const QString &program, const QStringList &arguments,
    const QString &workingDirectory, qint64 *pid)
{
    QProcessWrapper w;
    if (w.d->createSocket()) {
        const QPair<bool, qint64> result = callRemoteMethod<QPair<bool, qint64> >(w.d->stream,
            QLatin1String("QProcess::startDetached"), program, arguments, workingDirectory);
        if (pid != 0)
            *pid = result.second;
        return result.first;
    }
    return ::startDetached(program, arguments, workingDirectory, pid);
}

bool QProcessWrapper::startDetached(const QString &program, const QStringList &arguments)
{
    return startDetached(program, arguments, QDir::currentPath());
}

bool QProcessWrapper::startDetached(const QString &program)
{
    return startDetached(program, QStringList());
}

void QProcessWrapper::setProcessChannelMode(QProcessWrapper::ProcessChannelMode mode)
{
    const Private::TimerBlocker blocker(this);
    if (d->createSocket()) {
        callRemoteVoidMethod(d->stream, QLatin1String("QProcess::setProcessChannelMode"),
            static_cast<QProcess::ProcessChannelMode>(mode));
    } else {
        d->process.setProcessChannelMode(static_cast<QProcess::ProcessChannelMode>(mode));
    }
}

/*!
 Cancels the process. This methods tries to terminate the process
 gracefully by calling QProcess::terminate. After 10 seconds, the process gets killed.
 */
void QProcessWrapper::cancel()
{
    if (state() == QProcessWrapper::Running)
        terminate();

    if (!waitForFinished(10000))
        kill();
}

void QProcessWrapper::setReadChannel(QProcessWrapper::ProcessChannel chan)
{
    const Private::TimerBlocker blocker(this);
    if (d->createSocket()) {
        callRemoteVoidMethod(d->stream, QLatin1String("QProcess::setReadChannel"),
            static_cast<QProcess::ProcessChannel>(chan));
    } else {
        d->process.setReadChannel(static_cast<QProcess::ProcessChannel>(chan));
    }
}

bool QProcessWrapper::waitForFinished(int msecs)
{
    const Private::TimerBlocker blocker(this);
    if (d->createSocket())
        return callRemoteMethod< bool >(d->stream, QLatin1String("QProcess::waitForFinished"), msecs);
    else
        return d->process.waitForFinished(msecs);
}

bool QProcessWrapper::waitForStarted(int msecs)
{
    const Private::TimerBlocker blocker(this);
    if (d->createSocket())
        return callRemoteMethod< bool >(d->stream, QLatin1String("QProcess::waitForStarted"), msecs);
    else
        return d->process.waitForStarted(msecs);
}

qint64 QProcessWrapper::write(const QByteArray &data)
{
    const Private::TimerBlocker blocker(this);
    if (d->createSocket())
        return callRemoteMethod< qint64 >(d->stream, QLatin1String("QProcess::write"), data);
    else
        return d->process.write(data);
}

#undef RETURN_NO_ARGS_CONST
#define RETURN_NO_ARGS_CONST(RESULT, NAME) \
RESULT QProcessWrapper::NAME() const       \
{ \
    const Private::TimerBlocker blocker(this);\
    if (d->createSocket()) \
        return callRemoteMethod< RESULT >(d->stream, QLatin1String("QProcess::"#NAME)); \
    else \
        return static_cast< RESULT >(d->process.NAME()); \
}

#define RETURN_NO_ARGS(RESULT, NAME) \
RESULT QProcessWrapper::NAME()       \
{ \
    const Private::TimerBlocker blocker(this);\
    if (d->createSocket()) \
        return callRemoteMethod< RESULT >(d->stream, QLatin1String("QProcess::"#NAME)); \
    else \
        return d->process.NAME(); \
}

#undef RETURN_ONE_ARG_CONST
#define RETURN_ONE_ARG_CONST(RESULT, NAME, TYPE1) \
RESULT QProcessWrapper::NAME(TYPE1 param1) const \
{ \
    const Private::TimerBlocker blocker(this);\
    if (d->createSocket()) \
        return callRemoteMethod< RESULT >(d->stream, QLatin1String("QProcess::"#NAME), param1); \
    else \
        return d->process.NAME(param1); \
}

#undef RETURN_TWO_ARGS_CONST
#define RETURN_TWO_ARGS_CONST(RESULT, NAME, TYPE1, TYPE2) \
RESULT QProcessWrapper::NAME(TYPE1 param1, TYPE2 param2) const \
{ \
    const Private::TimerBlocker blocker(this);\
    if (d->createSocket()) \
        return callRemoteMethod< RESULT >(d->stream, QLatin1String("QProcess::"#NAME), param1, param2); \
    else \
        return d->process.NAME(param1, param2); \
}

#undef VOID_NO_ARGS
#define VOID_NO_ARGS(NAME) \
void QProcessWrapper::NAME() \
{ \
    qDebug() << Q_FUNC_INFO; \
    const Private::TimerBlocker blocker(this);\
    if (d->createSocket()) \
        callRemoteVoidMethod(d->stream, QLatin1String("QProcess::"#NAME)); \
    else \
        d->process.NAME(); \
}

#undef VOID_ONE_ARG
#define VOID_ONE_ARG(NAME, TYPE1) \
void QProcessWrapper::NAME(TYPE1 param1) \
{ \
    qDebug() << Q_FUNC_INFO; \
    const Private::TimerBlocker blocker(this);\
    if (d->createSocket()) \
        callRemoteVoidMethod(d->stream, QLatin1String("QProcess::"#NAME), param1); \
    else \
        d->process.NAME(param1); \
}

#undef VOID_TWO_ARGS
#define VOID_TWO_ARGS(NAME, TYPE1, TYPE2) \
void QProcessWrapper::NAME(TYPE1 param1, TYPE2 param2) \
{ \
    qDebug() << Q_FUNC_INFO; \
    const Private::TimerBlocker blocker(this);\
    if (d->createSocket()) \
        callRemoteVoidMethod(d->stream, QLatin1String("QProcess::"#NAME), param1, param2); \
    else \
        d->process.NAME(param1, param2); \
}

#define VOID_THREE_ARGS(NAME, TYPE1, TYPE2, TYPE3) \
void QProcessWrapper::NAME(TYPE1 param1, TYPE2 param2, TYPE3 param3) \
{ \
    qDebug() << Q_FUNC_INFO; \
    const Private::TimerBlocker blocker(this);\
    if (d->createSocket()) \
        callRemoteVoidMethod(d->stream, QLatin1String("QProcess::"#NAME), param1, param2, param3); \
    else \
        d->process.NAME(param1, param2, param3); \
}

VOID_NO_ARGS(closeWriteChannel);
RETURN_NO_ARGS_CONST(int, exitCode);
RETURN_NO_ARGS_CONST(QProcessWrapper::ExitStatus, exitStatus);
VOID_NO_ARGS(kill)
RETURN_NO_ARGS(QByteArray, readAll);
RETURN_NO_ARGS(QByteArray, readAllStandardOutput);
VOID_THREE_ARGS(start, const QString&, const QStringList&, QIODevice::OpenMode)
VOID_ONE_ARG(start, const QString&)

RETURN_NO_ARGS_CONST(QProcessWrapper::ProcessState, state)
VOID_NO_ARGS(terminate)
RETURN_NO_ARGS_CONST(QProcessWrapper::ProcessChannel, readChannel)
RETURN_NO_ARGS_CONST(QProcessWrapper::ProcessChannelMode, processChannelMode)
RETURN_NO_ARGS_CONST(QString, workingDirectory)
VOID_ONE_ARG(setEnvironment, const QStringList&)
#ifdef Q_OS_WIN
VOID_ONE_ARG(setNativeArguments, const QString&)
#endif
VOID_ONE_ARG(setWorkingDirectory, const QString&)
