#include <Windows.h>

#include <pdm.h>
#include <pdm_data.h>
#include <protobuf.h>
#define SENTRY_BUILD_STATIC 1
#include <sentry.h>

#include "sentryhelper.h"
#include "utils.h"

#include <QByteArray>
#include <QDebug>
#include <QDir>
#include <QMap>
#include <QMapIterator>
#include <QSysInfo>
#include <QThread>
#include <QUuid>

void sentry_logger(sentry_level_e level, const char * message, va_list args, void *)
{
    char buf[1024]{};
    vsprintf(buf, message, args);

    switch (level)
    {
        case SENTRY_LEVEL_DEBUG:
            qDebug() << "sentry |" << buf;
            break;
        case SENTRY_LEVEL_INFO:
            qInfo() << "sentry |" << buf;
            break;
        case SENTRY_LEVEL_WARNING:
            qWarning() << "sentry |" << buf;
            break;
        case SENTRY_LEVEL_ERROR:
            qCritical() << "sentry |" << buf;
            break;
        case SENTRY_LEVEL_FATAL:
            qFatal("sentry | %s", buf);
            break;
    }
}

void setSentryKey(sentry_value_t &context, const char *key, const char *value)
{
    sentry_value_set_by_key(context, key, sentry_value_new_string(value));
}

void setSentryKey(sentry_value_t &context, const char *key, QString value)
{
    setSentryKey(context, key, value.toLocal8Bit().constData());
}

void setSentryKey(sentry_value_t &context, const char *key, QByteArray value)
{
    setSentryKey(context, key, value.toBase64().constData());
}

void setSentryKey(sentry_value_t &context, const char *key, QUuid value)
{
    setSentryKey(context, key, value.toRfc4122());
}

void setSentryKey(sentry_value_t &context, const char *key, int value)
{
    sentry_value_set_by_key(context, key, sentry_value_new_int32(value));
}

void setSentryContext(const char *name, QMap<const char*, QString> map)
{
    // Stop if no pairs have been supplied
    if (map.empty())
    {
        qDebug() << "Context '" << name << "' supplied with an empty map. Not populating";
    }

    // Create the context
    sentry_value_t context = sentry_value_new_object();

    // Iterate through the map and add all the pairs to the context
    QMapIterator<const char*, QString> i(map);
    while (i.hasNext())
    {
        i.next();

        // Add pair to the context
        setSentryKey(context, i.key(), i.value());
    }

    // Finally add the context to Sentry
    sentry_set_context(name, context);
}

void setSentryContext(const char *name, const char *key, QString value)
{
    QMap<const char*, QString> map {{key, value}};
    setSentryContext(name, map);
}

QString getComputerNameOnDomain(const wchar_t *domain)
{
    wchar_t computerName [MAX_COMPUTERNAME_LENGTH + 1];
    DWORD sizeComputerName = sizeof ( computerName );

    // Get the name of the domain for the computer
    // We generally want this so that we can treat machines that are on the CCP network specially
    std::wstring ccpDomainTest;
    ccpDomainTest.resize( 128 );
    DWORD envResultSize = GetEnvironmentVariable(L"USERDNSDOMAIN", &ccpDomainTest[0], 128);
    qDebug() << "envResultSize is: " << QString::number(envResultSize);
    if (envResultSize != 0)
    {
        qDebug() << "ccpDomainTest: '" << QString::fromWCharArray(ccpDomainTest.c_str()) << "'";
        if (!ccpDomainTest.empty() && wcscmp(ccpDomainTest.c_str(), domain) == 0)
        {
            // Get the name of the computer
            GetComputerName(computerName, &sizeComputerName);
            return QString::fromWCharArray(computerName);
        }
    }

    return QString();
}

void setComputerOnCcpDomain()
{
    QString ccpDomain = QString::fromWCharArray(CCP_DOMAIN);
    qDebug() << "Domain: " << ccpDomain;
    QString computerName = getComputerNameOnDomain(CCP_DOMAIN);
    if(computerName.isEmpty())
    {
        return;
    }

    qDebug() << "Computer Name: " << computerName;

    QMap<const char*, QString> map {
        { "Domain", ccpDomain },
        { "Computer Name", computerName },
    };

    setSentryContext("On CCP Domain", map);
}

