/**************************************************************************
**
** This file is part of Installer Framework
**
** Copyright (c) 2010, 2012 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (info@qt.nokia.com)
**
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** Other Usage
**
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
** If you have questions regarding the use of this file, please contact
** Nokia at info@qt.nokia.com.
**
**************************************************************************/
#include "installerbase_p.h"

#include "installerbasecommons.h"
#include "tabcontroller.h"

#include <common/binaryformat.h>
#include <common/errors.h>
#include <common/fileutils.h>
#include <common/utils.h>
#include <fsengineserver.h>
#include <init.h>
#include <lib7z_facade.h>
#include <operationrunner.h>
#include <packagemanagercore.h>
#include <packagemanagergui.h>
#include <qinstallerglobal.h>
#include <settings.h>
#include <updater.h>

#include <kdselfrestarter.h>
#include <kdrunoncechecker.h>
#include "kdupdaterfiledownloaderfactory.h"

#include <QtCore/QTranslator>
#include <QtGui/QMessageBox>

#include <QtNetwork/QNetworkProxyFactory>

#include <iostream>
#include <fstream>

#include <string>

#define QUOTE_(x) #x
#define QUOTE(x) QUOTE_(x)
#define VERSION "Installerbase SHA1: \"" QUOTE(_GIT_SHA1_) "\" , Build date: " QUOTE(__DATE__) "."

using namespace QInstaller;
using namespace QInstallerCreator;

static QSet<Repository> repositories(const QStringList &arguments, const int index)
{
    QSet<Repository> set;
    if (index < arguments.size()) {
        QStringList items = arguments.at(index).split(QLatin1Char(','));
        foreach (const QString &item, items) {
            set.insert(Repository(item, false));
            qDebug() << "Adding custom repository:" << item;
        }
    } else {
        std::cerr << "No repository specified" << std::endl;
    }
    return set;
}

static bool allowMaintenanceTool()
{
    try {
        Settings m_settings = Settings(Settings::fromFileAndPrefix(QLatin1String(":/metadata/installer-config/config.xml"),
            QLatin1String(":/metadata/installer-config/")));

        if (m_settings.value(QLatin1String("AllowExecuteMaintenanceTool"), QLatin1String("true")).toString().toLower()
            == QLatin1String("true"))
        {
            return true;
        }
    } catch (const Error &e) {
        qWarning("Could not parse Config: %s", qPrintable(e.message()));
        return true;
    }
    return false;
}

// -- main

