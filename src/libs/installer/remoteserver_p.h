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

#ifndef REMOTESERVER_P_H
#define REMOTESERVER_P_H

#include "protocol.h"
#include "remoteserver.h"
#include "remoteserverconnection.h"

#include <QHostAddress>
#include <QPointer>
#include <QLocalServer>
#include <QTimer>

namespace QInstaller {

class LocalServer : public QLocalServer
{
    Q_OBJECT
    Q_DISABLE_COPY(LocalServer)

public:
    LocalServer(const QString &socketName, const QString &key)
        : QLocalServer(0)
        , m_key(key)
        , m_shutdown(false)
    {
        setSocketOptions(QLocalServer::WorldAccessOption);
        listen(socketName);
    }

    ~LocalServer() {
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
    void incomingConnection(quintptr socketDescriptor) override {
        if (m_shutdown)
            return;

        RemoteServerConnection *thread = new RemoteServerConnection(socketDescriptor, m_key, this);
        connect(thread, &QThread::finished, thread, &QObject::deleteLater);
        connect(thread, &RemoteServerConnection::shutdownRequested, this, &LocalServer::shutdown);
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
        , m_localServer(0)
        , m_key(QLatin1String(Protocol::DefaultAuthorizationKey))
        , m_mode(Protocol::Mode::Debug)
        , m_watchdog(new QTimer)
    {
        m_watchdog->setInterval(30000);
        m_watchdog->setSingleShot(true);
    }

private:
    RemoteServer *q_ptr;
    LocalServer *m_localServer;

    QString m_key;
    QString m_socketName;
    QThread m_thread;
    Protocol::Mode m_mode;
    QScopedPointer<QTimer> m_watchdog;
};

} // namespace QInstaller

#endif // REMOTESERVER_P_H
