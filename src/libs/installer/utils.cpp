/**************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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

#include "utils.h"

#include "fileutils.h"

#include "qsettingswrapper.h"

#define SENTRY_BUILD_STATIC 1
#include <sentry.h>

#include <pdm.h>
#include <pdm_data.h>
#include <protobuf.h>

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QNetworkInterface>
#include <QProcessEnvironment>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QThread>
#include <QVector>
#include <QUuid>
#include <QtCore/QSettings>

#if defined(Q_OS_WIN) || defined(Q_OS_WINCE)
#   include "qt_windows.h"
#endif

#include <fstream>
#include <iostream>
#include <sstream>

#ifdef Q_OS_UNIX
#include <errno.h>
#include <signal.h>
#include <time.h>
#endif

#define QUOTE_(x) #x
#define QUOTE(x) QUOTE_(x)

void QInstaller::uiDetachedWait(int ms)
{
    QTime timer;
    timer.start();
    do {
        QCoreApplication::processEvents(QEventLoop::AllEvents, ms);
        QThread::msleep(10UL);
    } while (timer.elapsed() < ms);
}

/*!
    Starts the program \a program with the arguments \a arguments in a new process, and detaches
    from it. Returns true on success; otherwise returns false. If the calling process exits, the
    detached process will continue to live.

    Note that arguments that contain spaces are not passed to the process as separate arguments.

    Unix:    The started process will run in its own session and act like a daemon.
    Windows: Arguments that contain spaces are wrapped in quotes. The started process will run as
             a regular standalone process.

    The process will be started in the directory \a workingDirectory.

    If the function is successful then \a *pid is set to the process identifier of the started
    process.

    Additional note: The difference in using this function over its equivalent from QProcess
                     appears on Windows. While this function will truly detach and not show a GUI
                     window for the started process, the QProcess version will.
*/
bool QInstaller::startDetached(const QString &program, const QStringList &arguments,
    const QString &workingDirectory, qint64 *pid)
{
    bool success = false;
#ifdef Q_OS_WIN
    PROCESS_INFORMATION pinfo;
    STARTUPINFOW startupInfo = { sizeof(STARTUPINFO), nullptr, nullptr, nullptr,
        static_cast<ulong>(CW_USEDEFAULT), static_cast<ulong>(CW_USEDEFAULT),
        static_cast<ulong>(CW_USEDEFAULT), static_cast<ulong>(CW_USEDEFAULT),
        0, 0, 0, STARTF_USESHOWWINDOW, SW_HIDE, 0, nullptr, nullptr, nullptr, nullptr
    };  // That's the difference over QProcess::startDetached(): STARTF_USESHOWWINDOW, SW_HIDE.

    const QString commandline = QInstaller::createCommandline(program, arguments);
    if (CreateProcessW(nullptr, (wchar_t*) commandline.utf16(),
        nullptr, nullptr, false, CREATE_UNICODE_ENVIRONMENT | CREATE_NEW_CONSOLE,
        nullptr, workingDirectory.isEmpty() ? nullptr : (wchar_t*) workingDirectory.utf16(),
        &startupInfo, &pinfo)) {
        success = true;
        CloseHandle(pinfo.hThread);
        CloseHandle(pinfo.hProcess);
        if (pid)
            *pid = pinfo.dwProcessId;
    }
#else
    success = QProcess::startDetached(program, arguments, workingDirectory, pid);
#endif
    return success;
}

// Returns ["en-us", "en"] for "en-us"
QStringList QInstaller::localeCandidates(const QString &locale_)
{
    QStringList candidates;
    QString locale = locale_;
    candidates.reserve(locale.count(QLatin1Char('-')));
    forever {
        candidates.append(locale);
        int r = locale.lastIndexOf(QLatin1Char('-'));
        if (r <= 0)
            break;
        locale.truncate(r);
    }
    return candidates;
}

static QString installerVersion;

void QInstaller::setInstallerVersion(const QString& version)
{
    installerVersion = version;
}

QString QInstaller::getInstallerVersion()
{
    return installerVersion;
}

static bool isThisAnInstaller;

void QInstaller::setIsInstaller(bool installer)
{
    isThisAnInstaller = installer;
}

bool QInstaller::isInstaller()
{
    return isThisAnInstaller;
}

