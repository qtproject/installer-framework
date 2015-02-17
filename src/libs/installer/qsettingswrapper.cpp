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

#include "qsettingswrapper.h"
#include "permissionsettings.h"

#include <QStringList>

namespace QInstaller {


// -- QSettingsWrapper::Private

class QSettingsWrapper::Private
{
public:
    Private(const QString &organization, const QString &application)
        : m_application(application)
        , m_organization(organization)
        , m_scope(QSettings::UserScope)
        , m_format(QSettings::NativeFormat)
        , settings(organization, application)
    {
    }

    Private(QSettings::Scope scope, const QString &organization, const QString &application)
        : m_application(application)
        , m_organization(organization)
        , m_scope(scope)
        , m_format(QSettings::NativeFormat)
        , settings(scope, organization, application)
    {
    }

    Private(QSettings::Format format, QSettings::Scope scope, const QString &organization,
        const QString &application)
        : m_application(application)
        , m_organization(organization)
        , m_scope(scope)
        , m_format(format)
        , settings(format, scope, organization, application)
    {
    }

    Private(const QString &fileName, QSettings::Format format)
        : m_filename(fileName)
        , settings(fileName, format)
    {
        m_format = format;
        m_scope = settings.scope();
        m_application = settings.applicationName();
        m_organization = settings.organizationName();
    }

