#include "eventlogger.h"

#include <pdm.h>
#include <protobuf.h>

#include <google/protobuf/util/json_util.h>

#include <QNetworkInterface>
#include <QRandomGenerator>
#include <QUuid>

EventLogger::EventLogger()
{
    QCryptographicHash hasher(QCryptographicHash::Md5);

    auto interfaces = QNetworkInterface::allInterfaces();
    if(!interfaces.isEmpty())
    {
        auto macAddress = interfaces.first().hardwareAddress();
        hasher.addData(macAddress.toLocal8Bit());
    }
    QString timestamp = QString(QLatin1String("%1")).arg(QDateTime::currentMSecsSinceEpoch());
    hasher.addData(timestamp.toLocal8Bit());
    QString randomNumber = QString(QLatin1String("%1")).arg(QRandomGenerator::securelySeeded().generate());
    hasher.addData(randomNumber.toLocal8Bit());

    m_sessionId = hasher.result();

    std::string osUuidString = PDM::GetMachineUuid();
    QUuid osUuid = QUuid::fromString(QString::fromStdString(osUuidString));
    m_operatingSystemUuid = osUuid.toRfc4122();

    // m_session = "ls" + hasher.result().toHex();
    m_session = QString(QLatin1String("ls")) + QString(QLatin1String(hasher.result().toHex()));

    m_httpThreadController = new HttpThreadController();
}

EventLogger::~EventLogger()
{
    delete m_httpThreadController;
}

EventLogger *EventLogger::instance()
{
    static EventLogger s_instance;
    return &s_instance;
}

void EventLogger::initialize(eve_launcher::application::Application_Region region, const QString& version, eve_launcher::application::Application_BuildType buildType, bool provider, const QString& providerName)
{
    s_region = region;
    s_version = version;
    s_buildType = buildType;
    s_provider = provider;
    s_providerName = providerName;
}

void EventLogger::sendAllocatedEvent(google::protobuf::Message* payload)
{
    // Create the event
    eve_launcher::installer::Event event;

    // Set the timestamp
    event.set_allocated_occurred(getTimestamp());

    // Populated by the gateway so we skip it
    // google.protobuf.Timestamp received = 2;

    // todo: Create a uuid
    // bytes uuid = 3; 

    // Populated by the gateway so we skip it
    // IPAddress ip_address = 4;
    if (s_region == eve_launcher::application::Application_Region_REGION_CHINA)
    {
        event.set_tenant("Serenity");
    }
    else
    {
        event.set_tenant("Tranquility");
    }

    // Set the payload
    auto any = new google::protobuf::Any;
    any->PackFrom(*payload);
    event.set_allocated_payload(any);

    // todo: Set the Journey ID https://wiki.ccpgames.com/x/cRwuC
    // bytes journey = 7;

    std::string jsonEvent = toJSON(&event);
    QByteArray byteEvent = QByteArray::fromStdString(jsonEvent);

    // todo: choose url based on region
    QString url = QString::fromLatin1("https://localhost:5001/weatherforecast");
    m_httpThreadController->postTelemetry(byteEvent, url);
}

std::string EventLogger::toJSON(google::protobuf::Message* message)
{
    std::string jsonString;
    google::protobuf::util::JsonPrintOptions options;
    // Following options are good for testing
    // options.add_whitespace = true;
    // options.always_print_primitive_fields = true;
    // options.preserve_proto_field_names = true;
    MessageToJsonString(*message, &jsonString, options);
    replace(jsonString, "type.googleapis.com", "type.evetech.net");
    return jsonString;
}

bool EventLogger::replace(std::string& str, const std::string& from, const std::string& to)
{
    size_t start_pos = str.find(from);
    if(start_pos == std::string::npos)
        return false;
    str.replace(start_pos, from.length(), to);
    return true;
}

QString EventLogger::getSession()
{
    return m_session;
}

eve_launcher::application::Application* EventLogger::getApplication()
{
    eve_launcher::application::Application* app = new eve_launcher::application::Application;

    app->set_version(s_version.toStdString());
    app->set_build_type(s_buildType);
    app->set_region(s_region);

    if (!s_provider)
    {
        app->set_no_installer_provider(true);
    }
    else
    {
        app->set_installer_provider(s_providerName.toStdString());
    }

    return app;
}

