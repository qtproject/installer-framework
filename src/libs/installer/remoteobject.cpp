/**************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
**
** $QT_END_LICENSE$
**
**************************************************************************/

#include "remoteobject.h"

#include "protocol.h"
#include "remoteclient.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QThread>

namespace QInstaller {

RemoteObject::RemoteObject(const QString &wrappedType, QObject *parent)
    : QObject(parent)
    , dummy(0)
    , m_type(wrappedType)
    , m_socket(0)
{
    Q_ASSERT_X(!m_type.isEmpty(), Q_FUNC_INFO, "The wrapped Qt type needs to be passed as "
        "argument and cannot be empty.");
}

RemoteObject::~RemoteObject()
{
    if (m_socket) {
        if (QThread::currentThread() == m_socket->thread()) {
            m_stream << QString::fromLatin1(Protocol::Destroy) << m_type;
            m_socket->waitForBytesWritten(-1);
        } else {
            Q_ASSERT_X(false, Q_FUNC_INFO, "Socket running in a different Thread than this object.");
        }
        m_socket->deleteLater();
        m_socket = 0;
    }
}

bool RemoteObject::authorize()
{
    if (m_socket && (m_socket->state() == QAbstractSocket::ConnectedState))
        return true;

    if (m_socket)
        delete m_socket;

    QScopedPointer<QTcpSocket> socket(new QTcpSocket);
    socket->connectToHost(RemoteClient::instance().address(), RemoteClient::instance().port());

    QElapsedTimer stopWatch;
    stopWatch.start();
    while ((socket->state() == QAbstractSocket::ConnectingState)
        && (stopWatch.elapsed() < 30000)) {
        if ((stopWatch.elapsed() % 2500) == 0)
            QCoreApplication::processEvents();
    }

    if (socket->state() == QAbstractSocket::ConnectedState) {
        QDataStream stream;
        stream.setDevice(socket.data());
        stream << QString::fromLatin1(Protocol::Authorize) << RemoteClient::instance()
            .authorizationKey();

        socket->waitForBytesWritten(-1);
        if (!socket->bytesAvailable())
            socket->waitForReadyRead(-1);

        quint32 size; stream >> size;
        bool authorized = false;
        stream >> authorized;
        if (authorized) {
            m_socket = socket.take();
            m_stream.setDevice(m_socket);
            return true;
        }
    }
    return false;
}

bool RemoteObject::connectToServer(const QVariantList &arguments)
{
    if (!RemoteClient::instance().isActive())
        return false;

     if (m_socket && (m_socket->state() == QAbstractSocket::ConnectedState))
         return true;

    if (!authorize())
        return false;

    m_stream << QString::fromLatin1(Protocol::Create) << m_type;
    foreach (const QVariant &arg, arguments)
        m_stream << arg;
    m_socket->waitForBytesWritten(-1);

    return true;
}

bool RemoteObject::isConnectedToServer() const
{
    if ((!m_socket) || (!RemoteClient::instance().isActive()))
        return false;
    if (m_socket && (m_socket->state() == QAbstractSocket::ConnectedState))
        return true;
    return false;
}

void RemoteObject::callRemoteMethod(const QString &name)
{
    writeData(name, dummy, dummy, dummy);
}

} // namespace QInstaller
