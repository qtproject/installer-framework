/**************************************************************************
**
** Copyright (C) 2022 The Qt Company Ltd.
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

#include "loggingutils.h"

#include "component.h"
#include "componentalias.h"
#include "globals.h"
#include "fileutils.h"
#include "packagemanagercore.h"
#include "remoteclient.h"
#include "remotefileengine.h"

#include <QXmlStreamWriter>
#include <QElapsedTimer>

#include <iostream>
#if defined(Q_OS_UNIX)
#include <unistd.h>
#elif defined(Q_OS_WIN)
#include <stdio.h>
#include <io.h>
#endif

namespace QInstaller {

/*!
    \class QInstaller::LoggingHandler
    \inmodule QtInstallerFramework
    \brief The LoggingHandler class provides methods for manipulating the
    application-wide verbosiveness and format of printed debug messages.

    The class also contains utility methods for printing common preformatted messages.
*/

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::Uptime
    \internal
*/

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::VerboseWriter
    \internal
*/

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::VerboseWriterOutput
    \internal
*/

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::PlainVerboseWriterOutput
    \internal
*/

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::VerboseWriterAdminOutput
    \internal
*/

/*!
    \enum LoggingHandler::VerbosityLevel
    \brief This enum holds the possible levels of output verbosity.

    \value Silent
    \value Normal
    \value Detailed
    \value Minimum
           Minimum possible verbosity level. Synonym for \c VerbosityLevel::Silent.
    \value Maximum
           Maximum possible verbosity level. Synonym for \c VerbosityLevel::Detailed.
*/

// start timer on construction (so we can use it as static member)
class Uptime : public QElapsedTimer {
public:
    Uptime() { start(); }
};

/*!
    \internal
*/
LoggingHandler::LoggingHandler()
    : m_verbLevel(VerbosityLevel::Silent)
    , m_outputRedirected(false)
{
#if defined(Q_OS_UNIX)
    m_outputRedirected = !isatty(fileno(stdout));
#elif defined(Q_OS_WIN)
    m_outputRedirected = !_isatty(_fileno(stdout));
#endif
}

/*!
    \internal
*/
LoggingHandler::~LoggingHandler()
{
}

/*!
    Prints out preformatted debug messages, warnings, critical and fatal error messages
    specified by \a msg and \a type. The message \a context provides information about
    the source code location the message was generated.
*/
void LoggingHandler::messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QMutexLocker _(&m_mutex);

    // suppress warning from QPA minimal plugin
    if (msg.contains(QLatin1String("This plugin does not support propagateSizeHints")))
        return;

    if (context.category == lcProgressIndicator().categoryName()) {
        if (!outputRedirected())
            std::cout << msg.toStdString() << "\r" << std::flush;
        return;
    }

    static Uptime uptime;

    QString ba = QLatin1Char('[') + QString::number(uptime.elapsed()) + QStringLiteral("] ");
    ba += trimAndPrepend(type, msg);

    if (type != QtDebugMsg && context.file) {
        ba += QString(QStringLiteral(" (%1:%2, %3)")).arg(
                    QString::fromLatin1(context.file)).arg(context.line).arg(
                    QString::fromLatin1(context.function));
    }

    if (VerboseWriter *log = VerboseWriter::instance())
        log->appendLine(ba);

    if (type != QtDebugMsg || isVerbose())
        std::cout << qPrintable(ba) << std::endl;

    if (type == QtFatalMsg) {
        QtMessageHandler oldMsgHandler = qInstallMessageHandler(nullptr);
        qt_message_output(type, context, msg);
        qInstallMessageHandler(oldMsgHandler);
    }
}

/*!
    Trims the trailing space character and surrounding quotes from \a msg.
    Also prepends the message \a type to the message.
*/
QString LoggingHandler::trimAndPrepend(QtMsgType type, const QString &msg) const
{
    QString ba(msg);
    // last character is a space from qDebug
    if (ba.endsWith(QLatin1Char(' ')))
        ba.chop(1);

    // remove quotes if the whole message is surrounded with them
    if (ba.startsWith(QLatin1Char('"')) && ba.endsWith(QLatin1Char('"')))
        ba = ba.mid(1, ba.length() - 2);

    // prepend the message type, skip QtDebugMsg
    switch (type) {
        case QtWarningMsg:
            ba.prepend(QStringLiteral("Warning: "));
        break;

        case QtCriticalMsg:
            ba.prepend(QStringLiteral("Critical: "));
        break;

        case QtFatalMsg:
            ba.prepend(QStringLiteral("Fatal: "));
        break;

        default:
            break;
    }
    return ba;
}