static bool verb = false;

void QInstaller::setVerbose(bool v)
{
    verb = v;
}

bool QInstaller::isVerbose()
{
    return verb;
}

static bool crashModeOn = false;

void QInstaller::setCrashAndBurnMode(bool on)
{
    crashModeOn = on;
}

bool QInstaller::isCrashAndBurnMode()
{
    return crashModeOn;
}

static QString logFileName;

bool QInstaller::isLogFileEnabled()
{
    return !logFileName.isEmpty();
}

void QInstaller::setLogFileName(const QString& fileName)
{
    logFileName = fileName;
}

QString QInstaller::getLogFileName()
{
    return logFileName;
}

static QString autoLogFileName;

bool QInstaller::isAutoLogEnabled()
{
    return !autoLogFileName.isEmpty();
}

void QInstaller::setAutoLogFileName(const QString& fileName)
{
    autoLogFileName = fileName;
}

QString QInstaller::getAutoLogFileName()
{
    return autoLogFileName;
}

QString createInstallerDir(const QString &string, bool temp);

QString createInstallerDir(const QString &string, bool temp)
{
    QString envPath = QInstaller::environmentVariable(QLatin1String(temp ? "temp" : "localappdata"));

    // Couldn't get location of %localappdata% / %temp% so we return an empty string
    if (envPath.isEmpty())
        return QString();

    // We want to store this in the same place as the Launcher and Client store their data
    QString dirPath = QString::fromLatin1("%1/CCP/EVE/%2").arg(envPath).arg(string);
    QDir dir(dirPath);

    // Create the directory if it doesn't exist
    if (!dir.exists()) {
        dir.mkpath(QLatin1String("."));
    }

    return dirPath;
}

/*!
    Returns new filename for a logfile.
    It also creates eve-installer under %temp% if %temp% is found.
    This is where the logfile will be stored. If %temp% is not found, we return an empty string.
    The logfile name will be in this format:
    yyyy.MM.dd-hh.mm.ss.zzz.txt
    The time is in UTC.
    Example: 2020.10.28-13.27.01.083
*/
QString QInstaller::getNewAutoLogFileName()
{
    QDateTime local(QDateTime::currentDateTime());
    QDateTime utcTime(local.toUTC());
    QString currentTime = utcTime.toString(QLatin1String("yyyy.MM.dd-hh.mm.ss.zzz"));

    QString dirPath = createInstallerDir(QString::fromLatin1("Installer"), false);
    if (dirPath.isEmpty())
        return QString();

    // Return the final name
    return QString::fromLatin1("%1/installerlog-%2.txt").arg(dirPath).arg(currentTime);
}

/*!
    NOTE! THIS IS A COPY OF PackageManagerCore::environmentVariable SINCE THIS IS HAPPENING
    BEFORE WE INITIALIZE THE PackageManagerCore!!!
    Returns the content of the environment variable \a name. An empty string is returned if the
    environment variable is not set.
*/
QString QInstaller::environmentVariable(const QString &name)
{
    if (name.isEmpty())
        return QString();

#ifdef Q_OS_WIN
    static TCHAR buffer[32767];
    DWORD size = GetEnvironmentVariable(LPCWSTR(name.utf16()), buffer, 32767);
    QString value = QString::fromUtf16((const unsigned short *) buffer, size);

    if (value.isEmpty()) {
        static QLatin1String userEnvironmentRegistryPath("HKEY_CURRENT_USER\\Environment");
        value = QSettings(userEnvironmentRegistryPath, QSettings::NativeFormat).value(name).toString();
        if (value.isEmpty()) {
            static QLatin1String systemEnvironmentRegistryPath("HKEY_LOCAL_MACHINE\\SYSTEM\\"
                "CurrentControlSet\\Control\\Session Manager\\Environment");
            value = QSettings(systemEnvironmentRegistryPath, QSettings::NativeFormat).value(name).toString();
        }
    }
    return value;
#else
    return QString::fromUtf8(qgetenv(name.toLatin1()));
#endif
}

static QString protoUrl;

bool QInstaller::sendProtoMessages()
{
    return !protoUrl.isEmpty();
}

void QInstaller::setProtoMessageEndpoint(const QString& url)
{
    protoUrl = url;
}

