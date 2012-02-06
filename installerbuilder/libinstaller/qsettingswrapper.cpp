/**************************************************************************
**
** This file is part of Installer Framework
**
** Copyright (c) 2011-2012 Nokia Corporation and/or its subsidiary(-ies).
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

#include "qsettingswrapper.h"

#include "fsengineclient.h"
#include "templates.cpp"

#include <QtCore/QSettings>
#include <QtCore/QThread>

#include <QtNetwork/QTcpSocket>


// -- QSettingsWrapper::Private

class QSettingsWrapper::Private
{
public:
    Private(const QString &organization, const QString &application)
        : native(true),
        settings(organization, application),
        socket(0)
    {
    }

    Private(QSettings::Scope scope, const QString &organization, const QString &application)
        : native(true),
        settings(scope, organization, application),
        socket(0)
    {
    }

    Private(QSettings::Format format, QSettings::Scope scope, const QString &organization,
        const QString &application)
        : native(format == QSettings::NativeFormat),
        settings(format, scope, organization, application),
        socket(0)
    {
    }

    Private(const QString &fileName, QSettings::Format format)
        : native(format == QSettings::NativeFormat),
        fileName(fileName),
        settings(fileName, format),
        socket(0)
    {
    }

    Private()
        : native(true),
        socket(0)
    {
    }

    bool createSocket()
    {
        if (!native || !FSEngineClientHandler::instance().isActive())
            return false;

        if (socket != 0 && socket->state() == static_cast<int>(QAbstractSocket::ConnectedState))
            return true;

        if (socket != 0)
            delete socket;

        socket = new QTcpSocket;
        if (!FSEngineClientHandler::instance().connect(socket))
            return false;

        stream.setDevice(socket);
        stream.setVersion(QDataStream::Qt_4_2);

        stream << QString::fromLatin1("createQSettings");
        stream << this->fileName;
        socket->flush();
        stream.device()->waitForReadyRead(-1);
        quint32 test;
        stream >> test;
        stream.device()->readAll();
        return true;
    }

    const bool native;
    const QString fileName;
    QSettings settings;
    mutable QTcpSocket *socket;
    mutable QDataStream stream;
};


// -- QSettingsWrapper

QSettingsWrapper::QSettingsWrapper(const QString &organization, const QString &application, QObject *parent)
    : QObject(parent)
    , d(new Private(organization, application))
{
}

QSettingsWrapper::QSettingsWrapper(QSettingsWrapper::Scope scope, const QString &organization,
        const QString &application, QObject *parent)
    : QObject(parent)
    , d(new Private(static_cast<QSettings::Scope>(scope), organization, application))
{
}

QSettingsWrapper::QSettingsWrapper(QSettingsWrapper::Format format, QSettingsWrapper::Scope scope,
        const QString &organization, const QString &application, QObject *parent)
    : QObject(parent)
    , d(new Private(static_cast<QSettings::Format>(format), static_cast<QSettings::Scope> (scope),
        organization, application))
{
}

QSettingsWrapper::QSettingsWrapper(const QString &fileName, QSettingsWrapper::Format format, QObject *parent)
    : QObject(parent)
    , d(new Private(fileName, static_cast<QSettings::Format>(format)))
{
}

QSettingsWrapper::QSettingsWrapper(QObject *parent)
    : QObject(parent)
    , d(new Private)
{
}

QSettingsWrapper::~QSettingsWrapper()
{
    if (d->socket != 0) {
        d->stream << QString::fromLatin1("destroyQSettings");
        d->socket->flush();
        quint32 result;
        d->stream >> result;

        if (QThread::currentThread() == d->socket->thread()) {
            d->socket->close();
            delete d->socket;
        } else {
            d->socket->deleteLater();
        }
    }
    delete d;
}

QStringList QSettingsWrapper::allKeys() const
{
    if (d->createSocket())
        return callRemoteMethod<QStringList>(d->stream, QLatin1String("QSettings::allKeys"));
    return static_cast<QStringList>(d->settings.allKeys());
}

QString QSettingsWrapper::applicationName() const
{
    if (d->createSocket())
        return callRemoteMethod<QString>(d->stream, QLatin1String("QSettings::applicationName"));
    return static_cast<QString>(d->settings.applicationName());
}

void QSettingsWrapper::beginGroup(const QString &param1)
{
    if (d->createSocket())
        callRemoteVoidMethod(d->stream, QLatin1String("QSettings::beginGroup"), param1);
    else
        d->settings.beginGroup(param1);
}

int QSettingsWrapper::beginReadArray(const QString &param1)
{
    if (d->createSocket())
        return callRemoteMethod<int>(d->stream, QLatin1String("QSettings::beginReadArray"), param1);
    return d->settings.beginReadArray(param1);
}

void QSettingsWrapper::beginWriteArray(const QString &param1, int param2)
{
    if (d->createSocket())
        callRemoteVoidMethod(d->stream, QLatin1String("QSettings::beginWriteArray"), param1, param2);
    else
        d->settings.beginWriteArray(param1, param2);
}

QStringList QSettingsWrapper::childGroups() const
{
    if (d->createSocket())
        return callRemoteMethod<QStringList>(d->stream, QLatin1String("QSettings::childGroups"));
    return static_cast<QStringList>(d->settings.childGroups());
}

QStringList QSettingsWrapper::childKeys() const
{
    if (d->createSocket())
        return callRemoteMethod<QStringList>(d->stream, QLatin1String("QSettings::childKeys"));
    return static_cast<QStringList>(d->settings.childKeys());
}

void QSettingsWrapper::clear()
{
    if (d->createSocket())
        callRemoteVoidMethod<void>(d->stream, QLatin1String("QSettings::clear"));
    else d->settings.clear();
}

bool QSettingsWrapper::contains(const QString &param1) const
{
    if (d->createSocket())
        return callRemoteMethod<bool>(d->stream, QLatin1String("QSettings::contains"), param1);
    return d->settings.contains(param1);
}

void QSettingsWrapper::endArray()
{
    if (d->createSocket())
        callRemoteVoidMethod<void>(d->stream, QLatin1String("QSettings::endArray"));
    else
        d->settings.endArray();
}

void QSettingsWrapper::endGroup()
{
    if (d->createSocket())
        callRemoteVoidMethod<void>(d->stream, QLatin1String("QSettings::endGroup"));
    else
        d->settings.endGroup();
}

bool QSettingsWrapper::fallbacksEnabled() const
{
    if (d->createSocket())
        return callRemoteMethod<bool>(d->stream, QLatin1String("QSettings::fallbacksEnabled"));
    return static_cast<bool>(d->settings.fallbacksEnabled());
}

QString QSettingsWrapper::fileName() const
{
    if (d->createSocket())
        return callRemoteMethod<QString>(d->stream, QLatin1String("QSettings::fileName"));
    return static_cast<QString>(d->settings.fileName());
}

QSettingsWrapper::Format QSettingsWrapper::format() const
{
    return static_cast<QSettingsWrapper::Format>(d->settings.format());
}

QString QSettingsWrapper::group() const
{
    if (d->createSocket())
        return callRemoteMethod<QString>(d->stream, QLatin1String("QSettings::group"));
    return static_cast<QString>(d->settings.group());
}

QTextCodec* QSettingsWrapper::iniCodec() const
{
    return d->settings.iniCodec();
}

bool QSettingsWrapper::isWritable() const
{
    if (d->createSocket())
        return callRemoteMethod<bool>(d->stream, QLatin1String("QSettings::isWritable"));
    return static_cast<bool>(d->settings.isWritable());
}

QString QSettingsWrapper::organizationName() const
{
    if (d->createSocket())
        return callRemoteMethod<QString>(d->stream, QLatin1String("QSettings::organizationName"));
    return static_cast<QString>(d->settings.organizationName());
}

void QSettingsWrapper::remove(const QString &param1)
{
    if (d->createSocket())
        callRemoteVoidMethod(d->stream, QLatin1String("QSettings::remove"), param1);
    else d->settings.remove(param1);
}

QSettingsWrapper::Scope QSettingsWrapper::scope() const
{
    return static_cast<QSettingsWrapper::Scope>(d->settings.scope());
}

void QSettingsWrapper::setArrayIndex(int param1)
{
    if (d->createSocket())
        callRemoteVoidMethod(d->stream, QLatin1String("QSettings::setArrayIndex"), param1);
    else
        d->settings.setArrayIndex(param1);
}

void QSettingsWrapper::setFallbacksEnabled(bool param1)
{
    if (d->createSocket())
        callRemoteVoidMethod(d->stream, QLatin1String("QSettings::setFallbacksEnabled"), param1);
    else
        d->settings.setFallbacksEnabled(param1);
}

void QSettingsWrapper::setIniCodec(QTextCodec *codec)
{
    d->settings.setIniCodec(codec);
}

void QSettingsWrapper::setIniCodec(const char *codecName)
{
    d->settings.setIniCodec(codecName);
}

void QSettingsWrapper::setValue(const QString &param1, const QVariant &param2)
{
    if (d->createSocket())
        callRemoteVoidMethod(d->stream, QLatin1String("QSettings::setValue"), param1, param2);
    else
        d->settings.setValue(param1, param2);
}

QSettingsWrapper::Status QSettingsWrapper::status() const
{
    if (d->createSocket())
        return callRemoteMethod<QSettingsWrapper::Status>(d->stream, QLatin1String("QSettings::status"));
    return static_cast<QSettingsWrapper::Status>(d->settings.status());
}

void QSettingsWrapper::sync()
{
    if (d->createSocket())
        callRemoteVoidMethod<void>(d->stream, QLatin1String("QSettings::sync"));
    else
        d->settings.sync();
}

QVariant QSettingsWrapper::value(const QString &param1, const QVariant &param2) const
{
    if (d->createSocket())
        return callRemoteMethod<QVariant>(d->stream, QLatin1String("QSettings::value"), param1, param2);
    return d->settings.value(param1, param2);
}
