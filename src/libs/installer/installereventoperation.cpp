#include "installereventoperation.h"

#include "eventlogger.h"
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
    // if (!checkArgumentCount(1, 5, tr(" ([\"init\"|message type], [\"china\"|\"world\"] (init only), version (init only))")))
    //     return false;

    // todo: Make sure we have the correct number of arguments

    const QStringList args = arguments();
    const QString action = args.at(0);

    if(action == QString::fromLatin1("init"))
    {
        return sendInit(args);
    }
    else if(action == QString::fromLatin1("uninstaller"))
    {
        return sendUninstallerEvent(args);
    }
    else if(action == QString::fromLatin1("installer"))
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
    // if (!checkArgumentCount(5, 5, tr(" ([\"init\"|message type], [\"china\"|\"world\"] (init only), version (init only))")))
    //     return false;

    // Get region
    eve_launcher::application::Application_Region region;
    if (args.at(1) == QString::fromLatin1("china"))
    {
        region = eve_launcher::application::Application_Region_REGION_CHINA;
    }
    else
    {
        region = eve_launcher::application::Application_Region_REGION_WORLD;
    }

    // Get version
    QString version = args.at(2);

    // Get build type
    eve_launcher::application::Application_BuildType buildType;
    if (args.at(3) == QString::fromLatin1("release"))
    {
        buildType = eve_launcher::application::Application_BuildType_BUILDTYPE_RELEASE;
    }
    else
    {
        buildType = eve_launcher::application::Application_BuildType_BUILDTYPE_DEV;
    }

    // Get provider
    bool provider;
    QString providerName;
    if (args.at(4) == QString::fromLatin1("none"))
    {
        provider = false;
        providerName = QString::fromLatin1("");
    }
    else
    {
        provider = true;
        providerName = args.at(4);
    }

    EventLogger::instance()->initialize(region, version, buildType, provider, providerName);

    return true;
}

