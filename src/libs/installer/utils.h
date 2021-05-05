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

    void INSTALLER_EXPORT setVerbose(bool v);
    bool INSTALLER_EXPORT isVerbose();

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

    void INSTALLER_EXPORT setJourneyId(const QUuid& journeyId);
    QUuid INSTALLER_EXPORT getJourneyId();

    QString INSTALLER_EXPORT getKeyFromRegistry(const QString& path, const QString& name);
    void INSTALLER_EXPORT setGlobalId(const QUuid& id);
    QUuid INSTALLER_EXPORT getGlobalId();

    // %temp%/installer-resources
    QString INSTALLER_EXPORT getTempPath();

    // Sentry related
    QString INSTALLER_EXPORT getCrashDb();
    QString INSTALLER_EXPORT getCrashpadHandlerName();

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
