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

#include "remoteobject.h"

#include "protocol.h"
#include "remoteclient.h"
#include "localsocket.h"

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
            writeData(QLatin1String(Protocol::Destroy), m_type, dummy, dummy);
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

    m_socket = new LocalSocket;
    m_socket->connectToServer(RemoteClient::instance().socketName());

    if (m_socket->waitForConnected()) {
        bool authorized = callRemoteMethod<bool>(QString::fromLatin1(Protocol::Authorize),
                                                 RemoteClient::instance().authorizationKey());
        if (authorized)
            return true;
    }
    delete m_socket;
    m_socket = 0;
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

void RemoteObject::callRemoteMethod(const QString &name)
{
    writeData(name, dummy, dummy, dummy);
}

} // namespace QInstaller