eve_launcher::application::EventMetadata* EventLogger::getEventMetadata()
{
    auto data = new eve_launcher::application::EventMetadata;

    // Set the md5_session
    data->set_allocated_md5_session(new std::string(m_sessionId.data(), size_t(m_sessionId.size())));

    // Set the application
    auto app = getApplication();
    data->set_allocated_application(app);

    // Set the operating_system_uuid if any
    if(m_operatingSystemUuid != nullptr)
    {
        data->set_allocated_operating_system_uuid(new std::string(m_operatingSystemUuid.data(), size_t(m_operatingSystemUuid.size())));
    }

    return data;
}

google::protobuf::Timestamp* EventLogger::getTimestamp(qint64 millisecondsSinceEpoch)
{
    if(millisecondsSinceEpoch == 0)
    {
        millisecondsSinceEpoch = QDateTime::currentMSecsSinceEpoch();
    }

    auto now = new google::protobuf::Timestamp;
    now->set_seconds(millisecondsSinceEpoch / 1000);
    now->set_nanos(static_cast<google::protobuf::int32>(millisecondsSinceEpoch % 1000) * 1000000);
    return now;
}

//-----------------------------------------MESSAGE CONSTRUCTION:------------------------------------------------
// uninstaller
void EventLogger::uninstallerStarted(int duration)
{
    auto evt = new eve_launcher::uninstaller::Started;
    evt->set_allocated_event_metadata(getEventMetadata());
    evt->set_duration(duration);
    auto inf = pdm_proto::GetData();
    evt->set_allocated_system_information(&inf);
    sendAllocatedEvent(evt);
}

void EventLogger::uninstallerPageDisplayed(eve_launcher::uninstaller::Page previousPage, eve_launcher::uninstaller::Page currentPage, eve_launcher::uninstaller::PageDisplayed_FlowDirection flow)
{
    auto evt = new eve_launcher::uninstaller::PageDisplayed;
    evt->set_allocated_event_metadata(getEventMetadata());
    evt->set_previous_page(static_cast<eve_launcher::uninstaller::Page>(previousPage));
    evt->set_current_page(static_cast<eve_launcher::uninstaller::Page>(currentPage));
    evt->set_flow(static_cast<eve_launcher::uninstaller::PageDisplayed_FlowDirection>(flow));
    sendAllocatedEvent(evt);
}

void EventLogger::uninstallerUserCancelled(eve_launcher::uninstaller::Page page, eve_launcher::uninstaller::UserCancelled_Progress progress)
{
    auto evt = new eve_launcher::uninstaller::UserCancelled;
    evt->set_allocated_event_metadata(getEventMetadata());
    evt->set_page(static_cast<eve_launcher::uninstaller::Page>(page));
    evt->set_progress(static_cast<eve_launcher::uninstaller::UserCancelled_Progress>(progress));
    sendAllocatedEvent(evt);
}

void EventLogger::uninstallerShutDown(eve_launcher::uninstaller::Page page, eve_launcher::uninstaller::ShutDown_State state, bool finishButton)
{
    auto evt = new eve_launcher::uninstaller::ShutDown;
    evt->set_allocated_event_metadata(getEventMetadata());
    evt->set_page(static_cast<eve_launcher::uninstaller::Page>(page));
    evt->set_state(static_cast<eve_launcher::uninstaller::ShutDown_State>(state));
    evt->set_finish_button(finishButton);
    sendAllocatedEvent(evt);
    m_httpThreadController->lastChanceToFinish();
}

void EventLogger::uninstallerDetailsDisplayed()
{
    auto evt = new eve_launcher::uninstaller::DetailsDisplayed;
    evt->set_allocated_event_metadata(getEventMetadata());
    sendAllocatedEvent(evt);
}

void EventLogger::uninstallerDetailsHidden()
{
    auto evt = new eve_launcher::uninstaller::DetailsHidden;
    evt->set_allocated_event_metadata(getEventMetadata());
    sendAllocatedEvent(evt);
}

void EventLogger::uninstallerUninstallationStarted()
{
    auto evt = new eve_launcher::uninstaller::UninstallationStarted;
    evt->set_allocated_event_metadata(getEventMetadata());
    sendAllocatedEvent(evt);
}

void EventLogger::uninstallerUninstallationInterrupted(int duration)
{
    auto evt = new eve_launcher::uninstaller::UninstallationInterrupted;
    evt->set_allocated_event_metadata(getEventMetadata());
    evt->set_duration(duration);
    sendAllocatedEvent(evt);
}