bool InstallerEventOperation::sendUninstallerEvent(QStringList args)
{
    const QString event = args.at(1);
    
    if (event == QString::fromLatin1("Started")) {
        int duration = args.at(2).toInt();
        EventLogger::instance()->uninstallerStarted(duration);
    }
    else if (event == QString::fromLatin1("PageDisplayed"))
    {
        auto previousPage = static_cast<eve_launcher::uninstaller::Page>(args.at(2).toInt());
        auto currentPage = static_cast<eve_launcher::uninstaller::Page>(args.at(3).toInt());
        auto flow = static_cast<eve_launcher::uninstaller::PageDisplayed_FlowDirection>(args.at(4).toInt());
        EventLogger::instance()->uninstallerPageDisplayed(previousPage, currentPage, flow);
    }
    else if (event == QString::fromLatin1("UserCancelled"))
    {
        auto page = static_cast<eve_launcher::uninstaller::Page>(args.at(2).toInt());
        auto progress = static_cast<eve_launcher::uninstaller::UserCancelled_Progress>(args.at(3).toInt());
        EventLogger::instance()->uninstallerUserCancelled(page, progress);
    }
    else if (event == QString::fromLatin1("ShutDown"))
    {
        auto page = static_cast<eve_launcher::uninstaller::Page>(args.at(2).toInt());
        auto state = static_cast<eve_launcher::uninstaller::ShutDown_State>(args.at(3).toInt());
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
    else if (event == QString::fromLatin1("UninstallationInterrupted"))
    {
        int duration = args.at(2).toInt();
        EventLogger::instance()->uninstallerUninstallationInterrupted(duration);
    }
    else if (event == QString::fromLatin1("UninstallationFinished"))
    {
        int duration = args.at(2).toInt();
        EventLogger::instance()->uninstallerUninstallationFinished(duration);
    }
    else if (event == QString::fromLatin1("UninstallationFailed"))
    {
        int duration = args.at(2).toInt();
        EventLogger::instance()->uninstallerUninstallationFailed(duration);
    }
    else if (event == QString::fromLatin1("ErrorEncountered"))
    {
        auto code = static_cast<eve_launcher::uninstaller::ErrorEncountered_ErrorCode>(args.at(2).toInt());
        auto page = static_cast<eve_launcher::uninstaller::Page>(args.at(3).toInt());
        EventLogger::instance()->uninstallerErrorEncountered(code, page);
    }
    else if (event == QString::fromLatin1("AnalyticsMessageSent"))
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
    const QString event = args.at(1);

    if (event == QString::fromLatin1("Started")) {
        int duration = args.at(2).toInt();
        EventLogger::instance()->installerStarted(duration);
    }
    else if (event == QString::fromLatin1("PageDisplayed"))
    {
        auto previousPage = static_cast<eve_launcher::installer::Page>(args.at(2).toInt());
        auto currentPage = static_cast<eve_launcher::installer::Page>(args.at(3).toInt());
        auto flow = static_cast<eve_launcher::installer::PageDisplayed_FlowDirection>(args.at(4).toInt());
        EventLogger::instance()->installerPageDisplayed(previousPage, currentPage, flow);
    }
    else if (event == QString::fromLatin1("UserCancelled"))
    {
        auto page = static_cast<eve_launcher::installer::Page>(args.at(2).toInt());
        auto progress = static_cast<eve_launcher::installer::UserCancelled_Progress>(args.at(3).toInt());
        EventLogger::instance()->installerUserCancelled(page, progress);
    }
    else if (event == QString::fromLatin1("ShutDown"))
    {
        auto page = static_cast<eve_launcher::installer::Page>(args.at(2).toInt());
        auto state = static_cast<eve_launcher::installer::ShutDown_State>(args.at(3).toInt());
        bool finishButton = QVariant(args.at(4)).toBool();
        EventLogger::instance()->installerShutDown(page, state, finishButton);
    }
    else if (event == QString::fromLatin1("PreparationStarted"))
    {
        EventLogger::instance()->installerPreparationStarted();
    }
    else if (event == QString::fromLatin1("PreparationFinished"))
    {
        int duration = args.at(2).toInt();
        EventLogger::instance()->installerPreparationFinished(duration);
    }
    else if (event == QString::fromLatin1("LocationChanged")) {
        auto source = static_cast<eve_launcher::installer::LocationChanged_Source>(args.at(2).toInt());
        auto provider = static_cast<eve_launcher::installer::LocationChanged_Provider>(args.at(3).toInt());
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
    else if (event == QString::fromLatin1("RedistSearchConcluded"))
    {
        auto version = static_cast<eve_launcher::installer::RedistVersion>(args.at(2).toInt());
        auto reason = static_cast<eve_launcher::installer::RedistSearchConcluded_RedistReason>(args.at(3).toInt());
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
    else if (event == QString::fromLatin1("SharedCacheMessageClosed"))
    {
        auto messageBoxButton = static_cast<eve_launcher::installer::MessageBoxButton>(args.at(2).toInt());
        int timeDisplayed = args.at(3).toInt();
        EventLogger::instance()->installerSharedCacheMessageClosed(messageBoxButton, timeDisplayed);
    }
    else if (event == QString::fromLatin1("InstallationStarted"))
    {
        EventLogger::instance()->installerInstallationStarted();
    }
    else if (event == QString::fromLatin1("InstallationInterrupted"))
    {
        int duration = args.at(2).toInt();
        EventLogger::instance()->installerInstallationInterrupted(duration);
    }
    else if (event == QString::fromLatin1("InstallationFinished"))
    {
        int duration = args.at(2).toInt();
        EventLogger::instance()->installerInstallationFinished(duration);
    }
    else if (event == QString::fromLatin1("InstallationFailed"))
    {
        int duration = args.at(2).toInt();
        EventLogger::instance()->installerInstallationFailed(duration);
    }
    else if (event == QString::fromLatin1("UninstallerCreationStarted"))
    {
        EventLogger::instance()->installerUninstallerCreationStarted();
    }
    else if (event == QString::fromLatin1("UninstallerCreationFinished"))
    {
        int duration = args.at(2).toInt();
        EventLogger::instance()->installerUninstallerCreationFinished(duration);
    }
    else if (event == QString::fromLatin1("ComponentInitializationStarted"))
    {
        auto component = static_cast<eve_launcher::installer::Component>(args.at(2).toInt());
        auto redistVersion = static_cast<eve_launcher::installer::RedistVersion>(args.at(3).toInt());
        EventLogger::instance()->installerComponentInitializationStarted(component, redistVersion);
    }
    else if (event == QString::fromLatin1("ComponentInitializationFinished"))
    {
        auto component = static_cast<eve_launcher::installer::Component>(args.at(2).toInt());
        auto redistVersion = static_cast<eve_launcher::installer::RedistVersion>(args.at(3).toInt());
        int duration = args.at(4).toInt();
        EventLogger::instance()->installerComponentInitializationFinished(component, redistVersion, duration);
    }
    else if (event == QString::fromLatin1("ComponentInstallationStarted"))
    {
        auto component = static_cast<eve_launcher::installer::Component>(args.at(2).toInt());
        auto redistVersion = static_cast<eve_launcher::installer::RedistVersion>(args.at(3).toInt());
        EventLogger::instance()->installerComponentInstallationStarted(component, redistVersion);
    }
    else if (event == QString::fromLatin1("ComponentInstallationFinished"))
    {
        auto component = static_cast<eve_launcher::installer::Component>(args.at(2).toInt());
        auto redistVersion = static_cast<eve_launcher::installer::RedistVersion>(args.at(3).toInt());
        int duration = args.at(4).toInt();
        EventLogger::instance()->installerComponentInstallationFinished(component, redistVersion, duration);
    }
    else if (event == QString::fromLatin1("ComponentsInitializationStarted"))
    {
        EventLogger::instance()->installerComponentsInitializationStarted();
    }
    else if (event == QString::fromLatin1("ComponentsInitializationFinished"))
    {
        int duration = args.at(2).toInt();
        EventLogger::instance()->installerComponentsInitializationFinished(duration);
    }
    else if (event == QString::fromLatin1("ComponentsInstallationStarted"))
    {
        EventLogger::instance()->installerComponentsInstallationStarted();
    }
    else if (event == QString::fromLatin1("ComponentsInstallationFinished"))
    {
        int duration = args.at(2).toInt();
        EventLogger::instance()->installerComponentsInstallationFinished(duration);
    }
    else if (event == QString::fromLatin1("ErrorEncountered"))
    {
        auto code = static_cast<eve_launcher::installer::ErrorEncountered_ErrorCode>(args.at(2).toInt());
        auto page = static_cast<eve_launcher::installer::Page>(args.at(3).toInt());
        auto component = static_cast<eve_launcher::installer::Component>(args.at(4).toInt());
        auto redistVersion = static_cast<eve_launcher::installer::RedistVersion>(args.at(5).toInt());
        EventLogger::instance()->installerErrorEncountered(code, page, component, redistVersion);
    }
    else if (event == QString::fromLatin1("AnalyticsMessageSent"))
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
