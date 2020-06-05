/**************************************************************************
**
** Copyright (C) 2020 The Qt Company Ltd.
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

static const QLatin1String scCommand("Command");
static const QLatin1String scArguments("Args");
static const QLatin1String scInstallerValue("InstallerValue");

} // namespace CommandLineOptions

CommandLineParser::CommandLineParser()
    : d(new CommandLineParserPrivate())
{
    static const QString preformatted = QLatin1String("\nQt Installer Framework supports both GUI and "
        "headless mode. The installation operations can be invoked with the following commands and "
        "options. Note that the options marked with \"CLI\" are available in the headless mode only.\n")
        + QLatin1String("\nCommands:\n")
        + QString::fromLatin1("\t%1, %2 - install default or selected packages - <pkg1 pkg2 pkg3...>\n")
            .arg(CommandLineOptions::scInstallShort, CommandLineOptions::scInstallLong)
        + QString::fromLatin1("\t%1, %2 - show available updates information on maintenance tool\n")
            .arg(CommandLineOptions::scCheckUpdatesShort, CommandLineOptions::scCheckUpdatesLong)
        + QString::fromLatin1("\t%1, %2 - update all or selected packages - <pkg1 pkg2 pkg3...>\n")
            .arg(CommandLineOptions::scUpdateShort, CommandLineOptions::scUpdateLong)
        + QString::fromLatin1("\t%1, %2 - uninstall packages and their child components - <pkg1 pkg2 pkg3...>\n")
            .arg(CommandLineOptions::scRemoveShort, CommandLineOptions::scRemoveLong)
        + QString::fromLatin1("\t%1, %2 - list currently installed packages\n")
            .arg(CommandLineOptions::scListShort, CommandLineOptions::scListLong)
        + QString::fromLatin1("\t%1, %2 - search available packages - <regexp>\n")
            .arg(CommandLineOptions::scSearchShort, CommandLineOptions::scSearchLong)
        + QString::fromLatin1("\t%1, %2 - uninstall all packages and remove entire program directory")
            .arg(CommandLineOptions::scPurgeShort, CommandLineOptions::scPurgeLong);

    m_parser.setApplicationDescription(preformatted);

    // Help & version information
    m_parser.addHelpOption();
    m_parser.addOption(QCommandLineOption(QStringList()
        << CommandLineOptions::scVersionShort << CommandLineOptions::scVersionLong,
        QLatin1String("Displays version information.")));

    // Output related options
    m_parser.addOption(QCommandLineOption(QStringList()
        << CommandLineOptions::scVerboseShort << CommandLineOptions::scVerboseLong,
        QLatin1String("Verbose mode. Prints out more information.")));
    m_parser.addOption(QCommandLineOption(QStringList()
        << CommandLineOptions::scLoggingRulesShort << CommandLineOptions::scLoggingRulesLong,
        QLatin1String("Enables logging according to passed rules. Comma separated logging rules "
                      "have the following syntax: loggingCategory=true/false. Passing empty logging "
                      "rules enables all logging categories. The following rules enable a single "
                      "category: ifw.*=false, ifw.category=true. The following logging categories "
                      "are available:\n") + QInstaller::loggingCategories().join(QLatin1Char('\n')),
        QLatin1String("rules")));

    // Repository management options
    m_parser.addOption(QCommandLineOption(QStringList()
        << CommandLineOptions::scAddRepositoryShort << CommandLineOptions::scAddRepositoryLong,
        QLatin1String("Add a local or remote repository to the list of user defined repositories."),
        QLatin1String("URI,...")));
    m_parser.addOption(QCommandLineOption(QStringList()
        << CommandLineOptions::scAddTmpRepositoryShort << CommandLineOptions::scAddTmpRepositoryLong,
        QLatin1String("Add a local or remote repository to the list of temporary available repositories."),
        QLatin1String("URI,...")));
    m_parser.addOption(QCommandLineOption(QStringList()
        << CommandLineOptions::scSetTmpRepositoryShort << CommandLineOptions::scSetTmpRepositoryLong,
        QLatin1String("Set a local or remote repository as temporary repository, it is the only "
                      "one used during fetch.\nNote: URI must be prefixed with the protocol, i.e. "
                      "file:///, https://, http:// or ftp://."),
        QLatin1String("URI,...")));

    // Proxy options
    m_parser.addOption(QCommandLineOption(QStringList()
        << CommandLineOptions::scSystemProxyShort << CommandLineOptions::scSystemProxyLong,
        QLatin1String("Use system proxy on Windows and Linux. This option has no effect on macOS. (Default)")));
    m_parser.addOption(QCommandLineOption(QStringList()
        << CommandLineOptions::scNoProxyShort << CommandLineOptions::scNoProxyLong,
        QLatin1String("Do not use system proxy.")));

    // Starting mode options
    m_parser.addOption(QCommandLineOption(QStringList()
        << CommandLineOptions::scStartUpdaterShort << CommandLineOptions::scStartUpdaterLong,
        QLatin1String("Start application in updater mode. This will override the internal "
                      "marker that is used to distinguish which kind of binary is currently running.")));
    m_parser.addOption(QCommandLineOption(QStringList()
        << CommandLineOptions::scStartPackageManagerShort << CommandLineOptions::scStartPackageManagerLong,
        QLatin1String("Start application in package manager mode. This will override the internal "
                      "marker that is used to distinguish which kind of binary is currently running.")));
    m_parser.addOption(QCommandLineOption(QStringList()
        << CommandLineOptions::scStartUninstallerShort << CommandLineOptions::scStartUninstallerLong,
        QLatin1String("Start application in uninstaller mode. This will override the internal "
                      "marker that is used to distinguish which kind of binary is currently running.")));

    // Misc installation options
    m_parser.addOption(QCommandLineOption(QStringList()
        << CommandLineOptions::scRootShort << CommandLineOptions::scRootLong,
        QLatin1String("[CLI] Set installation root directory."),
        QLatin1String("directory")));
    m_parser.addOption(QCommandLineOption(QStringList()
        << CommandLineOptions::scPlatformShort << CommandLineOptions::scPlatformLong,
        QLatin1String("Use the specified platform plugin."),
        QLatin1String("plugin")));
    m_parser.addOption(QCommandLineOption(QStringList()
        << CommandLineOptions::scNoForceInstallationShort << CommandLineOptions::scNoForceInstallationLong,
        QLatin1String("Allow deselecting components that are marked as forced.")));
    m_parser.addOption(QCommandLineOption(QStringList()
        << CommandLineOptions::scNoDefaultInstallationLong,
        QLatin1String("Deselects components that are marked as default.")));
    m_parser.addOption(QCommandLineOption(QStringList()
        << CommandLineOptions::scNoSizeCheckingShort << CommandLineOptions::scNoSizeCheckingLong,
        QLatin1String("Disable checking of free space for installation target.")));
    m_parser.addOption(QCommandLineOption(QStringList()
        << CommandLineOptions::scShowVirtualComponentsShort << CommandLineOptions::scShowVirtualComponentsLong,
        QLatin1String("Show virtual components in installer and package manager.")));
    m_parser.addOption(QCommandLineOption(QStringList()
        << CommandLineOptions::scInstallCompressedRepositoryShort
        << CommandLineOptions::scInstallCompressedRepositoryLong,
        QLatin1String("Installs QBSP or 7z file. The QBSP (Board Support Package) file must be a .7z "
                      "file which contains a valid repository."),
        QLatin1String("URI,...")));
    m_parser.addOption(QCommandLineOption(QStringList()
        << CommandLineOptions::scCreateLocalRepositoryShort << CommandLineOptions::scCreateLocalRepositoryLong,
        QLatin1String("Create a local repository inside the installation directory. This option "
                      "has no effect on online installers.")));

    // Message query options
    m_parser.addOption(QCommandLineOption(QStringList() << CommandLineOptions::scAcceptMessageQuery,
         QLatin1String("[CLI] Accepts all message queries without user input.")));
    m_parser.addOption(QCommandLineOption(QStringList() << CommandLineOptions::scRejectMessageQuery,
         QLatin1String("[CLI] Rejects all message queries without user input.")));
    m_parser.addOption(QCommandLineOption(QStringList() << CommandLineOptions::scMessageAutomaticAnswer,
         QLatin1String("[CLI] Automatically answers the message queries with the message identifier and button value. "
                       "Several identifier=value pairs can be given separated with comma, "
                       "for example --auto-answer message.id=Ok,message.id2=Cancel."),
         QLatin1String("identifier=value")));
     m_parser.addOption(QCommandLineOption(QStringList() << CommandLineOptions::scMessageDefaultAnswer,
        QLatin1String("[CLI] Automatically answers to message queries with their default values.")));
    m_parser.addOption(QCommandLineOption(QStringList() << CommandLineOptions::scAcceptLicenses,
         QLatin1String("[CLI] Accepts all licenses without user input.")));
    m_parser.addOption(QCommandLineOption(QStringList() << CommandLineOptions::scFileDialogAutomaticAnswer,
         QLatin1String("[CLI] Automatically sets the QFileDialog values getExistingDirectory() or getOpenFileName() "
                       "requested by install script. "
                       "Several identifier=value pairs can be given separated with comma, "
                       "for example --file-query filedialog.id=C:\Temp,filedialog.id2=C:\Temp2"),
         QLatin1String("identifier=value")));

    // Developer options
    m_parser.addOption(QCommandLineOption(QStringList()
         << CommandLineOptions::scScriptShort << CommandLineOptions::scScriptLong,
        QLatin1String("Execute the script given as argument."),
        QLatin1String("file")));
    m_parser.addOption(QCommandLineOption(QStringList()
        << CommandLineOptions::scStartServerShort << CommandLineOptions::scStartServerLong,
        QLatin1String("Starts the application as headless process waiting for commands to execute. Mode "
                      "can be DEBUG or PRODUCTION. In DEBUG mode, the option values can be omitted. Note: "
                      "The server will not shutdown on his own, you need to quit the process by hand."),
        QLatin1String("mode, socketname, key")));
    m_parser.addOption(QCommandLineOption(QStringList()
        << CommandLineOptions::scStartClientShort << CommandLineOptions::scStartClientLong,
        QLatin1String("Starts the application to debug the client-server communication. If a value is "
                      "omitted, the client will use a default instead. Note: The server process is not "
                      "started by the client application in that case, you need to start it on your own."),
        QLatin1String("socketname, key")));
    m_parser.addOption(QCommandLineOption(QStringList()
        << CommandLineOptions::scSquishPortShort << CommandLineOptions::scSquishPortLong,
        QLatin1String("Give a port where Squish can connect to. If no port is given, default port 11233 is "
                      "used. Note: To enable Squish support you first need to build IFW with SQUISH_PATH "
                      "parameter where SQUISH_PATH is pointing to your Squish installation folder: "
                      "<path_to_qt>/bin/qmake -r SQUISH_PATH=<pat_to_squish>"),
        QLatin1String("port number")));

    // Deprecated options
    QCommandLineOption deprecatedUpdater(CommandLineOptions::scDeprecatedUpdater);
    deprecatedUpdater.setHidden(true);
    m_parser.addOption(deprecatedUpdater);

    QCommandLineOption deprecatedCheckUpdates(CommandLineOptions::scDeprecatedCheckUpdates);
    deprecatedCheckUpdates.setHidden(true);
    m_parser.addOption(deprecatedCheckUpdates); // Behaves like check-updates but does not default to verbose output

    // Custom extension options
    m_parser.addOptions(d->extensionsOptions());

    // Positional arguments
    m_parser.addPositionalArgument(CommandLineOptions::scCommand,
        QLatin1String("Command to be run by installer. Note that this feature may be disabled by vendor."),
        QLatin1String("command"));
    m_parser.addPositionalArgument(CommandLineOptions::scArguments,
        QLatin1String("Extra arguments for command, each separated by space."),
        QLatin1String("<args>"));
    m_parser.addPositionalArgument(CommandLineOptions::scInstallerValue,
        QLatin1String("Key-value pair to be set internally by the framework."),
        QLatin1String("<key=value>"));
}

CommandLineParser::~CommandLineParser()
{
    delete d;
}
