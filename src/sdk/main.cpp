/**************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
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
#include "updatechecker.h"

#include <errors.h>
#include <selfrestarter.h>
#include <remoteserver.h>
#include <utils.h>

#include <QCommandLineParser>
#include <QDateTime>
#include <QNetworkProxyFactory>

#include <iostream>

#if defined(Q_OS_OSX) or defined(Q_OS_UNIX)
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
#if defined (Q_OS_OSX)
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

    QStringList mutually;
    if (parser.isSet(QLatin1String(CommandLineOptions::CheckUpdates)))
        mutually << QLatin1String(CommandLineOptions::CheckUpdates);
    if (parser.isSet(QLatin1String(CommandLineOptions::Updater)))
        mutually << QLatin1String(CommandLineOptions::Updater);
    if (parser.isSet(QLatin1String(CommandLineOptions::ManagePackages)))
        mutually << QLatin1String(CommandLineOptions::ManagePackages);

    const bool help = parser.isSet(QLatin1String(CommandLineOptions::HelpShort))
        || parser.isSet(QLatin1String(CommandLineOptions::HelpLong));
    if (help
            || parser.isSet(QLatin1String(CommandLineOptions::Version))
            || parser.isSet(QLatin1String(CommandLineOptions::FrameworkVersion))
            || mutually.count() > 1) {
        Console c;
        QCoreApplication app(argc, argv);

        if (parser.isSet(QLatin1String(CommandLineOptions::Version))) {
            std::cout << VERSION << std::endl << BUILDDATE << std::endl << SHA << std::endl;
            const QDateTime dateTime = QDateTime::fromString(QLatin1String(PLACEHOLDER),
                QLatin1String("yyyy-MM-dd - HH:mm:ss"));
            if (dateTime.isValid())
                std::cout << "Installer creation time: " << PLACEHOLDER << std::endl;
            return EXIT_SUCCESS;
        }

        if (parser.isSet(QLatin1String(CommandLineOptions::FrameworkVersion))) {
            std::cout << QUOTE(IFW_VERSION_STR) << std::endl;
            return EXIT_SUCCESS;
        }

        std::cout << qPrintable(parser.helpText()) << std::endl;
        if (mutually.count() > 1) {
            std::cerr << qPrintable(QString::fromLatin1("The following options are mutually "
                "exclusive: %1.").arg(mutually.join(QLatin1String(", ")))) << std::endl;
        }
        return help ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    if (parser.isSet(QLatin1String(CommandLineOptions::StartServer))) {
        const QStringList arguments = parser.value(QLatin1String(CommandLineOptions::StartServer))
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
#if defined(Q_OS_UNIX) && !defined(Q_OS_OSX)
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
            std::cerr << "Wrong argument(s) for option --startserver." << std::endl;
            return EXIT_FAILURE;
        }

#if defined(Q_OS_OSX)
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
        if (parser.isSet(QLatin1String(CommandLineOptions::VerboseShort))
            || parser.isSet(QLatin1String(CommandLineOptions::VerboseLong))) {
                console.reset(new Console);
                QInstaller::setVerbose(true);
        }

        // On Windows we need the console window from above, we are a GUI application.
        const QStringList unknownOptionNames = parser.unknownOptionNames();
        if (!unknownOptionNames.isEmpty()) {
            const QString options = unknownOptionNames.join(QLatin1String(", "));
            std::cerr << "Unknown option: " << qPrintable(options) << std::endl;
        }

        if (parser.isSet(QLatin1String(CommandLineOptions::Proxy))) {
            // Make sure we honor the system's proxy settings
            QNetworkProxyFactory::setUseSystemConfiguration(true);
        }

        if (parser.isSet(QLatin1String(CommandLineOptions::NoProxy)))
            QNetworkProxyFactory::setUseSystemConfiguration(false);

        if (parser.isSet(QLatin1String(CommandLineOptions::CheckUpdates)))
            return UpdateChecker(argc, argv).check();

        if (QInstaller::isVerbose())
            std::cout << VERSION << std::endl << BUILDDATE << std::endl << SHA << std::endl;

        const SelfRestarter restarter(argc, argv);
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
