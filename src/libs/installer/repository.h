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

    static void registerMetaType();
    static Repository fromUserInput(const QString &repositoryUrl);

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

    QString displayname() const;
    void setDisplayName(const QString &displayname);

    bool operator==(const Repository &other) const;
    bool operator!=(const Repository &other) const;

    uint qHash(const Repository &repository);
    const Repository &operator=(const Repository &other);

    friend INSTALLER_EXPORT QDataStream &operator>>(QDataStream &istream, Repository &repository);
    friend INSTALLER_EXPORT QDataStream &operator<<(QDataStream &ostream, const Repository &repository);

private:
    QUrl m_url;
    bool m_default;
    bool m_enabled;
    QString m_username;
    QString m_password;
    QString m_displayname;
};

inline uint qHash(const Repository &repository)
{
    return qHash(repository.url());
}

INSTALLER_EXPORT QDataStream &operator>>(QDataStream &istream, Repository &repository);
INSTALLER_EXPORT QDataStream &operator<<(QDataStream &ostream, const Repository &repository);

} // namespace QInstaller

Q_DECLARE_METATYPE(QInstaller::Repository)

#endif // REPOSITORY_H
