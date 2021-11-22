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

#ifndef QINSTALLER_UTILS_H
#define QINSTALLER_UTILS_H

#include "installer_global.h"

#include <QtCore/QBuffer>
#include <QtCore/QCryptographicHash>
#include <QtCore/QHash>
#include <QtCore/QUrl>
#include <QtCore/QTextStream>

#include <ostream>

#define DEV_VERSION "9999999"

QT_BEGIN_NAMESPACE
class QIODevice;
QT_END_NAMESPACE

namespace QInstaller {
    void INSTALLER_EXPORT uiDetachedWait(int ms);
    bool INSTALLER_EXPORT startDetached(const QString &program, const QStringList &arguments,
        const QString &workingDirectory, qint64 *pid = 0);

    QByteArray INSTALLER_EXPORT calculateHash(QIODevice *device, QCryptographicHash::Algorithm algo);
    QByteArray INSTALLER_EXPORT calculateHash(const QString &path, QCryptographicHash::Algorithm algo);

    QString INSTALLER_EXPORT replaceVariables(const QHash<QString,QString> &vars, const QString &str);
    QString INSTALLER_EXPORT replaceWindowsEnvironmentVariables(const QString &str);
    QStringList INSTALLER_EXPORT parseCommandLineArgs(int argc, char **argv);

#ifdef Q_OS_WIN
    QString windowsErrorString(int errorCode);
    QString createCommandline(const QString &program, const QStringList &arguments);
#endif

    QStringList INSTALLER_EXPORT localeCandidates(const QString &locale);

    void INSTALLER_EXPORT setInstallerVersion(const QString& version);
    QString INSTALLER_EXPORT getInstallerVersion();

    void INSTALLER_EXPORT setIsInstaller(bool installer);
    bool INSTALLER_EXPORT isInstaller();

    void INSTALLER_EXPORT setVerbose(bool v);
    bool INSTALLER_EXPORT isVerbose();

    void INSTALLER_EXPORT setCrashAndBurnMode(bool on);
    bool INSTALLER_EXPORT isCrashAndBurnMode();

    // Log to file
    bool INSTALLER_EXPORT isLogFileEnabled();
    void INSTALLER_EXPORT setLogFileName(const QString& fileName);
    QString INSTALLER_EXPORT getLogFileName();

    // Automatic logging
    bool INSTALLER_EXPORT isAutoLogEnabled();
    void INSTALLER_EXPORT setAutoLogFileName(const QString& fileName);
    QString INSTALLER_EXPORT getAutoLogFileName();
    QString INSTALLER_EXPORT getNewAutoLogFileName();
    QString INSTALLER_EXPORT environmentVariable(const QString& name);

    // Proto messages
    bool INSTALLER_EXPORT sendProtoMessages();
    void INSTALLER_EXPORT setProtoMessageEndpoint(const QString& url);
    QString INSTALLER_EXPORT getProtoMessageEndpoint();

    // Telemetry endpoint
    bool INSTALLER_EXPORT useProvidedTelemetryEndpoint();
    void INSTALLER_EXPORT setProvidedTelemetryEndpoint(const QString& url);
    QString INSTALLER_EXPORT getProvidedTelemetryEndpoint();

    void INSTALLER_EXPORT setInstallerFileName(const QString& fileName);
    QString INSTALLER_EXPORT getInstallerFileName();

    void INSTALLER_EXPORT setCCPRegistryKey(const QString& name, const QString& value, const QString& path = QLatin1String("EVE"));
    QString INSTALLER_EXPORT getCCPRegistryKey(const QString& name, const QString& path = QLatin1String("EVE"));