/*!
    Returns the only instance of this class.
*/
LoggingHandler &LoggingHandler::instance()
{
    static LoggingHandler instance;
    return instance;
}

/*!
    Sets to verbose output if \a v is set to \c true. Calling this multiple
    times increases or decreases the verbosity level accordingly.
*/
void LoggingHandler::setVerbose(bool v)
{
    if (v)
        m_verbLevel++;
    else
        m_verbLevel--;
}

/*!
    Returns \c true if the installer is set to verbose output.
*/
bool LoggingHandler::isVerbose() const
{
    return m_verbLevel != VerbosityLevel::Silent;
}

/*!
    Returns the current verbosity level.
*/
LoggingHandler::VerbosityLevel LoggingHandler::verboseLevel() const
{
    return m_verbLevel;
}

/*!
    Returns \c true if output is redirected, i.e. to a file.
*/
bool LoggingHandler::outputRedirected() const
{
    return m_outputRedirected;
}

/*!
    Prints update information from \a components.
*/
void LoggingHandler::printUpdateInformation(const QList<Component *> &components) const
{
    QString output;
    QXmlStreamWriter stream(&output);
    stream.setAutoFormatting(true);
    stream.writeStartDocument();

    stream.writeStartElement(QLatin1String("updates"));
    foreach (const Component *component, components) {
        stream.writeStartElement(QLatin1String("update"));
        stream.writeAttribute(QLatin1String("name"), component->value(scDisplayName));
        stream.writeAttribute(QLatin1String("version"), component->value(scVersion));
        stream.writeAttribute(QLatin1String("size"), component->value(scUncompressedSize));
        stream.writeAttribute(QLatin1String("id"), component->value(scName));
        stream.writeEndElement();
    }
    stream.writeEndElement();

    stream.writeEndDocument();
    std::cout << qPrintable(output);
}

/*!
    Prints basic or more detailed information about local \a packages,
    depending on the current verbosity level.
*/
void LoggingHandler::printLocalPackageInformation(const QList<KDUpdater::LocalPackage> &packages) const
{
    QString output;
    QXmlStreamWriter stream(&output);
    stream.setAutoFormatting(true);
    stream.writeStartDocument();

    stream.writeStartElement(QLatin1String("localpackages"));
    foreach (KDUpdater::LocalPackage package, packages) {
        stream.writeStartElement(QLatin1String("package"));
        stream.writeAttribute(QLatin1String("name"), package.name);
        stream.writeAttribute(QLatin1String("displayname"), package.title);
        stream.writeAttribute(QLatin1String("version"), package.version);
        if (verboseLevel() == VerbosityLevel::Detailed) {
            stream.writeAttribute(QLatin1String("description"), package.description);
            stream.writeAttribute(QLatin1String("sortingPriority"), QVariant(package.sortingPriority).toString());
            stream.writeAttribute(QLatin1String("treeName"), package.treeName.first);
            stream.writeAttribute(QLatin1String("moveChildren"), QVariant(package.treeName.second).toString());
            stream.writeAttribute(QLatin1String("dependencies"), package.dependencies.join(QLatin1Char(',')));
            stream.writeAttribute(QLatin1String("autoDependencies"), package.autoDependencies.join(QLatin1Char(',')));
            stream.writeAttribute(QLatin1String("virtual"), QVariant(package.virtualComp).toString());
            stream.writeAttribute(QLatin1String("forcedInstallation"), QVariant(package.forcedInstallation).toString());
            stream.writeAttribute(QLatin1String("checkable"), QVariant(package.checkable).toString());
            stream.writeAttribute(QLatin1String("uncompressedSize"), QVariant(package.uncompressedSize).toString());
            stream.writeAttribute(QLatin1String("installDate"), package.installDate.toString());
            stream.writeAttribute(QLatin1String("lastUpdateDate"), package.lastUpdateDate.toString());
        }
        stream.writeEndElement();
    }
    stream.writeEndElement();

    stream.writeEndDocument();
    std::cout << qPrintable(output);
}