void addUser()
{
    // https://docs.sentry.io/platforms/native/enriching-events/identify-user/
    sentry_value_t user = sentry_value_new_object();
    setSentryKey(user, "id", QInstaller::getDeviceId());
    setSentryKey(user, "ip_address", "{{auto}}");
    setSentryKey(user, "OS Uuid", QInstaller::getOsId());
    setSentryKey(user, "Journey ID", QInstaller::getJourneyId());
    setSentryKey(user, "Session", QInstaller::getSessionId());
    sentry_set_user(user);
}

// Here we gather what versions of libraries we are using
void addVersions()
{
    QMap<const char*, QString> versions {
        { "PDM", QInstaller::getPdmVersion() },
        { "Qt", QInstaller::getQtVersion() },
        { "Qt IFW", QInstaller::getQtIfwVersion() },
        { "Protobuf", QInstaller::getProtobufVersion() },
        { "CCP IFW", QInstaller::getCcpIfwVersion() },
    };

    setSentryContext("Library versions", versions);
}

void addApplicationContext()
{
    QMap<const char*, QString> map = {
        { "Application", QLatin1String("Installer") },
        { "Application Type", QLatin1String("Installer") },
        { "Installer Version", QInstaller::getInstallerVersion() },
        { "Launcher Version", QInstaller::getLauncherVersion() },
        { "Environment", QInstaller::getEnvironment() },
        { "Region", QLatin1String("World") },
    };

    if (!QInstaller::isInstaller())
    {
        map["Application Type"] = QLatin1String("Uninstaller");
    }

    if (QInstaller::isChina())
    {
        map["Region"] = QLatin1String("China");

        // Add the provider, if provided
        QString provider = QInstaller::getPartnerId();
        if(QInstaller::hasPartnerId() && !provider.isEmpty())
        {
            map["Installer Provider"] = provider;
        }
    }

    setSentryContext("Application", map);
}

void addSystemInformation()
{
    QMap<const char*, QString> map = {
        { "Current CPU Architecture", QSysInfo::currentCpuArchitecture() },
        { "Kernel Type", QSysInfo::kernelType() },
        { "Kernel Version", QSysInfo::kernelVersion() },
        { "OS Pretty Name", QSysInfo::prettyProductName() },
        { "Product Type", QSysInfo::productType() },
        { "Product Version", QSysInfo::productVersion() },
    };

    setSentryContext("System Information", map);
}

void setCommandLineArguments(const QStringList &arguments)
{
    // arguments[0] is always the installer executable (full path)
    // arguments[1] is the first argument supplied to the installer
    // We don't care about the installer path, and we only want to send this
    // to Sentry, if the user supplied at least one argument
    if (arguments.length() < 2)
    {
        return;
    }

    sentry_value_t args = sentry_value_new_object();
    for (int i=1; i<arguments.length(); i++)
    {
        setSentryKey(args, std::to_string(i).c_str(), arguments[i]);
    }
    sentry_set_context("Command Line Arguments", args);
}

void initializeSentry(const QStringList &arguments)
{
    qDebug() << "framework | sentryhelper::initializeSentry | Initialize Sentry";

    QString crashmonName = QInstaller::getCrashpadHandlerName();
    qDebug() << "framework | sentryhelper::initializeSentry | CrashpadHandlerName:" << crashmonName;

    // First thing we want to do is set up all the folders we need
    QString sentryCachePath = QInstaller::getCrashDb();
    QString tempPath = QInstaller::getTempPath();

    QString handlerPath = QString::fromLatin1("%1/%2").arg(tempPath).arg(crashmonName);
    qDebug() << "framework | sentryhelper::initializeSentry | Handler path: " << handlerPath;
    qDebug() << "framework | sentryhelper::initializeSentry | Sentry cache path: " << sentryCachePath;

    // If the crashpad_handler is missing, we can't initialize Sentry.
    if (!QDir(tempPath).exists(crashmonName))
    {
        qDebug() << "Crashpad handler not found, can't initialize Sentry";
        return;
    }

    // For now test against eve-installer-crashes
    QString dsnUrl = QLatin1String("https://755a650ea5df47859fe53b92fc1b05c1@o198460.ingest.sentry.io/5631340");
    // QString dsnUrl = QInstaller::getSentryDsn();

    sentry_options_t *options = sentry_options_new();
    sentry_options_set_dsn(options, dsnUrl.toLocal8Bit().constData());

    QString version = QInstaller::getInstallerVersion();
    QString installerVersion = QString::fromLatin1("eve-installer@%1").arg(version);
    qDebug() << "framework | sentryhelper::initializeSentry | Version:" << version;

    QString environment = QInstaller::getEnvironment();

    // Is this a dev or release build?
    if (QInstaller::isLocalDevelopmentBuild())
    {
        // We only want to enable Sentry logging on local development
        sentry_options_set_debug(options, 1);
        sentry_options_set_logger(options, &sentry_logger, nullptr);
    }

    qDebug() << "framework | sentryhelper::initializeSentry | Environment:" << environment;

    sentry_options_set_release(options, installerVersion.toLocal8Bit().constData());
    sentry_options_set_environment(options, environment.toLocal8Bit().constData());

    // Since we are Windows only, use pathw
    sentry_options_set_handler_pathw(options, (const wchar_t *)handlerPath.utf16());
    sentry_options_set_database_pathw(options, (const wchar_t *)sentryCachePath.utf16());

    sentry_options_set_symbolize_stacktraces(options, true);

    sentry_init(options);

    addUser();
    addVersions();
    addApplicationContext();
    addSystemInformation();
    setCommandLineArguments(arguments);

    setComputerOnCcpDomain();

    setTag(QLatin1String("region"), QLatin1String(QInstaller::isChina() ? "China" : "World"));
    setTag(QLatin1String("app.type"), QLatin1String(QInstaller::isInstaller() ? "Installer" : "Uninstaller"));
    setTag(QLatin1String("launcher.version"), QInstaller::getLauncherVersion());
}

