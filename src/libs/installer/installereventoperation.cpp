#include "installereventoperation.h"

#include "packagemanagercore.h"
#include "globals.h"

#include <QtCore/QDebug>

using namespace QInstaller;

InstallerEventOperation::InstallerEventOperation(PackageManagerCore *core)
    : UpdateOperation(core)
{
    setName(QLatin1String("InstallerEvent"));
}

void InstallerEventOperation::backup()
{
}

bool InstallerEventOperation::performOperation()
{
    const QStringList args = arguments();

    if (args.isEmpty()) return false;

    const QString action = args.at(0);

    if (action == QString::fromLatin1("init"))
    {
        return sendInit(args);
    }

    if (action == QString::fromLatin1("uninstaller"))
    {
        return sendUninstallerEvent(args);
    }

    if (action == QString::fromLatin1("installer"))
    {
        return sendInstallerEvent(args);
    }

    return false;
}

// init takes: the following parameters
//   1. region: "world" | "china"
//   2. version: "1.0.x"
//   3. environment: "release" | "dev"
//   4. provider: "none" | "A" | "B" | ...
bool InstallerEventOperation::sendInit(QStringList args)
{
    if (args.size() != 5) return false;

    // Get region
    eve_launcher::application::Application_Region region = eve_launcher::application::Application_Region_REGION_WORLD;
    if (args.at(1) == QString::fromLatin1("china"))
    {
        region = eve_launcher::application::Application_Region_REGION_CHINA;
    }

    // Get version
    QString version = args.at(2);

    // Get build type
    eve_launcher::application::Application_BuildType buildType = eve_launcher::application::Application_BuildType_BUILDTYPE_DEV;
    if (args.at(3) == QString::fromLatin1("release"))
    {
        buildType = eve_launcher::application::Application_BuildType_BUILDTYPE_RELEASE;
    }

    // Get provider
    bool provider = false;
    QString providerName = QString::fromLatin1("");
    if (!args.at(4).isEmpty() && args.at(4) != QString::fromLatin1("none"))
    {
        provider = true;
        providerName = args.at(4);
    }

    EventLogger::instance()->initialize(region, version, buildType, provider, providerName);

    return true;
}

eve_launcher::uninstaller::Page InstallerEventOperation::toUninstallerPage(bool *ok, QString value)
{
    int id = value.toInt(ok);
    if (!*ok) return eve_launcher::uninstaller::PAGE_UNSPECIFIED;
    if (!eve_launcher::uninstaller::Page_IsValid(id)) return eve_launcher::uninstaller::PAGE_UNSPECIFIED;

    return static_cast<eve_launcher::uninstaller::Page>(id);
}

eve_launcher::installer::Page InstallerEventOperation::toInstallerPage(bool *ok, QString value)
{
    int id = value.toInt(ok);
    if (!*ok) return eve_launcher::installer::PAGE_UNSPECIFIED;
    if (!eve_launcher::installer::Page_IsValid(id)) return eve_launcher::installer::PAGE_UNSPECIFIED;

    return static_cast<eve_launcher::installer::Page>(id);
}

eve_launcher::installer::Component InstallerEventOperation::toInstallerComponent(bool *ok, QString value)
{
    int id = value.toInt(ok);
    if (!*ok) return eve_launcher::installer::COMPONENT_UNSPECIFIED;
    if (!eve_launcher::installer::Component_IsValid(id)) return eve_launcher::installer::COMPONENT_UNSPECIFIED;

    return static_cast<eve_launcher::installer::Component>(id);
}

eve_launcher::installer::RedistVersion InstallerEventOperation::toInstallerRedistVersion(bool *ok, QString value)
{
    int id = value.toInt(ok);
    if (!*ok) return eve_launcher::installer::REDISTVERSION_UNSPECIFIED;
    if (!eve_launcher::installer::RedistVersion_IsValid(id)) return eve_launcher::installer::REDISTVERSION_UNSPECIFIED;

    return static_cast<eve_launcher::installer::RedistVersion>(id);
}

