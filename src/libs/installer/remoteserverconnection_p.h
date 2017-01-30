/**************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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

#ifndef REMOTESERVERCONNECTION_P_H
#define REMOTESERVERCONNECTION_P_H

#include "protocol.h"

#include <QMutex>
#include <QProcess>
#include <QVariant>

namespace QInstaller {

class QProcessSignalReceiver : public QObject
{
    Q_OBJECT
    friend class RemoteServerConnection;

private:
    explicit QProcessSignalReceiver(QProcess *process)
        : QObject(process)
    {
        connect(process, &QIODevice::bytesWritten, this, &QProcessSignalReceiver::onBytesWritten);
        connect(process, &QIODevice::aboutToClose, this, &QProcessSignalReceiver::onAboutToClose);
        connect(process, &QIODevice::readChannelFinished, this, &QProcessSignalReceiver::onReadChannelFinished);
        connect(process, SIGNAL(error(QProcess::ProcessError)),
            SLOT(onError(QProcess::ProcessError)));
        connect(process, &QProcess::readyReadStandardOutput,
                this, &QProcessSignalReceiver::onReadyReadStandardOutput);
        connect(process, &QProcess::readyReadStandardError,
                this, &QProcessSignalReceiver::onReadyReadStandardError);
        connect(process, SIGNAL(finished(int,QProcess::ExitStatus)),
                SLOT(onFinished(int,QProcess::ExitStatus)));
        connect(process, &QIODevice::readyRead, this, &QProcessSignalReceiver::onReadyRead);
        connect(process, &QProcess::started, this, &QProcessSignalReceiver::onStarted);
        connect(process, &QProcess::stateChanged,
            this, &QProcessSignalReceiver::onStateChanged);
    }

private Q_SLOTS:
    void onBytesWritten(qint64 count) {
        QMutexLocker _(&m_lock);
        m_receivedSignals.append(QLatin1String(Protocol::QProcessSignalBytesWritten));
        m_receivedSignals.append(count);
    }

    void onAboutToClose() {
        QMutexLocker _(&m_lock);
        m_receivedSignals.append(QLatin1String(Protocol::QProcessSignalAboutToClose));
    }

    void onReadChannelFinished() {
        QMutexLocker _(&m_lock);
        m_receivedSignals.append(QLatin1String(Protocol::QProcessSignalReadChannelFinished));
    }

    void onError(QProcess::ProcessError error) {
        QMutexLocker _(&m_lock);
        m_receivedSignals.append(QLatin1String(Protocol::QProcessSignalError));
        m_receivedSignals.append(static_cast<int> (error));
    }

    void onReadyReadStandardOutput() {
        QMutexLocker _(&m_lock);
        m_receivedSignals.append(QLatin1String(Protocol::QProcessSignalReadyReadStandardOutput));
    }

    void onReadyReadStandardError() {
        QMutexLocker _(&m_lock);
        m_receivedSignals.append(QLatin1String(Protocol::QProcessSignalReadyReadStandardError));
    }

    void onFinished(int exitCode, QProcess::ExitStatus exitStatus) {
        QMutexLocker _(&m_lock);
        m_receivedSignals.append(QLatin1String(Protocol::QProcessSignalFinished));
        m_receivedSignals.append(exitCode);
        m_receivedSignals.append(static_cast<int> (exitStatus));
    }

    void onReadyRead() {
        QMutexLocker _(&m_lock);
        m_receivedSignals.append(QLatin1String(Protocol::QProcessSignalReadyRead));
    }

    void onStarted() {
        QMutexLocker _(&m_lock);
        m_receivedSignals.append(QLatin1String(Protocol::QProcessSignalStarted));
    }

    void onStateChanged(QProcess::ProcessState newState) {
        QMutexLocker _(&m_lock);
        m_receivedSignals.append(QLatin1String(Protocol::QProcessSignalStateChanged));
        m_receivedSignals.append(static_cast<int>(newState));
    }

private:
    QMutex m_lock;
    QVariantList m_receivedSignals;
};

} // namespace QInstaller

#endif // REMOTESERVERCONNECTION_P_H
