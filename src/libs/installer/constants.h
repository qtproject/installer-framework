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
static const QLatin1String scRepositorySettingsPageVisible("RepositorySettingsPageVisible");
static const QLatin1String scAllowSpaceInPath("AllowSpaceInPath");
static const QLatin1String scWizardStyle("WizardStyle");
static const QLatin1String scStyleSheet("StyleSheet");
static const QLatin1String scTitleColor("TitleColor");
static const QLatin1String scWizardDefaultWidth("WizardDefaultWidth");
static const QLatin1String scWizardDefaultHeight("WizardDefaultHeight");
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

const char HelpShort[] = "h";
const char HelpLong[] = "help";
const char Version[] = "version";
const char FrameworkVersion[] = "framework-version";
const char VerboseShort[] = "v";
const char VerboseLong[] = "verbose";
const char Proxy[] = "proxy";
const char NoProxy[] = "no-proxy";
const char Script[] = "script";
const char CheckUpdates[] = "checkupdates";
const char Updater[] = "updater";
const char ManagePackages[] = "manage-packages";
const char NoForceInstallation[] = "no-force-installations";
const char ShowVirtualComponents[] = "show-virtual-components";
const char LoggingRules[] = "logging-rules";
const char CreateLocalRepository[] = "create-local-repository";
const char AddRepository[] = "addRepository";
const char AddTmpRepository[] = "addTempRepository";
const char SetTmpRepository[] = "setTempRepository";
const char StartServer[] = "startserver";
const char StartClient[] = "startclient";
const char InstallCompressedRepository[] = "installCompressedRepository";
const char SilentUpdate[] = "silentUpdate";
const char UpdatePackages[] = "updatePackages";
const char ListInstalledPackages[] = "listInstalledPackages";
const char ListPackages[] = "listPackages";
const char InstallPackages[] = "installPackages";
const char InstallDefault[] = "installDefault";
const char TargetDir[] = "targetDir";
const char UninstallSelectedPackages[] = "uninstallPackages";
const char Platform[] = "platform";
const char SquishPort[] = "squish-port";

static const QStringList scCommandLineInterfaceOptions = {
    QLatin1String(ListInstalledPackages),
    QLatin1String(ListPackages),
    QLatin1String(UpdatePackages),
    QLatin1String(InstallPackages),
    QLatin1String(InstallDefault),
    QLatin1String(UninstallSelectedPackages),
    QLatin1String(CheckUpdates)
};

} // namespace CommandLineOptions
#endif  // CONSTANTS_H
