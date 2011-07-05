/**************************************************************************
**
** This file is part of Qt SDK**
**
** Copyright (c) 2010 Nokia Corporation and/or its subsidiary(-ies).*
**
** Contact:  Nokia Corporation qt-info@nokia.com**
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
** rights. These rights are described in the Nokia Qt LGPL Exception version
** 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** If you are unsure which license is appropriate for your use, please contact
** (qt-info@nokia.com).
**
**************************************************************************/
#ifndef QINSTALLER_GLOBAL_H
#define QINSTALLER_GLOBAL_H

#include <installer_global.h>

#include <KDUpdater/Update>
#include <KDUpdater/UpdateOperation>
#include <KDUpdater/PackagesInfo>

#include <QtCore/QString>

QT_BEGIN_NAMESPACE
class QScriptContext;
class QScriptEngine;
class QScriptValue;
QT_END_NAMESPACE


namespace QInstaller {

enum INSTALLER_EXPORT RunMode
{
    AllMode,
    UpdaterMode
};

enum INSTALLER_EXPORT JobError
{
    InvalidUrl = 0x24B04,
    Timeout,
    DownloadError,
    InvalidUpdatesXml,
    InvalidMetaInfo,
    ExtractionError,
    UserIgnoreError
};

typedef KDUpdater::UpdateOperation Operation;
typedef QList<QInstaller::Operation*> OperationList;

typedef KDUpdater::Update Package;
typedef QList<QInstaller::Package*> PackagesList;

typedef KDUpdater::PackageInfo LocalPackage;
typedef QHash<QString, LocalPackage> LocalPackagesHash;

QString uncaughtExceptionString(QScriptEngine *scriptEngine);
QScriptValue qInstallerComponentByName(QScriptContext *context, QScriptEngine *engine);

QScriptValue qDesktopServicesOpenUrl(QScriptContext *context, QScriptEngine *engine);
QScriptValue qDesktopServicesDisplayName(QScriptContext *context, QScriptEngine *engine);
QScriptValue qDesktopServicesStorageLocation(QScriptContext *context, QScriptEngine *engine);

// constants used throughout several classes
static const QLatin1String scTrue("true");
static const QLatin1String scFalse("false");

static const QLatin1String scName("Name");
static const QLatin1String scVersion("Version");
static const QLatin1String scReplaces("Replaces");
static const QLatin1String scImportant("Important");
static const QLatin1String scTargetDir("TargetDir");
static const QLatin1String scReleaseDate("ReleaseDate");
static const QLatin1String scDescription("Description");
static const QLatin1String scDisplayName("DisplayName");
static const QLatin1String scDependencies("Dependencies");
static const QLatin1String scNewComponent("NewComponent");
static const QLatin1String scRepositories("Repositories");
static const QLatin1String scCompressedSize("CompressedSize");
static const QLatin1String scInstalledVersion("InstalledVersion");
static const QLatin1String scUncompressedSize("UncompressedSize");
static const QLatin1String scForcedInstallation("ForcedInstallation");
static const QLatin1String scRequiresAdminRights("RequiresAdminRights");

// constants used throughout the components class
static const QLatin1String scScript("Script");
static const QLatin1String scVirtual("Virtual");
static const QLatin1String scDefault("Default");
static const QLatin1String scInstalled("Installed");
static const QLatin1String scUpdateText("UpdateText");
static const QLatin1String scUninstalled("Uninstalled");
static const QLatin1String scCurrentState("CurrentState");
static const QLatin1String scInstallPriority("InstallPriority");
static const QLatin1String scSortingPriority("SortingPriority");

// constants used throughout the settings class
static const QLatin1String scIcon("Icon");
static const QLatin1String scLogo("Logo");
static const QLatin1String scTitle("Title");
static const QLatin1String scPages("Pages");
static const QLatin1String scPrefix("Prefix");
static const QLatin1String scLogoSmall("LogoSmall");
static const QLatin1String scPublisher("Publisher");
static const QLatin1String scWatermark("Watermark");
static const QLatin1String scPublicKey("PublicKey");
static const QLatin1String scPrivateKey("PrivateKey");
static const QLatin1String scProductUrl("ProductUrl");
static const QLatin1String scBackground("Background");
static const QLatin1String scRunProgram("RunProgram");
static const QLatin1String scStartMenuDir("StartMenuDir");
static const QLatin1String scAdminTargetDir("AdminTargetDir");
static const QLatin1String scRemoveTargetDir("RemoveTargetDir");
static const QLatin1String scUninstallerName("UninstallerName");
static const QLatin1String scMaintenanceTitle("MaintenanceTitle");
static const QLatin1String scUserRepositories("UserRepositories");
static const QLatin1String scUninstallerIniFile("UninstallerIniFile");
static const QLatin1String scRemoteRepositories("RemoteRepositories");
static const QLatin1String scSigningCertificate("SigningCertificate");
static const QLatin1String scRunProgramDescription("RunProgramDescription");
static const QLatin1String scTargetConfigurationFile("TargetConfigurationFile");
}

#endif // QINSTALLER_GLOBAL_H
