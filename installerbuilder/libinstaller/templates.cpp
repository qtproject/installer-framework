/**************************************************************************
**
** This file is part of Installer Framework
**
** Copyright (c) 2010-2012 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (qt-info@nokia.com)
**
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** Other Usage
**
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
** If you have questions regarding the use of this file, please contact
** Nokia at qt-info@nokia.com.
**
**************************************************************************/

#include<QtCore/QIODevice>

template<typename T>
QDataStream &operator>>(QDataStream &stream, T &state)
{
    int s;
    stream >> s;
    state = static_cast<T> (s);
    return stream;
}

template<typename UNUSED>
void callRemoteVoidMethod(QDataStream &stream, const QString &name)
{
    stream.device()->readAll();
    stream << name;
    stream.device()->waitForBytesWritten(-1);
    if (!stream.device()->bytesAvailable())
        stream.device()->waitForReadyRead(-1);
    quint32 test;
    stream >> test;
    stream.device()->readAll();
    return;
}

template<typename T>
void callRemoteVoidMethod(QDataStream & stream, const QString &name, const T &param1)
{
    stream.device()->readAll();
    stream << name;
    stream << param1;
    stream.device()->waitForBytesWritten(-1);
    if (!stream.device()->bytesAvailable())
        stream.device()->waitForReadyRead(-1);
    quint32 test;
    stream >> test;
    stream.device()->readAll();
    return;
}

template<typename T1, typename T2>
void callRemoteVoidMethod(QDataStream &stream, const QString &name, const T1 &param1, const T2 &param2)
{
    stream.device()->readAll();
    stream << name;
    stream << param1;
    stream << param2;
    stream.device()->waitForBytesWritten(-1);
    if (!stream.device()->bytesAvailable())
        stream.device()->waitForReadyRead(-1);
    quint32 test;
    stream >> test;
    stream.device()->readAll();
    return;
}

template<typename T1, typename T2, typename T3>
void callRemoteVoidMethod(QDataStream &stream, const QString &name, const T1 &param1, const T2 &param2,
    const T3 & param3)
{
    stream.device()->readAll();
    stream << name;
    stream << param1;
    stream << param2;
    stream << param3;
    stream.device()->waitForBytesWritten(-1);
    if (!stream.device()->bytesAvailable())
        stream.device()->waitForReadyRead(-1);
    quint32 test;
    stream >> test;
    stream.device()->readAll();
    return;
}

template<typename RESULT>
RESULT callRemoteMethod(QDataStream &stream, const QString &name)
{
    stream.device()->readAll();
    stream << name;
    stream.device()->waitForBytesWritten(-1);
    if (!stream.device()->bytesAvailable())
        stream.device()->waitForReadyRead(-1);
    quint32 test;
    stream >> test;
    RESULT result;
    stream >> result;
    stream.device()->readAll();
    return result;
}

template<typename RESULT, typename T>
RESULT callRemoteMethod(QDataStream &stream, const QString &name, const T &param1)
{
    stream.device()->readAll();
    stream << name;
    stream << param1;
    stream.device()->waitForBytesWritten(-1);
    if (!stream.device()->bytesAvailable())
        stream.device()->waitForReadyRead(-1);
    quint32 test;
    stream >> test;
    RESULT result;
    stream >> result;
    stream.device()->readAll();
    return result;
}

template<typename RESULT, typename T1, typename T2>
RESULT callRemoteMethod(QDataStream &stream, const QString &name, const T1 & param1, const T2 &param2)
{
    stream.device()->readAll();
    stream << name;
    stream << param1;
    stream << param2;
    stream.device()->waitForBytesWritten(-1);
    if (!stream.device()->bytesAvailable())
        stream.device()->waitForReadyRead(-1);
    quint32 test;
    stream >> test;
    RESULT result;
    stream >> result;
    stream.device()->readAll();
    return result;
}

template<typename RESULT, typename T1, typename T2, typename T3>
RESULT callRemoteMethod(QDataStream &stream, const QString &name, const T1 &param1, const T2 &param2,
    const T3 &param3)
{
    stream.device()->readAll();
    stream << name;
    stream << param1;
    stream << param2;
    stream << param3;
    stream.device()->waitForBytesWritten(-1);
    if (!stream.device()->bytesAvailable())
        stream.device()->waitForReadyRead(-1);
    quint32 test;
    stream >> test;
    RESULT result;
    stream >> result;
    stream.device()->readAll();
    return result;
}
