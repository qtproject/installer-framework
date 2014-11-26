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

#ifndef REMOTESERVER_P_H
#define REMOTESERVER_P_H

#include "protocol.h"
#include "remoteserver.h"
#include "remoteserverconnection.h"

#include <QPointer>
#include <QTcpServer>
#include <QTimer>

namespace QInstaller {

class TcpServer : public QTcpServer
{
    Q_OBJECT
    Q_DISABLE_COPY(TcpServer)

public:
    TcpServer(quint16 port, const QHostAddress &address, RemoteServer *server)
        : QTcpServer(0)
        , m_port(port)
        , m_address(address)
        , m_server(server)
    {
        listen(m_address, m_port);
    }

    ~TcpServer() {
        const QList<QThread *> threads = findChildren<QThread *>();
        foreach (QThread *thread, threads) {
            thread->quit();
            thread->wait();
        }
    }

signals:
    void newIncomingConnection();

private:
    void incomingConnection(qintptr socketDescriptor) Q_DECL_OVERRIDE {
        QThread *const thread = new RemoteServerConnection(socketDescriptor, m_server);
        connect(thread, SIGNAL(finished()), thread, SLOT(deleteLater()));
        thread->start();
        emit newIncomingConnection();
    }

private:
    quint16 m_port;
    QHostAddress m_address;
    QPointer<RemoteServer> m_server;
};

class RemoteServerPrivate
{
    Q_DECLARE_PUBLIC(RemoteServer)
    Q_DISABLE_COPY(RemoteServerPrivate)

public:
    explicit RemoteServerPrivate(RemoteServer *server)
        : q_ptr(server)
        , m_tcpServer(0)
        , m_key(QLatin1String(Protocol::DefaultAuthorizationKey))
        , m_port(Protocol::DefaultPort)
        , m_address(QLatin1String(Protocol::DefaultHostAddress))
        , m_mode(Protocol::Mode::Debug)
        , m_watchdog(new QTimer)
    {
        m_watchdog->setInterval(30000);
        m_watchdog->setSingleShot(true);
    }

private:
    RemoteServer *q_ptr;
    TcpServer *m_tcpServer;

    QString m_key;
    quint16 m_port;
    QThread m_thread;
    QHostAddress m_address;
    Protocol::Mode m_mode;
    QScopedPointer<QTimer> m_watchdog;
};

} // namespace QInstaller

#endif // REMOTESERVER_P_H
