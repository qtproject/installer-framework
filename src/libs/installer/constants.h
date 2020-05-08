/**************************************************************************
**
** Copyright (C) 2020 The Qt Company Ltd.
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

#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <QtCore/QString>
#include <QtCore/QStringList>

namespace QInstaller {

// constants used throughout several classes
static const QLatin1String scTrue("true");
static const QLatin1String scFalse("false");
static const QLatin1String scScript("script");
static const QLatin1String scAllUsersStartMenuProgramsPath("AllUsersStartMenuProgramsPath");
static const QLatin1String scUserStartMenuProgramsPath("UserStartMenuProgramsPath");

static const QLatin1String scName("Name");
static const QLatin1String scVersion("Version");
static const QLatin1String scDefault("Default");
static const QLatin1String scDisplayVersion("DisplayVersion");
static const QLatin1String scRemoteDisplayVersion("RemoteDisplayVersion");
static const QLatin1String scInheritVersion("inheritVersionFrom");
static const QLatin1String scReplaces("Replaces");
static const QLatin1String scDownloadableArchives("DownloadableArchives");
static const QLatin1String scEssential("Essential");
static const QLatin1String scTargetDir("TargetDir");
static const QLatin1String scReleaseDate("ReleaseDate");
static const QLatin1String scDescription("Description");
static const QLatin1String scDisplayName("DisplayName");
static const QLatin1String scDependencies("Dependencies");
static const QLatin1String scAutoDependOn("AutoDependOn");
static const QLatin1String scNewComponent("NewComponent");
static const QLatin1String scRepositories("Repositories");
static const QLatin1String scCompressedSize("CompressedSize");
static const QLatin1String scInstalledVersion("InstalledVersion");
static const QLatin1String scUncompressedSize("UncompressedSize");
static const QLatin1String scUncompressedSizeSum("UncompressedSizeSum");
static const QLatin1String scRequiresAdminRights("RequiresAdminRights");
static const QLatin1String scSHA1("SHA1");

// constants used throughout the components class
static const QLatin1String scVirtual("Virtual");
static const QLatin1String scSortingPriority("SortingPriority");
static const QLatin1String scCheckable("Checkable");

// constants used throughout the settings and package manager core class
static const QLatin1String scTitle("Title");
static const QLatin1String scPublisher("Publisher");
static const QLatin1String scRunProgram("RunProgram");
static const QLatin1String scRunProgramArguments("RunProgramArguments");
static const QLatin1String scStartMenuDir("StartMenuDir");
static const QLatin1String scRemoveTargetDir("RemoveTargetDir");
static const QLatin1String scRunProgramDescription("RunProgramDescription");
static const QLatin1String scTargetConfigurationFile("TargetConfigurationFile");
static const QLatin1String scAllowNonAsciiCharacters("AllowNonAsciiCharacters");
static const QLatin1String scDisableAuthorizationFallback("DisableAuthorizationFallback");
static const QLatin1String scDisableCommandLineInterface("DisableCommandLineInterface");
static const QLatin1String scRepositorySettingsPageVisible("RepositorySettingsPageVisible");
static const QLatin1String scAllowSpaceInPath("AllowSpaceInPath");
static const QLatin1String scWizardStyle("WizardStyle");
static const QLatin1String scStyleSheet("StyleSheet");
static const QLatin1String scTitleColor("TitleColor");
static const QLatin1String scWizardDefaultWidth("WizardDefaultWidth");
static const QLatin1String scWizardDefaultHeight("WizardDefaultHeight");
static const QLatin1String scWizardShowPageList("WizardShowPageList");
static const QLatin1String scUrlQueryString("UrlQueryString");
static const QLatin1String scProductUUID("ProductUUID");
static const QLatin1String scAllUsers("AllUsers");
static const QLatin1String scSupportsModify("SupportsModify");
static const QLatin1String scAllowUnstableComponents("AllowUnstableComponents");
static const QLatin1String scSaveDefaultRepositories("SaveDefaultRepositories");
static const QLatin1String scRepositoryCategoryDisplayName("RepositoryCategoryDisplayName");

const char scRelocatable[] = "@RELOCATABLE_PATH@";
}

namespace CommandLineOptions {

// Help & version information
static const QLatin1String scHelpShort("h");
static const QLatin1String scHelpLong("help");
static const QLatin1String scVersionShort("v");
static const QLatin1String scVersionLong("version");

// Output related options
static const QLatin1String scVerboseShort("d");
static const QLatin1String scVerboseLong("verbose");
static const QLatin1String scLoggingRulesShort("g");
static const QLatin1String scLoggingRulesLong("logging-rules");

// Consumer commands
static const QLatin1String scInstallShort("in");
static const QLatin1String scInstallLong("install");
static const QLatin1String scCheckUpdatesShort("ch");
static const QLatin1String scCheckUpdatesLong("check-updates");
static const QLatin1String scUpdateShort("up");
static const QLatin1String scUpdateLong("update");
static const QLatin1String scRemoveShort("rm");
static const QLatin1String scRemoveLong("remove");
static const QLatin1String scListShort("li");
static const QLatin1String scListLong("list");
static const QLatin1String scSearchShort("se");
static const QLatin1String scSearchLong("search");
static const QLatin1String scPurgeShort("pr");
static const QLatin1String scPurgeLong("purge");

// Repository management options
static const QLatin1String scAddRepositoryShort("ar");
static const QLatin1String scAddRepositoryLong("add-repository");
static const QLatin1String scAddTmpRepositoryShort("at");
static const QLatin1String scAddTmpRepositoryLong("add-temp-repository");
static const QLatin1String scSetTmpRepositoryShort("st");
static const QLatin1String scSetTmpRepositoryLong("set-temp-repository");

// Proxy options
static const QLatin1String scSystemProxyShort("sp");
static const QLatin1String scSystemProxyLong("system-proxy");
static const QLatin1String scNoProxyShort("np");
static const QLatin1String scNoProxyLong("no-proxy");

// Starting mode options
static const QLatin1String scStartUpdaterShort("su");
static const QLatin1String scStartUpdaterLong("start-updater");
static const QLatin1String scStartPackageManagerShort("sm");
static const QLatin1String scStartPackageManagerLong("start-package-manager");
static const QLatin1String scStartUninstallerShort("sr");
static const QLatin1String scStartUninstallerLong("start-uninstaller");

// Message acceptance options
static const QLatin1String scAcceptMessageQuery("accept-messages");
static const QLatin1String scRejectMessageQuery("reject-messages");
static const QLatin1String scMessageAutomaticAnswer("auto-answer");
static const QLatin1String scMessageDefaultAnswer("default-answer");
static const QLatin1String scAcceptLicenses("accept-licenses");
static const QLatin1String scFileDialogAutomaticAnswer("file-query");

// Misc installation options
static const QLatin1String scRootShort("t");
static const QLatin1String scRootLong("root");
static const QLatin1String scPlatformShort("p");
static const QLatin1String scPlatformLong("platform");
static const QLatin1String scNoForceInstallationShort("nf");
static const QLatin1String scNoForceInstallationLong("no-force-installations");
static const QLatin1String scNoSizeCheckingShort("ns");
static const QLatin1String scNoSizeCheckingLong("no-size-checking");
static const QLatin1String scShowVirtualComponentsShort("sv");
static const QLatin1String scShowVirtualComponentsLong("show-virtual-components");
static const QLatin1String scInstallCompressedRepositoryShort("i");
static const QLatin1String scInstallCompressedRepositoryLong("install-compressed-repository");
static const QLatin1String scCreateLocalRepositoryShort("c");
static const QLatin1String scCreateLocalRepositoryLong("create-local-repository");
static const QLatin1String scNoDefaultInstallationLong("no-default-installations");

// Developer options
static const QLatin1String scScriptShort("s");
static const QLatin1String scScriptLong("script");
static const QLatin1String scStartServerShort("ss");
static const QLatin1String scStartServerLong("start-server");
static const QLatin1String scStartClientShort("sc");
static const QLatin1String scStartClientLong("start-client");
static const QLatin1String scSquishPortShort("q");
static const QLatin1String scSquishPortLong("squish-port");

// Deprecated options, provided only for backward compatibility
static const QLatin1String scDeprecatedUpdater("updater");
static const QLatin1String scDeprecatedCheckUpdates("checkupdates");

// Options supposed to be used without graphical interface
static const QStringList scCommandLineInterfaceOptions = {
    scInstallShort,
    scInstallLong,
    scCheckUpdatesShort,
    scCheckUpdatesLong,
    scUpdateShort,
    scUpdateLong,
    scRemoveShort,
    scRemoveLong,
    scListShort,
    scListLong,
    scSearchShort,
    scSearchLong,
    scPurgeShort,
    scPurgeLong
};

} // namespace CommandLineOptions
#endif  // CONSTANTS_H
