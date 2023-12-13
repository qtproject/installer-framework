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

#ifndef SETTINGS_H
#define SETTINGS_H

#include "constants.h"
#include "installer_global.h"
#include "repositorycategory.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QSharedDataPointer>
#include <QtCore/QStringList>
#include <QtCore/QVariant>

#include <QtNetwork/QNetworkProxy>

namespace QInstaller {
class Repository;
typedef QMultiHash<QString, QPair<Repository, Repository> > RepoHash;

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
    QString pageListPixmap() const;
    QString installerApplicationIcon() const;
    QString installerWindowIcon() const;
    QString systemIconSuffix() const;
    QString wizardStyle() const;
    QString styleSheet() const;
    QString titleColor() const;
    int wizardDefaultWidth() const;
    int wizardDefaultHeight() const;
    int wizardMinimumWidth() const;
    int wizardMinimumHeight() const;
    bool wizardShowPageList() const;
    QMap<QString, QVariant> productImages() const;
    void setProductImages(const QMap<QString, QVariant> &images);

    QString aliasDefinitionsFile() const;

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
    QString maintenanceToolAlias() const;

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

    QSet<RepositoryCategory> repositoryCategories() const;
    QMap<QString, RepositoryCategory> organizedRepositoryCategories() const;
    void setRepositoryCategories(const QSet<RepositoryCategory> &repositories);
    void addRepositoryCategories(const QSet<RepositoryCategory> &repositories);
    Settings::Update updateRepositoryCategories(const RepoHash &updates);

    QSet<Repository> temporaryRepositories() const;
    void setTemporaryRepositories(const QSet<Repository> &repositories, bool replace);
    void addTemporaryRepositories(const QSet<Repository> &repositories, bool replace);

    QSet<Repository> userRepositories() const;
    void setUserRepositories(const QSet<Repository> &repositories);
    void addUserRepositories(const QSet<Repository> &repositories);
    Settings::Update updateUserRepositories(const RepoHash &updates);

    bool allowSpaceInPath() const;
    bool allowNonAsciiCharacters() const;
    bool allowRepositoriesForOfflineInstaller() const;

    bool disableAuthorizationFallback() const;
    bool disableCommandLineInterface() const;

    bool containsValue(const QString &key) const;
    QVariant value(const QString &key, const QVariant &defaultValue = QVariant()) const;
    QVariantList values(const QString &key, const QVariantList &defaultValue = QVariantList()) const;

    bool repositorySettingsPageVisible() const;
    void setRepositorySettingsPageVisible(bool visible);

    bool persistentLocalCache() const;
    void setPersistentLocalCache(bool enable);

    QString localCacheDir() const;
    void setLocalCacheDir(const QString &dir);

    QString localCachePath() const;
    void setLocalCachePath(const QString &path);

    Settings::ProxyType proxyType() const;
    void setProxyType(Settings::ProxyType type);

    QNetworkProxy ftpProxy() const;
    void setFtpProxy(const QNetworkProxy &proxy);

    QNetworkProxy httpProxy() const;
    void setHttpProxy(const QNetworkProxy &proxy);

    QStringList translations() const;
    void setTranslations(const QStringList &translations);

    QString controlScript() const;

    bool supportsModify() const;

    bool allowUnstableComponents() const;
    void setAllowUnstableComponents(bool allow);

    bool saveDefaultRepositories() const;
    void setSaveDefaultRepositories(bool save);

    QString repositoryCategoryDisplayName() const;
    void setRepositoryCategoryDisplayName(const QString &displayName);

private:
    class Private;
    QSharedDataPointer<Private> d;
};

}

Q_DECLARE_METATYPE(QInstaller::Settings)

#endif  // SETTINGS_H
