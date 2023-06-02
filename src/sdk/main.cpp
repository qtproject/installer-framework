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

#include "constants.h"
#include "commandlineparser.h"
#include "installerbase.h"
#include "sdkapp.h"
#include "commandlineinterface.h"

#include <errors.h>
#include <selfrestarter.h>
#include <remoteserver.h>
#include <utils.h>
#include <loggingutils.h>

#ifdef IFW_LIB7Z
#include <7zVersion.h>
#endif
#ifdef IFW_LIBARCHIVE
#include <archive.h>
#endif

#include <QCommandLineParser>
#include <QDateTime>
#include <QNetworkProxyFactory>
#include <QThread>
#include <QThreadPool>
#include <QDeadlineTimer>

#include <iostream>

#if defined(Q_OS_MACOS) || defined(Q_OS_UNIX)
#  include <unistd.h>
#  include <sys/types.h>
#endif

#define QUOTE_(x) #x
#define QUOTE(x) QUOTE_(x)
#define VERSION "IFW Version: " QUOTE(IFW_VERSION_STR) ", built with Qt " QT_VERSION_STR "."
#define BUILDDATE "Build date: " __DATE__
#define SHA "Installer Framework SHA1: " QUOTE(_GIT_SHA1_)
static const char PLACEHOLDER[32] = "MY_InstallerCreateDateTime_MY";

#ifdef Q_OS_WIN
static void cleanupUpdate(const CommandLineParser &parser, bool *exit)
{
    QString cleanupPath;
    QString cleanupOption;
    *exit = false;

    if (parser.isSet(CommandLineOptions::scCleanupUpdate)) {
        cleanupPath = parser.value(CommandLineOptions::scCleanupUpdate);
        cleanupOption = CommandLineOptions::scCleanupUpdate;
    } else if (parser.isSet(CommandLineOptions::scCleanupUpdateOnly)) {
        cleanupPath = parser.value(CommandLineOptions::scCleanupUpdateOnly);
        cleanupOption = CommandLineOptions::scCleanupUpdateOnly;
        *exit = true;
    }

    if (cleanupOption.isEmpty())
        return;

    // Since Windows does not support that the maintenance tool deletes itself we
    // remove the old executable here after update (as the new maintenance tool).
    if (!cleanupPath.isEmpty()) {
        QFile fileToRemove(cleanupPath);
        // Give up after 120 seconds if the old process has not exited and released the file
        QDeadlineTimer deadline(120000);
        while (fileToRemove.exists() && !deadline.hasExpired()) {
            if (fileToRemove.remove()) {
                std::cout << "Removed leftover file: " << qPrintable(cleanupPath)
                          << " after update." << std::endl;
            }
            QThread::msleep(1000);
        }
    } else {
        std::cout << "Invalid value for option " << qPrintable(cleanupOption);
    }
}
#endif

