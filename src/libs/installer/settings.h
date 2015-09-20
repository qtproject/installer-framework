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

#ifndef SETTINGS_H
#define SETTINGS_H

#include "constants.h"
#include "installer_global.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QSharedDataPointer>
#include <QtCore/QStringList>
#include <QtCore/QVariant>

#include <QtNetwork/QNetworkProxy>

namespace QInstaller {
class Repository;
typedef QHash<QString, QPair<Repository, Repository> > RepoHash;

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

    enum ParseMode {
        StrictParseMode,
        RelaxedParseMode
    };
    explicit Settings();
    ~Settings();

    Settings(const Settings &other);
    Settings &operator=(const Settings &other);

    static Settings fromFileAndPrefix(const QString &path, const QString &prefix,
        ParseMode parseMode = StrictParseMode);

    QString logo() const;
    QString title() const;
    QString publisher() const;
    QString url() const;
    QString watermark() const;
    QString banner() const;
    QString background() const;
    QString installerApplicationIcon() const;
    QString installerWindowIcon() const;
    QString systemIconSuffix() const;
    QString wizardStyle() const;
    QString styleSheet() const;
    QString titleColor() const;
    int wizardDefaultWidth() const;
    int wizardDefaultHeight() const;

    QString applicationName() const;
    QString version() const;

    QString runProgram() const;
    QStringList runProgramArguments() const;
    void setRunProgramArguments(const QStringList &arguments);
    QString runProgramDescription() const;

    QString startMenuDir() const;
    QString targetDir() const;
    QString adminTargetDir() const;

    QString removeTargetDir() const;
    QString maintenanceToolName() const;
    QString maintenanceToolIniFile() const;

    QString configurationFileName() const;

    bool createLocalRepository() const;
    bool installActionColumnVisible() const;

    bool dependsOnLocalInstallerBinary() const;
    bool hasReplacementRepos() const;
    QSet<Repository> repositories() const;

    QSet<Repository> defaultRepositories() const;
    void setDefaultRepositories(const QSet<Repository> &repositories);
    void addDefaultRepositories(const QSet<Repository> &repositories);
    Settings::Update updateDefaultRepositories(const RepoHash &updates);

    QSet<Repository> temporaryRepositories() const;
    void setTemporaryRepositories(const QSet<Repository> &repositories, bool replace);
    void addTemporaryRepositories(const QSet<Repository> &repositories, bool replace);

    QSet<Repository> userRepositories() const;
    void setUserRepositories(const QSet<Repository> &repositories);
    void addUserRepositories(const QSet<Repository> &repositories);
    Settings::Update updateUserRepositories(const RepoHash &updates);

    bool allowSpaceInPath() const;
    bool allowNonAsciiCharacters() const;

    bool containsValue(const QString &key) const;
    QVariant value(const QString &key, const QVariant &defaultValue = QVariant()) const;
    QVariantList values(const QString &key, const QVariantList &defaultValue = QVariantList()) const;

    bool repositorySettingsPageVisible() const;
    void setRepositorySettingsPageVisible(bool visible);

    Settings::ProxyType proxyType() const;
    void setProxyType(Settings::ProxyType type);

    QNetworkProxy ftpProxy() const;
    void setFtpProxy(const QNetworkProxy &proxy);

    QNetworkProxy httpProxy() const;
    void setHttpProxy(const QNetworkProxy &proxy);

    QStringList translations() const;
    void setTranslations(const QStringList &translations);

    QString controlScript() const;

private:
    class Private;
    QSharedDataPointer<Private> d;
};

}

Q_DECLARE_METATYPE(QInstaller::Settings)

#endif  // SETTINGS_H
