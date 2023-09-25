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

#ifndef REPOSITORYCATEGORY_H
#define REPOSITORYCATEGORY_H

#include "installer_global.h"
#include "repository.h"
#include "qinstallerglobal.h"

#include <QtCore/QMetaType>
#include <QtCore/QUrl>
#include <QSet>

namespace QInstaller {

class INSTALLER_EXPORT RepositoryCategory
{

public:
    explicit RepositoryCategory();
    RepositoryCategory(const RepositoryCategory &other);

    static void registerMetaType();

    QString displayname() const;
    void setDisplayName(const QString &displayname);

    QString tooltip() const;
    void setTooltip(const QString &tooltip);

    QSet<Repository> repositories() const;
    void setRepositories(const QSet<Repository> repositories, const bool replace = false);
    void addRepository(const Repository &repository);

    bool isEnabled() const;
    void setEnabled(bool enabled);

    bool operator==(const RepositoryCategory &other) const;
    bool operator!=(const RepositoryCategory &other) const;

    hashValue qHash(const RepositoryCategory &repository);

    friend INSTALLER_EXPORT QDataStream &operator>>(QDataStream &istream, RepositoryCategory &repository);
    friend INSTALLER_EXPORT QDataStream &operator<<(QDataStream &ostream, const RepositoryCategory &repository);

private:
    QMultiHash<QString, QVariant> m_data;
    QString m_displayname;
    QString m_tooltip;
    bool m_enabled;
};

inline hashValue qHash(const RepositoryCategory &repository)
{
    return qHash(repository.displayname());
}

INSTALLER_EXPORT QDataStream &operator>>(QDataStream &istream, RepositoryCategory &repository);
INSTALLER_EXPORT QDataStream &operator<<(QDataStream &ostream, const RepositoryCategory &repository);

} // namespace QInstaller

Q_DECLARE_METATYPE(QInstaller::RepositoryCategory)

#endif // REPOSITORYCATEGORY_H
