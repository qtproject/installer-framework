/**************************************************************************
**
** This file is part of Installer Framework
**
** Copyright (c) 2010-2012 Nokia Corporation and/or its subsidiary(-ies).
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

#include "qprocesswrapper.h"

#include "fsengineclient.h"
#include "templates.cpp"

#include <QtCore/QThread>

#include <QtNetwork/QTcpSocket>

// -- QProcessWrapper::Private

class QProcessWrapper::Private
{
public:
    Private(QProcessWrapper *qq)
        : q(qq),
          ignoreTimer(false),
          socket(0)
    {}

    bool createSocket()
    {
        if (!FSEngineClientHandler::instance().isActive())
            return false;
        if (socket != 0 && socket->state() == static_cast< int >(QAbstractSocket::ConnectedState))
            return true;
        if (socket != 0)
            delete socket;
        socket = new QTcpSocket;

        if (!FSEngineClientHandler::instance().connect(socket))
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
            : w(const_cast<QProcessWrapper *>(wrapper))
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
    mutable QTcpSocket *socket;
    mutable QDataStream stream;
};


// -- QProcessWrapper

QProcessWrapper::QProcessWrapper(QObject *parent)
    : QObject(parent),
      d(new Private(this))
{
    connect(&d->process, SIGNAL(bytesWritten(qint64)), SIGNAL(bytesWritten(qint64)));
    connect(&d->process, SIGNAL(aboutToClose()), SIGNAL(aboutToClose()));
    connect(&d->process, SIGNAL(readChannelFinished()), SIGNAL(readChannelFinished()));
    connect(&d->process, SIGNAL(error(QProcess::ProcessError)), SIGNAL(error(QProcess::ProcessError)));
    connect(&d->process, SIGNAL(readyReadStandardOutput()), SIGNAL(readyReadStandardOutput()));
    connect(&d->process, SIGNAL(readyReadStandardError()), SIGNAL(readyReadStandardError()));
    connect(&d->process, SIGNAL(finished(int)), SIGNAL(finished(int)));
    connect(&d->process, SIGNAL(finished(int,QProcess::ExitStatus)), SIGNAL(finished(int,QProcess::ExitStatus)));
    connect(&d->process, SIGNAL(readyRead()), SIGNAL(readyRead()));
    connect(&d->process, SIGNAL(started()), SIGNAL(started()));
    connect(&d->process, SIGNAL(stateChanged(QProcess::ProcessState)), SIGNAL(stateChanged(QProcess::ProcessState)));
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
        return callRemoteMethod<bool>(d->stream, QLatin1String("QProcess::waitForFinished"), msecs);
    return d->process.waitForFinished(msecs);
}

bool QProcessWrapper::waitForStarted(int msecs)
{
    const Private::TimerBlocker blocker(this);
    if (d->createSocket())
        return callRemoteMethod<bool>(d->stream, QLatin1String("QProcess::waitForStarted"), msecs);
    return d->process.waitForStarted(msecs);
}

qint64 QProcessWrapper::write(const QByteArray &data)
{
    const Private::TimerBlocker blocker(this);
    if (d->createSocket())
        return callRemoteMethod<qint64>(d->stream, QLatin1String("QProcess::write"), data);
    return d->process.write(data);
}

void QProcessWrapper::closeWriteChannel()
{
    const Private::TimerBlocker blocker(this);
    if (d->createSocket())
        callRemoteVoidMethod<void>(d->stream, QLatin1String("QProcess::closeWriteChannel"));
    else
        d->process.closeWriteChannel();
}

int QProcessWrapper::exitCode() const
{
    const Private::TimerBlocker blocker(this);
    if (d->createSocket())
        return callRemoteMethod<int>(d->stream, QLatin1String("QProcess::exitCode"));
    return static_cast< int>(d->process.exitCode());
}

QProcessWrapper::ExitStatus QProcessWrapper::exitStatus() const
{
    const Private::TimerBlocker blocker(this);
    if (d->createSocket())
        return callRemoteMethod<QProcessWrapper::ExitStatus>(d->stream, QLatin1String("QProcess::exitStatus"));
    return static_cast< QProcessWrapper::ExitStatus>(d->process.exitStatus());
}

void QProcessWrapper::kill()
{
    const Private::TimerBlocker blocker(this);
    if (d->createSocket())
        callRemoteVoidMethod<void>(d->stream, QLatin1String("QProcess::kill"));
    else
        d->process.kill();
}

QByteArray QProcessWrapper::readAll()
{
    const Private::TimerBlocker blocker(this);
    if (d->createSocket())
        return callRemoteMethod<QByteArray>(d->stream, QLatin1String("QProcess::readAll"));
    return d->process.readAll();
}

QByteArray QProcessWrapper::readAllStandardOutput()
{
    const Private::TimerBlocker blocker(this);
    if (d->createSocket())
        return callRemoteMethod<QByteArray>(d->stream, QLatin1String("QProcess::readAllStandardOutput"));
    return d->process.readAllStandardOutput();
}

void QProcessWrapper::start(const QString &param1, const QStringList &param2, QIODevice::OpenMode param3)
{
    const Private::TimerBlocker blocker(this);
    if (d->createSocket())
        callRemoteVoidMethod(d->stream, QLatin1String("QProcess::start"), param1, param2, param3);
    else
        d->process.start(param1, param2, param3);
}

void QProcessWrapper::start(const QString &param1)
{
    const Private::TimerBlocker blocker(this);
    if (d->createSocket())
        callRemoteVoidMethod(d->stream, QLatin1String("QProcess::start"), param1);
    else
        d->process.start(param1);
}

QProcessWrapper::ProcessState QProcessWrapper::state() const
{
    const Private::TimerBlocker blocker(this);
    if (d->createSocket())
        return callRemoteMethod<QProcessWrapper::ProcessState>(d->stream, QLatin1String("QProcess::state"));
    return static_cast< QProcessWrapper::ProcessState>(d->process.state());
}

void QProcessWrapper::terminate()
{
    const Private::TimerBlocker blocker(this);
    if (d->createSocket())
        callRemoteVoidMethod<void>(d->stream, QLatin1String("QProcess::terminate"));
    else
        d->process.terminate();
}

QProcessWrapper::ProcessChannel QProcessWrapper::readChannel() const
{
    const Private::TimerBlocker blocker(this);
    if (d->createSocket()) {
        return callRemoteMethod<QProcessWrapper::ProcessChannel>(d->stream,
            QLatin1String("QProcess::readChannel"));
    }
    return static_cast< QProcessWrapper::ProcessChannel>(d->process.readChannel());
}

QProcessWrapper::ProcessChannelMode QProcessWrapper::processChannelMode() const
{
    const Private::TimerBlocker blocker(this);
    if (d->createSocket()) {
        return callRemoteMethod<QProcessWrapper::ProcessChannelMode>(d->stream,
            QLatin1String("QProcess::processChannelMode"));
    }
    return static_cast< QProcessWrapper::ProcessChannelMode>(d->process.processChannelMode());
}

QString QProcessWrapper::workingDirectory() const
{
    const Private::TimerBlocker blocker(this);
    if (d->createSocket())
        return callRemoteMethod<QString>(d->stream, QLatin1String("QProcess::workingDirectory"));
    return static_cast< QString>(d->process.workingDirectory());
}

void QProcessWrapper::setEnvironment(const QStringList &param1)
{
    const Private::TimerBlocker blocker(this);
    if (d->createSocket())
        callRemoteVoidMethod(d->stream, QLatin1String("QProcess::setEnvironment"), param1);
    else
        d->process.setEnvironment(param1);
}

#ifdef Q_OS_WIN
void QProcessWrapper::setNativeArguments(const QString &param1)
{
    const Private::TimerBlocker blocker(this);
    if (d->createSocket())
        callRemoteVoidMethod(d->stream, QLatin1String("QProcess::setNativeArguments"), param1);
    else
        d->process.setNativeArguments(param1);
}
#endif

void QProcessWrapper::setWorkingDirectory(const QString &param1)
{
    const Private::TimerBlocker blocker(this);
    if (d->createSocket())
        callRemoteVoidMethod(d->stream, QLatin1String("QProcess::setWorkingDirectory"), param1);
    else
        d->process.setWorkingDirectory(param1);
}
