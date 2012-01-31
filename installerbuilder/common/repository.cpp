/**************************************************************************
**
** This file is part of Installer Framework
**
** Copyright (c) 2012 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (info@qt.nokia.com)
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
** Nokia at info@qt.nokia.com.
**
**************************************************************************/
#include "repository.h"

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
{
    registerMetaType();
}

/*!
    Constructs a new repository by setting it's address to \a url and it's default state.
*/
Repository::Repository(const QUrl &url, bool isDefault)
    : m_url(url)
    , m_default(isDefault)
    , m_enabled(true)
{
    registerMetaType();
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
    Compares the values of this repository to \a other and returns true if they are equal (same server,
    default state, enabled state as well as username and password). \sa operator!=()
*/
bool Repository::operator==(const Repository &other) const
{
    return m_url == other.m_url && m_default == other.m_default && m_enabled == other.m_enabled
        && m_username == other.m_username && m_password == other.m_password;
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

    return *this;
}

void Repository::registerMetaType()
{
    qRegisterMetaType<Repository>("Repository");
    qRegisterMetaTypeStreamOperators<Repository>("Repository");
}

QDataStream &operator>>(QDataStream &istream, Repository &repository)
{
    QByteArray url, username, password;
    istream >> url >> repository.m_default >> repository.m_enabled >> username >> password;
    repository.setUrl(QUrl::fromEncoded(QByteArray::fromBase64(url)));
    repository.setUsername(QString::fromUtf8(QByteArray::fromBase64(username)));
    repository.setPassword(QString::fromUtf8(QByteArray::fromBase64(password)));
    return istream;
}

QDataStream &operator<<(QDataStream &ostream, const Repository &repository)
{
    return ostream << repository.m_url.toEncoded().toBase64() << repository.m_default << repository.m_enabled
        << repository.m_username.toUtf8().toBase64() << repository.m_password.toUtf8().toBase64();
}

}
