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

#include "remoteclient.h"
#include "remoteclient_p.h"

#include <QDir>

namespace QInstaller {

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::RemoteClient
    \internal
*/

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::RemoteClientPrivate
    \internal
*/

RemoteClient *RemoteClient::s_instance = nullptr;

RemoteClient::RemoteClient()
    : d_ptr(new RemoteClientPrivate(this))
{
}

RemoteClient::~RemoteClient()
{
}

RemoteClient &RemoteClient::instance()
{
    if (!s_instance)
        s_instance = new RemoteClient;
    return *s_instance;
}

QString RemoteClient::socketName() const
{
    Q_D(const RemoteClient);
    return d->m_socketName;
}

QString RemoteClient::authorizationKey() const
{
    Q_D(const RemoteClient);
    return d->m_key;
}

QString RemoteClient::socketPathName(const QString &socketName) const
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
    Initializes the client with \a socketName, with the \a key the client
    sends to authenticate with the server, \a mode and \a startAs.
*/
void RemoteClient::init(const QString &socketName, const QString &key, Protocol::Mode mode,
    Protocol::StartAs startAs)
{
    Q_D(RemoteClient);

    // Since Qt 5.12.0, we should determince the full socket path on Unix
    // platforms before calling QLocalSocketPrivate::_q_connectToSocket().
    // Otherwise the new canonical implementation of QDir::tempPath()
    // presents unintended usage of RemoteFileEngine.

#if QT_VERSION >= QT_VERSION_CHECK(5,12,0) && defined(Q_OS_UNIX)
    d->init(socketPathName(socketName), key, mode, startAs);
#else
    d->init(socketName, key, mode, startAs);
#endif
}

void RemoteClient::setAuthorizationFallbackDisabled(bool disabled)
{
    Q_D(RemoteClient);
    d->setAuthorizationFallbackDisabled(disabled);
}

void RemoteClient::shutdown()
{
    Q_D(RemoteClient);
    d->shutdown();
    d_ptr.reset(new RemoteClientPrivate(this));
}

void RemoteClient::destroy()
{
    delete s_instance;
    s_instance = nullptr;
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

} // namespace QInstaller
