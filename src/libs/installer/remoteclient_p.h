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

#ifndef REMOTECLIENT_P_H
#define REMOTECLIENT_P_H

#include "adminauthorization.h"
#include "keepaliveobject.h"
#include "messageboxhandler.h"
#include "protocol.h"
#include "remoteclient.h"
#include "utils.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QHostAddress>
#include <QMutex>
#include <QTcpSocket>
#include <QThread>
#include <QTimer>

namespace QInstaller {

class RemoteClientPrivate
{
    Q_DECLARE_PUBLIC(RemoteClient)
    Q_DISABLE_COPY(RemoteClientPrivate)

public:
    RemoteClientPrivate(RemoteClient *parent)
        : q_ptr(parent)
        , m_mutex(QMutex::Recursive)
        , m_address(QLatin1String(Protocol::DefaultHostAddress))
        , m_port(Protocol::DefaultPort)
        , m_startServerAs(Protocol::StartAs::User)
        , m_serverStarted(false)
        , m_serverStarting(false)
        , m_active(false)
        , m_key(QLatin1String(Protocol::DefaultAuthorizationKey))
        , m_mode(Protocol::Mode::Debug)
        , m_object(0)
    {
    }

    ~RemoteClientPrivate()
    {
        shutdown();
    }

    void shutdown()
    {
        if (m_object)
            m_object->finish();
        m_object = 0;

        m_thread.quit();
        m_thread.wait();
    }

    void init(quint16 port, const QString &key, Protocol::Mode mode, Protocol::StartAs startAs)
    {
        m_mode = mode;
        if (mode == Protocol::Mode::Production) {
            m_key = key;
            m_port = port;
            m_mode = mode;
            m_startServerAs = startAs;
            m_serverCommand = QCoreApplication::applicationFilePath();
            m_serverArguments = QStringList() << QLatin1String("--startserver")
                << QString::fromLatin1("%1,%2,%3")
                    .arg(QLatin1String(Protocol::ModeProduction))
                    .arg(port)
                    .arg(key);

            if (!m_object) {
                m_object = new KeepAliveObject;
                m_object->moveToThread(&m_thread);
                QObject::connect(&m_thread, SIGNAL(started()), m_object, SLOT(start()));
                QObject::connect(&m_thread, SIGNAL(finished()), m_object, SLOT(deleteLater()));
                m_thread.start();
            } else {
                Q_ASSERT_X(false, Q_FUNC_INFO, "Keep alive thread already started.");
            }
        } else if (mode == Protocol::Mode::Debug) {
            // To be able to debug the client-server connection start and stop the server manually,
            // e.g. installer --startserver debug. The server is listening on localhost:39999 then.
        }
    }

    void maybeStartServer() {
        if (m_mode == Protocol::Mode::Debug)
            m_serverStarted = true; // we expect the server to be started by the developer

        if (m_serverStarted)
            return;

        const QMutexLocker ml(&m_mutex);
        if (m_serverStarted)
            return;

        m_serverStarting = true;
        if (m_startServerAs == Protocol::StartAs::SuperUser) {
            m_serverStarted = AdminAuthorization::execute(0, m_serverCommand, m_serverArguments);

            if (!m_serverStarted) {
                // something went wrong with authorizing, either user pressed cancel or entered
                // wrong password
                const QString fallback = m_serverCommand + QLatin1String(" ") + m_serverArguments
                    .join(QLatin1String(" "));

                const QMessageBox::Button res =
                    MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(),
                    QLatin1String("AuthorizationError"),
                    RemoteClient::tr("Could not get authorization."),
                    RemoteClient::tr("Could not get authorization that is needed for continuing "
                    "the installation.\n Either abort the installation or use the fallback "
                    "solution by running\n\n%1\n\nas root and then clicking OK.").arg(fallback),
                    QMessageBox::Abort | QMessageBox::Ok, QMessageBox::Ok);

                if (res == QMessageBox::Ok)
                    m_serverStarted = true;
            }
        } else {
            m_serverStarted = QInstaller::startDetached(m_serverCommand, m_serverArguments,
                QCoreApplication::applicationDirPath());
        }

        if (m_serverStarted) {
            QElapsedTimer t;
            t.start();
             // 30 seconds ought to be enough for the app to start
            while (m_serverStarting && m_serverStarted && t.elapsed() < 30000) {
                Q_Q(RemoteClient);
                QTcpSocket socket;
                if (q->connect(&socket))
                    m_serverStarting = false;
            }
        }
        m_serverStarting = false;
    }

    void maybeStopServer()
    {
        if (m_mode == Protocol::Mode::Debug)
            m_serverStarted = false; // we never started the server in debug mode

        if (!m_serverStarted)
            return;

        const QMutexLocker ml(&m_mutex);
        if (!m_serverStarted)
            return;

        Q_Q(RemoteClient);
        QTcpSocket socket;
        if (q->connect(&socket)) {
            QDataStream stream(&socket);
            stream << QString::fromLatin1(Protocol::Authorize);
            stream << m_key;
            stream << QString::fromLatin1(Protocol::Shutdown);
            socket.flush();
        }
        m_serverStarted = false;
    }

private:
    RemoteClient *q_ptr;
    QMutex m_mutex;
    QHostAddress m_address;
    quint16 m_port;
    QString m_socket;
    Protocol::StartAs m_startServerAs;
    bool m_serverStarted;
    bool m_serverStarting;
    bool m_active;
    QString m_serverCommand;
    QStringList m_serverArguments;
    QString m_key;
    QThread m_thread;
    Protocol::Mode m_mode;
    KeepAliveObject *m_object;
};

} // namespace QInstaller

#endif // REMOTECLIENT_P_H