QString QInstaller::getProtoMessageEndpoint()
{
    return protoUrl;
}

static QString telemetryUrl;

bool QInstaller::useProvidedTelemetryEndpoint()
{
    return !telemetryUrl.isEmpty();
}

void QInstaller::setProvidedTelemetryEndpoint(const QString& url)
{
    telemetryUrl = url;
}

QString QInstaller::getProvidedTelemetryEndpoint()
{
    return telemetryUrl;
}

static QString installerFileName;

void QInstaller::setInstallerFileName(const QString& fileName)
{
    installerFileName = fileName;
}

QString QInstaller::getInstallerFileName()
{
    return installerFileName;
}

static QString baseCCPRegistryPath = QString::fromLatin1("HKEY_CURRENT_USER\\SOFTWARE\\CCP");

QString getPath(const QString& path)
{
    QString registryPath = baseCCPRegistryPath;
    if (!path.isEmpty())
    {
        registryPath = QString::fromLatin1("%1\\%2").arg(registryPath).arg(path);
    }
    return registryPath;
}

void QInstaller::setCCPRegistryKey(const QString& name, const QString& value, const QString& path)
{
    if (name.isEmpty())
    {
        return;
    }

#ifdef Q_OS_WIN
    QSettingsWrapper settings(getPath(path), QSettingsWrapper::NativeFormat);
    settings.setValue(name, value);
#endif
}

QString QInstaller::getCCPRegistryKey(const QString& name, const QString& path)
{
    if (name.isEmpty())
    {
        return QString();
    }

#ifdef Q_OS_WIN
    static QLatin1String userEnvironmentRegistryPath(getPath(path).toLocal8Bit());
    return QSettings(userEnvironmentRegistryPath, QSettings::NativeFormat).value(name).toString();
#else
    return QString();
#endif
}

static QUuid deviceId;

void QInstaller::setDeviceId(const QUuid& id)
{
    deviceId = id;
}

QUuid QInstaller::getDeviceId()
{
    return deviceId;
}

static QUuid journeyId;

void QInstaller::setJourneyId(const QUuid& id)
{
    journeyId = id;
}

QUuid QInstaller::getJourneyId()
{
    return journeyId;
}

static QString journeyToken;

void QInstaller::setJourneyToken(const QString& token)
{
    journeyToken = token;
}

QString QInstaller::getJourneyToken()
{
    return journeyToken;
}

static QUuid osId;

void QInstaller::setOsId(const QUuid& id)
{
    osId = id;
}

QUuid QInstaller::getOsId()
{
    return osId;
}

static QByteArray sessionHash;

void QInstaller::setSessionHash(const QByteArray& hash)
{
    sessionHash = hash;
}

QByteArray QInstaller::getSessionHash()
{
    return sessionHash;
}

static QString sessionId;

QString QInstaller::getSessionId()
{
    if (sessionId.isEmpty())
    {
        sessionId = QString(QLatin1String("ls")) + QString(QLatin1String(sessionHash.toHex()));
    }

    return sessionId;
}

QString QInstaller::getCrashDb()
{
    return createInstallerDir(QString::fromLatin1("Installer/Crashes"), false);
}

QString QInstaller::getTempPath()
{
    return createInstallerDir(QString::fromLatin1("installer-resources"), true);
}

// If you want to change this name, you need to update installer.qrc as well
static QString crashpadHandlerName = QString::fromLatin1("eve_installer_crashmon.exe");

QString QInstaller::getCrashpadHandlerName()
{
    return crashpadHandlerName;
}

static QString sentryDsn;

void QInstaller::setSentryDsn(const QString& dsn)
{
    sentryDsn = dsn;
}

QString QInstaller::getSentryDsn()
{
    return sentryDsn;
}

static QString pdmVersion;

void QInstaller::setPdmVersion(const QString& version)
{
    pdmVersion = version;
}

QString QInstaller::getPdmVersion()
{
    return pdmVersion;
}

static QString protobufVersion;

void QInstaller::setProtobufVersion(const QString& version)
{
    protobufVersion = version;
}

QString QInstaller::getProtobufVersion()
{
    return protobufVersion;
}

static QString sentryNativeVersion;

void QInstaller::setSentryNativeSdkVersion(const QString& version)
{
    sentryNativeVersion = version;
}

