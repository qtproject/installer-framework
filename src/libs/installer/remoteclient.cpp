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

#include "remoteclient.h"
#include "remoteclient_p.h"

namespace QInstaller {

RemoteClient *RemoteClient::s_instance = 0;

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

/*!
    Initializes the client with \a socketName, with the \a key the client
    sends to authenticate with the server, \a mode and \a startAs.
*/
void RemoteClient::init(const QString &socketName, const QString &key, Protocol::Mode mode,
    Protocol::StartAs startAs)
{
    Q_D(RemoteClient);
    d->init(socketName, key, mode, startAs);
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
    s_instance = 0;
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
