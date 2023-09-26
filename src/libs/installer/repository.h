/**************************************************************************
**
** Copyright (C) 2023 The Qt Company Ltd.
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
    explicit Repository(const QUrl &url, bool isDefault, bool compressed = false);

    static void registerMetaType();
    static Repository fromUserInput(const QString &repositoryUrl, bool compressed = false);

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

    QString categoryname() const;
    void setCategoryName(const QString &categoryname);

    QByteArray xmlChecksum() const;
    void setXmlChecksum(const QByteArray &checksum);

    bool isCompressed() const;
    bool postLoadComponentScript() const;
    void setPostLoadComponentScript(const bool postLoad);

    bool operator==(const Repository &other) const;
    bool operator!=(const Repository &other) const;

    hashValue qHash(const Repository &repository);
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
    QString m_categoryname;
    bool m_compressed;
    QByteArray m_xmlChecksum;
    bool m_postLoadComponentScript;
};

inline hashValue qHash(const Repository &repository)
{
    return qHash(repository.url());
}

INSTALLER_EXPORT QDataStream &operator>>(QDataStream &istream, Repository &repository);
INSTALLER_EXPORT QDataStream &operator<<(QDataStream &ostream, const Repository &repository);

} // namespace QInstaller

Q_DECLARE_METATYPE(QInstaller::Repository)

#endif // REPOSITORY_H
