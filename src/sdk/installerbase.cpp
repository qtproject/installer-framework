/**************************************************************************
**
** Copyright (C) 2012-2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
**************************************************************************/

#include "console.h"
#include "installerbasecommons.h"
#include "sdkapp.h"
#include "tabcontroller.h"
#include "updatechecker.h"

#include <binarycontent.h>
#include <errors.h>
#include <init.h>
#include <messageboxhandler.h>
#include <productkeycheck.h>
#include <remoteserver.h>
#include <settings.h>
#include <utils.h>

#include <kdselfrestarter.h>
#include <kdrunoncechecker.h>
#include <kdupdaterfiledownloaderfactory.h>

#include <QCommandLineParser>
#include <QDirIterator>
#include <QNetworkProxyFactory>
#include <QTranslator>

#include <iostream>

#define QUOTE_(x) #x
#define QUOTE(x) QUOTE_(x)
#define VERSION "IFW Version: \"" QUOTE(IFW_VERSION) "\""
#define BUILDDATE "Build date: " QUOTE(__DATE__)
#define SHA "Installer Framework SHA1: \"" QUOTE(_GIT_SHA1_) "\""
static const char PLACEHOLDER[32] = "MY_InstallerCreateDateTime_MY";

using namespace QInstaller;

static QStringList repositories(const QString &list)
{
    const QStringList items = list.split(QLatin1Char(','), QString::SkipEmptyParts);
    foreach (const QString &item, items)
        qDebug() << "Adding custom repository:" << item;
    return items;
}

// -- main

