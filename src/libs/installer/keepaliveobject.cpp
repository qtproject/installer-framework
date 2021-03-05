/**************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
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

#include "keepaliveobject.h"
#include "remoteclient.h"

#include <QLocalSocket>
#include <QTimer>

namespace QInstaller {

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::KeepAliveObject
    \internal
*/

KeepAliveObject::KeepAliveObject()
    : m_timer(nullptr)
    , m_socket(nullptr)
{
}

void KeepAliveObject::start()
{
    if (m_timer)
        delete m_timer;
    m_timer = new QTimer(this);

    if (m_socket)
        delete m_socket;
    m_socket = new QLocalSocket(this);

    connect(m_timer, &QTimer::timeout, [this]() {
        if (m_socket->state() != QLocalSocket::UnconnectedState)
            return;
        m_socket->connectToServer(RemoteClient::instance().socketName());
    });

    connect(m_socket, &QLocalSocket::connected, [this]() {
        m_socket->close();
    });

    m_timer->setInterval(5000);
    m_timer->start();
}

} // namespace QInstaller