QString QInstaller::getSentryNativeSdkVersion()
{
    return sentryNativeVersion;
}

static QString qtVersion;

void QInstaller::setQtVersion(const QString& version)
{
    qtVersion = version;
}

QString QInstaller::getQtVersion()
{
    return qtVersion;
}

static QString qtIfwVersion;

void QInstaller::setQtIfwVersion(const QString& version)
{
    qtIfwVersion = version;
}

QString QInstaller::getQtIfwVersion()
{
    return qtIfwVersion;
}

static QString ccpIfwVersion;

void QInstaller::setCcpIfwVersion(const QString& version)
{
    ccpIfwVersion = version;
}

QString QInstaller::getCcpIfwVersion()
{
    return ccpIfwVersion;
}

static bool isThisRelease = false;

void QInstaller::setReleaseBuild(bool isRelease)
{
    isThisRelease = isRelease;
}

bool QInstaller::isReleaseBuild()
{
    return isThisRelease;
}

static bool isLocalDevelopmentBuid = false;

void QInstaller::setLocalDevelopmentBuild(bool isLocal)
{
    isLocalDevelopmentBuid = isLocal;
}

bool QInstaller::isLocalDevelopmentBuild()
{
    return isLocalDevelopmentBuid;
}

static QString installerRegion = QLatin1String("world");

void QInstaller::setRegion(const QString& region)
{
    installerRegion = region;
}

QString QInstaller::getRegion()
{
    return installerRegion;
}

bool QInstaller::isChina()
{
    return QString::compare(installerRegion, QLatin1String("china"), Qt::CaseInsensitive) == 0;
}

static QString launcherVersion = QString::fromLatin1(DEV_VERSION);

void QInstaller::setLauncherVersion(const QString& version)
{
    launcherVersion = version;
}

QString QInstaller::getLauncherVersion()
{
    return launcherVersion;
}

static QString installerEnvironment;

void QInstaller::setEnvironment(const QString& environment)
{
    installerEnvironment = environment;
}

QString QInstaller::getEnvironment()
{
    return installerEnvironment;
}

bool QInstaller::hasPartnerId()
{
    return
        isChina() &&
        QString::compare(getPartnerId(), QLatin1String("none"), Qt::CaseInsensitive) != 0;
}

static QString installerPartnerId = QLatin1String("none");

void QInstaller::setPartnerId(const QString& partnerId)
{
    installerPartnerId = partnerId;
}

QString QInstaller::getPartnerId()
{
    return installerPartnerId;
}

void QInstaller::initializeVersions()
{
    // Here we read in the versions of everything we're using
    QString pdmVersion = QString::fromStdString(PDM::GetPDMVersion());
    QString protobufVersion = QString::fromStdString(google::protobuf::internal::VersionString(GOOGLE_PROTOBUF_VERSION));
    QString sentryVersion = QString::fromStdString(SENTRY_SDK_VERSION);
    QString qtVersion = QString::fromStdString(QT_VERSION_STR);
    QString qtIfwVersion = QString::fromStdString(QUOTE(IFW_VERSION_STR));
    QString ccpIfwVersion = QString::fromStdString(QUOTE(CCP_FRAMEWORK_STRING));

    setPdmVersion(pdmVersion);
    setProtobufVersion(protobufVersion);
    setSentryNativeSdkVersion(sentryVersion);
    setQtVersion(qtVersion);
    setQtIfwVersion(qtIfwVersion);
    setCcpIfwVersion(ccpIfwVersion);

    QMap<const char*, QString> libraries {
        { "PDM", getPdmVersion() },
        { "Qt", getQtVersion() },
        { "Qt IFW", getQtIfwVersion() },
        { "Protobuf", getProtobufVersion() },
        { "CCP IFW", getCcpIfwVersion() },
        { "Sentry Native SDK", getSentryNativeSdkVersion() },
    };

    // Print the versions to the log file
    qDebug() << "Library versions: ";
    QMapIterator<const char*, QString> i(libraries);
    while (i.hasNext()) {
        i.next();
        qDebug().nospace() << "-- " << i.key() << ": " << i.value();
    }
}