void EventLogger::uninstallerUninstallationFinished(int duration)
{
    auto evt = new eve_launcher::uninstaller::UninstallationFinished;
    evt->set_allocated_event_metadata(getEventMetadata());
    evt->set_duration(duration);
    sendAllocatedEvent(evt);
}

void EventLogger::uninstallerUninstallationFailed(int duration)
{
    auto evt = new eve_launcher::uninstaller::UninstallationFailed;
    evt->set_allocated_event_metadata(getEventMetadata());
    evt->set_duration(duration);
    sendAllocatedEvent(evt);
}

void EventLogger::uninstallerErrorEncountered(eve_launcher::uninstaller::ErrorEncountered_ErrorCode code, eve_launcher::uninstaller::Page page)
{
    auto evt = new eve_launcher::uninstaller::ErrorEncountered;
    evt->set_allocated_event_metadata(getEventMetadata());
    evt->set_code(static_cast<eve_launcher::uninstaller::ErrorEncountered_ErrorCode>(code));
    evt->set_page(static_cast<eve_launcher::uninstaller::Page>(page));
    sendAllocatedEvent(evt);
}

void EventLogger::uninstallerAnalyticsMessageSent(const QString& message)
{
    auto evt = new eve_launcher::uninstaller::AnalyticsMessageSent;
    evt->set_allocated_event_metadata(getEventMetadata());
    evt->set_message(message.toStdString());
    sendAllocatedEvent(evt);
}

// installer
void EventLogger::installerStarted(int duration)
{
    auto evt = new eve_launcher::installer::Started;
    evt->set_allocated_event_metadata(getEventMetadata());
    evt->set_duration(duration);
    auto inf = pdm_proto::GetData();
    evt->set_allocated_system_information(&inf);
    sendAllocatedEvent(evt);
}

void EventLogger::installerPageDisplayed(eve_launcher::installer::Page previousPage, eve_launcher::installer::Page currentPage, eve_launcher::installer::PageDisplayed_FlowDirection flow)
{
    auto evt = new eve_launcher::installer::PageDisplayed;
    evt->set_allocated_event_metadata(getEventMetadata());
    evt->set_previous_page(static_cast<eve_launcher::installer::Page>(previousPage));
    evt->set_current_page(static_cast<eve_launcher::installer::Page>(currentPage));
    evt->set_flow(static_cast<eve_launcher::installer::PageDisplayed_FlowDirection>(flow));
    sendAllocatedEvent(evt);
}

void EventLogger::installerUserCancelled(eve_launcher::installer::Page page, eve_launcher::installer::UserCancelled_Progress progress)
{
    auto evt = new eve_launcher::installer::UserCancelled;
    evt->set_allocated_event_metadata(getEventMetadata());
    evt->set_page(static_cast<eve_launcher::installer::Page>(page));
    evt->set_progress(static_cast<eve_launcher::installer::UserCancelled_Progress>(progress));
    sendAllocatedEvent(evt);
}

void EventLogger::installerShutDown(eve_launcher::installer::Page page, eve_launcher::installer::ShutDown_State state, bool finishButton)
{
    auto evt = new eve_launcher::installer::ShutDown;
    evt->set_allocated_event_metadata(getEventMetadata());
    evt->set_page(static_cast<eve_launcher::installer::Page>(page));
    evt->set_state(static_cast<eve_launcher::installer::ShutDown_State>(state));
    evt->set_finish_button(finishButton);
    sendAllocatedEvent(evt);
    m_httpThreadController->lastChanceToFinish();
}

void EventLogger::installerPreparationStarted()
{
    auto evt = new eve_launcher::installer::PreparationStarted;
    evt->set_allocated_event_metadata(getEventMetadata());
    sendAllocatedEvent(evt);
}

void EventLogger::installerPreparationFinished(int duration)
{
    auto evt = new eve_launcher::installer::PreparationFinished;
    evt->set_allocated_event_metadata(getEventMetadata());
    evt->set_duration(duration);
    sendAllocatedEvent(evt);
}

void EventLogger::installerLocationChanged(eve_launcher::installer::LocationChanged_Source source, eve_launcher::installer::LocationChanged_Provider provider, const QString& path)
{
    auto evt = new eve_launcher::installer::LocationChanged;
    evt->set_allocated_event_metadata(getEventMetadata());
    evt->set_source(static_cast<eve_launcher::installer::LocationChanged_Source>(source));
    evt->set_provider(static_cast<eve_launcher::installer::LocationChanged_Provider>(provider));
    evt->set_path(path.toStdString());
    sendAllocatedEvent(evt);
}

