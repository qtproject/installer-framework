#include "eventlogger.h"

#include <pdm.h>
#include <protobuf.h>

#include <google/protobuf/util/json_util.h>
#include <google/protobuf/message_lite.h>

#include <QNetworkInterface>
#include <QRandomGenerator>
#include <QUuid>
#include <QDebug>
#include <QRegularExpression>

#define SENTRY_BUILD_STATIC 1
#include <sentry.h>

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

    std::string osUuidString = PDM::GetMachineUuidString();
    qDebug() << "framework | EventLogger::EventLogger | os uuid =" << QString::fromStdString(osUuidString);
    QUuid osUuid = QUuid::fromString(QString::fromStdString(osUuidString));
    m_operatingSystemUuid = osUuid.toRfc4122();

    // m_session = "ls" + hasher.result().toHex();
    m_session = QString(QLatin1String("ls")) + QString(QLatin1String(hasher.result().toHex()));

    m_httpThreadController = new HttpThreadController();

    // Get the journeyId from the installer filename
    QUuid journeyId;
    QString appName = QInstaller::getInstallerFileName().split(QLatin1String("/")).last();
    if (appName.length() > 35)
    {
        QString pattern = QLatin1String("[0-9A-F]{8}-[0-9A-F]{4}-4[0-9A-F]{3}-[89AB][0-9A-F]{3}-[0-9A-F]{12}");
        QRegularExpression re(pattern, QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatch match = re.match(appName);
        if (match.hasMatch()) {
           qDebug() << "framework | EventLogger::EventLogger | JourneyId found in filename:" << match.captured(0);
           journeyId = QUuid::fromString(match.captured(0));
           qDebug() << "framework | EventLogger::EventLogger | JourneyId:" << journeyId.toString(QUuid::WithoutBraces);
           qDebug() << "framework | EventLogger::EventLogger | JourneyId (base64):" << QLatin1String(journeyId.toRfc4122().toBase64());
        }
    }

    // If journey Id was found, or we weren't able to create a QUuid from it, we create a new one instead
    if (journeyId.isNull())
    {
        qDebug() << "framework | EventLogger::EventLogger | No JourneyId provided, one will be created instead";
        journeyId = QUuid::createUuid();
        qDebug() << "framework | EventLogger::EventLogger | JourneyId:" << journeyId.toString(QUuid::WithoutBraces);
        qDebug() << "framework | EventLogger::EventLogger | JourneyId (base64):" << QLatin1String(journeyId.toRfc4122().toBase64());
    }

    QInstaller::setJourneyId(journeyId);
    m_journeyId = journeyId.toRfc4122();

    qDebug() << "framework | EventLogger::EventLogger | Trying to cause a crash";

    // Now that we have the os uuid, we can add user information to our crashes
    // All crashes that happen before this point will not contain user information
    sentry_value_t user = sentry_value_new_object();

    // Add the os uuid as id
    sentry_value_set_by_key(user, "id", sentry_value_new_string(osUuid.toRfc4122().toBase64()));
    sentry_value_set_by_key(user, "ip_address", sentry_value_new_string("{{auto}}"));
    
    // Add the journey ID
    sentry_value_set_by_key(user, "Journey ID", sentry_value_new_string(m_journeyId.toBase64()));

    // Add the session ID
    sentry_value_set_by_key(user, "Session", sentry_value_new_string(m_session.toLocal8Bit().constData()));

    sentry_set_user(user);
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
    qDebug() << "framework | EventLogger::initialize | starting |" << region << version << buildType << provider << providerName;
    s_region = region;
    s_version = version;
    s_buildType = buildType;
    s_provider = provider;
    s_providerName = providerName;
    s_initSuccessful = true;
    s_gatewayUrl = getGatewayUrl();

    // Add an app context for Sentry
    sentry_value_t app = sentry_value_new_object();

    // Region
    sentry_value_t app_region = sentry_value_new_string("Missing");
    if (s_region == eve_launcher::application::Application_Region_REGION_WORLD)
    {
        app_region = sentry_value_new_string("World");
    }
    else if (s_region == eve_launcher::application::Application_Region_REGION_CHINA)
    {
        app_region = sentry_value_new_string("China");
    }
    sentry_value_set_by_key(app, "Region", app_region);

    // Version
    sentry_value_set_by_key(app, "Version", sentry_value_new_string(s_version.toLocal8Bit().constData()));

    // Buildtype
    sentry_value_t app_buildtype = sentry_value_new_string("Missing");
    if (s_buildType == eve_launcher::application::Application_BuildType_BUILDTYPE_RELEASE)
    {
        app_buildtype = sentry_value_new_string("Release");
    }
    else if (s_buildType == eve_launcher::application::Application_BuildType_BUILDTYPE_BETA)
    {
        app_buildtype = sentry_value_new_string("Beta");
    }
    else if (s_buildType == eve_launcher::application::Application_BuildType_BUILDTYPE_DEV)
    {
        app_buildtype = sentry_value_new_string("Development");
    }
    sentry_value_set_by_key(app, "Buildtype", app_buildtype);

    // Provider
    if (s_provider && s_region == eve_launcher::application::Application_Region_REGION_CHINA)
    {
        sentry_value_set_by_key(app, "Provider (China only)", sentry_value_new_string(s_providerName.toLocal8Bit().constData()));
    }
    
    sentry_set_context("App", app);

    qDebug() << "framework | EventLogger::initialize | initialized |" << s_region << s_version << s_buildType << s_provider << s_providerName << s_gatewayUrl;
}

