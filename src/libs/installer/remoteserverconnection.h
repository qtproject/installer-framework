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

#ifndef REMOTESERVERCONNECTION_H
#define REMOTESERVERCONNECTION_H

#include "abstractarchive.h"

#include <QPointer>
#include <QThread>
#include <QProcess>

#include <QtCore/private/qfsfileengine_p.h>

QT_BEGIN_NAMESPACE
class QLocalSocket;
QT_END_NAMESPACE

namespace QInstaller {

class PermissionSettings;

class QProcessSignalReceiver;
class AbstractArchiveSignalReceiver;

class RemoteServerReply
{
public:
    explicit RemoteServerReply(QLocalSocket *socket);
    ~RemoteServerReply();

    template <typename T>
    void send(const T &data);

private:
    QLocalSocket *m_socket;
    bool m_sent;
};

class RemoteServerConnection : public QThread
{
    Q_OBJECT
    Q_DISABLE_COPY(RemoteServerConnection)

public:
    RemoteServerConnection(qintptr socketDescriptor, const QString &authorizationKey,
                           QObject *parent);

    void run() override;

signals:
    void shutdownRequested();

private:
    void handleQProcess(RemoteServerReply *reply, const QString &command, QDataStream &data);
    void handleQSettings(RemoteServerReply *reply, const QString &command, QDataStream &data,
                         PermissionSettings *settings);
    void handleQFSFileEngine(RemoteServerReply *reply, const QString &command, QDataStream &data);
    void handleArchive(RemoteServerReply *reply, const QString &command, QDataStream &data);

private:
    qintptr m_socketDescriptor;
    QString m_authorizationKey;

    QScopedPointer<QProcess> m_process;
    QScopedPointer<QFSFileEngine> m_engine;
    QScopedPointer<AbstractArchive> m_archive;

    QProcessSignalReceiver *m_processSignalReceiver;
    AbstractArchiveSignalReceiver *m_archiveSignalReceiver;
};

} // namespace QInstaller

#endif // REMOTESERVERCONNECTION_H