void EventLogger::installerDetailsDisplayed()
{
    auto evt = new eve_launcher::installer::DetailsDisplayed;
    evt->set_allocated_event_metadata(getEventMetadata());
    sendAllocatedEvent(evt);
}

void EventLogger::installerDetailsHidden()
{
    auto evt = new eve_launcher::installer::DetailsHidden;
    evt->set_allocated_event_metadata(getEventMetadata());
    sendAllocatedEvent(evt);
}

void EventLogger::installerAutoRunEnabled()
{
    auto evt = new eve_launcher::installer::AutoRunEnabled;
    evt->set_allocated_event_metadata(getEventMetadata());
    sendAllocatedEvent(evt);
}

void EventLogger::installerAutoRunDisabled()
{
    auto evt = new eve_launcher::installer::AutoRunDisabled;
    evt->set_allocated_event_metadata(getEventMetadata());
    sendAllocatedEvent(evt);
}

void EventLogger::installerEulaAccepted()
{
    auto evt = new eve_launcher::installer::EulaAccepted;
    evt->set_allocated_event_metadata(getEventMetadata());
    sendAllocatedEvent(evt);
}

void EventLogger::installerEulaDeclined()
{
    auto evt = new eve_launcher::installer::EulaDeclined;
    evt->set_allocated_event_metadata(getEventMetadata());
    sendAllocatedEvent(evt);
}

void EventLogger::installerRedistSearchConcluded(eve_launcher::installer::RedistVersion version, eve_launcher::installer::RedistSearchConcluded_RedistReason reason)
{
    auto evt = new eve_launcher::installer::RedistSearchConcluded;
    evt->set_allocated_event_metadata(getEventMetadata());
    evt->set_version(static_cast<eve_launcher::installer::RedistVersion>(version));
    evt->set_reason(static_cast<eve_launcher::installer::RedistSearchConcluded_RedistReason>(reason));
    sendAllocatedEvent(evt);
}

void EventLogger::installerProvidedClientFound()
{
    auto evt = new eve_launcher::installer::ProvidedClientFound;
    evt->set_allocated_event_metadata(getEventMetadata());
    sendAllocatedEvent(evt);
}

void EventLogger::installerSharedCacheMessageShown()
{
    auto evt = new eve_launcher::installer::SharedCacheMessageShown;
    evt->set_allocated_event_metadata(getEventMetadata());
    sendAllocatedEvent(evt);
}

void EventLogger::installerSharedCacheMessageClosed(eve_launcher::installer::MessageBoxButton messageBoxButton, int timeDisplayed)
{
    auto evt = new eve_launcher::installer::SharedCacheMessageClosed;
    evt->set_allocated_event_metadata(getEventMetadata());
    evt->set_message_box_button(static_cast<eve_launcher::installer::MessageBoxButton>(messageBoxButton));
    evt->set_time_displayed(timeDisplayed);
    sendAllocatedEvent(evt);
}

void EventLogger::installerInstallationStarted()
{
    auto evt = new eve_launcher::installer::InstallationStarted;
    evt->set_allocated_event_metadata(getEventMetadata());
    sendAllocatedEvent(evt);
}

void EventLogger::installerInstallationInterrupted(int duration)
{
    auto evt = new eve_launcher::installer::InstallationInterrupted;
    evt->set_allocated_event_metadata(getEventMetadata());
    evt->set_duration(duration);
    sendAllocatedEvent(evt);
}

void EventLogger::installerInstallationFinished(int duration)
{
    auto evt = new eve_launcher::installer::InstallationFinished;
    evt->set_allocated_event_metadata(getEventMetadata());
    evt->set_duration(duration);
    sendAllocatedEvent(evt);
}

void EventLogger::installerInstallationFailed(int duration)
{
    auto evt = new eve_launcher::installer::InstallationFailed;
    evt->set_allocated_event_metadata(getEventMetadata());
    evt->set_duration(duration);
    sendAllocatedEvent(evt);
}

void EventLogger::installerUninstallerCreationStarted()
{
    auto evt = new eve_launcher::installer::UninstallerCreationStarted;
    evt->set_allocated_event_metadata(getEventMetadata());
    sendAllocatedEvent(evt);
}

void EventLogger::installerUninstallerCreationFinished(int duration)
{
    auto evt = new eve_launcher::installer::UninstallerCreationFinished;
    evt->set_allocated_event_metadata(getEventMetadata());
    evt->set_duration(duration);
    sendAllocatedEvent(evt);
}