int main(int argc, char *argv[])
{
// increase maximum numbers of file descriptors
#if defined (Q_OS_OSX)
    struct rlimit rl;
    getrlimit(RLIMIT_NOFILE, &rl);
    rl.rlim_cur = qMin((rlim_t)OPEN_MAX, rl.rlim_max);
    setrlimit(RLIMIT_NOFILE, &rl);
#endif

    qsrand(QDateTime::currentDateTime().toTime_t());

    QCommandLineParser parser;
    QCommandLineOption help = parser.addHelpOption();

    QCommandLineOption version(QLatin1String("version"),
        QLatin1String("Displays version information."));
    parser.addOption(version);

    QCommandLineOption verbose(QStringList() << QLatin1String("v") << QLatin1String("verbose"),
        QLatin1String("Verbose mode. Prints out more information."));
    parser.addOption(verbose);

    QCommandLineOption proxy(QLatin1String("proxy"),
        QLatin1String("Use system proxy on Windows and OS X. This option has no effect on Linux."));
    parser.addOption(proxy);

    QCommandLineOption script(QLatin1String("script"),
        QLatin1String("Execute the script given as argument."), QLatin1String("file"));
    parser.addOption(script);

    QCommandLineOption checkUpdates(QLatin1String("checkupdates"),
        QLatin1String("Check for updates and return an XML description."));
    parser.addOption(checkUpdates);

    QCommandLineOption updater(QLatin1String("updater"),
        QLatin1String("Start application in updater mode."));
    parser.addOption(updater);

    QCommandLineOption pkgManager(QLatin1String("manage-packages"),
        QLatin1String("Start application in package manager mode."));
    parser.addOption(pkgManager);

    QCommandLineOption noForce(QLatin1String("no-force-installations"),
        QLatin1String("Allow deselection of components that are marked as forced."));
    parser.addOption(noForce);

    QCommandLineOption showVirtuals(QLatin1String("show-virtual-components"),
        QLatin1String("Show virtual components in installer and package manager."));
    parser.addOption(showVirtuals);

    QCommandLineOption offlineRepo(QLatin1String("create-offline-repository"), QLatin1String(
        "Create a local repository inside the installation directory. This option has no effect "
        "on online installer's"));
    parser.addOption(offlineRepo);

    QCommandLineOption addRepo(QLatin1String("addRepository"),
        QLatin1String("Add a local or remote repository to the list of user defined repositories."),
        QLatin1String("URI,..."));
    parser.addOption(addRepo);

    QCommandLineOption addTmpRepo(QLatin1String("addTempRepository"), QLatin1String(
        "Add a local or remote repository to the list of temporary available repositories."),
        QLatin1String("URI,..."));
    parser.addOption(addTmpRepo);

    QCommandLineOption setTmpRepo(QLatin1String("setTempRepository"),
        QLatin1String("Set a local or remote repository as temporary repository, it is the only "
        "one used during fetch.\nNote: URI must be prefixed with the protocol, i.e. file:///, "
        "https://, http:// or ftp://."), QLatin1String("URI,..."));
    parser.addOption(setTmpRepo);

    QCommandLineOption startServer(QLatin1String("startserver"), QLatin1String("Starts the "
        "application as headless process waiting for commands to execute."),
        QLatin1String("port,key"));
    parser.addOption(startServer);

    parser.addPositionalArgument(QLatin1String("Key=Value"),
        QLatin1String("Key Value pair to be set."));

    // We need to start either a command line application or a GUI application. Since we
    // fail doing so at least on Linux while parsing the argument using a core application
    // object and later starting the GUI application, we now parse the arguments first.
    parser.parse(QInstaller::parseCommandLineArgs(argc, argv));

    QStringList mutually;
    if (parser.isSet(checkUpdates))
        mutually << checkUpdates.names();
    if (parser.isSet(updater))
        mutually << updater.names();
    if (parser.isSet(pkgManager))
        mutually << pkgManager.names();

    if (parser.isSet(help) || parser.isSet(version) || (mutually.count() > 1)) {
        Console c;
        QCoreApplication app(argc, argv);

        if (parser.isSet(version)) {
            std::cout << VERSION << std::endl << BUILDDATE << std::endl << SHA << std::endl;
            const QDateTime dateTime = QDateTime::fromString(QLatin1String(PLACEHOLDER),
                QLatin1String("yyyy-MM-dd - HH:mm:ss"));
            if (dateTime.isValid())
                std::cout << "Installer creation time: " << PLACEHOLDER << std::endl;
            return EXIT_SUCCESS;
        }

        if (mutually.count() > 1) {
            std::cerr << qPrintable(QString::fromLatin1("The following options are mutually "
                "exclusive: %1.").arg(mutually.join(QLatin1String(", ")))) << std::endl;
        }

        std::cout << qPrintable(parser.helpText()) << std::endl;
        return parser.isSet(help) ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    if (parser.isSet(startServer)) {
        const QString argument = parser.value(startServer);
        const QString port = argument.section(QLatin1Char(','), 0, 0);
        const QString key = argument.section(QLatin1Char(','), 1, 1);

        QStringList missing;
        if (port.isEmpty())
            missing << QLatin1String("Port");
        if (key.isEmpty())
            missing << QLatin1String("Key");

        SDKApp<QCoreApplication> app(argc, argv);
        if (missing.count()) {
            Console c;
            std::cerr << qPrintable(QString::fromLatin1("Missing argument(s) for option "
                "'startserver': %2").arg(missing.join(QLatin1String(", ")))) << std::endl;
            std::cout << qPrintable(parser.helpText()) << std::endl;
            return EXIT_FAILURE;
        }

        RemoteServer *server = new RemoteServer;
        QObject::connect(server, SIGNAL(destroyed()), &app, SLOT(quit()));
        server->init(port.toInt(), QHostAddress::LocalHost, Protocol::Mode::Release);
        server->setAuthorizationKey(key);
        server->start();
        return app.exec();
    }

    try {
        QScopedPointer<Console> console;
        if (parser.isSet(verbose)) {
            console.reset(new Console);
            QInstaller::setVerbose(parser.isSet(verbose));
        }

        // On Windows we need the console window from above, we are a GUI application.
        const QStringList unknownOptionNames = parser.unknownOptionNames();
        if (!unknownOptionNames.isEmpty()) {
            const QString options = unknownOptionNames.join(QLatin1String(", "));
            std::cerr << "Unknown option: " << qPrintable(options) << std::endl;
        }

        if (parser.isSet(proxy)) {
            // Make sure we honor the system's proxy settings
#if defined(Q_OS_UNIX) && !defined(Q_OS_OSX)
            QUrl proxyUrl(QString::fromLatin1(qgetenv("http_proxy")));
            if (proxyUrl.isValid()) {
                QNetworkProxy proxy(QNetworkProxy::HttpProxy, proxyUrl.host(), proxyUrl.port(),
                    proxyUrl.userName(), proxyUrl.password());
                QNetworkProxy::setApplicationProxy(proxy);
            }
#else
            QNetworkProxyFactory::setUseSystemConfiguration(true);
#endif
        }

        if (parser.isSet(checkUpdates))
            return UpdateChecker().check(argc, argv);

        SDKApp<QApplication> app(argc, argv);
        KDRunOnceChecker runCheck(QLatin1String("lockmyApp1234865.lock"));

        if (runCheck.isRunning(KDRunOnceChecker::ProcessList)
            || runCheck.isRunning(KDRunOnceChecker::Lockfile)) {
                QInstaller::MessageBoxHandler::information(0, QLatin1String("AlreadyRunning"),
                    QString::fromLatin1("Waiting for %1").arg(qAppName()),
                    QString::fromLatin1("Another %1 instance is already running. Wait "
                    "until it finishes, close it, or restart your system.").arg(qAppName()));
            return EXIT_FAILURE;
        }

        const KDSelfRestarter restarter(argc, argv);
        QInstaller::init(); // register custom operations

        if (QInstaller::isVerbose()) {
            qDebug() << VERSION;
            qDebug() << "Arguments:" << app.arguments();
            qDebug() << "Language: " << QLocale().uiLanguages().value(0,
                QLatin1String("No UI language set"));
        }

        BinaryContent content = BinaryContent::readAndRegisterFromBinary(app.binaryFile());

        // instantiate the installer we are actually going to use
        PackageManagerCore core(content.magicMarker(), content.performedOperations());
        ProductKeyCheck::instance()->init(&core);

        QString controlScript;
        if (parser.isSet(script)) {
            controlScript = parser.value(script);
            if (!QFileInfo(controlScript).exists())
                throw Error(QLatin1String("Script file does not exist."));
        }

        if (parser.isSet(proxy)) {
            core.settings().setProxyType(QInstaller::Settings::SystemProxy);
            KDUpdater::FileDownloaderFactory::instance().setProxyFactory(core.proxyFactory());
        }

        if (parser.isSet(showVirtuals)) {
            QFont f;
            f.setItalic(true);
            PackageManagerCore::setVirtualComponentsFont(f);
            PackageManagerCore::setVirtualComponentsVisible(true);
        }

        if (parser.isSet(updater)) {
            if (core.isInstaller())
                throw Error(QLatin1String("Cannot start installer binary as updater."));
            core.setUpdater();
        }

        if (parser.isSet(pkgManager)) {
            if (core.isInstaller())
                throw Error(QLatin1String("Cannot start installer binary as package manager."));
            core.setPackageManager();
        }

        if (parser.isSet(addRepo)) {
            const QStringList repoList = repositories(parser.value(addRepo));
            if (repoList.isEmpty())
                throw Error(QLatin1String("Empty repository list for option 'addRepository'."));
            core.addUserRepositories(repoList);
        }

        if (parser.isSet(addTmpRepo)) {
            const QStringList repoList = repositories(parser.value(addTmpRepo));
            if (repoList.isEmpty())
                throw Error(QLatin1String("Empty repository list for option 'addTempRepository'."));
            core.setTemporaryRepositories(repoList, false);
        }

        if (parser.isSet(setTmpRepo)) {
            const QStringList repoList = repositories(parser.value(setTmpRepo));
            if (repoList.isEmpty())
                throw Error(QLatin1String("Empty repository list for option 'setTempRepository'."));
            core.setTemporaryRepositories(repoList, true);
        }

        PackageManagerCore::setNoForceInstallation(parser.isSet(noForce));
        PackageManagerCore::setCreateLocalRepositoryFromBinary(parser.isSet(offlineRepo));

        QHash<QString, QString> params;
        const QStringList positionalArguments = parser.positionalArguments();
        foreach (const QString &argument, positionalArguments) {
            if (argument.contains(QLatin1Char('='))) {
                const QString name = argument.section(QLatin1Char('='), 0, 0);
                const QString value = argument.section(QLatin1Char('='), 1, 1);
                params.insert(name, value);
                core.setValue(name, value);
            }
        }

        // this needs to happen after we parse the arguments, but before we use the actual resources
        const QString newDefaultResource = core.value(QString::fromLatin1("DefaultResourceReplacement"));
        if (!newDefaultResource.isEmpty())
            content.registerAsDefaultQResource(newDefaultResource);

        if (QInstaller::isVerbose()) {
            qDebug() << "Resource tree:";
            QDirIterator it(QLatin1String(":/"), QDir::NoDotAndDotDot | QDir::AllEntries | QDir::Hidden,
                QDirIterator::Subdirectories);
            while (it.hasNext()) {
                const QString path = it.next();
                if (path.startsWith(QLatin1String(":/trolltech"))
                        || path.startsWith(QLatin1String(":/qt-project.org"))) {
                    continue;
                }
                qDebug() << "    " << path.toUtf8().constData();
            }
        }

        const QString directory = QLatin1String(":/translations");
        const QStringList translations = core.settings().translations();

        // install the default Qt translator
        QScopedPointer<QTranslator> translator(new QTranslator(&app));
        foreach (const QLocale locale, QLocale().uiLanguages()) {
            // As there is no qt_en.qm, we simply end the search when the next
            // preferred language is English.
            if (locale.language() == QLocale::English)
                break;
            if (translator->load(locale, QLatin1String("qt"), QString::fromLatin1("_"), directory)) {
                app.installTranslator(translator.take());
                break;
            }
        }

        translator.reset(new QTranslator(&app));
        // install English translation as fallback so that correct license button text is used
        if (translator->load(QLatin1String("en_us"), directory))
            app.installTranslator(translator.take());

        if (translations.isEmpty()) {
            translator.reset(new QTranslator(&app));
            foreach (const QLocale locale, QLocale().uiLanguages()) {
                if (translator->load(locale, QLatin1String(""), QLatin1String(""), directory)) {
                    app.installTranslator(translator.take());
                    break;
                }
            }
        } else {
            foreach (const QString &translation, translations) {
                translator.reset(new QTranslator(&app));
                if (translator->load(translation, QLatin1String(":/translations")))
                    app.installTranslator(translator.take());
            }
        }

        //create the wizard GUI
        TabController controller(0);
        controller.setManager(&core);
        controller.setManagerParams(params);
        controller.setControlScript(controlScript);

        if (core.isInstaller()) {
            controller.setGui(new InstallerGui(&core));
        } else {
            controller.setGui(new MaintenanceGui(&core));
        }

        PackageManagerCore::Status status = PackageManagerCore::Status(controller.init());
        if (status != PackageManagerCore::Success)
            return status;

        const int result = app.exec();
        if (result != 0)
            return result;

        if (core.finishedWithSuccess())
            return PackageManagerCore::Success;

        status = core.status();
        switch (status) {
            case PackageManagerCore::Success:
                return status;

            case PackageManagerCore::Canceled:
                return status;

            default:
                break;
        }
        return PackageManagerCore::Failure;
    } catch(const Error &e) {
        std::cerr << qPrintable(e.message()) << std::endl;
    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
    } catch(...) {
         std::cerr << "Unknown error, aborting." << std::endl;
    }

    return PackageManagerCore::Failure;
}
