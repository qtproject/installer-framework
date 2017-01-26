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

#ifndef REMOTECLIENT_P_H
#define REMOTECLIENT_P_H

#include "adminauthorization.h"
#include "keepaliveobject.h"
#include "messageboxhandler.h"
#include "protocol.h"
#include "remoteclient.h"
#include "remoteobject.h"
#include "utils.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QMutex>
#include <QThread>

namespace QInstaller {

class RemoteClientPrivate : public RemoteObject
{
    Q_DECLARE_PUBLIC(RemoteClient)
    Q_DISABLE_COPY(RemoteClientPrivate)

public:
    RemoteClientPrivate(RemoteClient *parent)
        : RemoteObject(QLatin1String("RemoteClientPrivate"))
        , q_ptr(parent)
        , m_mutex(QMutex::Recursive)
        , m_startServerAs(Protocol::StartAs::User)
        , m_serverStarted(false)
        , m_active(false)
        , m_key(QLatin1String(Protocol::DefaultAuthorizationKey))
        , m_mode(Protocol::Mode::Debug)
    {
        m_thread.setObjectName(QLatin1String("KeepAlive"));
    }

    ~RemoteClientPrivate()
    {
        shutdown();
    }

    void shutdown()
    {
        m_thread.quit();
        m_thread.wait();
        maybeStopServer();
    }

    void init(const QString &socketName, const QString &key, Protocol::Mode mode,
              Protocol::StartAs startAs)
    {
        m_socketName = socketName;
        m_key = key;
        m_mode = mode;
        if (mode == Protocol::Mode::Production) {
            m_startServerAs = startAs;
            m_serverCommand = QCoreApplication::applicationFilePath();
            m_serverArguments = QStringList() << QLatin1String("--startserver")
                << QString::fromLatin1("%1,%2,%3")
                    .arg(QLatin1String(Protocol::ModeProduction))
                    .arg(socketName)
                    .arg(key);

            KeepAliveObject *object = new KeepAliveObject;
            object->moveToThread(&m_thread);
            QObject::connect(&m_thread, &QThread::started, object, &KeepAliveObject::start);
            QObject::connect(&m_thread, &QThread::finished, object, &QObject::deleteLater);
            m_thread.start();
        } else if (mode == Protocol::Mode::Debug) {
            // To be able to debug the client-server connection start and stop the server manually,
            // e.g. installer --startserver DEBUG.
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
        m_serverStarted = false;

        bool started = false;
        if (m_startServerAs == Protocol::StartAs::SuperUser) {
            started = AdminAuthorization::execute(0, m_serverCommand, m_serverArguments);

            if (!started) {
                // something went wrong with authorizing, either user pressed cancel or entered
                // wrong password
                const QString fallback = m_serverCommand + QLatin1String(" ") + m_serverArguments
                    .join(QLatin1String(" "));

                const QMessageBox::Button res =
                    MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(),
                    QLatin1String("AuthorizationError"),
                    QCoreApplication::translate("RemoteClient", "Could not get authorization."),
                    QCoreApplication::translate("RemoteClient", "Could not get authorization that "
                        "is needed for continuing the installation.\n Either abort the "
                        "installation or use the fallback solution by running\n\n%1\n\nas root "
                        "and then clicking OK.").arg(fallback),
                    QMessageBox::Abort | QMessageBox::Ok, QMessageBox::Ok);

                if (res == QMessageBox::Ok)
                    started = true;
            }
        } else {
            started = QInstaller::startDetached(m_serverCommand, m_serverArguments,
                QCoreApplication::applicationDirPath());
        }

        if (started) {
            QElapsedTimer t;
            t.start();
            // 30 seconds waiting ought to be enough for the app to start
            while ((!m_serverStarted) && (t.elapsed() < 30000))
                m_serverStarted = authorize();
        }
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

        if (!authorize())
            return;
        m_serverStarted = !callRemoteMethod<bool>(QString::fromLatin1(Protocol::Shutdown));
    }

private:
    RemoteClient *q_ptr;
    QMutex m_mutex;
    QString m_socketName;
    Protocol::StartAs m_startServerAs;
    bool m_serverStarted;
    bool m_active;
    QString m_serverCommand;
    QStringList m_serverArguments;
    QString m_key;
    QThread m_thread;
    Protocol::Mode m_mode;
};

} // namespace QInstaller

#endif // REMOTECLIENT_P_H