void QInstaller::initializeJourneyIds()
{
    QUuid journeyId = getJourneyId();
    if (!journeyId.isNull())
    {
        // Journey ID has been read from the file name
        qDebug() << "framework | QInstaller::initializeJourneyIds | JourneyId found in filename:" << journeyId.toString(QUuid::WithoutBraces);
    }
    else
    {
        qDebug() << "framework | QInstaller::initializeJourneyIds | No JourneyId provided, one will be created instead";
        journeyId = QUuid::createUuid();
        setJourneyId(journeyId);
    }

    qDebug() << "JourneyId:";
    qDebug() << "-- JourneyId:" << journeyId.toString(QUuid::WithoutBraces);
    qDebug() << "-- JourneyId (base64 urlsafe):" << QLatin1String(journeyId.toRfc4122().toBase64(QByteArray::Base64UrlEncoding));

    QString keyName = QLatin1String("DeviceId");
    QUuid deviceId;
    // Try to get DeviceId from the registry
    QString value = getCCPRegistryKey(keyName);
    if (!value.isEmpty()) {
        qDebug() << "framework | QInstaller::initializeJourneyIds | DeviceId found in registry";
        deviceId = QUuid::fromString(value);
        qDebug() << "DeviceId:";
        qDebug() << "-- DeviceId:" << deviceId.toString(QUuid::WithoutBraces);
        qDebug() << "-- DeviceId (base64):" << QLatin1String(deviceId.toRfc4122().toBase64());
    }

    // If DeviceId was not found in the registry, then we use the current JourneyId
    if (deviceId.isNull())
    {
        qDebug() << "framework | QInstaller::initializeJourneyIds | Rolling a new DeviceId";
        deviceId = QUuid::createUuid();
        qDebug() << "DeviceId:";
        qDebug() << "-- DeviceId:" << deviceId.toString(QUuid::WithoutBraces);
        qDebug() << "-- DeviceId (base64):" << QLatin1String(deviceId.toRfc4122().toBase64());

        // We then store the DeviceId in the registry
        qDebug() << "framework | QInstaller::initializeJourneyIds | Storing DeviceId to registry";
        setCCPRegistryKey(keyName, deviceId.toString(QUuid::WithoutBraces));
        qDebug() << "framework | QInstaller::initializeJourneyIds | DeviceId stored to registry";
    }

    setDeviceId(deviceId);
}

void QInstaller::initializeOsId()
{
    std::string osUuidString = PDM::GetMachineUuidString();
    QUuid osId = QUuid::fromString(QString::fromStdString(osUuidString));

    qDebug() << "OS UUID:";
    qDebug() << "-- OsId:" << osId.toString(QUuid::WithoutBraces);
    qDebug() << "-- OsId (base64):" << QLatin1String(osId.toRfc4122().toBase64());

    setOsId(osId);
}

void QInstaller::initializeSessionHash()
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

    setSessionHash(hasher.result());

    qDebug() << "Session:";
    qDebug() << "-- ID:" << getSessionId();
}

void QInstaller::initializeIds()
{
    initializeJourneyIds();
    initializeOsId();
    initializeSessionHash();

    // Store the Journey Token in the registry
    QString token = getJourneyToken();
    if (!token.isNull())
    {
        qDebug() << "Storing Journey Token to registry:" << token;
        setCCPRegistryKey(QLatin1String("JourneyToken"), token);
    }
}

std::ostream &QInstaller::operator<<(std::ostream &os, const QString &string)
{
    return os << qPrintable(string);
}

QByteArray QInstaller::calculateHash(QIODevice *device, QCryptographicHash::Algorithm algo)
{
    Q_ASSERT(device);
    QCryptographicHash hash(algo);
    static QByteArray buffer(1024 * 1024, '\0');
    while (true) {
        const qint64 numRead = device->read(buffer.data(), buffer.size());
        if (numRead <= 0)
            return hash.result();
        hash.addData(buffer.constData(), numRead);
    }
    return QByteArray(); // never reached
}

QByteArray QInstaller::calculateHash(const QString &path, QCryptographicHash::Algorithm algo)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return QByteArray();
    return calculateHash(&file, algo);
}