bool InstallerEventOperation::sendUninstallerEvent(QStringList args)
{
    if (args.size() < 2) return false;

    const QString event = args.at(1);
    bool ok;
    int id;
    
    if (event == QString::fromLatin1("Started") && args.size() == 3)
    {
        int duration = args.at(2).toInt(&ok);
        if (!ok) return false;

        EventLogger::instance()->uninstallerStarted(duration);
    }
    else if (event == QString::fromLatin1("PageDisplayed") && args.size() == 5)
    {
        auto previousPage = toUninstallerPage(&ok, args.at(2));
        if (!ok) return false;

        auto currentPage = toUninstallerPage(&ok, args.at(3));
        if (!ok) return false;

        id = args.at(4).toInt(&ok);
        if (!ok) return false;
        if (!eve_launcher::uninstaller::PageDisplayed_FlowDirection_IsValid(id)) return false;
        auto flow = static_cast<eve_launcher::uninstaller::PageDisplayed_FlowDirection>(id);

        EventLogger::instance()->uninstallerPageDisplayed(previousPage, currentPage, flow);
    }
    else if (event == QString::fromLatin1("ShutDown") && args.size() == 5)
    {
        auto page = toUninstallerPage(&ok, args.at(2));
        if (!ok) return false;

        id = args.at(3).toInt(&ok);
        if (!ok) return false;
        if (!eve_launcher::uninstaller::ShutDown_State_IsValid(id)) return false;
        auto state = static_cast<eve_launcher::uninstaller::ShutDown_State>(id);

        bool finishButton = QVariant(args.at(4)).toBool();

        EventLogger::instance()->uninstallerShutDown(page, state, finishButton);
    }
    else if (event == QString::fromLatin1("DetailsDisplayed"))
    {
        EventLogger::instance()->uninstallerDetailsDisplayed();
    }
    else if (event == QString::fromLatin1("DetailsHidden"))
    {
        EventLogger::instance()->uninstallerDetailsHidden();
    }
    else if (event == QString::fromLatin1("UninstallationStarted"))
    {
        EventLogger::instance()->uninstallerUninstallationStarted();
    }
    else if (event == QString::fromLatin1("UninstallationInterrupted") && args.size() == 3)
    {
        int duration = args.at(2).toInt(&ok);
        if (!ok) return false;

        EventLogger::instance()->uninstallerUninstallationInterrupted(duration);
    }
    else if (event == QString::fromLatin1("UninstallationFinished") && args.size() == 3)
    {
        int duration = args.at(2).toInt(&ok);
        if (!ok) return false;

        EventLogger::instance()->uninstallerUninstallationFinished(duration);
    }
    else if (event == QString::fromLatin1("UninstallationFailed") && args.size() == 3)
    {
        int duration = args.at(2).toInt(&ok);
        if (!ok) return false;

        EventLogger::instance()->uninstallerUninstallationFailed(duration);
    }
    else if (event == QString::fromLatin1("ErrorEncountered") && args.size() == 4)
    {
        id = args.at(2).toInt(&ok);
        if (!ok) return false;
        if (!eve_launcher::uninstaller::ErrorEncountered_ErrorCode_IsValid(id)) return false;
        auto code = static_cast<eve_launcher::uninstaller::ErrorEncountered_ErrorCode>(id);
        
        auto page = toUninstallerPage(&ok, args.at(3));
        if (!ok) return false;

        EventLogger::instance()->uninstallerErrorEncountered(code, page);
    }
    else if (event == QString::fromLatin1("AnalyticsMessageSent") && args.size() == 3)
    {
        QString message = args.at(2);

        EventLogger::instance()->uninstallerAnalyticsMessageSent(message);
    }
    else
    {
        return false;
    }

    return true;
}

