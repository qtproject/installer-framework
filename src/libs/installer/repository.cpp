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

#include "repository.h"
#include "kdupdaterfiledownloaderfactory.h"

#include <QDataStream>
#include <QFileInfo>
#include <QStringList>

namespace QInstaller {

/*
    Constructs an invalid Repository object.
*/
Repository::Repository()
    : m_default(false)
    , m_enabled(false)
{
    registerMetaType();
}

/*!
    Constructs a new repository by using all fields of the given repository \a other.
*/
Repository::Repository(const Repository &other)
    : m_url(other.m_url)
    , m_default(other.m_default)
    , m_enabled(other.m_enabled)
    , m_username(other.m_username)
    , m_password(other.m_password)
    , m_displayname(other.m_displayname)
{
    registerMetaType();
}

/*!
    Constructs a new repository by setting its address to \a url and its default state.
*/
Repository::Repository(const QUrl &url, bool isDefault)
    : m_url(url)
    , m_default(isDefault)
    , m_enabled(true)
{
    registerMetaType();
}

/*!
    Constructs a new repository by setting its address to \a repositoryUrl as string and its
    default state.

    Note: user and password can be inside the \a repositoryUrl string: http://user:password@repository.url
*/
Repository Repository::fromUserInput(const QString &repositoryUrl)
{
    QUrl url = QUrl::fromUserInput(repositoryUrl);
    const QStringList supportedSchemes = KDUpdater::FileDownloaderFactory::supportedSchemes();
    if (!supportedSchemes.contains(url.scheme()) && QFileInfo(url.toString()).exists())
        url = QLatin1String("file:///") + url.toString();

    QString userName = url.userName();
    QString password = url.password();
    url.setUserName(QString());
    url.setPassword(QString());

    Repository repository(url, false);
    repository.setUsername(userName);
    repository.setPassword(password);
    return repository;
}

/*!
    Returns true if the repository URL is valid; otherwise returns false.

    Note: The URL is simply run through a conformance test. It is not checked that the repository
    actually exists.
*/
bool Repository::isValid() const
{
    return m_url.isValid();
}

/*!
    Returns true if the repository was set using the package manager configuration file; otherwise returns
    false.
*/
bool Repository::isDefault() const
{
    return m_default;
}

/*!
    Returns the URL of the repository. By default an invalid \sa QUrl is returned.
*/
QUrl Repository::url() const
{
    return m_url;
}

/*!
    Sets the repository URL to the one specified at \a url.
*/
void Repository::setUrl(const QUrl &url)
{
    m_url = url;
}

/*!
    Returns whether the repository is enabled and used during information retrieval.
*/
bool Repository::isEnabled() const
{
    return m_enabled;
}

/*!
    Sets this repository to \n enabled state and thus to use this repository for information retrieval or not.
*/
void Repository::setEnabled(bool enabled)
{
    m_enabled = enabled;
}

/*!
    Returns the user name used for authentication.
*/
QString Repository::username() const
{
    return m_username;
}

/*!
    Sets the user name for authentication to be \a username.
*/
void Repository::setUsername(const QString &username)
{
    m_username = username;
}

/*!
    Returns the password used for authentication.
*/
QString Repository::password() const
{
    return m_password;
}

/*!
    Sets the password for authentication to be \a password.
*/
void Repository::setPassword(const QString &password)
{
    m_password = password;
}

/*!
    Returns the Name for the repository to be displayed instead of the URL
*/
QString Repository::displayname() const
{
    return m_displayname.isEmpty() ? m_url.toString() : m_displayname;
}

/*!
    Sets the DisplayName of the repository to \a displayname.
*/
void Repository::setDisplayName(const QString &displayname)
{
    m_displayname = displayname;
}

/*!
    Compares the values of this repository to \a other and returns true if they are equal (same server,
    default state, enabled state as well as username and password). \sa operator!=()
*/
bool Repository::operator==(const Repository &other) const
{
    return m_url == other.m_url && m_default == other.m_default && m_enabled == other.m_enabled
        && m_username == other.m_username && m_password == other.m_password && m_displayname == other.m_displayname;
}

/*!
    Returns true if the \a other repository is not equal to this repository; otherwise returns false. Two
    repositories are considered equal if they contain the same elements. \sa operator==()
*/
bool Repository::operator!=(const Repository &other) const
{
    return !(*this == other);
}

/*!
    Assigns the values of repository \a other to this repository.
*/
const Repository &Repository::operator=(const Repository &other)
{
    if (this == &other)
        return *this;

    m_url = other.m_url;
    m_default = other.m_default;
    m_enabled = other.m_enabled;
    m_username = other.m_username;
    m_password = other.m_password;
    m_displayname = other.m_displayname;

    return *this;
}

void Repository::registerMetaType()
{
    qRegisterMetaType<Repository>("Repository");
    qRegisterMetaTypeStreamOperators<Repository>("Repository");
}

QDataStream &operator>>(QDataStream &istream, Repository &repository)
{
    QByteArray url, username, password, displayname;
    istream >> url >> repository.m_default >> repository.m_enabled >> username >> password >> displayname;
    repository.setUrl(QUrl::fromEncoded(QByteArray::fromBase64(url)));
    repository.setUsername(QString::fromUtf8(QByteArray::fromBase64(username)));
    repository.setPassword(QString::fromUtf8(QByteArray::fromBase64(password)));
    repository.setDisplayName(QString::fromUtf8(QByteArray::fromBase64(displayname)));
    return istream;
}

QDataStream &operator<<(QDataStream &ostream, const Repository &repository)
{
    return ostream << repository.m_url.toEncoded().toBase64() << repository.m_default << repository.m_enabled
        << repository.m_username.toUtf8().toBase64() << repository.m_password.toUtf8().toBase64()
        << repository.m_displayname.toUtf8().toBase64();
}

}