int main(int argc, char *argv[])
{
    QStringList args = QInstaller::parseCommandLineArgs(argc, argv);

// hack to use cleanlooks if it is under Ubuntu 11.10
#if defined(Q_OS_UNIX) && !defined(Q_OS_MAC)
    std::string standardString;
    std::string cleanLooks ="-style=cleanlooks";
    std::ifstream input("/etc/lsb-release");
    bool isUbuntu = false;
    bool is11_10 = false;
    while (std::getline(input, standardString)) {
        if (standardString == "DISTRIB_ID=Ubuntu")
            isUbuntu = true;
        else if (standardString == "DISTRIB_RELEASE=11.10")
            is11_10 = true;
    }

    if (isUbuntu && is11_10) {
        argc++;
        char **newArgv = new char* [argc];
        newArgv[0] = argv[0];
        newArgv[1] = const_cast<char*>(cleanLooks.data());
        for (int i = 1; i < argc-1; ++i) {
            newArgv[i+1] = argv[i];
        }
        argv = newArgv;
    }
#endif

    qsrand(QDateTime::currentDateTime().toTime_t());
    const KDSelfRestarter restarter(argc, argv);
    KDRunOnceChecker runCheck(QLatin1String("lockmyApp1234865.lock"));

    try {
        if (args.contains(QLatin1String("--version"))) {
            InstallerBase::showVersion(QLatin1String(VERSION));
            return 0;
        }

        // this is the FSEngineServer as an admin rights process upon request:
        if (args.count() >= 3 && args[1] == QLatin1String("--startserver")) {
            MyCoreApplication app(argc, argv);
            FSEngineServer* const server = new FSEngineServer(args[2].toInt());
            if (args.count() >= 4)
                server->setAuthorizationKey(args[3]);
            QObject::connect(server, SIGNAL(destroyed()), &app, SLOT(quit()));
            return app.exec();
        }

        // Make sure we honor the system's proxy settings
#if defined(Q_OS_UNIX) && !defined(Q_OS_MAC)
        QUrl proxyUrl(QString::fromLatin1(qgetenv("http_proxy")));
        if (proxyUrl.isValid()) {
            QNetworkProxy proxy(QNetworkProxy::HttpProxy, proxyUrl.host(), proxyUrl.port(),
                proxyUrl.userName(), proxyUrl.password());
            QNetworkProxy::setApplicationProxy(proxy);
        }
#else
        if (args.contains(QLatin1String("--proxy")))
            QNetworkProxyFactory::setUseSystemConfiguration(true);
#endif

        if (args.contains(QLatin1String("--checkupdates"))) {
            MyCoreApplication app(argc, argv);
            if (runCheck.isRunning(KDRunOnceChecker::ProcessList))
                return 0;

            Updater u;
            u.setVerbose(args.contains(QLatin1String("--verbose")));
            return u.checkForUpdates() ? 0 : 1;
        }

        if (args.contains(QLatin1String("--runoperation"))
            || args.contains(QLatin1String("--undooperation"))) {
            MyCoreApplication app(argc, argv);
            OperationRunner o;
            o.setVerbose(args.contains(QLatin1String("--verbose")));
            return o.runOperation(args);
        }

        if (args.contains(QLatin1String("--update-installerbase"))) {
            MyCoreApplication app(argc, argv);
            if (runCheck.isRunning(KDRunOnceChecker::ProcessList))
                return 0;

            return InstallerBase().replaceMaintenanceToolBinary(args);
        }

        // from here, the "normal" installer binary is running
        MyApplication app(argc, argv);
        args = app.arguments();

        if (runCheck.isRunning(KDRunOnceChecker::ProcessList)) {
            if (runCheck.isRunning(KDRunOnceChecker::Lockfile))
                return 0;

            while (runCheck.isRunning(KDRunOnceChecker::ProcessList))
                Sleep::sleep(1);
        }

        if (args.contains(QLatin1String("--verbose")) || args.contains(QLatin1String("Verbose"))) {
            app.setVerbose();
            QInstaller::setVerbose(true);
        }

        // install the default translator
        const QString localeFile =
            QString::fromLatin1(":/translations/qt_%1").arg(QLocale::system().name());
        {
            QTranslator* const translator = new QTranslator(&app);
            translator->load(localeFile);
            app.installTranslator(translator);
        }

        // install "our" default translator
        const QString ourLocaleFile =
            QString::fromLatin1(":/translations/%1.qm").arg(QLocale().name().toLower());
        if (QFile::exists(ourLocaleFile)) {
            QTranslator* const translator = new QTranslator(&app);
            translator->load(ourLocaleFile);
            app.installTranslator(translator);
        }

        if (QInstaller::isVerbose()) {
            qDebug() << VERSION;
            qDebug() << "Arguments:" << args;
            qDebug() << "Resource tree before loading the in-binary resource:";

            QDir dir(QLatin1String(":/"));
            foreach (const QString &i, dir.entryList()) {
                const QByteArray ba = i.toUtf8();
                qDebug().nospace() << "    :/" << ba.constData();
            }
        }

        // register custom operations before reading the binary content cause they may used in
        // the uninstaller for the recorded list of during the installation performed operations
        QInstaller::init();

        // load the embedded binary resource
        BinaryContent content = BinaryContent::readFromApplicationFile();
        content.registerEmbeddedQResources();

        // instantiate the installer we are actually going to use
        QInstaller::PackageManagerCore core(content.magicmaker(), content.performedOperations());
        Settings m_settings = Settings(Settings::fromFileAndPrefix(QLatin1String(":/metadata/installer-config/config.xml"),
            QLatin1String(":/metadata/installer-config/")));

        // check execution rights for the maintanance tool
        if (!core.isInstaller() && !allowMaintenanceTool() && !args.contains(QLatin1String("--script"))) {
            QString reason = m_settings.value(QLatin1String("DisallowExecuteReason"),
                                QString::fromLatin1("You are not allowed to run %1.").arg(qAppName())).toString();
            QMessageBox::information(0, QLatin1String("Update notification"), reason);
            return 0;
        }

        if (QInstaller::isVerbose()) {
            qDebug() << "Resource tree after loading the in-binary resource:";

            QDir dir = QDir(QLatin1String(":/"));
            foreach (const QString &i, dir.entryList())
                qDebug() << QString::fromLatin1("    :/%1").arg(i);

            dir = QDir(QLatin1String(":/metadata/"));
            foreach (const QString &i, dir.entryList())
                qDebug() << QString::fromLatin1("    :/metadata/%1").arg(i);
        }

        QString controlScript;
        QHash<QString, QString> params;
        for (int i = 1; i < args.size(); ++i) {
            const QString &argument = args.at(i);
            if (argument.isEmpty())
                continue;

            if (argument.contains(QLatin1Char('='))) {
                const QString name = argument.section(QLatin1Char('='), 0, 0);
                const QString value = argument.section(QLatin1Char('='), 1, 1);
                params.insert(name, value);
                core.setValue(name, value);
            } else if (argument == QLatin1String("--script") || argument == QLatin1String("Script")) {
                ++i;
                if (i < args.size()) {
                    controlScript = args.at(i);
                    if (!QFileInfo(controlScript).exists())
                        return PackageManagerCore::Failure;
                } else {
                    return PackageManagerCore::Failure;
                }
             } else if (argument == QLatin1String("--verbose") || argument == QLatin1String("Verbose")) {
                core.setVerbose(true);
             } else if (argument == QLatin1String("--proxy")) {
                    core.settings().setProxyType(QInstaller::Settings::SystemProxy);
                    KDUpdater::FileDownloaderFactory::instance().setProxyFactory(core.proxyFactory());
             } else if (argument == QLatin1String("--show-virtual-components")
                 || argument == QLatin1String("ShowVirtualComponents")) {
                     QFont f;
                     f.setItalic(true);
                     PackageManagerCore::setVirtualComponentsFont(f);
                     PackageManagerCore::setVirtualComponentsVisible(true);
            } else if ((argument == QLatin1String("--updater")
                || argument == QLatin1String("Updater")) && core.isUninstaller()) {
                    core.setUpdater();
            } else if ((argument == QLatin1String("--manage-packages")
                || argument == QLatin1String("ManagePackages")) && core.isUninstaller()) {
                    core.setPackageManager();
            } else if (argument == QLatin1String("--help") || argument == QLatin1String("-h")) {
                InstallerBase::showUsage();
                return PackageManagerCore::Success;
            } else if (argument == QLatin1String("--addTempRepository")
                || argument == QLatin1String("--setTempRepository")) {
                    ++i;
                    QSet<Repository> repoList = repositories(args, i);
                    if (repoList.isEmpty())
                        return PackageManagerCore::Failure;

                    // We cannot use setRemoteRepositories as that is a synchronous call which "
                    // tries to get the data from server and this isn't what we want at this point
                    const bool replace = (argument == QLatin1String("--setTempRepository"));
                    core.setTemporaryRepositories(repoList, replace);
            } else if (argument == QLatin1String("--addRepository")) {
                ++i;
                QSet<Repository> repoList = repositories(args, i);
                if (repoList.isEmpty())
                    return PackageManagerCore::Failure;
                core.addUserRepositories(repoList);
            } else if (argument == QLatin1String("--no-force-installations")) {
                PackageManagerCore::setNoForceInstallation(true);
            } else {
                std::cerr << "Unknown option: " << argument << std::endl;
            }
        }

        // Create the wizard gui
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
