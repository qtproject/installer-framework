/**************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
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
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
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

#include <QHostAddress>
#include <QPointer>
#include <QTcpServer>
#include <QTimer>

namespace QInstaller {

class TcpServer : public QTcpServer
{
    Q_OBJECT
    Q_DISABLE_COPY(TcpServer)

public:
    TcpServer(quint16 port, const QString &key)
        : QTcpServer(0)
        , m_key(key)
        , m_shutdown(false)
    {
        listen(QHostAddress(QLatin1String(Protocol::DefaultHostAddress)), port);
    }

    ~TcpServer() {
        shutdown();
    }

signals:
    void shutdownRequested();
    void newIncomingConnection();

private slots:
    void shutdown() {
        m_shutdown = true;
        const QList<QThread *> threads = findChildren<QThread *>();
        foreach (QThread *thread, threads) {
            thread->quit();
            thread->wait();
        }
        emit shutdownRequested();
    }

private:
    void incomingConnection(qintptr socketDescriptor) Q_DECL_OVERRIDE {
        if (m_shutdown)
            return;

        QThread *const thread = new RemoteServerConnection(socketDescriptor, m_key, this);
        connect(thread, SIGNAL(finished()), thread, SLOT(deleteLater()));
        connect(thread, SIGNAL(shutdownRequested()), this, SLOT(shutdown()));
        thread->start();
        emit newIncomingConnection();
    }

private:
    QString m_key;
    bool m_shutdown;
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
    Protocol::Mode m_mode;
    QScopedPointer<QTimer> m_watchdog;
};

} // namespace QInstaller

#endif // REMOTESERVER_P_H