    QString m_filename;
    QString m_application;
    QString m_organization;
    QSettings::Scope m_scope;
    QSettings::Format m_format;
    PermissionSettings settings;
};


// -- QSettingsWrapper

QSettingsWrapper::QSettingsWrapper(const QString &organization, const QString &application,
        QObject *parent)
    : RemoteObject(QLatin1String(Protocol::QSettings), parent)
    , d(new Private(organization, application))
{
}

QSettingsWrapper::QSettingsWrapper(QSettingsWrapper::Scope scope, const QString &organization,
        const QString &application, QObject *parent)
    : RemoteObject(QLatin1String(Protocol::QSettings), parent)
    , d(new Private(static_cast<QSettings::Scope>(scope), organization, application))
{
}

QSettingsWrapper::QSettingsWrapper(QSettingsWrapper::Format format, QSettingsWrapper::Scope scope,
        const QString &organization, const QString &application, QObject *parent)
    : RemoteObject(QLatin1String(Protocol::QSettings), parent)
    , d(new Private(static_cast<QSettings::Format>(format), static_cast<QSettings::Scope> (scope),
        organization, application))
{
}

QSettingsWrapper::QSettingsWrapper(const QString &fileName, QSettingsWrapper::Format format,
        QObject *parent)
    : RemoteObject(QLatin1String(Protocol::QSettings), parent)
    , d(new Private(fileName, static_cast<QSettings::Format>(format)))
{
}

QSettingsWrapper::~QSettingsWrapper()
{
    delete d;
}

QStringList QSettingsWrapper::allKeys() const
{
    if (createSocket())
        return callRemoteMethod<QStringList>(QLatin1String(Protocol::QSettingsAllKeys));
    return d->settings.allKeys();
}

QString QSettingsWrapper::applicationName() const
{
    if (createSocket())
        return callRemoteMethod<QString>(QLatin1String(Protocol::QSettingsApplicationName));
    return d->settings.applicationName();
}

void QSettingsWrapper::beginGroup(const QString &param1)
{
    if (createSocket())
        callRemoteMethod(QLatin1String(Protocol::QSettingsBeginGroup), param1, dummy);
    else
        d->settings.beginGroup(param1);
}

int QSettingsWrapper::beginReadArray(const QString &param1)
{
    if (createSocket())
        return callRemoteMethod<qint32>(QLatin1String(Protocol::QSettingsBeginReadArray), param1);
    return d->settings.beginReadArray(param1);
}

void QSettingsWrapper::beginWriteArray(const QString &param1, int param2)
{
    if (createSocket())
        callRemoteMethod(QLatin1String(Protocol::QSettingsBeginWriteArray), param1, qint32(param2));
    else
        d->settings.beginWriteArray(param1, param2);
}

QStringList QSettingsWrapper::childGroups() const
{
    if (createSocket())
        return callRemoteMethod<QStringList>(QLatin1String(Protocol::QSettingsChildGroups));
    return d->settings.childGroups();
}

QStringList QSettingsWrapper::childKeys() const
{
    if (createSocket())
        return callRemoteMethod<QStringList>(QLatin1String(Protocol::QSettingsChildKeys));
    return d->settings.childKeys();
}

void QSettingsWrapper::clear()
{
    if (createSocket())
        callRemoteMethod(QLatin1String(Protocol::QSettingsClear));
    else d->settings.clear();
}

bool QSettingsWrapper::contains(const QString &param1) const
{
    if (createSocket())
        return callRemoteMethod<bool>(QLatin1String(Protocol::QSettingsContains), param1);
    return d->settings.contains(param1);
}

void QSettingsWrapper::endArray()
{
    if (createSocket())
        callRemoteMethod(QLatin1String(Protocol::QSettingsEndArray));
    else
        d->settings.endArray();
}

void QSettingsWrapper::endGroup()
{
    if (createSocket())
        callRemoteMethod(QLatin1String(Protocol::QSettingsEndGroup));
    else
        d->settings.endGroup();
}

bool QSettingsWrapper::fallbacksEnabled() const
{
    if (createSocket())
        return callRemoteMethod<bool>(QLatin1String(Protocol::QSettingsFallbacksEnabled));
    return d->settings.fallbacksEnabled();
}

QString QSettingsWrapper::fileName() const
{
    if (createSocket())
        return callRemoteMethod<QString>(QLatin1String(Protocol::QSettingsFileName));
    return d->settings.fileName();
}

QSettingsWrapper::Format QSettingsWrapper::format() const
{
    // No need to talk to the server, we've setup the local settings object the same way.
    return static_cast<QSettingsWrapper::Format>(d->settings.format());
}

QString QSettingsWrapper::group() const
{
    if (createSocket())
        return callRemoteMethod<QString>(QLatin1String(Protocol::QSettingsGroup));
    return d->settings.group();
}

bool QSettingsWrapper::isWritable() const
{
    if (createSocket())
        return callRemoteMethod<bool>(QLatin1String(Protocol::QSettingsIsWritable));
    return d->settings.isWritable();
}

QString QSettingsWrapper::organizationName() const
{
    if (createSocket())
        return callRemoteMethod<QString>(QLatin1String(Protocol::QSettingsOrganizationName));
    return d->settings.organizationName();
}

void QSettingsWrapper::remove(const QString &param1)
{
    if (createSocket())
        callRemoteMethod(QLatin1String(Protocol::QSettingsRemove), param1, dummy);
    else
        d->settings.remove(param1);
}

QSettingsWrapper::Scope QSettingsWrapper::scope() const
{
    // No need to talk to the server, we've setup the local settings object the same way.
    return static_cast<QSettingsWrapper::Scope>(d->settings.scope());
}

void QSettingsWrapper::setArrayIndex(int param1)
{
    if (createSocket())
        callRemoteMethod(QLatin1String(Protocol::QSettingsSetArrayIndex), qint32(param1), dummy);
    else
        d->settings.setArrayIndex(param1);
}

void QSettingsWrapper::setFallbacksEnabled(bool param1)
{
    if (createSocket())
        callRemoteMethod(QLatin1String(Protocol::QSettingsSetFallbacksEnabled), param1, dummy);
    else
        d->settings.setFallbacksEnabled(param1);
}

void QSettingsWrapper::setValue(const QString &param1, const QVariant &param2)
{
    if (createSocket())
        callRemoteMethod(QLatin1String(Protocol::QSettingsSetValue), param1, param2);
    else
        d->settings.setValue(param1, param2);
}

QSettingsWrapper::Status QSettingsWrapper::status() const
{
    if (createSocket()) {
        return static_cast<QSettingsWrapper::Status>
            (callRemoteMethod<qint32>(QLatin1String(Protocol::QSettingsStatus)));
    }
    return static_cast<QSettingsWrapper::Status>(d->settings.status());
}

void QSettingsWrapper::sync()
{
    if (createSocket())
        callRemoteMethod(QLatin1String(Protocol::QSettingsSync));
    else
        d->settings.sync();
}

QVariant QSettingsWrapper::value(const QString &param1, const QVariant &param2) const
{
    if (createSocket())
        return callRemoteMethod<QVariant>(QLatin1String(Protocol::QSettingsValue), param1, param2);
    return d->settings.value(param1, param2);
}


// -- private

bool QSettingsWrapper::createSocket() const
{
    if ((d->m_format != QSettings::NativeFormat) && (d->m_format != QSettings::IniFormat)) {
        Q_ASSERT_X(false, Q_FUNC_INFO, "Settings wrapper only supports QSettingsWrapper::NativeFormat"
                   " and QSettingsWrapper::IniFormat.");
    }
    return (const_cast<QSettingsWrapper *>(this))->connectToServer(QVariantList()
        << d->m_application << d->m_organization << d->m_scope << d->m_format << d->m_filename);
}

} // namespace QInstaller