void setTag(const QString& tag, const QString& value)
{
    sentry_set_tag(tag.toLocal8Bit().constData(), value.toLocal8Bit().constData());
}

void verifySentryMessage()
{
    sentry_capture_event(sentry_value_new_message_event(
    /*   level */ SENTRY_LEVEL_INFO,
    /*  logger */ "SentryTesting",
    /* message */ "Sentry setup verified!"
    ));
}

void verifySentryException()
{
    sendException(
        QLatin1String("SentryTestException"),
        QLatin1String("Sentry exception setup verified!"),
        QLatin1String("verifySentryException"),
        QLatin1String("sentryhelper.cpp"),
        361);
}

int SentryTestCrash(int x)
{
    // By taking in 0, it causes a division by zero crash
    return 5 / x;
}

void verifySentryCrash()
{
    int crashResult = SentryTestCrash(0);
    qDebug() << "Printing out the result of the crash function, so that the compiler doesn't optimize the function away: " << crashResult;
}

// This is to test all aspects of Sentry: message, exception and crash
// The only way to test a crash is to cause a crash, thus calling this
// function will crash the Launcher
void crashApplication()
{
    verifySentryMessage();
    QThread::msleep(100);
    verifySentryException();
    QThread::msleep(1000);
    verifySentryCrash();
}

sentry_value_t getStacktrace(QString functionName, QString file, int line)
{
    // The stacktrace needs to have a key called frames
    // frames must be a list of frame objects
    // frame objects must follow the Stack Trace Interface: https://develop.sentry.dev/sdk/event-payloads/stacktrace/

    // Begin with creating the frame
    sentry_value_t frame = sentry_value_new_object();
    setSentryKey(frame, "filename", file);
    setSentryKey(frame, "function", functionName);
    setSentryKey(frame, "lineno", line);
    setSentryKey(frame, "stack_start", "true");

    // Now create the list of frames
    sentry_value_t frames = sentry_value_new_list();
    // And add our frame to the list
    sentry_value_append(frames, frame);

    // Finally create the stacktrace object
    sentry_value_t stacktrace = sentry_value_new_object();

    // Add the frames to the stacktrace object
    sentry_value_set_by_key(stacktrace, "frames", frames);

    return stacktrace;
}

void sendException(const QString &exceptionType, const QString &value, const QString &functionName, const QString &file, int line)
{
    // Create the exception
    sentry_value_t exception = sentry_value_new_exception(
        exceptionType.toLocal8Bit().constData(),
        value.toLocal8Bit().constData()
    );

    // Create the stacktrace
    sentry_value_t stacktrace = getStacktrace(functionName, file, line);
    sentry_value_set_by_key(exception, "stacktrace", stacktrace);

    // Finally create the event we want to send to Sentry
    sentry_value_t event = sentry_value_new_event();
    sentry_value_set_by_key(event, "exception", exception);

    sentry_capture_event(event);
}

void closeSentry()
{
    qDebug() << "Close Sentry";
    sentry_close();
}
