#ifndef EVENTLOGGER_H
#define EVENTLOGGER_H

#include "httpthreadcontroller.h"
#include "utils.h"

#include "eve_launcher/application.pb.h"
#include "eve_launcher/installer.pb.h"
#include "eve_launcher/uninstaller.pb.h"

#include <QPointer>
#include <QObject>

class EventLogger : QObject
{
    Q_OBJECT

public:
    static EventLogger* instance();

    // uninstaller
    void uninstallerStarted(int duration);
    void uninstallerIntroductionPageDisplayed();
    void uninstallerExecutionPageDisplayed();
    void uninstallerFinishedPageDisplayed();
    void uninstallerFailedPageDisplayed();
    void uninstallerShutDown(eve_launcher::uninstaller::Page page, eve_launcher::uninstaller::ShutDown_State state, bool finishButton);
    void uninstallerUninstallationStarted();
    void uninstallerUninstallationInterrupted(int duration);
    void uninstallerUninstallationFinished(int duration);
    void uninstallerUninstallationFailed(int duration);
    void uninstallerErrorEncountered(eve_launcher::uninstaller::ErrorEncountered_ErrorCode code, eve_launcher::uninstaller::Page page);
    void uninstallerAnalyticsMessageSent(const QString& message);

    // installer
    void installerStarted(const QString& startMenuItemPath, int duration);
    void installerIntroductionPageDisplayed();
    void installerEulaPageDisplayed();
    void installerExecutionPageDisplayed();
    void installerFinishedPageDisplayed();
    void installerFailedPageDisplayed();
    void installerShutDown(eve_launcher::installer::Page page, eve_launcher::installer::ShutDown_State state, bool finishButton);
    void installerPreparationStarted();
    void installerPreparationFinished(int duration);
    void installerAutoRunEnabled();
    void installerAutoRunDisabled();
    void installerEulaAccepted();
    void installerEulaDeclined();
    void installerRedistSearchConcluded(eve_launcher::installer::RedistVersion version, eve_launcher::installer::RedistSearchConcluded_RedistReason reason);
    void installerProvidedClientFound();
    void installerSharedCacheMessageShown();
    void installerSharedCacheMessageClosed(eve_launcher::installer::MessageBoxButton messageBoxButton, int timeDisplayed);
    void installerInstallationStarted(const QString& installPath, eve_launcher::installer::RedistVersion redistVersion);
    void installerInstallationInterrupted(int duration);
    void installerInstallationFinished(const QString& sharedCachePath, int duration);
    void installerInstallationFailed(int duration);
    void installerUninstallerCreationStarted();
    void installerUninstallerCreationFinished(int duration);
    void installerComponentInitializationStarted();
    void installerComponentInitializationFinished(int duration);
    void installerComponentInstallationStarted();
    void installerComponentInstallationFinished(int duration);
    void installerErrorEncountered(eve_launcher::installer::ErrorEncountered_ErrorCode code, eve_launcher::installer::Page page, eve_launcher::installer::RedistVersion redistVersion);
    void installerAnalyticsMessageSent(const QString& message);

protected:
    ~EventLogger();

    QString m_session;
    QByteArray m_sessionId;
    QByteArray m_operatingSystemUuid;
    QByteArray m_journeyId;
    QByteArray m_deviceId;
    QString m_version;
    bool m_provider;
    QString m_providerName;
    eve_launcher::application::Application_Region m_region;
    eve_launcher::application::Application_BuildType m_buildType;
    QString m_gatewayUrl;
    QPointer<HttpThreadController> m_httpThreadController;

    explicit EventLogger();
    google::protobuf::Timestamp* getTimestamp(qint64 millisecondsSinceEpoch = 0);

    eve_launcher::application::Application* getApplication();
    eve_launcher::application::EventMetadata* getEventMetadata();

    void sendAllocatedEvent(google::protobuf::Message* payload);
    QString getGatewayUrl();
    void sendProtoMessage(google::protobuf::Message* message);
    void sendJsonMessage(google::protobuf::Message* message);
    QByteArray toJsonByteArray(google::protobuf::Message* message, bool verbose = false);
    std::string toJson(::google::protobuf::Message* event, bool verbose = false);
    bool replace(std::string& str, const std::string& from, const std::string& to);
};

#endif // EVENTLOGGER_H
