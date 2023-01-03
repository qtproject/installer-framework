/**************************************************************************
**
** Copyright (C) 2023 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
**************************************************************************/

#include "qprocesswrapper.h"

#include "protocol.h"
#include "utils.h"

#include <QDir>
#include <QVariant>

namespace QInstaller {

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::QProcessWrapper
    \internal
*/

QProcessWrapper::QProcessWrapper(QObject *parent)
    : RemoteObject(QLatin1String(Protocol::QProcess), parent)
{
    qRegisterMetaType<QProcess::ExitStatus>();
    qRegisterMetaType<QProcess::ProcessError>();
    qRegisterMetaType<QProcess::ProcessState>();

    m_timer.start(250);
    connect(&m_timer, &QTimer::timeout, this, &QProcessWrapper::processSignals);
    connect(&process, &QIODevice::bytesWritten, this, &QProcessWrapper::bytesWritten);
    connect(&process, &QIODevice::aboutToClose, this, &QProcessWrapper::aboutToClose);
    connect(&process, &QIODevice::readChannelFinished, this, &QProcessWrapper::readChannelFinished);
    connect(&process, SIGNAL(errorOccurred(QProcess::ProcessError)), SIGNAL(errorOccurred(QProcess::ProcessError)));
    connect(&process, &QProcess::readyReadStandardOutput, this, &QProcessWrapper::readyReadStandardOutput);
    connect(&process, &QProcess::readyReadStandardError, this, &QProcessWrapper::readyReadStandardError);
    connect(&process, SIGNAL(finished(int,QProcess::ExitStatus)), SIGNAL(finished(int,QProcess::ExitStatus)));
    connect(&process, &QIODevice::readyRead, this, &QProcessWrapper::readyRead);
    connect(&process, &QProcess::started, this, &QProcessWrapper::started);
    connect(&process, &QProcess::stateChanged, this, &QProcessWrapper::stateChanged);
}

QProcessWrapper::~QProcessWrapper()
{
    m_timer.stop();
}

void QProcessWrapper::processSignals()
{
    if (!isConnectedToServer())
        return;

    if (!m_lock.tryLockForRead())
        return;

    QList<QVariant> receivedSignals =
        callRemoteMethod<QList<QVariant> >(QString::fromLatin1(Protocol::GetQProcessSignals));

    while (!receivedSignals.isEmpty()) {
        const QString name = receivedSignals.takeFirst().toString();
        if (name == QLatin1String(Protocol::QProcessSignalBytesWritten)) {
            emit bytesWritten(receivedSignals.takeFirst().value<qint64>());
        } else if (name == QLatin1String(Protocol::QProcessSignalAboutToClose)) {
            emit aboutToClose();
        } else if (name == QLatin1String(Protocol::QProcessSignalReadChannelFinished)) {
            emit readChannelFinished();
        } else if (name == QLatin1String(Protocol::QProcessSignalError)) {
            emit errorOccurred(static_cast<QProcess::ProcessError> (receivedSignals.takeFirst().toInt()));
        } else if (name == QLatin1String(Protocol::QProcessSignalReadyReadStandardOutput)) {
            emit readyReadStandardOutput();
        } else if (name == QLatin1String(Protocol::QProcessSignalReadyReadStandardError)) {
            emit readyReadStandardError();
        } else if (name == QLatin1String(Protocol::QProcessSignalStarted)) {
            emit started();
        } else if (name == QLatin1String(Protocol::QProcessSignalReadyRead)) {
            emit readyRead();
        } else if (name == QLatin1String(Protocol::QProcessSignalStateChanged)) {
            emit stateChanged(static_cast<QProcess::ProcessState> (receivedSignals.takeFirst()
                .toInt()));
        } else if (name == QLatin1String(Protocol::QProcessSignalFinished)) {
            emit finished(receivedSignals.takeFirst().toInt(),
                static_cast<QProcess::ExitStatus> (receivedSignals.takeFirst().toInt()));
        }
    }
    m_lock.unlock();
}

/*!
    Starts the \a program with \a arguments in the working directory \a workingDirectory as a detached
    process. The process id can be retrieved with the \a pid parameter. Compared to the QProcess
    implementation of the same method this does not show a window for the started process on Windows.
*/
bool QProcessWrapper::startDetached(const QString &program, const QStringList &arguments,
    const QString &workingDirectory, qint64 *pid)
{
    QProcessWrapper w;
    if (w.connectToServer()) {
        const QPair<bool, qint64> result =
            w.callRemoteMethod<QPair<bool, qint64> >(QLatin1String(Protocol::QProcessStartDetached),
                program, arguments, workingDirectory);
        if (pid != nullptr)
            *pid = result.second;
        w.processSignals();
        return result.first;
    }
    return QInstaller::startDetached(program, arguments, workingDirectory, pid);
}

/*!
    Starts the \a program with \a arguments as a detached process. Compared to the QProcess
    implementation of the same method this does not show a window for the started process on Windows.
*/
bool QProcessWrapper::startDetached(const QString &program, const QStringList &arguments)
{
    return startDetached(program, arguments, QDir::currentPath());
}

/*!
    Starts the \a program as a detached process. Compared to the QProcess implementation of the same
    method this does not show a window for the started process on Windows.
*/
bool QProcessWrapper::startDetached(const QString &program)
{
    return startDetached(program, QStringList());
}

/*!
    Starts the \a program as a detached process. The variants of the function suffixed with \c 2
    use the base \c QProcess::startDetached implementation internally to start the process.
*/
bool QProcessWrapper::startDetached2(const QString &program)
{
    return startDetached2(program, QStringList());
}

/*!
    Starts the \a program with \a arguments as a detached process. The variants of the function
    suffixed with \c 2 use the base \c QProcess::startDetached implementation internally to
    start the process.
*/
bool QProcessWrapper::startDetached2(const QString &program, const QStringList &arguments)
{
    return startDetached2(program, arguments, QDir::currentPath());
}

/*!
    Starts the \a program with \a arguments in the working directory \a workingDirectory as a detached
    process. The process id can be retrieved with the \a pid parameter. The variants
    of the function suffixed with \c 2 use the base \c QProcess::startDetached implementation
    internally to start the process.
*/
bool QProcessWrapper::startDetached2(const QString &program, const QStringList &arguments, const QString &workingDirectory, qint64 *pid)
{
    QProcessWrapper w;
    if (w.connectToServer()) {
        const QPair<bool, qint64> result =
            w.callRemoteMethod<QPair<bool, qint64> >(QLatin1String(Protocol::QProcessStartDetached2),
                program, arguments, workingDirectory);
        if (pid != nullptr)
            *pid = result.second;
        w.processSignals();
        return result.first;
    }
    return QProcess::startDetached(program, arguments, workingDirectory, pid);
}

void QProcessWrapper::setProcessChannelMode(QProcessWrapper::ProcessChannelMode mode)
{
    if (connectToServer()) {
        m_lock.lockForWrite();
        callRemoteMethodDefaultReply(QLatin1String(Protocol::QProcessSetProcessChannelMode),
            static_cast<QProcess::ProcessChannelMode>(mode));
        m_lock.unlock();
    } else {
        process.setProcessChannelMode(static_cast<QProcess::ProcessChannelMode>(mode));
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
    if (connectToServer()) {
        m_lock.lockForWrite();
        callRemoteMethodDefaultReply(QLatin1String(Protocol::QProcessSetReadChannel),
            static_cast<QProcess::ProcessChannel>(chan));
        m_lock.unlock();
    } else {
        process.setReadChannel(static_cast<QProcess::ProcessChannel>(chan));
    }
}

bool QProcessWrapper::waitForFinished(int msecs)
{
    if (connectToServer()) {
        m_lock.lockForWrite();
        const bool value = callRemoteMethod<bool>(QLatin1String(Protocol::QProcessWaitForFinished),
            qint32(msecs));
        m_lock.unlock();
        return value;
    }
    return process.waitForFinished(msecs);
}

bool QProcessWrapper::waitForStarted(int msecs)
{
    if (connectToServer()) {
        m_lock.lockForWrite();
        const bool value = callRemoteMethod<bool>(QLatin1String(Protocol::QProcessWaitForStarted),
            qint32(msecs));
        m_lock.unlock();
        return value;
    }
    return process.waitForStarted(msecs);
}

qint64 QProcessWrapper::write(const QByteArray &data)
{
    if (connectToServer()) {
        m_lock.lockForWrite();
        const qint64 value = callRemoteMethod<qint64>(QLatin1String(Protocol::QProcessWrite), data);
        m_lock.unlock();
        return value;
    }
    return process.write(data);
}

void QProcessWrapper::closeWriteChannel()
{
    if (connectToServer()) {
        m_lock.lockForWrite();
        callRemoteMethodDefaultReply(QLatin1String(Protocol::QProcessCloseWriteChannel));
        m_lock.unlock();
    } else {
        process.closeWriteChannel();
    }
}

int QProcessWrapper::exitCode() const
{
    if ((const_cast<QProcessWrapper *>(this))->connectToServer()) {
        m_lock.lockForWrite();
        const int value = callRemoteMethod<qint32>(QLatin1String(Protocol::QProcessExitCode));
        m_lock.unlock();
        return value;
    }
    return process.exitCode();
}

QProcessWrapper::ExitStatus QProcessWrapper::exitStatus() const
{
    if ((const_cast<QProcessWrapper *>(this))->connectToServer()) {
        m_lock.lockForWrite();
        const int status = callRemoteMethod<qint32>(QLatin1String(Protocol::QProcessExitStatus));
        m_lock.unlock();
        return static_cast<QProcessWrapper::ExitStatus>(status);
    }
    return static_cast<QProcessWrapper::ExitStatus>(process.exitStatus());
}

void QProcessWrapper::kill()
{
    if (connectToServer()) {
        m_lock.lockForWrite();
        callRemoteMethodDefaultReply(QLatin1String(Protocol::QProcessKill));
        m_lock.unlock();
    } else {
        process.kill();
    }
}

QByteArray QProcessWrapper::readAll()
{
    if (connectToServer()) {
        m_lock.lockForWrite();
        const QByteArray ba = callRemoteMethod<QByteArray>(QLatin1String(Protocol::QProcessReadAll));
        m_lock.unlock();
        return ba;
    }
    return process.readAll();
}

QByteArray QProcessWrapper::readAllStandardOutput()
{
    if (connectToServer()) {
        m_lock.lockForWrite();
        const QByteArray ba =
            callRemoteMethod<QByteArray>(QLatin1String(Protocol::QProcessReadAllStandardOutput));
        m_lock.unlock();
        return ba;
    }
    return process.readAllStandardOutput();
}

QByteArray QProcessWrapper::readAllStandardError()
{
    if (connectToServer()) {
        m_lock.lockForWrite();
        const QByteArray ba =
            callRemoteMethod<QByteArray>(QLatin1String(Protocol::QProcessReadAllStandardError));
        m_lock.unlock();
        return ba;
    }
    return process.readAllStandardError();
}

void QProcessWrapper::start(const QString &param1, const QStringList &param2,
    QIODevice::OpenMode param3)
{
    if (connectToServer()) {
        m_lock.lockForWrite();
        callRemoteMethodDefaultReply(QLatin1String(Protocol::QProcessStart3Arg), param1, param2, param3);
        m_lock.unlock();
    } else {
        process.start(param1, param2, param3);
    }
}

void QProcessWrapper::start(const QString &param1, QIODevice::OpenMode param2)
{
    if (connectToServer()) {
        m_lock.lockForWrite();
        callRemoteMethodDefaultReply(QLatin1String(Protocol::QProcessStart2Arg), param1, param2);
        m_lock.unlock();
    } else {
        process.start(param1, {}, param2);
    }
}

QProcessWrapper::ProcessState QProcessWrapper::state() const
{
    if ((const_cast<QProcessWrapper *>(this))->connectToServer()) {
        m_lock.lockForWrite();
        const int state = callRemoteMethod<qint32>(QLatin1String(Protocol::QProcessState));
        m_lock.unlock();
        return static_cast<QProcessWrapper::ProcessState>(state);
    }
    return static_cast<QProcessWrapper::ProcessState>(process.state());
}

void QProcessWrapper::terminate()
{
    if (connectToServer()) {
        m_lock.lockForWrite();
        callRemoteMethodDefaultReply(QLatin1String(Protocol::QProcessTerminate));
        m_lock.unlock();
    } else {
        process.terminate();
    }
}

QProcessWrapper::ProcessChannel QProcessWrapper::readChannel() const
{
    if ((const_cast<QProcessWrapper *>(this))->connectToServer()) {
        m_lock.lockForWrite();
        const int channel = callRemoteMethod<qint32>(QLatin1String(Protocol::QProcessReadChannel));
        m_lock.unlock();
        return static_cast<QProcessWrapper::ProcessChannel>(channel);
    }
    return static_cast<QProcessWrapper::ProcessChannel>(process.readChannel());
}

QProcessWrapper::ProcessChannelMode QProcessWrapper::processChannelMode() const
{
    if ((const_cast<QProcessWrapper *>(this))->connectToServer()) {
        m_lock.lockForWrite();
        const int mode = callRemoteMethod<qint32>(QLatin1String(Protocol::QProcessProcessChannelMode));
        m_lock.unlock();
        return static_cast<QProcessWrapper::ProcessChannelMode>(mode);
    }
    return static_cast<QProcessWrapper::ProcessChannelMode>(process.processChannelMode());
}

QString QProcessWrapper::workingDirectory() const
{
    if ((const_cast<QProcessWrapper *>(this))->connectToServer()) {
        m_lock.lockForWrite();
        const QString dir = callRemoteMethod<QString>(QLatin1String(Protocol::QProcessWorkingDirectory));
        m_lock.unlock();
        return dir;
    }
    return static_cast<QString>(process.workingDirectory());
}

QString QProcessWrapper::errorString() const
{
    if ((const_cast<QProcessWrapper *>(this))->connectToServer()) {
        m_lock.lockForWrite();
        const QString error = callRemoteMethod<QString>(QLatin1String(Protocol::QProcessErrorString));
        m_lock.unlock();
        return error;
    }
    return static_cast<QString>(process.errorString());
}

QStringList QProcessWrapper::environment() const
{
    if ((const_cast<QProcessWrapper *>(this))->connectToServer()) {
        m_lock.lockForWrite();
        const QStringList env =
            callRemoteMethod<QStringList>(QLatin1String(Protocol::QProcessEnvironment));
        m_lock.unlock();
        return env;
    }
    return process.environment();
}

void QProcessWrapper::setEnvironment(const QStringList &param1)
{
    if (connectToServer()) {
        m_lock.lockForWrite();
        callRemoteMethodDefaultReply(QLatin1String(Protocol::QProcessSetEnvironment), param1);
        m_lock.unlock();
    } else {
        process.setEnvironment(param1);
    }
}

#ifdef Q_OS_WIN
void QProcessWrapper::setNativeArguments(const QString &param1)
{
    if (connectToServer()) {
        m_lock.lockForWrite();
        callRemoteMethodDefaultReply(QLatin1String(Protocol::QProcessSetNativeArguments), param1);
        m_lock.unlock();
    } else {
        process.setNativeArguments(param1);
    }
}
#endif

void QProcessWrapper::setWorkingDirectory(const QString &param1)
{
    if (connectToServer()) {
        m_lock.lockForWrite();
        callRemoteMethodDefaultReply(QLatin1String(Protocol::QProcessSetWorkingDirectory), param1);
        m_lock.unlock();
    } else {
        process.setWorkingDirectory(param1);
    }
}

} // namespace QInstaller