void EventLogger::sendAllocatedEvent(google::protobuf::Message* payload)
{
    qDebug() << "framework | EventLogger::sendAllocatedEvent";

    // If we haven't managed to initialize our gateway connection, then we can't send anything
    if (!s_initSuccessful)
    {
        qWarning() << "framework | EventLogger::sendAllocatedEvent | not initialized";
        return;
    }

    // Create the event
    eve_launcher::installer::Event event;

    // Set the timestamp
    event.set_allocated_occurred(getTimestamp());

    // When we move over to eve_public domain, we will need to provide a uuid

    // Set the tenant
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

    qDebug() << "framework | EventLogger::sendAllocatedEvent | gateway url =" << s_gatewayUrl;
    sendJsonMessage(&event);

    // Send again now to a proto endpoint
    if (QInstaller::sendProtoMessages())
    {
        // We need to re-unpack the payload, to get the type namespace into the proto.
        // There is a bug in the json formatter that breaks the formatter if we provide our own namespace.
        auto anyWithType = new google::protobuf::Any;
        anyWithType->PackFrom(*payload, "type.evetech.net");
        event.set_allocated_payload(anyWithType);
        sendProtoMessage(&event);
    }
}

QString EventLogger::getGatewayUrl()
{
    bool dev = s_buildType == eve_launcher::application::Application_BuildType_BUILDTYPE_DEV;
    bool china = s_region == eve_launcher::application::Application_Region_REGION_CHINA;
    QString subDomain = dev ? QString::fromLatin1("dev") : QString::fromLatin1("live");
    QString domain = china ? QString::fromLatin1("evepc.163.com") : QString::fromLatin1("evetech.net");
    return QString(QLatin1String("https://elg-%1.%2:8081/v1/event/publish")).arg(subDomain).arg(domain);
}

void EventLogger::sendProtoMessage(google::protobuf::Message* message)
{
    qDebug() << "framework | EventLogger::sendProtoMessage | Sending messages as proto";
    std::string serializedEvent;
    if (message->SerializeToString(&serializedEvent))
    {
        QByteArray byteEvent = QByteArray::fromStdString(serializedEvent);
        m_httpThreadController->postTelemetry(byteEvent, QInstaller::getProtoMessageEndpoint(), QLatin1String("application/x-protobuf"));
    }
    else
    {
        qWarning() << "framework | EventLogger::sendProtoMessage | Couldn't serialize proto, no event sent!";
    }
}

