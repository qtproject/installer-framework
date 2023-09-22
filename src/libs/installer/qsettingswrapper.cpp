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

#include "qsettingswrapper.h"
#include "permissionsettings.h"

#include <QStringList>

namespace QInstaller {

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::QSettingsWrapper
    \internal
*/

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
        , m_format(format)
        , settings(fileName, format)
    {
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

QSettingsWrapper::QSettingsWrapper(QSettings::Format format, QSettingsWrapper::Scope scope,
        const QString &organization, const QString &application, QObject *parent)
    : RemoteObject(QLatin1String(Protocol::QSettings), parent)
    , d(new Private(format, static_cast<QSettings::Scope> (scope),
        organization, application))
{
}

QSettingsWrapper::QSettingsWrapper(const QString &fileName, QSettings::Format format,
        QObject *parent)
    : RemoteObject(QLatin1String(Protocol::QSettings), parent)
    , d(new Private(fileName, format))
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

#if QT_VERSION < QT_VERSION_CHECK(6, 4, 0)
    void QSettingsWrapper::beginGroup(const QString &prefix)
#else
    void QSettingsWrapper::beginGroup(QAnyStringView prefix)
#endif
{
    if (createSocket())
        callRemoteMethodDefaultReply(QLatin1String(Protocol::QSettingsBeginGroup), prefix);
    else
        d->settings.beginGroup(prefix);
}

#if QT_VERSION < QT_VERSION_CHECK(6, 4, 0)
    int QSettingsWrapper::beginReadArray(const QString &prefix)
#else
    int QSettingsWrapper::beginReadArray(QAnyStringView prefix)
#endif
{
    if (createSocket())
        return callRemoteMethod<qint32>(QLatin1String(Protocol::QSettingsBeginReadArray), prefix);
    return d->settings.beginReadArray(prefix);
}

#if QT_VERSION < QT_VERSION_CHECK(6, 4, 0)
    void QSettingsWrapper::beginWriteArray(const QString &prefix, int size)
#else
    void QSettingsWrapper::beginWriteArray(QAnyStringView prefix, int size)
#endif
{
    if (createSocket())
        callRemoteMethodDefaultReply(QLatin1String(Protocol::QSettingsBeginWriteArray), prefix, qint32(size));
    else
        d->settings.beginWriteArray(prefix, size);
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
        callRemoteMethodDefaultReply(QLatin1String(Protocol::QSettingsClear));
    else d->settings.clear();
}

#if QT_VERSION < QT_VERSION_CHECK(6, 4, 0)
    bool QSettingsWrapper::contains(const QString &key) const
#else
    bool QSettingsWrapper::contains(QAnyStringView key) const
#endif
{
    if (createSocket())
        return callRemoteMethod<bool>(QLatin1String(Protocol::QSettingsContains), key);
    return d->settings.contains(key);
}

void QSettingsWrapper::endArray()
{
    if (createSocket())
        callRemoteMethodDefaultReply(QLatin1String(Protocol::QSettingsEndArray));
    else
        d->settings.endArray();
}

void QSettingsWrapper::endGroup()
{
    if (createSocket())
        callRemoteMethodDefaultReply(QLatin1String(Protocol::QSettingsEndGroup));
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

QSettings::Format QSettingsWrapper::format() const
{
    // No need to talk to the server, we've setup the local settings object the same way.
    return d->settings.format();
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

#if QT_VERSION < QT_VERSION_CHECK(6, 4, 0)
    void QSettingsWrapper::remove(const QString &key)
#else
    void QSettingsWrapper::remove(QAnyStringView key)
#endif
{
    if (createSocket())
        callRemoteMethodDefaultReply(QLatin1String(Protocol::QSettingsRemove), key);
    else
        d->settings.remove(key);
}

QSettingsWrapper::Scope QSettingsWrapper::scope() const
{
    // No need to talk to the server, we've setup the local settings object the same way.
    return static_cast<QSettingsWrapper::Scope>(d->settings.scope());
}

void QSettingsWrapper::setArrayIndex(int param1)
{
    if (createSocket())
        callRemoteMethodDefaultReply(QLatin1String(Protocol::QSettingsSetArrayIndex), qint32(param1));
    else
        d->settings.setArrayIndex(param1);
}

void QSettingsWrapper::setFallbacksEnabled(bool param1)
{
    if (createSocket())
        callRemoteMethodDefaultReply(QLatin1String(Protocol::QSettingsSetFallbacksEnabled), param1);
    else
        d->settings.setFallbacksEnabled(param1);
}
#if QT_VERSION < QT_VERSION_CHECK(6, 4, 0)
void QSettingsWrapper::setValue(const QString &key, const QVariant &value)
#else
void QSettingsWrapper::setValue(QAnyStringView key, const QVariant &value)
#endif
{
    if (createSocket())
        callRemoteMethodDefaultReply(QLatin1String(Protocol::QSettingsSetValue), key, value);
    else
        d->settings.setValue(key, value);
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
        callRemoteMethodDefaultReply(QLatin1String(Protocol::QSettingsSync));
    else
        d->settings.sync();
}

#if QT_VERSION < QT_VERSION_CHECK(6, 4, 0)
QVariant QSettingsWrapper::value(const QString &key, const QVariant &value) const
#else
QVariant QSettingsWrapper::value(QAnyStringView key, const QVariant &value) const
#endif
{
    if (createSocket())
        return callRemoteMethod<QVariant>(QLatin1String(Protocol::QSettingsValue), key, value);
    return d->settings.value(key, value);
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
QVariant QSettingsWrapper::value(QAnyStringView key) const
{
    if (createSocket())
        return callRemoteMethod<QVariant>(QLatin1String(Protocol::QSettingsValue), key);
    return d->settings.value(key);
}
#endif


// -- private

bool QSettingsWrapper::createSocket() const
{
    return (const_cast<QSettingsWrapper *>(this))->connectToServer(QVariantList()
        << d->m_application << d->m_organization << d->m_scope << d->m_format << d->m_filename);
}

} // namespace QInstaller