void EventLogger::installerComponentInitializationStarted(eve_launcher::installer::Component component, eve_launcher::installer::RedistVersion redistVersion)
{
    auto evt = new eve_launcher::installer::ComponentInitializationStarted;
    evt->set_allocated_event_metadata(getEventMetadata());
    evt->set_component(static_cast<eve_launcher::installer::Component>(component));
    evt->set_redist_version(static_cast<eve_launcher::installer::RedistVersion>(redistVersion));
    sendAllocatedEvent(evt);
}

void EventLogger::installerComponentInitializationFinished(eve_launcher::installer::Component component, eve_launcher::installer::RedistVersion redistVersion, int duration)
{
    auto evt = new eve_launcher::installer::ComponentInitializationFinished;
    evt->set_allocated_event_metadata(getEventMetadata());
    evt->set_component(static_cast<eve_launcher::installer::Component>(component));
    evt->set_redist_version(static_cast<eve_launcher::installer::RedistVersion>(redistVersion));
    evt->set_duration(duration);
    sendAllocatedEvent(evt);
}

void EventLogger::installerComponentInstallationStarted(eve_launcher::installer::Component component, eve_launcher::installer::RedistVersion redistVersion)
{
    auto evt = new eve_launcher::installer::ComponentInstallationStarted;
    evt->set_allocated_event_metadata(getEventMetadata());
    evt->set_component(static_cast<eve_launcher::installer::Component>(component));
    evt->set_redist_version(static_cast<eve_launcher::installer::RedistVersion>(redistVersion));
    sendAllocatedEvent(evt);
}

void EventLogger::installerComponentInstallationFinished(eve_launcher::installer::Component component, eve_launcher::installer::RedistVersion redistVersion, int duration)
{
    auto evt = new eve_launcher::installer::ComponentInstallationFinished;
    evt->set_allocated_event_metadata(getEventMetadata());
    evt->set_component(static_cast<eve_launcher::installer::Component>(component));
    evt->set_redist_version(static_cast<eve_launcher::installer::RedistVersion>(redistVersion));
    evt->set_duration(duration);
    sendAllocatedEvent(evt);
}

void EventLogger::installerComponentsInitializationStarted()
{
    auto evt = new eve_launcher::installer::ComponentsInitializationStarted;
    evt->set_allocated_event_metadata(getEventMetadata());
    sendAllocatedEvent(evt);
}

void EventLogger::installerComponentsInitializationFinished(int duration)
{
    auto evt = new eve_launcher::installer::ComponentsInitializationFinished;
    evt->set_allocated_event_metadata(getEventMetadata());
    evt->set_duration(duration);
    sendAllocatedEvent(evt);
}

void EventLogger::installerComponentsInstallationStarted()
{
    auto evt = new eve_launcher::installer::ComponentsInstallationStarted;
    evt->set_allocated_event_metadata(getEventMetadata());
    sendAllocatedEvent(evt);
}

void EventLogger::installerComponentsInstallationFinished(int duration)
{
    auto evt = new eve_launcher::installer::ComponentsInstallationFinished;
    evt->set_allocated_event_metadata(getEventMetadata());
    evt->set_duration(duration);
    sendAllocatedEvent(evt);
}

void EventLogger::installerErrorEncountered(eve_launcher::installer::ErrorEncountered_ErrorCode code, eve_launcher::installer::Page page, eve_launcher::installer::Component component, eve_launcher::installer::RedistVersion redistVersion)
{
    auto evt = new eve_launcher::installer::ErrorEncountered;
    evt->set_allocated_event_metadata(getEventMetadata());
    evt->set_code(static_cast<eve_launcher::installer::ErrorEncountered_ErrorCode>(code));
    evt->set_page(static_cast<eve_launcher::installer::Page>(page));
    evt->set_component(static_cast<eve_launcher::installer::Component>(component));
    evt->set_redist_version(static_cast<eve_launcher::installer::RedistVersion>(redistVersion));
    sendAllocatedEvent(evt);
}

void EventLogger::installerAnalyticsMessageSent(const QString& message)
{
    auto evt = new eve_launcher::installer::AnalyticsMessageSent;
    evt->set_allocated_event_metadata(getEventMetadata());
    evt->set_message(message.toStdString());
    sendAllocatedEvent(evt);
}

// #endif