/*!
    Prints basic or more detailed information about available \a matchedPackages,
    depending on the current verbosity level. If a package is also present in \a installedPackages,
    the installed version will be included in printed information.
*/
void LoggingHandler::printPackageInformation(const PackagesList &matchedPackages, const LocalPackagesMap &installedPackages) const
{
    QString output;
    QXmlStreamWriter stream(&output);
    stream.setAutoFormatting(true);
    stream.writeStartDocument();

    stream.writeStartElement(QLatin1String("availablepackages"));
    foreach (Package *package, matchedPackages) {
        const QString name = package->data(scName).toString();
        stream.writeStartElement(QLatin1String("package"));
        stream.writeAttribute(QLatin1String("name"), name);
        stream.writeAttribute(QLatin1String("displayname"), package->data(scDisplayName).toString());
        stream.writeAttribute(QLatin1String("version"), package->data(scVersion).toString());
        //Check if package already installed
        if (installedPackages.contains(name))
            stream.writeAttribute(QLatin1String("installedVersion"), installedPackages.value(name).version);
        if (verboseLevel() == VerbosityLevel::Detailed) {
            stream.writeAttribute(QLatin1String("description"), package->data(scDescription).toString());
            stream.writeAttribute(QLatin1String("treeName"), package->data(scTreeName).value<QPair<QString, bool>>().first);
            stream.writeAttribute(QLatin1String("moveChildren"), QVariant(package->data(scTreeName).value<QPair<QString, bool>>().second).toString());
            stream.writeAttribute(QLatin1String("dependencies"), package->data(scDependencies).toString());
            stream.writeAttribute(QLatin1String("autoDependencies"), package->data(scAutoDependOn).toString());
            stream.writeAttribute(QLatin1String("virtual"), package->data(scVirtual).toString());
            stream.writeAttribute(QLatin1String("forcedInstallation"), package->data(QLatin1String("ForcedInstallation")).toString());
            stream.writeAttribute(QLatin1String("checkable"), package->data(scCheckable).toString());
            stream.writeAttribute(QLatin1String("default"), package->data(scDefault).toString());
            stream.writeAttribute(QLatin1String("essential"), package->data(scEssential).toString());
            stream.writeAttribute(QLatin1String("forcedUpdate"), package->data(scForcedUpdate).toString());
            stream.writeAttribute(QLatin1String("compressedsize"), package->data(QLatin1String("CompressedSize")).toString());
            stream.writeAttribute(QLatin1String("uncompressedsize"), package->data(QLatin1String("UncompressedSize")).toString());
            stream.writeAttribute(QLatin1String("releaseDate"), package->data(scReleaseDate).toString());
            stream.writeAttribute(QLatin1String("downloadableArchives"), package->data(scDownloadableArchives).toString());
            stream.writeAttribute(QLatin1String("licenses"), package->data(QLatin1String("Licenses")).toString());
            stream.writeAttribute(QLatin1String("script"), package->data(scScript).toString());
            stream.writeAttribute(QLatin1String("sortingPriority"), package->data(scSortingPriority).toString());
            stream.writeAttribute(QLatin1String("replaces"), package->data(scReplaces).toString());
            stream.writeAttribute(QLatin1String("requiresAdminRights"), package->data(scRequiresAdminRights).toString());
        }
        stream.writeEndElement();
    }
    stream.writeEndElement();

    stream.writeEndDocument();
    std::cout << qPrintable(output);
}

/*!
    Prints basic or more detailed information about component \a aliases,
    depending on the current verbosity level.
*/
void LoggingHandler::printAliasInformation(const QList<ComponentAlias *> &aliases)
{
    QList<ComponentAlias *> sortedAliases = aliases;
    std::sort(sortedAliases.begin(), sortedAliases.end(),
        [](const ComponentAlias *lhs, const ComponentAlias *rhs) {
            return lhs->name() < rhs->name();
        }
    );

    QString output;
    QTextStream stream(&output);

    stream << Qt::endl;
    for (auto *alias : qAsConst(sortedAliases)) {
        stream << "Name: " << alias->name() << Qt::endl;
        stream << "Display name: " << alias->displayName() << Qt::endl;
        stream << "Description: " << alias->description() << Qt::endl;
        stream << "Version: " << alias->version() << Qt::endl;
        if (verboseLevel() == VerbosityLevel::Detailed)
            stream << "Virtual: " << alias->value(scVirtual) << Qt::endl;

        stream << "Components: " << alias->value(scRequiredComponents) << Qt::endl;
        stream << "Required aliases: " << alias->value(scRequiredAliases) << Qt::endl;

        stream << "Optional components: " << alias->value(scOptionalComponents) << Qt::endl;
        stream << "Optional aliases: " << alias->value(scOptionalAliases) << Qt::endl;

        if (sortedAliases.indexOf(alias) != (sortedAliases.count() - 1))
            stream << "========================================" << Qt::endl;
    }

    std::cout << qPrintable(output);
}

