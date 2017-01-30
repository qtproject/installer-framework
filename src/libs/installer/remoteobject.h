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

#ifndef REMOTEOBJECT_H
#define REMOTEOBJECT_H

#include "errors.h"
#include "installer_global.h"
#include "protocol.h"

#include <QCoreApplication>
#include <QDataStream>
#include <QObject>
#include <QLocalSocket>

namespace QInstaller {

class INSTALLER_EXPORT RemoteObject : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(RemoteObject)

public:
    RemoteObject(const QString &wrappedType, QObject *parent = 0);
    virtual ~RemoteObject() = 0;

    bool isConnectedToServer() const;
    void callRemoteMethod(const QString &name);

    template<typename T1, typename T2>
    void callRemoteMethod(const QString &name, const T1 &arg, const T2 &arg2)
    {
        writeData(name, arg, arg2, dummy);
    }

    template<typename T1, typename T2, typename T3>
    void callRemoteMethod(const QString &name, const T1 &arg, const T2 &arg2, const T3 & arg3)
    {
        writeData(name, arg, arg2, arg3);
    }

    template<typename T>
    T callRemoteMethod(const QString &name) const
    {
        return callRemoteMethod<T>(name, dummy, dummy, dummy);
    }

    template<typename T, typename T1>
    T callRemoteMethod(const QString &name, const T1 &arg) const
    {
        return callRemoteMethod<T>(name, arg, dummy, dummy);
    }

    template<typename T, typename T1, typename T2>
    T callRemoteMethod(const QString &name, const T1 & arg, const T2 &arg2) const
    {
        return callRemoteMethod<T>(name, arg, arg2, dummy);
    }

    template<typename T, typename T1, typename T2, typename T3>
    T callRemoteMethod(const QString &name, const T1 &arg, const T2 &arg2, const T3 &arg3) const
    {
        writeData(name, arg, arg2, arg3);
        while (m_socket->bytesToWrite())
            m_socket->waitForBytesWritten();

        QByteArray command;
        QByteArray data;
        while (!receivePacket(m_socket, &command, &data)) {
            if (!m_socket->waitForReadyRead(-1)) {
                throw Error(tr("Cannot read all data after sending command: %1. "
                    "Bytes expected: %2, Bytes received: %3. Error: %4").arg(name).arg(0)
                    .arg(m_socket->bytesAvailable()).arg(m_socket->errorString()));
            }
        }

        Q_ASSERT(command == Protocol::Reply);

        QDataStream stream(&data, QIODevice::ReadOnly);

        T result;
        stream >> result;
        Q_ASSERT(stream.status() == QDataStream::Ok);
        Q_ASSERT(stream.atEnd());
        return result;
    }

protected:
    bool authorize();
    bool connectToServer(const QVariantList &arguments = QVariantList());

    // Use this structure to allow derived classes to manipulate the template
    // function signature of the callRemoteMethod templates, since most of the
    // generated functions will differ in return type rather given arguments.
    struct Dummy {}; Dummy *dummy;

private:
    template<typename T> bool isValueType(T) const
    {
        return true;
    }

    template<typename T> bool isValueType(T *dummy) const
    {
        // Force compiler error while passing anything different then Dummy* to the function.
        // It really doesn't make sense to send any pointer over to the server, so bail early.
        Q_UNUSED(static_cast<Dummy*> (dummy))
        return false;
    }

    template<typename T1, typename T2, typename T3>
    void writeData(const QString &name, const T1 &arg, const T2 &arg2, const T3 &arg3) const
    {
        QByteArray data;
        QDataStream out(&data, QIODevice::WriteOnly);

        if (isValueType(arg))
            out << arg;
        if (isValueType(arg2))
            out << arg2;
        if (isValueType(arg3))
            out << arg3;

        sendPacket(m_socket, name.toLatin1(), data);
        m_socket->flush();
    }

private:
    QString m_type;
    QLocalSocket *m_socket;
};

} // namespace QInstaller

#endif // REMOTEOBJECT_H
