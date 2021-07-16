#include "installereventoperation.h"

#include "packagemanagercore.h"
#include "globals.h"
#include "utils.h"
#include "sentryhelper.h"

#include <QtCore/QDebug>

using namespace QInstaller;

InstallerEventOperation::InstallerEventOperation(PackageManagerCore *core)
    : UpdateOperation(core)
{
    qDebug() << "framework | InstallerEventOperation::InstallerEventOperation";
    setName(QLatin1String("InstallerEvent"));
}

void InstallerEventOperation::backup()
{
}

bool InstallerEventOperation::performOperation()
{
    const QStringList args = arguments();

    qDebug() << "framework | InstallerEventOperation::performOperation |" << args;

    if (args.isEmpty()) return false;

    const QString action = args.at(0);

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
    else if (event == QString::fromLatin1("IntroductionPageDisplayed"))
    {
        EventLogger::instance()->uninstallerIntroductionPageDisplayed();
    }
    else if (event == QString::fromLatin1("ExecutionPageDisplayed"))
    {
        EventLogger::instance()->uninstallerExecutionPageDisplayed();
    }
    else if (event == QString::fromLatin1("FinishedPageDisplayed"))
    {
        EventLogger::instance()->uninstallerFinishedPageDisplayed();
    }
    else if (event == QString::fromLatin1("FailedPageDisplayed"))
    {
        EventLogger::instance()->uninstallerFailedPageDisplayed();
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
    else if (event == QString::fromLatin1("ErrorEncountered") && args.size() == 7)
    {
        id = args.at(2).toInt(&ok);
        if (!ok) return false;
        if (!eve_launcher::uninstaller::ErrorEncountered_ErrorCode_IsValid(id)) return false;
        auto code = static_cast<eve_launcher::uninstaller::ErrorEncountered_ErrorCode>(id);

        auto page = toUninstallerPage(&ok, args.at(3));
        if (!ok) return false;

        QString fileName = args.at(4);
        QString functionName = args.at(5);
        int lineNumber = args.at(6).toInt(&ok);
        if (!ok) return false;

        QString type;
        QString value;
        if (code == eve_launcher::uninstaller::ErrorEncountered_ErrorCode_ERRORCODE_QT_STATUS_FAILURE)
        {
            type = QLatin1String("QtStatus::Failure");
            value = QLatin1String("Status of application changed to Failed");
        }
        else if (code == eve_launcher::uninstaller::ErrorEncountered_ErrorCode_ERRORCODE_QT_STATUS_FORCE_UPDATE)
        {
            type = QLatin1String("QtStatus::ForceUpdate");
            value = QLatin1String("Status of application changed to ForceUpdate");
        }
        else if (code == eve_launcher::uninstaller::ErrorEncountered_ErrorCode_ERRORCODE_QT_STATUS_UNFINISHED)
        {
            type = QLatin1String("QtStatus::Unfinished");
            value = QLatin1String("Status of application changed to Unfinished");
        }
        else
        {
            type = QLatin1String("UnknownException");
            value = QLatin1String("Something unexpected happened");
        }

        type = QString::fromLatin1("Uninstaller::Scripts::%1").arg(type);
        sendException(type, value, functionName, fileName, lineNumber);

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
    qDebug() << "framework | InstallerEventOperation::sendInstallerEvent | args =" << args;
    if (args.size() < 2) return false;

    const QString event = args.at(1);
    bool ok;
    int id;

    if (event == QString::fromLatin1("Started") && args.size() == 4)
    {
        qDebug() << "framework | InstallerEventOperation::sendInstallerEvent | Started | args =" << args;

        QString startMenuItemPath = args.at(2);

        int duration = args.at(3).toInt(&ok);
        if (!ok) return false;

        EventLogger::instance()->installerStarted(startMenuItemPath, duration);
    }
    else if (event == QString::fromLatin1("IntroductionPageDisplayed"))
    {
        EventLogger::instance()->installerIntroductionPageDisplayed();
    }
    else if (event == QString::fromLatin1("EulaPageDisplayed"))
    {
        EventLogger::instance()->installerEulaPageDisplayed();
    }
    else if (event == QString::fromLatin1("ExecutionPageDisplayed"))
    {
        EventLogger::instance()->installerExecutionPageDisplayed();
    }
    else if (event == QString::fromLatin1("FinishedPageDisplayed"))
    {
        EventLogger::instance()->installerFinishedPageDisplayed();
    }
    else if (event == QString::fromLatin1("FailedPageDisplayed"))
    {
        EventLogger::instance()->installerFailedPageDisplayed();
    }
    else if (event == QString::fromLatin1("ShutDown") && args.size() == 5)
    {
        qDebug() << "framework | InstallerEventOperation::sendInstallerEvent | ShutDown | args =" << args;
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
    else if (event == QString::fromLatin1("InstallationStarted") && args.size() == 4)
    {
        QString installPath = args.at(2);

        auto redistVersion = toInstallerRedistVersion(&ok, args.at(3));
        if (!ok) return false;

        EventLogger::instance()->installerInstallationStarted(installPath, redistVersion);
    }
    else if (event == QString::fromLatin1("InstallationInterrupted") && args.size() == 3)
    {
        int duration = args.at(2).toInt(&ok);
        if (!ok) return false;

        EventLogger::instance()->installerInstallationInterrupted(duration);
    }
    else if (event == QString::fromLatin1("InstallationFinished") && args.size() == 4)
    {
        QString sharedCachePath = args.at(2);

        int duration = args.at(3).toInt(&ok);
        if (!ok) return false;

        EventLogger::instance()->installerInstallationFinished(sharedCachePath, duration);
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
    else if (event == QString::fromLatin1("ComponentInitializationStarted"))
    {
        EventLogger::instance()->installerComponentInitializationStarted();
    }
    else if (event == QString::fromLatin1("ComponentInitializationFinished") && args.size() == 3)
    {
        int duration = args.at(2).toInt(&ok);
        if (!ok) return false;

        EventLogger::instance()->installerComponentInitializationFinished(duration);
    }
    else if (event == QString::fromLatin1("ComponentInstallationStarted"))
    {
        EventLogger::instance()->installerComponentInstallationStarted();
    }
    else if (event == QString::fromLatin1("ComponentInstallationFinished") && args.size() == 3)
    {
        int duration = args.at(2).toInt(&ok);
        if (!ok) return false;

        EventLogger::instance()->installerComponentInstallationFinished(duration);
    }
    else if (event == QString::fromLatin1("ErrorEncountered") && args.size() == 8)
    {
        id = args.at(2).toInt(&ok);
        if (!ok) return false;
        if (!eve_launcher::installer::ErrorEncountered_ErrorCode_IsValid(id)) return false;
        auto code = static_cast<eve_launcher::installer::ErrorEncountered_ErrorCode>(id);

        auto page = toInstallerPage(&ok, args.at(3));
        if (!ok) return false;

        QString fileName = args.at(4);
        QString functionName = args.at(5);
        int lineNumber = args.at(6).toInt(&ok);
        if (!ok) return false;

        auto redistVersion = toInstallerRedistVersion(&ok, args.at(7));
        if (!ok) return false;

        QString type;
        QString value;
        if (code == eve_launcher::installer::ErrorEncountered_ErrorCode_ERRORCODE_QT_STATUS_FAILURE)
        {
            type = QLatin1String("QtStatus::Failure");
            value = QLatin1String("Status of application changed to Failed");
        }
        else if (code == eve_launcher::installer::ErrorEncountered_ErrorCode_ERRORCODE_QT_STATUS_FORCE_UPDATE)
        {
            type = QLatin1String("QtStatus::ForceUpdate");
            value = QLatin1String("Status of application changed to ForceUpdate");
        }
        else if (code == eve_launcher::installer::ErrorEncountered_ErrorCode_ERRORCODE_QT_STATUS_UNFINISHED)
        {
            type = QLatin1String("QtStatus::Unfinished");
            value = QLatin1String("Status of application changed to Unfinished");
        }
        else if (code == eve_launcher::installer::ErrorEncountered_ErrorCode_ERRORCODE_CREATE_OPERATIONS)
        {
            type = QLatin1String("LauncherInstallOperationException");
            value = QLatin1String("Application failed to prepare all the required operations to be able to install the Launcher");
        }
        else if (code == eve_launcher::installer::ErrorEncountered_ErrorCode_ERRORCODE_ADD_OPERATION)
        {
            type = QLatin1String("UniversalCrtOperationException");
            value = QLatin1String("Application failed to add the operation that installs the Microsoft Universal C RunTime.");
        }
        else if (code == eve_launcher::installer::ErrorEncountered_ErrorCode_ERRORCODE_SEARCH_DLL)
        {
            type = QLatin1String("DllLookupException");
            value = QLatin1String("Application failed when trying to look for UCRT runtime dll.");
        }
        else if (code == eve_launcher::installer::ErrorEncountered_ErrorCode_ERRORCODE_SEARCH_WINDOWS_UPDATE)
        {
            type = QLatin1String("WindowsUpdateLookupException");
            value = QLatin1String("Application failed when trying to look for installed Windows Updates.");
        }
        else if (code == eve_launcher::installer::ErrorEncountered_ErrorCode_ERRORCODE_MISSING_PREREQUISITE)
        {
            type = QLatin1String("PrerequisiteMissingException");
            value = QLatin1String("This Windows 8.1 version is missing a very large Windows Update, that is a prerequisite for the redist Windows Updates, we are therefore unable to install the redist, and the installation of the launcher will succeed, but the launcher will not start but fail instead.");
        }
        else
        {
            type = QLatin1String("UnknownException");
            value = QLatin1String("Something unexpected happened");
        }

        type = QString::fromLatin1("Installer::Scripts::%1").arg(type);
        sendException(type, value, functionName, fileName, lineNumber);

        EventLogger::instance()->installerErrorEncountered(code, page, redistVersion);
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