QString QInstaller::replaceVariables(const QHash<QString, QString> &vars, const QString &str)
{
    QString res;
    int pos = 0;
    while (true) {
        int pos1 = str.indexOf(QLatin1Char('@'), pos);
        if (pos1 == -1)
            break;
        int pos2 = str.indexOf(QLatin1Char('@'), pos1 + 1);
        if (pos2 == -1)
            break;
        res += str.mid(pos, pos1 - pos);
        QString name = str.mid(pos1 + 1, pos2 - pos1 - 1);
        res += vars.value(name);
        pos = pos2 + 1;
    }
    res += str.mid(pos);
    return res;
}

QString QInstaller::replaceWindowsEnvironmentVariables(const QString &str)
{
    const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QString res;
    int pos = 0;
    while (true) {
        int pos1 = str.indexOf(QLatin1Char( '%'), pos);
        if (pos1 == -1)
            break;
        int pos2 = str.indexOf(QLatin1Char( '%'), pos1 + 1);
        if (pos2 == -1)
            break;
        res += str.mid(pos, pos1 - pos);
        QString name = str.mid(pos1 + 1, pos2 - pos1 - 1);
        res += env.value(name);
        pos = pos2 + 1;
    }
    res += str.mid(pos);
    return res;
}

QInstaller::VerboseWriter::VerboseWriter()
{
    preFileBuffer.open(QIODevice::ReadWrite);
    stream.setDevice(&preFileBuffer);
    currentDateTimeAsString = QDateTime::currentDateTime().toString();
}

QInstaller::VerboseWriter::~VerboseWriter()
{
    if (preFileBuffer.isOpen()) {
        PlainVerboseWriterOutput output;
        (void)flush(&output);
    }
}

bool QInstaller::VerboseWriter::flush(VerboseWriterOutput *output)
{
    stream.flush();
    if (logFileName.isEmpty()) // binarycreator
        return true;
    if (!preFileBuffer.isOpen())
        return true;
    //if the installer installed nothing - there is no target directory - where the logfile can be saved
    if (!QFileInfo(logFileName).absoluteDir().exists())
        return true;

    QString logInfo;
    logInfo += QLatin1String("************************************* Invoked: ");
    logInfo += currentDateTimeAsString;
    logInfo += QLatin1String("\n");

    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly);
    buffer.write(logInfo.toLocal8Bit());
    buffer.write(preFileBuffer.data());
    buffer.close();

    if (output->write(logFileName, QIODevice::ReadWrite | QIODevice::Append | QIODevice::Text, buffer.data())) {
        preFileBuffer.close();
        stream.setDevice(nullptr);
        return true;
    }
    return false;
}

void QInstaller::VerboseWriter::setFileName(const QString &fileName)
{
    logFileName = fileName;
}


Q_GLOBAL_STATIC(QInstaller::VerboseWriter, verboseWriter)

QInstaller::VerboseWriter *QInstaller::VerboseWriter::instance()
{
    return verboseWriter();
}

void QInstaller::VerboseWriter::appendLine(const QString &msg)
{
    stream << msg << endl;
}

QInstaller::VerboseWriterOutput::~VerboseWriterOutput()
{
}

bool QInstaller::PlainVerboseWriterOutput::write(const QString &fileName, QIODevice::OpenMode openMode, const QByteArray &data)
{
    QFile output(fileName);
    if (output.open(openMode)) {
        output.write(data);
        setDefaultFilePermissions(&output, DefaultFilePermissions::NonExecutable);
        return true;
    }
    return false;
}

#ifdef Q_OS_WIN
// taken from qcoreapplication_p.h
template<typename Char>
static QVector<Char*> qWinCmdLine(Char *cmdParam, int length, int &argc)
{
    QVector<Char*> argv(8);
    Char *p = cmdParam;
    Char *p_end = p + length;

    argc = 0;

    while (*p && p < p_end) {                                // parse cmd line arguments
        while (QChar((short)(*p)).isSpace())                          // skip white space
            p++;
        if (*p && p < p_end) {                                // arg starts
            int quote;
            Char *start, *r;
            if (*p == Char('\"') || *p == Char('\'')) {        // " or ' quote
                quote = *p;
                start = ++p;
            } else {
                quote = 0;
                start = p;
            }
            r = start;
            while (*p && p < p_end) {
                if (quote) {
                    if (*p == quote) {
                        p++;
                        if (QChar((short)(*p)).isSpace())
                            break;
                        quote = 0;
                    }
                }
                if (*p == '\\') {                // escape char?
                    p++;
                    if (*p == Char('\"') || *p == Char('\''))
                        ;                        // yes
                    else
                        p--;                        // treat \ literally
                } else {
                    if (!quote && (*p == Char('\"') || *p == Char('\''))) {        // " or ' quote
                        quote = *p++;
                        continue;
                    } else if (QChar((short)(*p)).isSpace() && !quote)
                        break;
                }
                if (*p)
                    *r++ = *p++;
            }
            if (*p && p < p_end)
                p++;
            *r = Char('\0');

            if (argc >= (int)argv.size()-1)        // expand array
                argv.resize(argv.size()*2);
            argv[argc++] = start;
        }
    }
    argv[argc] = nullptr;

    return argv;
}