void EventLogger::sendJsonMessage(google::protobuf::Message* message)
{
    qDebug() << "framework | EventLogger::sendJsonMessage | Sending messages as json";
    QByteArray byteEvent = toJsonByteArray(message);
    m_httpThreadController->postTelemetry(byteEvent, s_gatewayUrl, QLatin1String("application/json"));

    // Also send the telemetry to our provided endpoint
    if (QInstaller::useProvidedTelemetryEndpoint())
    {
        QString customEndpoint = QInstaller::getProvidedTelemetryEndpoint();
        qDebug() << "framework | EventLogger::sendJsonMessage | Telemetry endpoint provided =" << customEndpoint;
        if (customEndpoint == s_gatewayUrl)
        {
            qDebug() << "framework | EventLogger::sendJsonMessage | Telemetry endpoint same as real endpoint, no need to send twice";
        }
        else
        {
            qDebug() << "framework | EventLogger::sendJsonMessage | Also sending to user provided telemetry endpoint =" << customEndpoint;
            QByteArray byteEvent = toJsonByteArray(message, QInstaller::isVerbose());
            m_httpThreadController->postTelemetry(byteEvent, customEndpoint, QLatin1String("application/json"));
        }
    }
}

QByteArray EventLogger::toJsonByteArray(google::protobuf::Message* message, bool verbose)
{
    std::string json = toJson(message, verbose);
    return QByteArray::fromStdString(json);
}

std::string EventLogger::toJson(google::protobuf::Message* message, bool verbose)
{
    std::string jsonString;
    google::protobuf::util::JsonPrintOptions options;
    
    if (verbose)
    {
        // Following options are good for testing
        options.add_whitespace = true;
        options.always_print_primitive_fields = true;
        // options.preserve_proto_field_names = true;
    }
    
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

    // Set the journey
    data->set_allocated_journey(new std::string(m_journeyId.data(), size_t(m_journeyId.size())));

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
    qDebug() << "framework | EventLogger::uninstallerStarted |" << duration << "ms";
    auto evt = new eve_launcher::uninstaller::Started;
    evt->set_allocated_event_metadata(getEventMetadata());
    evt->set_duration(duration);
    auto inf = pdm_proto::GetData();
    evt->set_allocated_system_information(&inf);
    sendAllocatedEvent(evt);
}

void EventLogger::uninstallerIntroductionPageDisplayed()
{
    auto evt = new eve_launcher::uninstaller::IntroductionPageDisplayed;
    evt->set_allocated_event_metadata(getEventMetadata());
    sendAllocatedEvent(evt);
}

void EventLogger::uninstallerExecutionPageDisplayed()
{
    auto evt = new eve_launcher::uninstaller::ExecutionPageDisplayed;
    evt->set_allocated_event_metadata(getEventMetadata());
    sendAllocatedEvent(evt);
}

void EventLogger::uninstallerFinishedPageDisplayed()
{
    auto evt = new eve_launcher::uninstaller::FinishedPageDisplayed;
    evt->set_allocated_event_metadata(getEventMetadata());
    sendAllocatedEvent(evt);
}

void EventLogger::uninstallerFailedPageDisplayed()
{
    auto evt = new eve_launcher::uninstaller::FailedPageDisplayed;
    evt->set_allocated_event_metadata(getEventMetadata());
    sendAllocatedEvent(evt);
}