bool InstallerEventOperation::sendInstallerEvent(QStringList args)
{
    if (args.size() < 2) return false;

    const QString event = args.at(1);
    bool ok;
    int id;

    if (event == QString::fromLatin1("Started") && args.size() == 3)
    {
        int duration = args.at(2).toInt(&ok);
        if (!ok) return false;

        EventLogger::instance()->installerStarted(duration);
    }
    else if (event == QString::fromLatin1("PageDisplayed") && args.size() == 5)
    {
        auto previousPage = toInstallerPage(&ok, args.at(2));
        if (!ok) return false;

        auto currentPage = toInstallerPage(&ok, args.at(3));
        if (!ok) return false;

        id = args.at(4).toInt(&ok);
        if (!ok) return false;
        if (!eve_launcher::installer::PageDisplayed_FlowDirection_IsValid(id)) return false;
        auto flow = static_cast<eve_launcher::installer::PageDisplayed_FlowDirection>(id);

        EventLogger::instance()->installerPageDisplayed(previousPage, currentPage, flow);
    }
    else if (event == QString::fromLatin1("ShutDown") && args.size() == 5)
    {
        auto page = toInstallerPage(&ok, args.at(2));
        if (!ok) return false;

        id = args.at(3).toInt(&ok);
        if (!ok) return false;
        if (!eve_launcher::installer::ShutDown_State_IsValid(id)) return false;
        auto state = static_cast<eve_launcher::installer::ShutDown_State>(id);

        bool finishButton = QVariant(args.at(4)).toBool();

        EventLogger::instance()->installerShutDown(page, state, finishButton);
    }
    else if (event == QString::fromLatin1("PreparationStarted"))
    {
        EventLogger::instance()->installerPreparationStarted();
    }
    else if (event == QString::fromLatin1("PreparationFinished") && args.size() == 3)
    {
        int duration = args.at(2).toInt(&ok);
        if (!ok) return false;

        EventLogger::instance()->installerPreparationFinished(duration);
    }
    else if (event == QString::fromLatin1("LocationChanged") && args.size() == 5)
    {
        id = args.at(2).toInt(&ok);
        if (!ok) return false;
        if (!eve_launcher::installer::LocationChanged_Source_IsValid(id)) return false;
        auto source = static_cast<eve_launcher::installer::LocationChanged_Source>(id);

        id = args.at(3).toInt(&ok);
        if (!ok) return false;
        if (!eve_launcher::installer::LocationChanged_Provider_IsValid(id)) return false;
        auto provider = static_cast<eve_launcher::installer::LocationChanged_Provider>(id);

        QString path = args.at(4);

        EventLogger::instance()->installerLocationChanged(source, provider, path);
    }
    else if (event == QString::fromLatin1("DetailsDisplayed"))
    {
        EventLogger::instance()->installerDetailsDisplayed();
    }
    else if (event == QString::fromLatin1("DetailsHidden"))
    {
        EventLogger::instance()->installerDetailsHidden();
    }
    else if (event == QString::fromLatin1("AutoRunEnabled"))
    {
        EventLogger::instance()->installerAutoRunEnabled();
    }
    else if (event == QString::fromLatin1("AutoRunDisabled"))
    {
        EventLogger::instance()->installerAutoRunDisabled();
    }
    else if (event == QString::fromLatin1("EulaAccepted"))
    {
        EventLogger::instance()->installerEulaAccepted();
    }
    else if (event == QString::fromLatin1("EulaDeclined"))
    {
        EventLogger::instance()->installerEulaDeclined();
    }
    else if (event == QString::fromLatin1("RedistSearchConcluded") && args.size() == 4)
    {
        auto version = toInstallerRedistVersion(&ok, args.at(2));
        if (!ok) return false;

        id = args.at(3).toInt(&ok);
        if (!ok) return false;
        if (!eve_launcher::installer::RedistSearchConcluded_RedistReason_IsValid(id)) return false;
        auto reason = static_cast<eve_launcher::installer::RedistSearchConcluded_RedistReason>(id);

        EventLogger::instance()->installerRedistSearchConcluded(version, reason);
    }
    else if (event == QString::fromLatin1("ProvidedClientFound"))
    {
        EventLogger::instance()->installerProvidedClientFound();
    }
    else if (event == QString::fromLatin1("SharedCacheMessageShown"))
    {
        EventLogger::instance()->installerSharedCacheMessageShown();
    }
    else if (event == QString::fromLatin1("SharedCacheMessageClosed") && args.size() == 4)
    {
        id = args.at(2).toInt(&ok);
        if (!ok) return false;
        if (!eve_launcher::installer::MessageBoxButton_IsValid(id)) return false;
        auto messageBoxButton = static_cast<eve_launcher::installer::MessageBoxButton>(id);

        int timeDisplayed = args.at(3).toInt(&ok);
        if (!ok) return false;
        
        EventLogger::instance()->installerSharedCacheMessageClosed(messageBoxButton, timeDisplayed);
    }
    else if (event == QString::fromLatin1("InstallationStarted"))
    {
        EventLogger::instance()->installerInstallationStarted();
    }
    else if (event == QString::fromLatin1("InstallationInterrupted") && args.size() == 3)
    {
        int duration = args.at(2).toInt(&ok);
        if (!ok) return false;

        EventLogger::instance()->installerInstallationInterrupted(duration);
    }
    else if (event == QString::fromLatin1("InstallationFinished") && args.size() == 3)
    {
        int duration = args.at(2).toInt(&ok);
        if (!ok) return false;

        EventLogger::instance()->installerInstallationFinished(duration);
    }
    else if (event == QString::fromLatin1("InstallationFailed") && args.size() == 3)
    {
        int duration = args.at(2).toInt(&ok);
        if (!ok) return false;

        EventLogger::instance()->installerInstallationFailed(duration);
    }
    else if (event == QString::fromLatin1("UninstallerCreationStarted"))
    {
        EventLogger::instance()->installerUninstallerCreationStarted();
    }
    else if (event == QString::fromLatin1("UninstallerCreationFinished") && args.size() == 3)
    {
        int duration = args.at(2).toInt(&ok);
        if (!ok) return false;

        EventLogger::instance()->installerUninstallerCreationFinished(duration);
    }
    else if (event == QString::fromLatin1("ComponentInitializationStarted") && args.size() == 4)
    {
        auto component = toInstallerComponent(&ok, args.at(2));
        if (!ok) return false;

        auto redistVersion = toInstallerRedistVersion(&ok, args.at(3));
        if (!ok) return false;

        EventLogger::instance()->installerComponentInitializationStarted(component, redistVersion);
    }
    else if (event == QString::fromLatin1("ComponentInitializationFinished") && args.size() == 5)
    {
        auto component = toInstallerComponent(&ok, args.at(2));
        if (!ok) return false;

        auto redistVersion = toInstallerRedistVersion(&ok, args.at(3));
        if (!ok) return false;

        int duration = args.at(4).toInt(&ok);
        if (!ok) return false;

        EventLogger::instance()->installerComponentInitializationFinished(component, redistVersion, duration);
    }
    else if (event == QString::fromLatin1("ComponentInstallationStarted") && args.size() == 4)
    {
        auto component = toInstallerComponent(&ok, args.at(2));
        if (!ok) return false;

        auto redistVersion = toInstallerRedistVersion(&ok, args.at(3));
        if (!ok) return false;

        EventLogger::instance()->installerComponentInstallationStarted(component, redistVersion);
    }
    else if (event == QString::fromLatin1("ComponentInstallationFinished") && args.size() == 5)
    {
        auto component = toInstallerComponent(&ok, args.at(2));
        if (!ok) return false;

        auto redistVersion = toInstallerRedistVersion(&ok, args.at(3));
        if (!ok) return false;

        int duration = args.at(4).toInt(&ok);
        if (!ok) return false;

        EventLogger::instance()->installerComponentInstallationFinished(component, redistVersion, duration);
    }
    else if (event == QString::fromLatin1("ComponentsInitializationStarted"))
    {
        EventLogger::instance()->installerComponentsInitializationStarted();
    }
    else if (event == QString::fromLatin1("ComponentsInitializationFinished") && args.size() == 3)
    {
        int duration = args.at(2).toInt(&ok);
        if (!ok) return false;

        EventLogger::instance()->installerComponentsInitializationFinished(duration);
    }
    else if (event == QString::fromLatin1("ComponentsInstallationStarted"))
    {
        EventLogger::instance()->installerComponentsInstallationStarted();
    }
    else if (event == QString::fromLatin1("ComponentsInstallationFinished") && args.size() == 3)
    {
        int duration = args.at(2).toInt(&ok);
        if (!ok) return false;

        EventLogger::instance()->installerComponentsInstallationFinished(duration);
    }
    else if (event == QString::fromLatin1("ErrorEncountered") && args.size() == 6)
    {
        id = args.at(2).toInt(&ok);
        if (!ok) return false;
        if (!eve_launcher::installer::ErrorEncountered_ErrorCode_IsValid(id)) return false;
        auto code = static_cast<eve_launcher::installer::ErrorEncountered_ErrorCode>(id);
        
        auto page = toInstallerPage(&ok, args.at(3));
        if (!ok) return false;

        auto component = toInstallerComponent(&ok, args.at(4));
        if (!ok) return false;

        auto redistVersion = toInstallerRedistVersion(&ok, args.at(5));
        if (!ok) return false;

        EventLogger::instance()->installerErrorEncountered(code, page, component, redistVersion);
    }
    else if (event == QString::fromLatin1("AnalyticsMessageSent") && args.size() == 3)
    {
        QString message = args.at(2);

        EventLogger::instance()->installerAnalyticsMessageSent(message);
    }
    else
    {
        return false;
    }

    return true;
}

bool InstallerEventOperation::undoOperation()
{
    return true;
}

bool InstallerEventOperation::testOperation()
{
    return true;
}
