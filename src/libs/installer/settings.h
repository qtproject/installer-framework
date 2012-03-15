/**************************************************************************
**
** This file is part of Installer Framework
**
** Copyright (c) 2010-2012 Nokia Corporation and/or its subsidiary(-ies).
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

#ifndef SETTINGS_H
#define SETTINGS_H

#include "constants.h"
#include "installer_global.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QSharedDataPointer>
#include <QtCore/QVariant>

#include <QtNetwork/QNetworkProxy>

Q_DECLARE_METATYPE(QNetworkProxy)

namespace QInstaller {
class Repository;

class INSTALLER_EXPORT Settings
{
    Q_DECLARE_TR_FUNCTIONS(Settings)

public:
    enum Update {
        UpdatesApplied,
        NoUpdatesApplied
    };

    enum ProxyType {
        NoProxy,
        SystemProxy,
        UserDefinedProxy
    };

    explicit Settings();
    ~Settings();

    Settings(const Settings &other);
    Settings &operator=(const Settings &other);

    static Settings fromFileAndPrefix(const QString &path, const QString &prefix);

    QString logo() const;
    QString logoSmall() const;
    QString title() const;
    QString publisher() const;
    QString url() const;
    QString watermark() const;
    QString background() const;
    QString icon() const;

    QString applicationName() const;
    QString applicationVersion() const;

    QString runProgram() const;
    QString runProgramDescription() const;
    QString startMenuDir() const;
    QString targetDir() const;
    QString adminTargetDir() const;

    QString removeTargetDir() const;
    QString uninstallerName() const;
    QString uninstallerIniFile() const;

    QString configurationFileName() const;

    bool hasReplacementRepos() const;
    QSet<Repository> repositories() const;

    QSet<Repository> defaultRepositories() const;
    void setDefaultRepositories(const QSet<Repository> &repositories);
    void addDefaultRepositories(const QSet<Repository> &repositories);
    Settings::Update updateDefaultRepositories(const QHash<QString, QPair<Repository, Repository> > &updates);

    QSet<Repository> temporaryRepositories() const;
    void setTemporaryRepositories(const QSet<Repository> &repositories, bool replace);
    void addTemporaryRepositories(const QSet<Repository> &repositories, bool replace);

    QSet<Repository> userRepositories() const;
    void setUserRepositories(const QSet<Repository> &repositories);
    void addUserRepositories(const QSet<Repository> &repositories);

    QStringList certificateFiles() const;
    bool allowNoneAsciiCharacters() const;

    QVariant value(const QString &key, const QVariant &defaultValue = QVariant()) const;
    QVariantList values(const QString &key, const QVariantList &defaultValue = QVariantList()) const;

    QVariantHash titlesForPage(const QString &pageName) const;
    QVariantHash subTitlesForPage(const QString &pageName) const;

    Settings::ProxyType proxyType() const;
    void setProxyType(Settings::ProxyType type);

    QNetworkProxy ftpProxy() const;
    void setFtpProxy(const QNetworkProxy &proxy);

    QNetworkProxy httpProxy() const;
    void setHttpProxy(const QNetworkProxy &proxy);

private:
    class Private;
    QSharedDataPointer<Private> d;
};

}

Q_DECLARE_METATYPE(QInstaller::Settings)

#endif  // SETTINGS_H