void EventLogger::uninstallerShutDown(eve_launcher::uninstaller::Page page, eve_launcher::uninstaller::ShutDown_State state, bool finishButton)
{
    auto evt = new eve_launcher::uninstaller::ShutDown;
    evt->set_allocated_event_metadata(getEventMetadata());
    evt->set_page(page);
    evt->set_state(state);
    evt->set_finish_button(finishButton);
    sendAllocatedEvent(evt);
    m_httpThreadController->lastChanceToFinish();
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
    // We also send this information to Sentry as an exception.
    // First we create the exception
    qDebug() << "framework | EventLogger::uninstallerErrorEncountered |" << code << page;
    sentry_value_t exc = sentry_value_new_object();
    if (code == eve_launcher::uninstaller::ErrorEncountered_ErrorCode_ERRORCODE_QT_STATUS_FAILURE)
    {
        sentry_value_set_by_key(exc, "type", sentry_value_new_string("Uninstaller::Scripts::QtStatus::Failure"));
        sentry_value_set_by_key(exc, "value", sentry_value_new_string("Status of application changed to Failed"));
    }
    else if (code == eve_launcher::uninstaller::ErrorEncountered_ErrorCode_ERRORCODE_QT_STATUS_FORCE_UPDATE)
    {
        sentry_value_set_by_key(exc, "type", sentry_value_new_string("Uninstaller::Scripts::QtStatus::ForceUpdate"));
        sentry_value_set_by_key(exc, "value", sentry_value_new_string("Status of application changed to ForceUpdate"));
    }
    else if (code == eve_launcher::uninstaller::ErrorEncountered_ErrorCode_ERRORCODE_QT_STATUS_UNFINISHED)
    {
        sentry_value_set_by_key(exc, "type", sentry_value_new_string("Uninstaller::Scripts::QtStatus::Unfinished"));
        sentry_value_set_by_key(exc, "value", sentry_value_new_string("Status of application changed to Unfinished"));
    }
    else 
    {
        sentry_value_set_by_key(exc, "type", sentry_value_new_string("Uninstaller::Scripts::UnknownException"));
        sentry_value_set_by_key(exc, "value", sentry_value_new_string("Something unexpected happened"));
    }

    // And then we send it
    sentry_value_t event = sentry_value_new_event();
    sentry_value_set_by_key(event, "exception", exc);
    sentry_capture_event(event);

    // Finally we move on to sending our event
    auto evt = new eve_launcher::uninstaller::ErrorEncountered;
    evt->set_allocated_event_metadata(getEventMetadata());
    evt->set_code(code);
    evt->set_page(page);
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
void EventLogger::installerStarted(const QString& startMenuItemPath, int duration)
{
    qDebug() << "framework | EventLogger::installerStarted |" << startMenuItemPath << ", " << duration << "ms";
    auto evt = new eve_launcher::installer::Started;
    evt->set_allocated_event_metadata(getEventMetadata());
    auto inf = pdm_proto::GetData();
    evt->set_allocated_system_information(&inf);
    QString fileName = QInstaller::getInstallerFileName().split(QLatin1String("/")).last();
    qDebug() << "framework | EventLogger::installerStarted | " << fileName;
    evt->set_filename(fileName.toStdString());
    evt->set_start_menu_item_path(startMenuItemPath.toStdString());
    evt->set_duration(duration);
    sendAllocatedEvent(evt);
}

void EventLogger::installerIntroductionPageDisplayed()
{
    auto evt = new eve_launcher::installer::IntroductionPageDisplayed;
    evt->set_allocated_event_metadata(getEventMetadata());
    sendAllocatedEvent(evt);
}

void EventLogger::installerEulaPageDisplayed()
{
    auto evt = new eve_launcher::installer::EulaPageDisplayed;
    evt->set_allocated_event_metadata(getEventMetadata());
    sendAllocatedEvent(evt);
}

void EventLogger::installerExecutionPageDisplayed()
{
    auto evt = new eve_launcher::installer::ExecutionPageDisplayed;
    evt->set_allocated_event_metadata(getEventMetadata());
    sendAllocatedEvent(evt);
}

void EventLogger::installerFinishedPageDisplayed()
{
    auto evt = new eve_launcher::installer::FinishedPageDisplayed;
    evt->set_allocated_event_metadata(getEventMetadata());
    sendAllocatedEvent(evt);
}

void EventLogger::installerFailedPageDisplayed()
{
    auto evt = new eve_launcher::installer::FailedPageDisplayed;
    evt->set_allocated_event_metadata(getEventMetadata());
    sendAllocatedEvent(evt);
}

void EventLogger::installerShutDown(eve_launcher::installer::Page page, eve_launcher::installer::ShutDown_State state, bool finishButton)
{
    qDebug() << "framework | EventLogger::installerShutDown |" << page << "|" << state << "|" << finishButton;
    auto evt = new eve_launcher::installer::ShutDown;
    evt->set_allocated_event_metadata(getEventMetadata());
    evt->set_page(page);
    evt->set_state(state);
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
    evt->set_version(version);
    evt->set_reason(reason);
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
    evt->set_message_box_button(messageBoxButton);
    evt->set_time_displayed(timeDisplayed);
    sendAllocatedEvent(evt);
}

void EventLogger::installerInstallationStarted(const QString& installPath, eve_launcher::installer::RedistVersion redistVersion)
{
    qDebug() << "framework | EventLogger::installerInstallationStarted | " << installPath << ", " << redistVersion;
    auto evt = new eve_launcher::installer::InstallationStarted;
    evt->set_allocated_event_metadata(getEventMetadata());
    evt->set_install_path(installPath.toStdString());
    evt->set_redist_version(redistVersion);
    sendAllocatedEvent(evt);
}

void EventLogger::installerInstallationInterrupted(int duration)
{
    auto evt = new eve_launcher::installer::InstallationInterrupted;
    evt->set_allocated_event_metadata(getEventMetadata());
    evt->set_duration(duration);
    sendAllocatedEvent(evt);
}

void EventLogger::installerInstallationFinished(const QString& sharedCachePath, int duration)
{
    qDebug() << "framework | EventLogger::installerInstallationFinished | " << sharedCachePath;
    auto evt = new eve_launcher::installer::InstallationFinished;
    evt->set_allocated_event_metadata(getEventMetadata());
    evt->set_shared_cache_path(sharedCachePath.toStdString());
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

void EventLogger::installerComponentInitializationStarted()
{
    auto evt = new eve_launcher::installer::ComponentInitializationStarted;
    evt->set_allocated_event_metadata(getEventMetadata());
    sendAllocatedEvent(evt);
}

void EventLogger::installerComponentInitializationFinished(int duration)
{
    auto evt = new eve_launcher::installer::ComponentInitializationFinished;
    evt->set_allocated_event_metadata(getEventMetadata());
    evt->set_duration(duration);
    sendAllocatedEvent(evt);
}

void EventLogger::installerComponentInstallationStarted()
{
    auto evt = new eve_launcher::installer::ComponentInstallationStarted;
    evt->set_allocated_event_metadata(getEventMetadata());
    sendAllocatedEvent(evt);
}

void EventLogger::installerComponentInstallationFinished(int duration)
{
    auto evt = new eve_launcher::installer::ComponentInstallationFinished;
    evt->set_allocated_event_metadata(getEventMetadata());
    evt->set_duration(duration);
    sendAllocatedEvent(evt);
}

void EventLogger::installerErrorEncountered(eve_launcher::installer::ErrorEncountered_ErrorCode code, eve_launcher::installer::Page page, eve_launcher::installer::RedistVersion redistVersion)
{
    // We also send this information to Sentry as an exception.
    // First we create the exception
    qDebug() << "framework | EventLogger::installerErrorEncountered |" << code << page;
    sentry_value_t exc = sentry_value_new_object();
    if (code == eve_launcher::installer::ErrorEncountered_ErrorCode_ERRORCODE_QT_STATUS_FAILURE)
    {
        sentry_value_set_by_key(exc, "type", sentry_value_new_string("Installer::Scripts::QtStatus::Failure"));
        sentry_value_set_by_key(exc, "value", sentry_value_new_string("Status of application changed to Failed"));
    }
    else if (code == eve_launcher::installer::ErrorEncountered_ErrorCode_ERRORCODE_QT_STATUS_FORCE_UPDATE)
    {
        sentry_value_set_by_key(exc, "type", sentry_value_new_string("Installer::Scripts::QtStatus::ForceUpdate"));
        sentry_value_set_by_key(exc, "value", sentry_value_new_string("Status of application changed to ForceUpdate"));
    }
    else if (code == eve_launcher::installer::ErrorEncountered_ErrorCode_ERRORCODE_QT_STATUS_UNFINISHED)
    {
        sentry_value_set_by_key(exc, "type", sentry_value_new_string("Installer::Scripts::QtStatus::Unfinished"));
        sentry_value_set_by_key(exc, "value", sentry_value_new_string("Status of application changed to Unfinished"));
    }
    else if (code == eve_launcher::installer::ErrorEncountered_ErrorCode_ERRORCODE_CREATE_OPERATIONS)
    {
        sentry_value_set_by_key(exc, "type", sentry_value_new_string("Installer::Scripts::LauncherInstallOperationException"));
        sentry_value_set_by_key(exc, "value", sentry_value_new_string("Application failed to prepare all the required operations to be able to install the Launcher"));
    }
    else if (code == eve_launcher::installer::ErrorEncountered_ErrorCode_ERRORCODE_ADD_OPERATION)
    {
        sentry_value_set_by_key(exc, "type", sentry_value_new_string("Installer::Scripts::UniversalCrtOperationException"));
        sentry_value_set_by_key(exc, "value", sentry_value_new_string("Application failed to add the operation that installs the Microsoft Universal C RunTime."));
    }
    else if (code == eve_launcher::installer::ErrorEncountered_ErrorCode_ERRORCODE_SEARCH_DLL)
    {
        sentry_value_set_by_key(exc, "type", sentry_value_new_string("Installer::Scripts::DllLookupException"));
        sentry_value_set_by_key(exc, "value", sentry_value_new_string("Application failed when trying to look for UCRT runtime dll."));
    }
    else if (code == eve_launcher::installer::ErrorEncountered_ErrorCode_ERRORCODE_SEARCH_WINDOWS_UPDATE)
    {
        sentry_value_set_by_key(exc, "type", sentry_value_new_string("Installer::Scripts::WindowsUpdateLookupException"));
        sentry_value_set_by_key(exc, "value", sentry_value_new_string("Application failed when trying to look for installed Windows Updates."));
    }
    else if (code == eve_launcher::installer::ErrorEncountered_ErrorCode_ERRORCODE_MISSING_PREREQUISITE)
    {
        sentry_value_set_by_key(exc, "type", sentry_value_new_string("Installer::Scripts::PrerequisiteMissingException"));
        sentry_value_set_by_key(exc, "value", sentry_value_new_string("This Windows 8.1 version is missing a very large Windows Update, that is a prerequisite for the redist Windows Updates, we are therefore unable to install the redist, and the installation of the launcher will succeed, but the launcher will not start but fail instead."));
    }
    else 
    {
        sentry_value_set_by_key(exc, "type", sentry_value_new_string("Installer::Scripts::UnknownException"));
        sentry_value_set_by_key(exc, "value", sentry_value_new_string("Something unexpected happened"));
    }

    // And then we send it
    sentry_value_t event = sentry_value_new_event();
    sentry_value_set_by_key(event, "exception", exc);
    sentry_capture_event(event);

    // Finally we move on to sending our event
    auto evt = new eve_launcher::installer::ErrorEncountered;
    evt->set_allocated_event_metadata(getEventMetadata());
    evt->set_code(code);
    evt->set_page(page);
    evt->set_redist_version(redistVersion);
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