    // Ids
    void INSTALLER_EXPORT setDeviceId(const QUuid& id);
    QUuid INSTALLER_EXPORT getDeviceId();
    void INSTALLER_EXPORT setJourneyId(const QUuid& id);
    QUuid INSTALLER_EXPORT getJourneyId();
    void INSTALLER_EXPORT setJourneyToken(const QString& token);
    QString INSTALLER_EXPORT getJourneyToken();
    void INSTALLER_EXPORT setOsId(const QUuid& id);
    QUuid INSTALLER_EXPORT getOsId();
    void INSTALLER_EXPORT setSessionHash(const QByteArray& hash);
    QByteArray INSTALLER_EXPORT getSessionHash();
    QString INSTALLER_EXPORT getSessionId();

    // %temp%/installer-resources
    QString INSTALLER_EXPORT getTempPath();

    // Sentry related
    QString INSTALLER_EXPORT getCrashDb();
    QString INSTALLER_EXPORT getCrashpadHandlerName();
    void INSTALLER_EXPORT setSentryDsn(const QString& dsn);
    QString INSTALLER_EXPORT getSentryDsn();

    // Versions
    void INSTALLER_EXPORT setPdmVersion(const QString& version);
    QString INSTALLER_EXPORT getPdmVersion();
    void INSTALLER_EXPORT setProtobufVersion(const QString& version);
    QString INSTALLER_EXPORT getProtobufVersion();
    void INSTALLER_EXPORT setSentryNativeSdkVersion(const QString& version);
    QString INSTALLER_EXPORT getSentryNativeSdkVersion();
    void INSTALLER_EXPORT setQtVersion(const QString& version);
    QString INSTALLER_EXPORT getQtVersion();
    void INSTALLER_EXPORT setQtIfwVersion(const QString& version);
    QString INSTALLER_EXPORT getQtIfwVersion();
    void INSTALLER_EXPORT setCcpIfwVersion(const QString& version);
    QString INSTALLER_EXPORT getCcpIfwVersion();


    // Launcher related
    void INSTALLER_EXPORT setReleaseBuild(bool isRelease);
    bool INSTALLER_EXPORT isReleaseBuild();
    void INSTALLER_EXPORT setLocalDevelopmentBuild(bool isLocal);
    bool INSTALLER_EXPORT isLocalDevelopmentBuild();
    bool INSTALLER_EXPORT isChina();
    void INSTALLER_EXPORT setRegion(const QString& region);
    QString INSTALLER_EXPORT getRegion();
    void INSTALLER_EXPORT setLauncherVersion(const QString& version);
    QString INSTALLER_EXPORT getLauncherVersion();
    void INSTALLER_EXPORT setEnvironment(const QString& environment);
    QString INSTALLER_EXPORT getEnvironment();
    bool INSTALLER_EXPORT hasPartnerId();
    void INSTALLER_EXPORT setPartnerId(const QString& partnerId);
    QString INSTALLER_EXPORT getPartnerId();

    // Init ids
    void initializeVersions();
    void initializeJourneyIds();
    void initializeOsId();
    void initializeSessionHash();
    void INSTALLER_EXPORT initializeIds();

    INSTALLER_EXPORT std::ostream& operator<<(std::ostream &os, const QString &string);

    class INSTALLER_EXPORT VerboseWriterOutput
    {
    public:
        virtual bool write(const QString &fileName, QIODevice::OpenMode openMode, const QByteArray &data) = 0;

    protected:
        ~VerboseWriterOutput();
    };

    class INSTALLER_EXPORT PlainVerboseWriterOutput : public VerboseWriterOutput
    {
    public:
        virtual bool write(const QString &fileName, QIODevice::OpenMode openMode, const QByteArray &data);
    };

    class INSTALLER_EXPORT VerboseWriter
    {
    public:
        VerboseWriter();
        ~VerboseWriter();

        static VerboseWriter *instance();

        bool flush(VerboseWriterOutput *output);

        void appendLine(const QString &msg);
        void setFileName(const QString &fileName);

    private:
        QTextStream stream;
        QBuffer preFileBuffer;
        QString logFileName;
        QString currentDateTimeAsString;
    };

}

#endif // QINSTALLER_UTILS_H
