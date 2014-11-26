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

#ifndef REMOTESERVERCONNECTION_H
#define REMOTESERVERCONNECTION_H

#include <QPointer>
#include <QThread>

#include <QtCore/private/qfsfileengine_p.h>

QT_BEGIN_NAMESPACE
class QProcess;
class QSettings;
QT_END_NAMESPACE

namespace QInstaller {

class QProcessSignalReceiver;

class RemoteServerConnection : public QThread
{
    Q_OBJECT
    Q_DISABLE_COPY(RemoteServerConnection)

public:
    RemoteServerConnection(qintptr socketDescriptor, const QString &authorizationKey);

    void run() Q_DECL_OVERRIDE;

signals:
    void shutdownRequested();

private:
    template <typename T>
    void sendData(QDataStream &stream, const T &arg);
    void handleQProcess(const QString &command, QDataStream &receivedStream);
    void handleQSettings(const QString &command, QDataStream &receivedStream);
    void handleQFSFileEngine(const QString &command, QDataStream &receivedStream);

private:
    qintptr m_socketDescriptor;

    QProcess *m_process;
    QSettings *m_settings;
    QFSFileEngine *m_engine;
    QString m_authorizationKey;
    QProcessSignalReceiver *m_signalReceiver;
};

} // namespace QInstaller

#endif // REMOTESERVERCONNECTION_H