/*!
    \internal
*/
VerboseWriter::VerboseWriter()
{
    m_preFileBuffer.open(QIODevice::ReadWrite);
    m_stream.setDevice(&m_preFileBuffer);
    m_currentDateTimeAsString = QDateTime::currentDateTime().toString();
}

/*!
    \internal
*/
VerboseWriter::~VerboseWriter()
{
    if (m_preFileBuffer.isOpen()) {
        PlainVerboseWriterOutput output;
        (void)flush(&output);
    }
}

/*!
    \internal
*/
bool VerboseWriter::flush(VerboseWriterOutput *output)
{
    m_stream.flush();
    if (m_logFileName.isEmpty()) // binarycreator
        return true;
    if (!m_preFileBuffer.isOpen())
        return true;
    //if the installer installed nothing - there is no target directory - where the logfile can be saved
    if (!QFileInfo(m_logFileName).absoluteDir().exists())
        return true;

    QString logInfo;
    logInfo += QLatin1String("************************************* Invoked: ");
    logInfo += m_currentDateTimeAsString;
    logInfo += QLatin1String("\n");

    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly);
    buffer.write(logInfo.toLocal8Bit());
    buffer.write(m_preFileBuffer.data());
    buffer.close();

    if (output->write(m_logFileName, QIODevice::ReadWrite | QIODevice::Append | QIODevice::Text, buffer.data())) {
        m_preFileBuffer.close();
        m_stream.setDevice(nullptr);
        return true;
    }
    return false;
}

/*!
    \internal
*/
void VerboseWriter::setFileName(const QString &fileName)
{
    m_logFileName = fileName;
}

Q_GLOBAL_STATIC(VerboseWriter, verboseWriter)

/*!
    \internal
*/
VerboseWriter *VerboseWriter::instance()
{
    return verboseWriter();
}

/*!
    \internal
*/
void VerboseWriter::appendLine(const QString &msg)
{
    m_stream << msg << Qt::endl;
}

/*!
    \internal
*/
VerboseWriterOutput::~VerboseWriterOutput()
{
}

/*!
    \internal
*/
bool PlainVerboseWriterOutput::write(const QString &fileName, QIODevice::OpenMode openMode, const QByteArray &data)
{
    QFile output(fileName);
    if (output.open(openMode)) {
        output.write(data);
        setDefaultFilePermissions(&output, DefaultFilePermissions::NonExecutable);
        return true;
    }
    return false;
}

/*!
    \internal
*/
bool VerboseWriterAdminOutput::write(const QString &fileName, QIODevice::OpenMode openMode, const QByteArray &data)
{
    bool gainedAdminRights = false;

    if (!RemoteClient::instance().isActive()) {
        m_core->gainAdminRights();
        gainedAdminRights = true;
    }

    RemoteFileEngine file;
    file.setFileName(fileName);
    if (file.open(openMode)) {
        file.write(data.constData(), data.size());
        file.close();
        if (gainedAdminRights)
            m_core->dropAdminRights();
        return true;
    }

    if (gainedAdminRights)
        m_core->dropAdminRights();

    return false;
}

/*!
    \internal

    Increments verbosity \a level.
*/
LoggingHandler::VerbosityLevel &operator++(LoggingHandler::VerbosityLevel &level, int)
{
    const int i = static_cast<int>(level) + 1;
    level = (i > LoggingHandler::VerbosityLevel::Maximum)
        ? LoggingHandler::VerbosityLevel::Maximum
        : static_cast<LoggingHandler::VerbosityLevel>(i);

    return level;
}

/*!
    \internal

    Decrements verbosity \a level.
*/
LoggingHandler::VerbosityLevel &operator--(LoggingHandler::VerbosityLevel &level, int)
{
    const int i = static_cast<int>(level) - 1;
    level = (i < LoggingHandler::VerbosityLevel::Minimum)
        ? LoggingHandler::VerbosityLevel::Minimum
        : static_cast<LoggingHandler::VerbosityLevel>(i);

    return level;
}

} // namespace QInstaller
