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

#ifndef REMOTEOBJECT_H
#define REMOTEOBJECT_H

#include "errors.h"
#include "installer_global.h"

#include <QDataStream>
#include <QObject>
#include <QTcpSocket>

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
        if (!m_socket->bytesAvailable())
            m_socket->waitForReadyRead(-1);

        quint32 size; m_stream >> size;
        while (m_socket->bytesAvailable() < size) {
            if (!m_socket->waitForReadyRead(30000)) {
                throw Error(tr("Could not read all data after sending command: %1. "
                    "Bytes expected: %2, Bytes received: %3. Error: %4").arg(name).arg(size)
                    .arg(m_socket->bytesAvailable()).arg(m_socket->errorString()));
            }
        }

        T result;
        m_stream >> result;
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

        m_stream << name;
        m_stream << quint32(data.size());
        m_stream << data;
        m_socket->waitForBytesWritten(-1);
    }

private:
    QString m_type;
    QTcpSocket *m_socket;
    mutable QDataStream m_stream;
};

} // namespace QInstaller

#endif // REMOTEOBJECT_H
