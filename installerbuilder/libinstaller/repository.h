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

#ifndef REPOSITORY_H
#define REPOSITORY_H

#include "installer_global.h"

#include <QtCore/QMetaType>
#include <QtCore/QUrl>

namespace QInstaller {

class INSTALLER_EXPORT Repository
{
public:
    explicit Repository();
    Repository(const Repository &other);
    explicit Repository(const QUrl &url, bool isDefault);

    bool isValid() const;
    bool isDefault() const;

    QUrl url() const;
    void setUrl(const QUrl &url);

    bool isEnabled() const;
    void setEnabled(bool enabled);

    QString username() const;
    void setUsername(const QString &username);

    QString password() const;
    void setPassword(const QString &password);

    bool operator==(const Repository &other) const;
    bool operator!=(const Repository &other) const;

    uint qHash(const Repository &repository);
    const Repository &operator=(const Repository &other);

    friend QDataStream &operator>>(QDataStream &istream, Repository &repository);
    friend QDataStream &operator<<(QDataStream &ostream, const Repository &repository);

private:
    void registerMetaType();

private:
    QUrl m_url;
    bool m_default;
    bool m_enabled;
    QString m_username;
    QString m_password;
};

inline uint qHash(const Repository &repository)
{
    return qHash(repository.url().toString());
}

QDataStream &operator>>(QDataStream &istream, Repository &repository);
QDataStream &operator<<(QDataStream &ostream, const Repository &repository);

} // namespace QInstaller

Q_DECLARE_METATYPE(QInstaller::Repository)

#endif // REPOSITORY_H
