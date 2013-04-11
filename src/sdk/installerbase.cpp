/**************************************************************************
**
** Copyright (C) 2012-2013 Digia Plc and/or its subsidiary(-ies).
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
#include "installerbase_p.h"

#include "installerbasecommons.h"
#include "tabcontroller.h"

#include <binaryformat.h>
#include <errors.h>
#include <fileutils.h>
#include <fsengineserver.h>
#include <init.h>
#include <lib7z_facade.h>
#include <operationrunner.h>
#include <packagemanagercore.h>
#include <packagemanagergui.h>
#include <qinstallerglobal.h>
#include <settings.h>
#include <utils.h>
#include <updater.h>

#include <kdselfrestarter.h>
#include <kdrunoncechecker.h>
#include <kdupdaterfiledownloaderfactory.h>

#include <QtCore/QTranslator>
#include <QMessageBox>

#include <QtNetwork/QNetworkProxyFactory>

#include <iostream>
#include <fstream>

#include <string>

#define QUOTE_(x) #x
#define QUOTE(x) QUOTE_(x)
#define VERSION "IFW Version: \"" QUOTE(IFW_VERSION) "\", Installer base SHA1: \"" QUOTE(_GIT_SHA1_) \
    "\", Build date: " QUOTE(__DATE__) "."

using namespace QInstaller;
using namespace QInstallerCreator;

static const char installer_create_datetime_placeholder [32] = "MY_InstallerCreateDateTime_MY";

static QStringList repositories(const QStringList &arguments, const int index)
{
    if (index < arguments.size()) {
        QStringList items = arguments.at(index).split(QLatin1Char(','));
        foreach (const QString &item, items) {
            qDebug() << "Adding custom repository:" << item;
        }
        return items;
    } else {
        std::cerr << "No repository specified" << std::endl;
    }
    return QStringList();
}

// -- main

int main(int argc, char *argv[])
{
    QStringList args = QInstaller::parseCommandLineArgs(argc, argv);

// hack to use cleanlooks if it is under Ubuntu
#if defined(Q_OS_UNIX) && !defined(Q_OS_MAC)
    std::string standardString;
    std::string cleanLooks ="-style=cleanlooks";
    std::ifstream input("/etc/os-release");
    bool isUbuntu = false;
    while (std::getline(input, standardString)) {
        if (standardString == "ID=ubuntu")
            isUbuntu = true;
    }

    if (isUbuntu) {
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
        if (args.contains(QLatin1String("--help")) || args.contains(QLatin1String("-h"))) {
            InstallerBase::showUsage();
            return 0;
        }

        if (args.contains(QLatin1String("--version"))) {
            QString versionOutPut;
            QDateTime dateTimeCheck = QDateTime::fromString(QLatin1String(
                installer_create_datetime_placeholder), QLatin1String("yyyy-MM-dd - HH:mm:ss"));
            if (dateTimeCheck.isValid()) {
                versionOutPut.append(QLatin1String("Installer creation time: "));
                versionOutPut.append(QLatin1String(installer_create_datetime_placeholder));
                versionOutPut.append(QLatin1String("\n"));
            }
            versionOutPut.append(QLatin1String(VERSION));
            InstallerBase::showVersion(versionOutPut);
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
            u.setVerbose(args.contains(QLatin1String("--verbose")) || args.contains(QLatin1String("-v")));
            return u.checkForUpdates() ? 0 : 1;
        }

        if (args.contains(QLatin1String("--runoperation"))
            || args.contains(QLatin1String("--undooperation"))) {
            MyCoreApplication app(argc, argv);
            OperationRunner o;
            o.setVerbose(args.contains(QLatin1String("--verbose"))
                         || args.contains(QLatin1String("-v")));
            return o.runOperation(args);
        }

        if (args.contains(QLatin1String("--update-installerbase"))) {
            MyCoreApplication app(argc, argv);
            if (runCheck.isRunning(KDRunOnceChecker::ProcessList))
                return 0;

            return InstallerBase().replaceMaintenanceToolBinary(args);
        }

        if (args.contains(QLatin1String("--dump-binary-data"))) {
            bool verbose = args.contains(QLatin1String("--verbose")) || args.contains(QLatin1String("-v"));

            args = args.mid(args.indexOf(QLatin1String("--dump-binary-data")) + 1, 4);
            // we expect at least -o and the output path
            if (args.count() < 2 || !args.contains(QLatin1String("-o"))) {
                InstallerBase::showUsage();
                return EXIT_FAILURE;
            }

            // output path
            const QString output = args.at(args.indexOf(QLatin1String("-o")) + 1);
            if (output.isEmpty()) {
                InstallerBase::showUsage();
                return EXIT_FAILURE;
            }

            MyCoreApplication app(argc, argv);

            // input, if not given use current app
            QString input;
            if (args.indexOf(QLatin1String("-i")) >= 0)
                input = args.value(args.indexOf(QLatin1String("-i")) + 1);

            if (input.isEmpty() || input == QLatin1String("-o") || input == output)
                 input = QCoreApplication::applicationFilePath();

            OperationRunner o(input);
            o.setVerbose(verbose);
            return o.runOperation(QStringList(QLatin1String("--runoperation"))
                << QLatin1String("CreateLocalRepository") << input << output);
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

        if (args.contains(QLatin1String("--verbose")) || args.contains(QLatin1String("-v"))) {
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

        // install English translation as fallback so that correct license button text is used
        const QString enLocaleFile = QString::fromLatin1(":/translations/en_us.qm");
        if (QFile::exists(enLocaleFile)) {
            QTranslator* const translator = new QTranslator(&app);
            translator->load(enLocaleFile);
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
            qDebug() << "Language: " << QLocale().name().toLower();
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

        QString binaryFile = QCoreApplication::applicationFilePath();
        const int index = args.indexOf(QLatin1String("--binarydatafile"));
        if (index >= 0) {
            binaryFile = args.value(index + 1);
            if (binaryFile.isEmpty()) {
                std::cerr << QFileInfo(app.applicationFilePath()).fileName() << " --binarydatafile [missing "
                    "argument]" << std::endl;
                return PackageManagerCore::Failure;
            }

            // Consume the arguments to avoid "Unknown option" output. Also there's no need to check for a
            // valid binary, will throw in readAndRegisterFromBinary(...) if the magic cookie can't be found.
            args.removeAt(index + 1);
            args.removeAll(QLatin1String("--binarydatafile"));
        }

#ifdef Q_OS_MAC
        // Load the external binary resource if we got one passed, otherwise assume we are a bundle. In that
        // case we need to figure out the path into the bundles resources folder to get the binary data.
        if (index < 0) {
            QDir resourcePath(QFileInfo(binaryFile).dir());
            resourcePath.cdUp();
            resourcePath.cd(QLatin1String("Resources"));
            binaryFile = resourcePath.filePath(QLatin1String("installer.dat"));
        }
#endif
        BinaryContent content = BinaryContent::readAndRegisterFromBinary(binaryFile);

        // instantiate the installer we are actually going to use
        QInstaller::PackageManagerCore core(content.magicMarker(), content.performedOperations());

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
             } else if (argument == QLatin1String("--verbose") || argument == QLatin1String("-v")) {
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
            } else if (argument == QLatin1String("--addTempRepository")
                || argument == QLatin1String("--setTempRepository")) {
                    ++i;
                    QStringList repoList = repositories(args, i);
                    if (repoList.isEmpty())
                        return PackageManagerCore::Failure;

                    // We cannot use setRemoteRepositories as that is a synchronous call which "
                    // tries to get the data from server and this isn't what we want at this point
                    const bool replace = (argument == QLatin1String("--setTempRepository"));
                    core.setTemporaryRepositories(repoList, replace);
            } else if (argument == QLatin1String("--addRepository")) {
                ++i;
                QStringList repoList = repositories(args, i);
                if (repoList.isEmpty())
                    return PackageManagerCore::Failure;
                core.addUserRepositories(repoList);
            } else if (argument == QLatin1String("--no-force-installations")) {
                PackageManagerCore::setNoForceInstallation(true);
            } else if (argument == QLatin1String("--create-offline-repository")) {
                PackageManagerCore::setCreateLocalRepositoryFromBinary(true);
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
