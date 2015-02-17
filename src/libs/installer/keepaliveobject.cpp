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

#include "keepaliveobject.h"
#include "remoteclient.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QHostAddress>
#include <QTcpSocket>
#include <QTimer>

namespace QInstaller {

KeepAliveObject::KeepAliveObject()
    : m_timer(0)
    , m_quit(false)
{
}

void KeepAliveObject::start()
{
    m_timer = new QTimer(this);
    connect(m_timer, SIGNAL(timeout()), this, SLOT(onTimeout()));
    m_timer->start(5000);
}

void KeepAliveObject::finish()
{
    m_quit = true;
}

void KeepAliveObject::onTimeout()
{
    m_timer->stop();
    {
        // Try to connect to the privileged running server. If we succeed the server side
        // watchdog gets restarted and the server keeps running for another 30 seconds.
        QTcpSocket socket;
        socket.connectToHost(RemoteClient::instance().address(), RemoteClient::instance().port());

        QElapsedTimer stopWatch;
        stopWatch.start();
        while ((socket.state() == QAbstractSocket::ConnectingState)
            && (stopWatch.elapsed() < 10000) && (!m_quit)) {
            if ((stopWatch.elapsed() % 2500) == 0)
                QCoreApplication::processEvents();
        }
    }
    if (!m_quit)
        m_timer->start(5000);
}

} // namespace QInstaller
