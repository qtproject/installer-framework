#ifndef EVENTLOGGER_H
#define EVENTLOGGER_H

#include "httpthreadcontroller.h"

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
    void uninstallerPageDisplayed(eve_launcher::uninstaller::Page previousPage, eve_launcher::uninstaller::Page currentPage, eve_launcher::uninstaller::PageDisplayed_FlowDirection flow);
    void uninstallerUserCancelled(eve_launcher::uninstaller::Page page, eve_launcher::uninstaller::UserCancelled_Progress progress);
    void uninstallerShutDown(eve_launcher::uninstaller::Page page, eve_launcher::uninstaller::ShutDown_State state, bool finishButton);
    void uninstallerDetailsDisplayed();
    void uninstallerDetailsHidden();
    void uninstallerUninstallationStarted();
    void uninstallerUninstallationInterrupted(int duration);
    void uninstallerUninstallationFinished(int duration);
    void uninstallerUninstallationFailed(int duration);
    void uninstallerErrorEncountered(eve_launcher::uninstaller::ErrorEncountered_ErrorCode code, eve_launcher::uninstaller::Page page);
    void uninstallerAnalyticsMessageSent(const QString& message);

    // installer
    void installerStarted(int duration);
    void installerPageDisplayed(eve_launcher::installer::Page previousPage, eve_launcher::installer::Page currentPage, eve_launcher::installer::PageDisplayed_FlowDirection flow);
    void installerUserCancelled(eve_launcher::installer::Page page, eve_launcher::installer::UserCancelled_Progress progress);
    void installerShutDown(eve_launcher::installer::Page page, eve_launcher::installer::ShutDown_State state, bool finishButton);
    void installerPreparationStarted();
    void installerPreparationFinished(int duration);
    void installerLocationChanged(eve_launcher::installer::LocationChanged_Source source, eve_launcher::installer::LocationChanged_Provider provider, const QString& path);
    void installerDetailsDisplayed();
    void installerDetailsHidden();
    void installerAutoRunEnabled();
    void installerAutoRunDisabled();
    void installerEulaAccepted();
    void installerEulaDeclined();
    void installerRedistSearchConcluded(eve_launcher::installer::RedistVersion version, eve_launcher::installer::RedistSearchConcluded_RedistReason reason);
    void installerProvidedClientFound();
    void installerSharedCacheMessageShown();
    void installerSharedCacheMessageClosed(eve_launcher::installer::MessageBoxButton messageBoxButton, int timeDisplayed);
    void installerInstallationStarted();
    void installerInstallationInterrupted(int duration);
    void installerInstallationFinished(int duration);
    void installerInstallationFailed(int duration);
    void installerUninstallerCreationStarted();
    void installerUninstallerCreationFinished(int duration);
    void installerComponentInitializationStarted(eve_launcher::installer::Component component, eve_launcher::installer::RedistVersion redistVersion);
    void installerComponentInitializationFinished(eve_launcher::installer::Component component, eve_launcher::installer::RedistVersion redistVersion, int duration);
    void installerComponentInstallationStarted(eve_launcher::installer::Component component, eve_launcher::installer::RedistVersion redistVersion);
    void installerComponentInstallationFinished(eve_launcher::installer::Component component, eve_launcher::installer::RedistVersion redistVersion, int duration);
    void installerComponentsInitializationStarted();
    void installerComponentsInitializationFinished(int duration);
    void installerComponentsInstallationStarted();
    void installerComponentsInstallationFinished(int duration);
    void installerErrorEncountered(eve_launcher::installer::ErrorEncountered_ErrorCode code, eve_launcher::installer::Page page, eve_launcher::installer::Component component, eve_launcher::installer::RedistVersion redistVersion);
    void installerAnalyticsMessageSent(const QString& message);

    QString getSession();

    void initialize(eve_launcher::application::Application_Region region, const QString& version, eve_launcher::application::Application_BuildType buildType, bool provider, const QString& providerName);
protected:
    ~EventLogger();

    QString m_session;
    QByteArray m_sessionId;
    QByteArray m_operatingSystemUuid;
    QPointer<HttpThreadController> m_httpThreadController;

    explicit EventLogger();
    google::protobuf::Timestamp* getTimestamp(qint64 millisecondsSinceEpoch = 0);

    eve_launcher::application::Application* getApplication();
    eve_launcher::application::EventMetadata* getEventMetadata();

    void sendAllocatedEvent(google::protobuf::Message* payload);
    std::string toJSON(::google::protobuf::Message* event);
    bool replace(std::string& str, const std::string& from, const std::string& to);

    eve_launcher::application::Application_Region s_region;
    QString s_version;
    eve_launcher::application::Application_BuildType s_buildType;
    bool s_provider;
    QString s_providerName;
};

#endif // EVENTLOGGER_H
