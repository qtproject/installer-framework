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

#include "remoteserver.h"

#include "remoteserver_p.h"
#include "repository.h"

namespace QInstaller {

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::LocalServer
    \internal
*/

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::RemoteServerPrivate
    \internal
*/

/*!
    Constructs an remote server object with \a parent.
*/
RemoteServer::RemoteServer(QObject *parent)
    : QObject(parent)
    , d_ptr(new RemoteServerPrivate(this))
{
    Repository::registerMetaType(); // register, cause we stream the type as QVariant
}

/*!
    Destroys the remote server object.
*/
RemoteServer::~RemoteServer()
{
    Q_D(RemoteServer);
    d->m_thread.quit();
    d->m_thread.wait();
}

/*!
    Starts the server.

    \note If running in debug mode, the timer that kills the server after 30 seconds without
    usage is not started, so the server runs in an endless loop.
*/
void RemoteServer::start()
{
    Q_D(RemoteServer);
    if (d->m_localServer)
        return;

#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    // avoid writing to stderr:
    // the parent process has redirected stderr to a pipe to work with sudo,
    // but is not reading anymore -> writing to stderr will block after a while.
    if (d->m_mode == Protocol::Mode::Production)
        fclose(stderr);
#endif

    d->m_localServer = new LocalServer(d->m_socketName, d->m_key);
    d->m_localServer->moveToThread(&d->m_thread);
    connect(&d->m_thread, &QThread::finished, d->m_localServer, &QObject::deleteLater);
    connect(d->m_localServer, &LocalServer::newIncomingConnection, this, &RemoteServer::restartWatchdog);
    connect(d->m_localServer, &LocalServer::shutdownRequested, this, &QObject::deleteLater);
    d->m_thread.start();

    if (d->m_mode == Protocol::Mode::Production) {
        connect(d->m_watchdog.data(), &QTimer::timeout, this, &QObject::deleteLater);
        d->m_watchdog->start();
    }
}

/*!
    Initializes the server with \a socketName, with \a key, the key the client
    needs to send to authenticate with the server, and \a mode.
*/
void RemoteServer::init(const QString &socketName, const QString &key, Protocol::Mode mode)
{
    Q_D(RemoteServer);

    // Since Qt 5.12.0, we should determince the full socket path on Unix
    // platforms before calling QLocalSocketPrivate::_q_connectToSocket().
    // Otherwise the new canonical implementation of QDir::tempPath()
    // presents unintended usage of RemoteFileEngine.

#if QT_VERSION >= QT_VERSION_CHECK(5,12,0) && defined(Q_OS_UNIX)
    d->m_socketName = socketPathName(socketName);
#else
    d->m_socketName = socketName;
#endif
    d->m_key = key;
    d->m_mode = mode;
}

/*!
    Returns the socket name the server is listening on.
*/
QString RemoteServer::socketName() const
{
    Q_D(const RemoteServer);
    return d->m_socketName;
}

/*!
    Returns the authorization key.
*/
QString RemoteServer::authorizationKey() const
{
    Q_D(const RemoteServer);
    return d->m_key;
}

QString RemoteServer::socketPathName(const QString &socketName) const
{
    QString socketPathName;
    if (socketName.startsWith(QLatin1Char('/'))) {
        socketPathName = socketName;
    } else {
        socketPathName = QDir::tempPath();
        socketPathName += QLatin1Char('/') + socketName;
    }
    return socketPathName;
}

/*!
    Restarts the watchdog that tries to kill the server.
*/
void RemoteServer::restartWatchdog()
{
    Q_D(RemoteServer);
    if (d->m_watchdog)
        d->m_watchdog->start();
}

} // namespace QInstaller