QStringList QInstaller::parseCommandLineArgs(int argc, char **argv)
{
    Q_UNUSED(argc)
    Q_UNUSED(argv)

    QStringList arguments;
    QString cmdLine = QString::fromWCharArray(GetCommandLine());

    QVector<wchar_t*> args = qWinCmdLine<wchar_t>((wchar_t *)cmdLine.utf16(), cmdLine.length(), argc);
    for (int a = 0; a < argc; ++a)
        arguments << QString::fromWCharArray(args[a]);
    return arguments;
}
#else
QStringList QInstaller::parseCommandLineArgs(int argc, char **argv)
{
    QStringList arguments;
    for (int a = 0; a < argc; ++a)
        arguments << QString::fromLocal8Bit(argv[a]);
    return arguments;
}
#endif

#ifdef Q_OS_WIN
// taken from qprocess_win.cpp
static QString qt_create_commandline(const QString &program, const QStringList &arguments)
{
    QString args;
    if (!program.isEmpty()) {
        QString programName = program;
        if (!programName.startsWith(QLatin1Char('\"')) && !programName.endsWith(QLatin1Char('\"'))
            && programName.contains(QLatin1Char(' '))) {
                programName = QLatin1Char('\"') + programName + QLatin1Char('\"');
        }
        programName.replace(QLatin1Char('/'), QLatin1Char('\\'));

        // add the program as the first arg ... it works better
        args = programName + QLatin1Char(' ');
    }

    for (int i = 0; i < arguments.size(); ++i) {
        QString tmp = arguments.at(i);
        // in the case of \" already being in the string the \ must also be escaped
        tmp.replace(QLatin1String("\\\""), QLatin1String("\\\\\""));
        // escape a single " because the arguments will be parsed
        tmp.replace(QLatin1Char('\"'), QLatin1String("\\\""));
        if (tmp.isEmpty() || tmp.contains(QLatin1Char(' ')) || tmp.contains(QLatin1Char('\t'))) {
            // The argument must not end with a \ since this would be interpreted
            // as escaping the quote -- rather put the \ behind the quote: e.g.
            // rather use "foo"\ than "foo\"
            QString endQuote(QLatin1Char('\"'));
            int i = tmp.length();
            while (i > 0 && tmp.at(i - 1) == QLatin1Char('\\')) {
                --i;
                endQuote += QLatin1Char('\\');
            }
            args += QLatin1String(" \"") + tmp.left(i) + endQuote;
        } else {
            args += QLatin1Char(' ') + tmp;
        }
    }
    return args;
}

QString QInstaller::createCommandline(const QString &program, const QStringList &arguments)
{
    return qt_create_commandline(program, arguments);
}

//copied from qsystemerror.cpp in Qt
QString QInstaller::windowsErrorString(int errorCode)
{
    QString ret;
    wchar_t *string = nullptr;
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
        nullptr,
        errorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPWSTR) &string,
        0,
        nullptr);
    ret = QString::fromWCharArray(string);
    LocalFree((HLOCAL) string);

    if (ret.isEmpty() && errorCode == ERROR_MOD_NOT_FOUND)
        ret = QCoreApplication::translate("QInstaller", "The specified module could not be found.");

    ret.append(QLatin1String(" (0x"));
    ret.append(QString::number(uint(errorCode), 16).rightJustified(8, QLatin1Char('0')));
    ret.append(QLatin1String(")"));

    return ret;
}

#endif
