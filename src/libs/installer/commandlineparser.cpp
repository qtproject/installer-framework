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

#include "commandlineparser.h"

#include "commandlineparser_p.h"
#include "constants.h"
#include "globals.h"

namespace CommandLineOptions {

static const QLatin1String scCommand("Command");
static const QLatin1String scArguments("Args");
static const QLatin1String scInstallerValue("InstallerValue");

} // namespace CommandLineOptions

CommandLineParser::CommandLineParser()
    : d(new CommandLineParserPrivate(this))
{
    static const QLatin1String indent("  ");
    static const QString preformatted = QLatin1String("\nQt Installer Framework supports both GUI and "
        "headless mode. The installation operations can be invoked with the following commands and "
        "options. Note that the options marked with \"CLI\" are available in the headless mode only.\n")
        + QLatin1String("\nCommands:\n")
        + indent + QString::fromLatin1("%1, %2 - install default or selected packages and aliases - <pkg|alias ...>\n")
            .arg(CommandLineOptions::scInstallShort, CommandLineOptions::scInstallLong)
        + indent + QString::fromLatin1("%1, %2 - show available updates information on maintenance tool\n")
            .arg(CommandLineOptions::scCheckUpdatesShort, CommandLineOptions::scCheckUpdatesLong)
        + indent + QString::fromLatin1("%1, %2 - update all or selected packages - <pkg ...>\n")
            .arg(CommandLineOptions::scUpdateShort, CommandLineOptions::scUpdateLong)
        + indent + QString::fromLatin1("%1, %2 - uninstall packages and their child components - <pkg ...>\n")
            .arg(CommandLineOptions::scRemoveShort, CommandLineOptions::scRemoveLong)
        + indent + QString::fromLatin1("%1, %2 - list currently installed packages - <regexp for pkg>\n")
            .arg(CommandLineOptions::scListShort, CommandLineOptions::scListLong)
        + indent + QString::fromLatin1("%1, %2 - search available aliases or packages - <regexp for pkg|alias>\n")
            .arg(CommandLineOptions::scSearchShort, CommandLineOptions::scSearchLong)
        + indent + indent + QString::fromLatin1("Note: The --%1 option can be used to specify\n")
            .arg(CommandLineOptions::scFilterPackagesLong)
        + indent + indent + QLatin1String("additional filters for the search operation\n")
        + indent + indent + QString::fromLatin1("Note: The --%1 option can be used to specify\n")
            .arg(CommandLineOptions::scTypeLong)
        + indent + indent + QLatin1String("the content type to search\n")
        + indent + QString::fromLatin1("%1, %2 - create offline installer from selected packages - <pkg ...>\n")
            .arg(CommandLineOptions::scCreateOfflineShort, CommandLineOptions::scCreateOfflineLong)
        + indent + QString::fromLatin1("%1, %2 - clear contents of the local metadata cache\n")
            .arg(CommandLineOptions::scClearCacheShort, CommandLineOptions::scClearCacheLong)
        + indent + QString::fromLatin1("%1, %2 - uninstall all packages and remove entire program directory")
            .arg(CommandLineOptions::scPurgeShort, CommandLineOptions::scPurgeLong);

    m_parser.setApplicationDescription(preformatted);

    // Help & version information
    m_parser.addHelpOption();
    addOption(QCommandLineOption(QStringList()
        << CommandLineOptions::scVersionShort << CommandLineOptions::scVersionLong,
        QLatin1String("Displays version information.")));

    // Output related options
    addOption(QCommandLineOption(QStringList()
        << CommandLineOptions::scVerboseShort << CommandLineOptions::scVerboseLong,
        QString::fromLatin1("Verbose mode. Prints out more information. Adding -%1 or --%2 more "
                      "than once increases verbosity.").arg(CommandLineOptions::scVerboseShort,
                      CommandLineOptions::scVerboseLong)));
    addOption(QCommandLineOption(QStringList()
        << CommandLineOptions::scLoggingRulesShort << CommandLineOptions::scLoggingRulesLong,
        QLatin1String("Enables logging according to passed rules. Comma separated logging rules "
                      "have the following syntax: loggingCategory=true/false. Passing empty logging "
                      "rules enables all logging categories. The following rules enable a single "
                      "category: ifw.*=false, ifw.category=true. The following logging categories "
                      "are available:\n") + QInstaller::loggingCategories().join(QLatin1Char('\n')),
        QLatin1String("rules")));

    // Repository management options
    addOption(QCommandLineOption(QStringList()
        << CommandLineOptions::scAddRepositoryShort << CommandLineOptions::scAddRepositoryLong,
        QLatin1String("Add a local or remote repository to the list of user defined repositories."),
        QLatin1String("URI,...")));
    addOption(QCommandLineOption(QStringList()
        << CommandLineOptions::scAddTmpRepositoryShort << CommandLineOptions::scAddTmpRepositoryLong,
        QLatin1String("Add a local or remote repository to the list of temporary available repositories."),
        QLatin1String("URI,...")));
    addOption(QCommandLineOption(QStringList()
        << CommandLineOptions::scSetTmpRepositoryShort << CommandLineOptions::scSetTmpRepositoryLong,
        QLatin1String("Set a local or remote repository as temporary repository, it is the only "
                      "one used during fetch.\nNote: URI must be prefixed with the protocol, i.e. "
                      "file:///, https://, http:// or ftp://."),
        QLatin1String("URI,...")));

    // Proxy options
    addOption(QCommandLineOption(QStringList()
        << CommandLineOptions::scSystemProxyShort << CommandLineOptions::scSystemProxyLong,
        QLatin1String("Use system proxy on Windows and Linux. This option has no effect on macOS. (Default)")));
    addOption(QCommandLineOption(QStringList()
        << CommandLineOptions::scNoProxyShort << CommandLineOptions::scNoProxyLong,
        QLatin1String("Do not use system proxy.")));

    // Starting mode options
    addOption(QCommandLineOption(QStringList()
        << CommandLineOptions::scStartUpdaterShort << CommandLineOptions::scStartUpdaterLong,
        QLatin1String("Start application in updater mode. This will override the internal "
                      "marker that is used to distinguish which kind of binary is currently running.")));
    addOption(QCommandLineOption(QStringList()
        << CommandLineOptions::scStartPackageManagerShort << CommandLineOptions::scStartPackageManagerLong,
        QLatin1String("Start application in package manager mode. This will override the internal "
                      "marker that is used to distinguish which kind of binary is currently running.")));
    addOption(QCommandLineOption(QStringList()
        << CommandLineOptions::scStartUninstallerShort << CommandLineOptions::scStartUninstallerLong,
        QLatin1String("Start application in uninstaller mode. This will override the internal "
                      "marker that is used to distinguish which kind of binary is currently running.")));

    // Misc installation options
    addOptionWithContext(QCommandLineOption(QStringList()
        << CommandLineOptions::scRootShort << CommandLineOptions::scRootLong,
        QLatin1String("[CLI] Set installation root directory."),
        QLatin1String("directory")), CommandLineOnly);
    addOptionWithContext(QCommandLineOption(QStringList()
        << CommandLineOptions::scOfflineInstallerNameShort << CommandLineOptions::scOfflineInstallerNameLong,
        QLatin1String("[CLI] Set custom filename for the generated offline installer. Without this "
                      "the original filename is used with an added \"_offline-yyyy-MM-dd\" suffix."),
        QLatin1String("filename")), CommandLineOnly);
    addOption(QCommandLineOption(QStringList()
        << CommandLineOptions::scPlatformShort << CommandLineOptions::scPlatformLong,
        QLatin1String("Use the specified platform plugin."),
        QLatin1String("plugin")));
    addOption(QCommandLineOption(QStringList()
        << CommandLineOptions::scNoForceInstallationShort << CommandLineOptions::scNoForceInstallationLong,
        QLatin1String("Allow deselecting components that are marked as forced.")));
    addOption(QCommandLineOption(QStringList() << CommandLineOptions::scNoDefaultInstallationShort
        << CommandLineOptions::scNoDefaultInstallationLong,
        QLatin1String("Deselects components that are marked as default.")));
    addOption(QCommandLineOption(QStringList()
        << CommandLineOptions::scNoSizeCheckingShort << CommandLineOptions::scNoSizeCheckingLong,
        QLatin1String("Disable checking of free space for installation target.")));
    addOption(QCommandLineOption(QStringList()
        << CommandLineOptions::scShowVirtualComponentsShort << CommandLineOptions::scShowVirtualComponentsLong,
        QLatin1String("Show virtual components in installer and package manager.")));
    addOption(QCommandLineOption(QStringList()
        << CommandLineOptions::scInstallCompressedRepositoryShort
        << CommandLineOptions::scInstallCompressedRepositoryLong,
        QLatin1String("Installs QBSP or 7z file. The QBSP (Board Support Package) file must be a .7z "
                      "file which contains a valid repository."),
        QLatin1String("URI,...")));
    addOption(QCommandLineOption(QStringList()
        << CommandLineOptions::scCreateLocalRepositoryShort << CommandLineOptions::scCreateLocalRepositoryLong,
        QLatin1String("Create a local repository inside the installation directory. This option "
                      "has no effect on online installers.")));
    addOptionWithContext(QCommandLineOption(QStringList()
        << CommandLineOptions::scFilterPackagesShort << CommandLineOptions::scFilterPackagesLong,
        QLatin1String("[CLI] Comma separated list of additional key-value pair filters used to query packages with the "
                      "search command. The keys can be any of the possible package information elements, like "
                      "\"DisplayName\" and \"Description\"."),
        QLatin1String("element=regex,...")), CommandLineOnly);
    addOption(QCommandLineOption(QStringList()
        << CommandLineOptions::scLocalCachePathShort << CommandLineOptions::scLocalCachePathLong,
        QLatin1String("Sets the path used for local metadata cache. The path must be writable by the current user."),
        QLatin1String("path")));
    addOption(QCommandLineOption(QStringList()
        << CommandLineOptions::scTypeLong,
        QLatin1String("[CLI] Sets the type of the given arguments for commands supporting multiple argument types, "
                      "like \"search\". By default aliases are searched first, and if no matching aliases are found, "
                      "then packages are searched with the same search pattern."),
        QLatin1String("package|alias")));

    // Message query options
    addOptionWithContext(QCommandLineOption(QStringList() << CommandLineOptions::scAcceptMessageQueryShort
         << CommandLineOptions::scAcceptMessageQueryLong,
         QLatin1String("[CLI] Accepts all message queries without user input.")), CommandLineOnly);
    addOptionWithContext(QCommandLineOption(QStringList() << CommandLineOptions::scRejectMessageQueryShort
         << CommandLineOptions::scRejectMessageQueryLong,
         QLatin1String("[CLI] Rejects all message queries without user input.")), CommandLineOnly);
    addOptionWithContext(QCommandLineOption(QStringList() << CommandLineOptions::scMessageAutomaticAnswerShort
         << CommandLineOptions::scMessageAutomaticAnswerLong,
         QLatin1String("[CLI] Automatically answers the message queries with the message identifier and button value. "
                       "Several identifier=value pairs can be given separated with comma, "
                       "for example --auto-answer message.id=Ok,message.id2=Cancel."),
         QLatin1String("identifier=value")), CommandLineOnly);
    addOptionWithContext(QCommandLineOption(QStringList() << CommandLineOptions::scMessageDefaultAnswerShort
         << CommandLineOptions::scMessageDefaultAnswerLong,
         QLatin1String("[CLI] Automatically answers to message queries with their default values.")), CommandLineOnly);
    addOptionWithContext(QCommandLineOption(QStringList() << CommandLineOptions::scAcceptLicensesShort
         << CommandLineOptions::scAcceptLicensesLong,
         QLatin1String("[CLI] Accepts all licenses without user input.")), CommandLineOnly);
    addOptionWithContext(QCommandLineOption(QStringList() << CommandLineOptions::scFileDialogAutomaticAnswer,
         QLatin1String("[CLI] Automatically sets the QFileDialog values getExistingDirectory() or getOpenFileName() "
                       "requested by install script. "
                       "Several identifier=value pairs can be given separated with comma, "
                       "for example --file-query filedialog.id=C:/Temp,filedialog.id2=C:/Temp2"),
         QLatin1String("identifier=value")), CommandLineOnly);
    addOptionWithContext(QCommandLineOption(QStringList() << CommandLineOptions::scConfirmCommandShort
        << CommandLineOptions::scConfirmCommandLong, QLatin1String("[CLI] Confirms starting of "
        "installation, update or removal of components without user input.")), CommandLineOnly);

    // Developer options
    addOption(QCommandLineOption(QStringList()
         << CommandLineOptions::scScriptShort << CommandLineOptions::scScriptLong,
        QLatin1String("Execute the script given as argument."),
        QLatin1String("file")));
    addOption(QCommandLineOption(QStringList()
        << CommandLineOptions::scStartServerShort << CommandLineOptions::scStartServerLong,
        QLatin1String("Starts the application as headless process waiting for commands to execute. Mode "
                      "can be DEBUG or PRODUCTION. In DEBUG mode, the option values can be omitted. Note: "
                      "The server will not shutdown on his own, you need to quit the process by hand."),
        QLatin1String("mode, socketname, key")));
    addOption(QCommandLineOption(QStringList()
        << CommandLineOptions::scStartClientShort << CommandLineOptions::scStartClientLong,
        QLatin1String("Starts the application to debug the client-server communication. If a value is "
                      "omitted, the client will use a default instead. Note: The server process is not "
                      "started by the client application in that case, you need to start it on your own."),
        QLatin1String("socketname, key")));
    addOption(QCommandLineOption(QStringList()
        << CommandLineOptions::scSquishPortShort << CommandLineOptions::scSquishPortLong,
        QLatin1String("Give a port where Squish can connect to. If no port is given, attach to squish "
                      "not done. Note: To enable Squish support you first need to build IFW with SQUISH_PATH "
                      "parameter where SQUISH_PATH is pointing to your Squish installation folder: "
                      "<path_to_qt>/bin/qmake -r SQUISH_PATH=<pat_to_squish>"),
        QLatin1String("port number")));
    addOption(QCommandLineOption(QStringList()
        << CommandLineOptions::scMaxConcurrentOperationsShort << CommandLineOptions::scMaxConcurrentOperationsLong,
        QLatin1String("Specifies the maximum number of threads used to perform concurrent operations "
                      "in the unpacking phase of components. Set to a positive number, or 0 (default) "
                      "to let the application determine the ideal thread count from the amount of logical "
                      "processor cores in the system."),
        QLatin1String("threads")));

    QCommandLineOption cleanupUpdate(CommandLineOptions::scCleanupUpdate);
    cleanupUpdate.setValueName(QLatin1String("path"));
    cleanupUpdate.setFlags(QCommandLineOption::HiddenFromHelp);
    addOption(cleanupUpdate);

    QCommandLineOption cleanupUpdateOnly(CommandLineOptions::scCleanupUpdateOnly);
    cleanupUpdateOnly.setValueName(QLatin1String("path"));
    cleanupUpdateOnly.setFlags(QCommandLineOption::HiddenFromHelp);
    addOption(cleanupUpdateOnly);

    // Deprecated options
    QCommandLineOption deprecatedUpdater(CommandLineOptions::scDeprecatedUpdater);
    deprecatedUpdater.setFlags(QCommandLineOption::HiddenFromHelp);
    addOption(deprecatedUpdater);

    QCommandLineOption deprecatedCheckUpdates(CommandLineOptions::scDeprecatedCheckUpdates);
    deprecatedCheckUpdates.setFlags(QCommandLineOption::HiddenFromHelp);
    addOption(deprecatedCheckUpdates); // Behaves like check-updates but does not default to verbose output

    // Custom extension options
    d->addExtensionsOptions();

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

bool CommandLineParser::addOptionWithContext(const QCommandLineOption &option, CommandLineParser::OptionContextFlags flags)
{
     if (!m_parser.addOption(option))
         return false;

     for (auto &name : option.names())
         m_optionContextFlagsNameHash.insert(name, flags);

     return true;
}

CommandLineParser::OptionContextFlags CommandLineParser::optionContextFlags(const QString &option) const
{
    return m_optionContextFlagsNameHash.value(option);
}

/*
    Returns the command line arguments of the application. The returned list
    is context-aware, i.e. options that are set on the parser with
    \c OptionContextFlag::NoEchoValue are returned with their value hidden.
*/
QStringList CommandLineParser::arguments() const
{
    const QStringList arguments = QCoreApplication::arguments();
    QStringList returnArguments;
    bool skipNext = false;
    for (const QString &arg : arguments) {
        if (skipNext) {
            skipNext = false;
            continue;
        }
        returnArguments << arg;
        // Append positional arguments as-is
        if (!arg.startsWith(QLatin1String("--")) && !arg.startsWith(QLatin1Char('-')))
            continue;

        QString normalizedOption = arg;
        while (normalizedOption.startsWith(QLatin1Char('-')))
            normalizedOption.remove(QLatin1Char('-'));

        const OptionContextFlags flags = optionContextFlags(normalizedOption);
        if (!flags.testFlag(OptionContextFlag::NoEchoValue))
            continue;

        QString nextArg = arguments.value(arguments.indexOf(arg) + 1);
        if (!nextArg.isEmpty() && !nextArg.startsWith(QLatin1String("--"))
                && !nextArg.startsWith(QLatin1Char('-'))) {
            nextArg = QLatin1String("******");
            returnArguments << nextArg;
            skipNext = true;
        }
    }
    return returnArguments;
}
