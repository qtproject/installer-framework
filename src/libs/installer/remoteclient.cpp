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
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
**************************************************************************/

#include "remoteclient.h"

#include "protocol.h"
#include "remoteclient_p.h"

#include <QElapsedTimer>
#include <QUuid>

namespace QInstaller {

RemoteClient::RemoteClient()
    : d_ptr(new RemoteClientPrivate(this))
{
    Q_D(RemoteClient);
    d->m_key = QUuid::createUuid().toString();
}

RemoteClient::~RemoteClient()
{
    Q_D(RemoteClient);
    d->m_quit = true;
}

RemoteClient &RemoteClient::instance()
{
    static RemoteClient instance;
    return instance;
}

QString RemoteClient::authorizationKey() const
{
    Q_D(const RemoteClient);
    return d->m_key;
}

void RemoteClient::setAuthorizationKey(const QString &key)
{
    Q_D(RemoteClient);
    if (d->m_serverStarted)
        return;
    d->m_key = key;
}

void RemoteClient::init(quint16 port, const QHostAddress &address, Mode mode)
{
    Q_D(RemoteClient);
    d->init(port, address, mode);
}

bool RemoteClient::connect(QTcpSocket *socket) const
{
    Q_D(const RemoteClient);
    if (d->m_quit)
        return false;

    int tries = 3;
    while ((tries > 0) && (!d->m_quit)) {
        socket->connectToHost(d->m_address, d->m_port);

        QElapsedTimer stopWatch;
        stopWatch.start();
        while ((socket->state() == QAbstractSocket::ConnectingState)
            && (stopWatch.elapsed() < 10000) && (!d->m_quit)) {
                --tries;
                qApp->processEvents();
                continue;
        }
        if ((socket->state() != QAbstractSocket::ConnectedState) || d->m_quit)
            return false;

        QDataStream stream;
        stream.setDevice(socket);
        stream << QString::fromLatin1(Protocol::Authorize) << d->m_key;

        socket->waitForBytesWritten(-1);
        if (!socket->bytesAvailable())
            socket->waitForReadyRead(-1);

        quint32 size; stream >> size;
        bool authorized; stream >> authorized;
        if (authorized && (!d->m_quit))
            return true;
    }
    return false;
}

bool RemoteClient::isActive() const
{
    Q_D(const RemoteClient);
    return d->m_active;
}

void RemoteClient::setActive(bool active)
{
    Q_D(RemoteClient);
    d->m_active = active;
    if (d->m_active) {
        d->maybeStartServer();
        d->m_active = d->m_serverStarted;
    }
}

void RemoteClient::setStartServerCommand(const QString &command, StartAs startAs)
{
    setStartServerCommand(command, QStringList(), startAs);
}

void RemoteClient::setStartServerCommand(const QString &command, const QStringList &arguments,
    StartAs startAs)
{
    Q_D(RemoteClient);
    d->maybeStopServer();
    d->m_serverCommand = command;
    d->m_serverArguments = arguments;
    d->m_startServerAsAdmin = startAs;
}

} // namespace QInstaller
