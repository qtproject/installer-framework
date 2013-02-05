/**************************************************************************
**
** Copyright (C) 2012-2013 Digia Plc and/or its subsidiary(-ies).
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
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
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
    return qHash(repository.url());
}

QDataStream &operator>>(QDataStream &istream, Repository &repository);
QDataStream &operator<<(QDataStream &ostream, const Repository &repository);

} // namespace QInstaller

Q_DECLARE_METATYPE(QInstaller::Repository)

#endif // REPOSITORY_H
