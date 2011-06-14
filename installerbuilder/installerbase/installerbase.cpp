/**************************************************************************
**
** This file is part of Qt SDK**
**
** Copyright (c) 2011 Nokia Corporation and/or its subsidiary(-ies).*
**
** Contact:  Nokia Corporation qt-info@nokia.com**
**
** No Commercial Usage
**
** This file contains pre-release code and may not be distributed.
** You may use this file in accordance with the terms and conditions
** contained in the Technology Preview License Agreement accompanying
** this package.
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
** rights. These rights are described in the Nokia Qt LGPL Exception version
** 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** If you are unsure which license is appropriate for your use, please contact
** (qt-info@nokia.com).
**
**************************************************************************/
#include "common/binaryformat.h"
#include "common/errors.h"
#include "common/fileutils.h"
#include "common/installersettings.h"
#include "common/utils.h"
#include "fsengineserver.h"
#include "init.h"
#include "installerbasecommons.h"
#include "lib7z_facade.h"
#include "qinstallerglobal.h"
#include "qinstallergui.h"
#include "tabcontroller.h"
#include "updater.h"

#include <qinstaller.h>

#include <QtCore/QTranslator>
#include <QtCore/QThread>

#include <QtGui/QApplication>

#include <QtNetwork/QNetworkProxyFactory>

#include <KDUpdater/Application>
#include <KDUpdater/PackagesInfo>

#include <KDToolsCore/KDSelfRestarter>
#include <KDToolsCore/KDRunOnceChecker>

#include <iostream>

using namespace QInstaller;
using namespace QInstallerCreator;

class Sleep : public QThread
{
public:
    static void sleep(unsigned long ms)
    {
        QThread::usleep(ms);
    }
};

static QList<Repository> repositories(const QStringList &arguments, const int index)
{
    QList<Repository> repoList;
    if (index < arguments.size()) {
        QStringList items = arguments.at(index).split(QLatin1Char(','));
        foreach (const QString &item, items) {
            verbose() << "Adding custom repository:" << item << std::endl;
            Repository rep(item);
            repoList.append(rep);
        }
    } else {
        std::cerr << "No repository specified" << std::endl;
    }
    return repoList;
}

int main(int argc, char *argv[])
{
    qsrand(QDateTime::currentDateTime().toTime_t());
    const KDSelfRestarter restarter(argc, argv);
    KDRunOnceChecker runCheck(QLatin1String("lockmyApp1234865.lock"));

    const QStringList args = QInstaller::parseCommandLineArgs(argc, argv);
    try {
        // this is the FSEngineServer as an admin rights process upon request:
        if (args.count() >= 3 && args[1] == QLatin1String("--startserver")) {
            QCoreApplication app(argc, argv);
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
            QCoreApplication app(argc, argv);
            if (runCheck.isRunning(KDRunOnceChecker::ProcessList))
                return 0;

            Updater u;
            u.setVerbose(args.contains(QLatin1String("--verbose")));
            return u.checkForUpdates() ? 0 : 1;
        }

        // from here, the "normal" installer binary is running
        QApplication app(argc, argv);

        if (runCheck.isRunning(KDRunOnceChecker::ProcessList)) {
            if (runCheck.isRunning(KDRunOnceChecker::Lockfile))
                return 0;

            while (runCheck.isRunning(KDRunOnceChecker::ProcessList))
                Sleep::sleep(1);
        }

        if (args.contains(QLatin1String("--verbose")) || args.contains(QLatin1String("Verbose")))
            QInstaller::setVerbose(true);

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
        if (QFile::exists(ourLocaleFile))
        {
            QTranslator* const translator = new QTranslator(&app);
            translator->load(ourLocaleFile);
            app.installTranslator(translator);
        }

        if (QInstaller::isVerbose()) {
            verbose() << "This is installerbase version " << INSTALLERBASE_VERSION << std::endl;
            verbose() << "ARGS: " << args << std::endl;

            verbose() << "resource tree before loading the in-binary resource: " << std::endl;

            QDir dir(QLatin1String(":/"));
            foreach (const QString &i, dir.entryList()) {
                const QByteArray ba = i.toUtf8();
                verbose() << ba.constData() << std::endl;
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

        if (QInstaller::isVerbose()) {
            verbose() << "resource tree after loading the in-binary resource: " << std::endl;

            QDir dir = QDir(QLatin1String(":/"));
            foreach (const QString &i, dir.entryList())
                verbose() << QString::fromLatin1(":/%1").arg(i) << std::endl;

            dir = QDir(QLatin1String(":/metadata/"));
            foreach (const QString &i, dir.entryList())
                verbose() << QString::fromLatin1(":/metadata/%1").arg(i) << std::endl;
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
#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
                QNetworkProxyFactory::setUseSystemConfiguration(true);
#endif
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
                return PackageManagerCore::Success;
            } else if (argument == QLatin1String("--addTempRepository")
                || argument == QLatin1String("--setTempRepository")) {
                    ++i;
                    QList<Repository> repoList = repositories(args, i);
                    if (repoList.isEmpty())
                        return PackageManagerCore::Failure;

                    // We cannot use setRemoteRepositories as that is a synchronous call which "
                    // tries to get the data from server and this isn't what we want at this point
                    const bool replace = (argument == QLatin1String("--setTempRepository"));
                    core.setTemporaryRepositories(repoList, replace);
            } else if (argument == QLatin1String("--addRepository")) {
                ++i;
                QList<Repository> repoList = repositories(args, i);
                if (repoList.isEmpty())
                    return PackageManagerCore::Failure;
                core.addRepositories(repoList);
            } else if (argument == QLatin1String("--no-force-installations")) {
                PackageManagerCore::setNoForceInstallation(true);
            } else {
                verbose() << "Unknown option: " << argument << std::endl;
            }
        }


        KDUpdater::Application updaterapp;
        const QString &productName = core.value(QLatin1String("ProductName"));
        updaterapp.packagesInfo()->setApplicationName(productName);
        updaterapp.packagesInfo()->setApplicationVersion(core.value(QLatin1String("ProductVersion")));
        if (content.magicmaker() == MagicInstallerMarker) {
            updaterapp.addUpdateSource(productName, productName, QString(),
                QUrl(QLatin1String("resource://metadata/")), 0);
        }
        core.setUpdaterApplication(&updaterapp);

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
