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

#include "console.h"
#include "constants.h"
#include "commandlineparser.h"
#include "installerbase.h"
#include "sdkapp.h"
#include "commandlineinterface.h"

#include <errors.h>
#include <selfrestarter.h>
#include <remoteserver.h>
#include <utils.h>

#include <QCommandLineParser>
#include <QDateTime>
#include <QNetworkProxyFactory>

#include <iostream>

#if defined(Q_OS_MACOS) or defined(Q_OS_UNIX)
#  include <unistd.h>
#  include <sys/types.h>
#endif

#define QUOTE_(x) #x
#define QUOTE(x) QUOTE_(x)
#define VERSION "IFW Version: " QUOTE(IFW_VERSION_STR) ", built with Qt " QT_VERSION_STR "."
#define BUILDDATE "Build date: " __DATE__
#define SHA "Installer Framework SHA1: " QUOTE(_GIT_SHA1_)
static const char PLACEHOLDER[32] = "MY_InstallerCreateDateTime_MY";

int main(int argc, char *argv[])
{
#if defined(Q_OS_WIN)
    if (!qEnvironmentVariableIsSet("QT_AUTO_SCREEN_SCALE_FACTOR")
            && !qEnvironmentVariableIsSet("QT_SCALE_FACTOR")
            && !qEnvironmentVariableIsSet("QT_SCREEN_SCALE_FACTORS")) {
        QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    }
#endif
    // increase maximum numbers of file descriptors
#if defined (Q_OS_MACOS)
    QCoreApplication::setSetuidAllowed(true);
    struct rlimit rl;
    getrlimit(RLIMIT_NOFILE, &rl);
    rl.rlim_cur = qMin((rlim_t) OPEN_MAX, rl.rlim_max);
    setrlimit(RLIMIT_NOFILE, &rl);
#endif

    qsrand(QDateTime::currentDateTime().toTime_t());

    // We need to start either a command line application or a GUI application. Since we
    // fail doing so at least on Linux while parsing the argument using a core application
    // object and later starting the GUI application, we now parse the arguments first.
    CommandLineParser parser;
    parser.parse(QInstaller::parseCommandLineArgs(argc, argv));

    bool sanityCheck = true;
    QString sanityMessage;

    QStringList mutually;
    if (parser.isSet(CommandLineOptions::scStartUpdaterLong))
        mutually << CommandLineOptions::scStartUpdaterLong;
    if (parser.isSet(CommandLineOptions::scStartPackageManagerLong))
        mutually << CommandLineOptions::scStartPackageManagerLong;
    if (parser.isSet(CommandLineOptions::scStartUninstallerLong))
        mutually << CommandLineOptions::scStartUninstallerLong;

    if (mutually.count() > 1) {
        sanityMessage = QString::fromLatin1("The following options are mutually exclusive: %1.")
            .arg(mutually.join(QLatin1String(", ")));
        sanityCheck = false;
    }
    const QSet<QString> commands = parser.positionalArguments().toSet()
        .intersect(CommandLineOptions::scCommandLineInterfaceOptions.toSet());
    // Check sanity of the given argument sequence.
    if (commands.size() > 1) {
        sanityMessage = QString::fromLatin1("%1 commands provided, only one can be used at a time.")
            .arg(commands.size());
        sanityCheck = false;
    } else if (!commands.isEmpty() && parser.positionalArguments().indexOf(commands.values().first()) != 0) {
        sanityMessage = QString::fromLatin1("Found command but it is not given as the first positional "
            "argument. \"%1\" given.").arg(parser.positionalArguments().first());
        sanityCheck = false;
    }
    const bool help = parser.isSet(CommandLineOptions::scHelpLong);
    if (help || parser.isSet(CommandLineOptions::scVersionLong) || !sanityCheck) {
        Console c;
        QCoreApplication app(argc, argv);

        if (parser.isSet(CommandLineOptions::scVersionLong)) {
            std::cout << VERSION << std::endl << BUILDDATE << std::endl << SHA << std::endl;
            const QDateTime dateTime = QDateTime::fromString(QLatin1String(PLACEHOLDER),
                QLatin1String("yyyy-MM-dd - HH:mm:ss"));
            if (dateTime.isValid())
                std::cout << "Installer creation time: " << PLACEHOLDER << std::endl;
            return EXIT_SUCCESS;
        }

        std::cout << qPrintable(parser.helpText()) << std::endl;
        if (!sanityCheck) {
            std::cerr << qPrintable(sanityMessage) << std::endl;
        }
        return help ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    if (parser.isSet(CommandLineOptions::scStartServerLong)) {
        const QStringList arguments = parser.value(CommandLineOptions::scStartServerLong)
            .split(QLatin1Char(','), QString::SkipEmptyParts);

        QString socketName, key;
        const QString mode = arguments.value(0);
        bool argumentsValid = (mode.compare(QLatin1String(QInstaller::Protocol::ModeDebug),
            Qt::CaseInsensitive) == 0);
        if (argumentsValid) {
            socketName = arguments.value(1, QLatin1String(QInstaller::Protocol::DefaultSocket));
            key  = arguments.value(2, QLatin1String(QInstaller::Protocol::DefaultAuthorizationKey));
        } else  {
            socketName = arguments.value(1);
            key =  arguments.value(2);
        }

        const bool production = (mode.compare(QLatin1String(QInstaller::Protocol::ModeProduction),
            Qt::CaseInsensitive) == 0);
        if (production) {
            argumentsValid = (!key.isEmpty()) && (!socketName.isEmpty());
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
            /* In production mode detach child so that sudo waiting on us will terminate. */
            pid_t child = fork();
            if (child <= -1) {
                std::cerr << "Fatal cannot fork and detach server." << std::endl;
                return EXIT_FAILURE;
            }

            if (child != 0) {
                return EXIT_SUCCESS;
            }

            ::setsid();
#endif
        }


        SDKApp<QCoreApplication> app(argc, argv);
        if (!argumentsValid) {
            Console c;
            std::cout << qPrintable(parser.helpText()) << std::endl;
            QString startServerStr = CommandLineOptions::scStartServerLong;
            std::cerr << "Wrong argument(s) for option --" << startServerStr.toStdString() << std::endl;
            return EXIT_FAILURE;
        }

#if defined(Q_OS_MACOS)
        // make sure effective == real user id.
        uid_t realUserId = getuid();
        uid_t effectiveUserId = geteuid();
        if (realUserId != effectiveUserId)
            setreuid(effectiveUserId, -1);
#endif

        QInstaller::RemoteServer *server = new QInstaller::RemoteServer;
        QObject::connect(server, &QInstaller::RemoteServer::destroyed, &app, &decltype(app)::quit);
        server->init(socketName, key, (production ? QInstaller::Protocol::Mode::Production
            : QInstaller::Protocol::Mode::Debug));

        server->start();
        return app.exec();
    }

    try {
        QScopedPointer<Console> console;

        // Check if any options requiring verbose output is set
        bool setVerbose = parser.isSet(CommandLineOptions::scVerboseLong);

        foreach (const QString &option, CommandLineOptions::scCommandLineInterfaceOptions) {
            if (setVerbose) break;
            setVerbose = parser.positionalArguments().contains(option);
        }
        if (setVerbose) {
            console.reset(new Console);
            QInstaller::setVerbose(true);
        }

        // On Windows we need the console window from above, we are a GUI application.
        const QStringList unknownOptionNames = parser.unknownOptionNames();
        if (!unknownOptionNames.isEmpty()) {
            const QString options = unknownOptionNames.join(QLatin1String(", "));
            std::cerr << "Unknown option: " << qPrintable(options) << std::endl;
        }

        if (parser.isSet(CommandLineOptions::scSystemProxyLong)) {
            // Make sure we honor the system's proxy settings
            QNetworkProxyFactory::setUseSystemConfiguration(true);
        }

        if (parser.isSet(CommandLineOptions::scNoProxyLong))
            QNetworkProxyFactory::setUseSystemConfiguration(false);

        const SelfRestarter restarter(argc, argv);

        if (parser.positionalArguments().contains(CommandLineOptions::scCheckUpdatesShort)
                || parser.positionalArguments().contains(CommandLineOptions::scCheckUpdatesLong)) {
            return CommandLineInterface(argc, argv).checkUpdates();
        } else if (parser.positionalArguments().contains(CommandLineOptions::scListShort)
                || parser.positionalArguments().contains(CommandLineOptions::scListLong)) {
            return CommandLineInterface(argc, argv).listInstalledPackages();
        } else if (parser.positionalArguments().contains(CommandLineOptions::scSearchShort)
                || parser.positionalArguments().contains(CommandLineOptions::scSearchLong)) {
            return CommandLineInterface(argc, argv).searchAvailablePackages();
        } else if (parser.positionalArguments().contains(CommandLineOptions::scUpdateShort)
                || parser.positionalArguments().contains(CommandLineOptions::scUpdateLong)) {
            return CommandLineInterface(argc, argv).updatePackages();
        } else if (parser.positionalArguments().contains(CommandLineOptions::scInstallShort)
                || parser.positionalArguments().contains(CommandLineOptions::scInstallLong)) {
            return CommandLineInterface(argc, argv).installPackages();
        } else if (parser.positionalArguments().contains(CommandLineOptions::scRemoveShort)
                || parser.positionalArguments().contains(CommandLineOptions::scRemoveLong)){
            return CommandLineInterface(argc, argv).uninstallPackages();
        } else if (parser.positionalArguments().contains(CommandLineOptions::scPurgeShort)
                || parser.positionalArguments().contains(CommandLineOptions::scPurgeLong)){
            return CommandLineInterface(argc, argv).removeInstallation();
        }
        if (QInstaller::isVerbose())
            std::cout << VERSION << std::endl << BUILDDATE << std::endl << SHA << std::endl;

        return InstallerBase(argc, argv).run();

    } catch (const QInstaller::Error &e) {
        std::cerr << qPrintable(e.message()) << std::endl;
    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
    } catch (...) {
        std::cerr << "Unknown exception caught." << std::endl;
    }

    return EXIT_FAILURE;
}