int main(int argc, char *argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    if (!qEnvironmentVariableIsSet("QT_AUTO_SCREEN_SCALE_FACTOR")
            && !qEnvironmentVariableIsSet("QT_SCALE_FACTOR")
            && !qEnvironmentVariableIsSet("QT_SCREEN_SCALE_FACTORS")) {
        QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    }
#if defined(Q_OS_WIN)
    QCoreApplication::setAttribute(Qt::AA_DisableWindowContextHelpButton);
#endif
#endif
    // increase maximum numbers of file descriptors
#if defined(Q_OS_MACOS)
    QCoreApplication::setSetuidAllowed(true);
    struct rlimit rl;
    getrlimit(RLIMIT_NOFILE, &rl);
    rl.rlim_cur = qMin((rlim_t) OPEN_MAX, rl.rlim_max);
    setrlimit(RLIMIT_NOFILE, &rl);
#endif

    // Try to avoid running into situations where the application would hang due to "nested" blocking
    // usage of the global threadpool that has only one thread available, i.e. main thread invokes
    // QtConcurrent::run(&myFunction) and  myFunction() calls QtConcurrent::blockingFiltered()
    // for a container.
    if (QThread::idealThreadCount() == 1)
        QThreadPool::globalInstance()->setMaxThreadCount(2);

    // We need to start either a command line application or a GUI application. Since we
    // fail doing so at least on Linux while parsing the argument using a core application
    // object and later starting the GUI application, we now parse the arguments first.
    CommandLineParser parser;
    parser.parse(QInstaller::parseCommandLineArgs(argc, argv));

    bool sanityCheck = true;
    QString sanityMessage;

    QStringList mutually;
    mutually = QInstaller::checkMutualOptions(parser, QStringList()
        << CommandLineOptions::scStartUpdaterLong
        << CommandLineOptions::scStartPackageManagerLong
        << CommandLineOptions::scStartUninstallerLong
        << CommandLineOptions::scDeprecatedUpdater);

    if (mutually.isEmpty()) {
        mutually = QInstaller::checkMutualOptions(parser, QStringList()
            << CommandLineOptions::scSystemProxyLong
            << CommandLineOptions::scNoProxyLong);
    }
    if (mutually.isEmpty()) {
        mutually = QInstaller::checkMutualOptions(parser, QStringList()
            << CommandLineOptions::scAcceptMessageQueryLong
            << CommandLineOptions::scRejectMessageQueryLong
            << CommandLineOptions::scMessageDefaultAnswerLong);
    }
    if (mutually.isEmpty()) {
        mutually = QInstaller::checkMutualOptions(parser, QStringList()
            << CommandLineOptions::scStartServerLong
            << CommandLineOptions::scStartClientLong);
    }
    if (!mutually.isEmpty()) {
        sanityMessage = QString::fromLatin1("The following options are mutually exclusive: %1.")
            .arg(mutually.join(QLatin1String(", ")));
        sanityCheck = false;
    }
    const QStringList positionalArgs = parser.positionalArguments();
    QSet<QString> commands(positionalArgs.begin(), positionalArgs.end());
    commands.intersect(QSet<QString>(CommandLineOptions::scCommandLineInterfaceOptions.begin(),
                                     CommandLineOptions::scCommandLineInterfaceOptions.end()));
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
        QCoreApplication app(argc, argv);

        if (parser.isSet(CommandLineOptions::scVersionLong)) {
            std::cout << VERSION << std::endl << BUILDDATE << std::endl << SHA << std::endl;
#ifdef IFW_LIB7Z
            std::cout << "LZMA SDK version: " << MY_VERSION << std::endl;
#endif
#ifdef IFW_LIBARCHIVE
            std::cout << "Libarchive version: " << archive_version_details() << std::endl;
#endif
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

#ifdef Q_OS_WIN
    {
        bool exit = false;
        cleanupUpdate(parser, &exit);
        if (exit)
            return EXIT_SUCCESS;
    }
#endif

    if (parser.isSet(CommandLineOptions::scStartServerLong)) {
        const QStringList arguments = parser.value(CommandLineOptions::scStartServerLong)
            .split(QLatin1Char(','), Qt::SkipEmptyParts);

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
        QStringList optionNames = parser.optionNames();

        //Verbose level can be increased by setting the verbose multiple times
        foreach (QString value, optionNames) {
            if (value == CommandLineOptions::scVerboseShort
                    || value == CommandLineOptions::scVerboseLong) {
                QInstaller::LoggingHandler::instance().setVerbose(true);
            }
        }

        foreach (const QString &option, CommandLineOptions::scCommandLineInterfaceOptions) {
            bool setVerbose = parser.positionalArguments().contains(option);
            if (setVerbose) {
                QInstaller::LoggingHandler::instance().setVerbose(setVerbose);
                break;
            }
        }

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
                || parser.positionalArguments().contains(CommandLineOptions::scCheckUpdatesLong)
                || parser.isSet(CommandLineOptions::scDeprecatedCheckUpdates)) {
            // Also check for deprecated --checkupdates option, which is superseded by check-updates
            // command in IFW 4.x.x. Should not be used for normal interactive usage.
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
        } else if (parser.positionalArguments().contains(CommandLineOptions::scCreateOfflineShort)
                || parser.positionalArguments().contains(CommandLineOptions::scCreateOfflineLong)) {
            return CommandLineInterface(argc, argv).createOfflineInstaller();
        } else if (parser.positionalArguments().contains(CommandLineOptions::scClearCacheShort)
                || parser.positionalArguments().contains(CommandLineOptions::scClearCacheLong)) {
            return CommandLineInterface(argc, argv).clearLocalCache();
        }
        if (QInstaller::LoggingHandler::instance().isVerbose()) {
            std::cout << VERSION << std::endl << BUILDDATE << std::endl << SHA << std::endl;
        } else {
#ifdef Q_OS_WIN
            // Check if installer is started from console. If so, restart the installer so it
            // won't reserve the console handles.
            DWORD procIDs[2];
            DWORD maxCount = 2;
            DWORD result = GetConsoleProcessList((LPDWORD)procIDs, maxCount);
            FreeConsole(); // Closes console in GUI version
            if (result > 1) {
                restarter.setRestartOnQuit(true);
                return EXIT_FAILURE;
            }
#endif
        }
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
