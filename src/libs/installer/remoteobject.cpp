/**************************************************************************
**
** Copyright (C) 2022 The Qt Company Ltd.
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

#include "remoteobject.h"

#include "protocol.h"
#include "remoteclient.h"
#include "globals.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QThread>

namespace QInstaller {

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::RemoteObject
    \internal
*/

RemoteObject::RemoteObject(const QString &wrappedType, QObject *parent)
    : QObject(parent)
    , m_type(wrappedType)
    , m_socket(nullptr)
{
    Q_ASSERT_X(!m_type.isEmpty(), Q_FUNC_INFO, "The wrapped Qt type needs to be passed as "
        "argument and cannot be empty.");
}

RemoteObject::~RemoteObject()
{
    if (m_socket) {
        if (QThread::currentThread() == m_socket->thread()) {
            if ((m_type != QLatin1String("RemoteClientPrivate"))
                    && (m_socket->state() == QLocalSocket::ConnectedState)) {
                while (m_socket->bytesToWrite()) {
                    // QAbstractSocket::waitForBytesWritten() may fail randomly on Windows, use
                    // an event loop and the bytesWritten() signal instead as the docs suggest.
                    QEventLoop loop;
                    connect(m_socket, &QLocalSocket::bytesWritten, &loop, &QEventLoop::quit);
                    loop.exec();
                }
                m_socket->disconnectFromServer();
                if (!(m_socket->state() == QLocalSocket::UnconnectedState
                        || m_socket->waitForDisconnected())) {
                    qCWarning(lcServer) << "Error while disconnecting from remote server:"
                                        << m_socket->error();
                }
            }
        } else {
            Q_ASSERT_X(false, Q_FUNC_INFO, "Socket running in a different Thread than this object.");
        }
        delete m_socket;
    }
}

bool RemoteObject::authorize()
{
    if (m_socket && (m_socket->state() == QLocalSocket::ConnectedState))
        return true;

    if (m_socket)
        delete m_socket;

    m_socket = new QLocalSocket;
    m_socket->connectToServer(RemoteClient::instance().socketName());

    if (m_socket->waitForConnected()) {
        bool authorized = callRemoteMethod<bool>(QString::fromLatin1(Protocol::Authorize),
                                                 RemoteClient::instance().authorizationKey());
        if (authorized)
            return true;
    }
    delete m_socket;
    m_socket = nullptr;
    return false;
}

bool RemoteObject::connectToServer(const QVariantList &arguments)
{
    if (!RemoteClient::instance().isActive())
        return false;

     if (m_socket && (m_socket->state() == QLocalSocket::ConnectedState))
         return true;

    if (!authorize())
        return false;

    QByteArray data;
    QDataStream out(&data, QIODevice::WriteOnly);
    out << m_type;
    foreach (const QVariant &arg, arguments)
        out << arg;

    sendPacket(m_socket, Protocol::Create, data);
    m_socket->flush();
    while (m_socket->bytesToWrite())
        m_socket->waitForBytesWritten();

    const QString reply = readData<QString>(QLatin1String(Protocol::Create));
    Q_ASSERT(reply == QLatin1String(Protocol::DefaultReply));

    return true;
}

bool RemoteObject::isConnectedToServer() const
{
    if ((!m_socket) || (!RemoteClient::instance().isActive()))
        return false;
    if (m_socket && (m_socket->state() == QLocalSocket::ConnectedState))
        return true;
    return false;
}

} // namespace QInstaller
