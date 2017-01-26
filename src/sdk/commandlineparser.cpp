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

#include "commandlineparser.h"

#include "constants.h"
#include "globals.h"

namespace CommandLineOptions {
const char KeyValue[] = "Key=Value";
} // namespace CommandLineOptions

CommandLineParser::CommandLineParser()
{
    m_parser.addHelpOption();

    m_parser.addOption(QCommandLineOption(QLatin1String(CommandLineOptions::Version),
        QLatin1String("Displays version information.")));

    m_parser.addOption(QCommandLineOption(QLatin1String(CommandLineOptions::FrameworkVersion),
        QLatin1String("Displays the version of the Qt Installer Framework.")));

    m_parser.addOption(QCommandLineOption(QStringList()
        << QLatin1String(CommandLineOptions::VerboseShort)
        << QLatin1String(CommandLineOptions::VerboseLong),
        QLatin1String("Verbose mode. Prints out more information.")));

    m_parser.addOption(QCommandLineOption(QLatin1String(CommandLineOptions::Proxy),
        QLatin1String("Use system proxy on Windows and Linux. This option has no effect on OS X.")));

    m_parser.addOption(QCommandLineOption(QLatin1String(CommandLineOptions::Script),
        QLatin1String("Execute the script given as argument."), QLatin1String("file")));

    m_parser.addOption(QCommandLineOption(QLatin1String(CommandLineOptions::CheckUpdates),
        QLatin1String("Check for updates and return an XML description.")));

    m_parser.addOption(QCommandLineOption(QLatin1String(CommandLineOptions::Updater),
        QLatin1String("Start application in updater mode.")));

    m_parser.addOption(QCommandLineOption(QLatin1String(CommandLineOptions::ManagePackages),
        QLatin1String("Start application in package manager mode.")));

    m_parser.addOption(QCommandLineOption(QLatin1String(CommandLineOptions::NoForceInstallation),
        QLatin1String("Allow deselecting components that are marked as forced.")));

    m_parser.addOption(QCommandLineOption(QLatin1String(CommandLineOptions::ShowVirtualComponents),
        QLatin1String("Show virtual components in installer and package manager.")));

    m_parser.addOption(QCommandLineOption(QLatin1String(CommandLineOptions::LoggingRules),
        QLatin1String("Enables logging according to passed rules. "
        "Comma separated logging rules have the following syntax: "
        "loggingCategory=true/false. "
        "Passing empty logging rules enables all logging categories. "
        "The following rules enable a single category: "
        "ifw.*=false,ifw.category=true "
        "The following logging categories are available:\n")
        + QInstaller::loggingCategories().join(QLatin1Char('\n')),
        QLatin1String("rules")));

    m_parser.addOption(QCommandLineOption(QLatin1String(CommandLineOptions::CreateLocalRepository),
        QLatin1String("Create a local repository inside the installation directory. This option "
        "has no effect on online installers.")));

    m_parser.addOption(QCommandLineOption(QLatin1String(CommandLineOptions::AddRepository),
        QLatin1String("Add a local or remote repository to the list of user defined repositories."),
        QLatin1String("URI,...")));

    m_parser.addOption(QCommandLineOption(QLatin1String(CommandLineOptions::AddTmpRepository),
        QLatin1String("Add a local or remote repository to the list of temporary available "
        "repositories."), QLatin1String("URI,...")));

    m_parser.addOption(QCommandLineOption(QLatin1String(CommandLineOptions::SetTmpRepository),
        QLatin1String("Set a local or remote repository as temporary repository, it is the only "
        "one used during fetch.\nNote: URI must be prefixed with the protocol, i.e. file:///, "
        "https://, http:// or ftp://."), QLatin1String("URI,...")));

    m_parser.addOption(QCommandLineOption(QLatin1String(CommandLineOptions::StartServer),
        QLatin1String("Starts the application as headless process waiting for commands to execute."
        " Mode can be DEBUG or PRODUCTION. In DEBUG mode, the option values can be omitted."
        "Note: The server will not shutdown on his own, you need to quit the process by hand."),
        QLatin1String("mode,socketname,key")));
    m_parser.addOption(QCommandLineOption(QLatin1String(CommandLineOptions::StartClient),
        QString::fromLatin1("Starts the application to debug the client-server communication. If "
        "a value is omitted, the client will use a default instead. Note: The server process is "
        "not started by the client application in that case, you need to start it on your own."),
        QLatin1String("socketname,key")));

    m_parser.addPositionalArgument(QLatin1String(CommandLineOptions::KeyValue),
        QLatin1String("Key Value pair to be set."));
}
