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
#include "qinstallercomponentmodel.h"
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

static void printUsage(bool isInstaller, const QString &productName,
    const QString &installerBinaryPath)
{
    QString str;
    if (isInstaller) {
        str = QString::fromLatin1("  [--script <scriptfile>] [<name>=<value>...]\n"
            "\n      Runs the %1 installer\n"
            "\n      --script runs the the installer non-interactively, without UI, using the "
            "script <scriptfile> to perform the installation.\n").arg(productName);
    } else {
        str = QString::fromLatin1("  [<name>=<value>...]\n\n      Runs the %1 uninstaller.\n")
            .arg(productName);
    }
    str = QLatin1String("\nUsage: ") + installerBinaryPath + str;
    std::cerr << qPrintable(str) << std::endl;
}

int main(int argc, char *argv[])
{
    qsrand(QDateTime::currentDateTime().toTime_t());
    const KDSelfRestarter restarter(argc, argv);
    KDRunOnceChecker runCheck(QLatin1String("lockmyApp1234865.lock"));

    try {

        {
            QApplication app(argc, argv);
            const QStringList args = app.arguments();

            // this isthe FSEngineServer as an admin rights process upon request:
            if (args.count() >= 3 && args[1] == QLatin1String("--startserver")) {
#ifdef FSENGINE_TCP
                FSEngineServer* const server = new FSEngineServer(args[2].toInt());
#else
                FSEngineServer* const server = new FSEngineServer(args[2]);
#endif
                if (args.count() >= 4)
                    server->setAuthorizationKey(args[3]);
                QObject::connect(server, SIGNAL(destroyed()), &app, SLOT(quit()));
                return app.exec();
            }

            // Make sure we honor the system's proxy settings
            #if defined(Q_OS_UNIX) && !defined(Q_OS_MAC)
                QUrl proxyUrl(QString::fromLatin1(qgetenv("http_proxy")));
                if (proxyUrl.isValid()) {
                    QNetworkProxy proxy(QNetworkProxy::HttpProxy, proxyUrl.host(),
                    proxyUrl.port(), proxyUrl.userName(), proxyUrl.password());
                    QNetworkProxy::setApplicationProxy(proxy);
                }
            #else
                if (args.contains(QLatin1String("--proxy")))
                    QNetworkProxyFactory::setUseSystemConfiguration(true);
            #endif

            if (args.contains(QLatin1String("--checkupdates"))) {
                if (runCheck.isRunning(KDRunOnceChecker::ProcessList))
                    return 0;

                Updater u;
                u.setVerbose(args.contains(QLatin1String("--verbose")));
                return u.checkForUpdates(true) ? 0 : 1;
            }
        }

        QApplication app(argc, argv);
        if (runCheck.isRunning(KDRunOnceChecker::ProcessList)) {
            if (runCheck.isRunning(KDRunOnceChecker::Lockfile))
                return 0;

            while (runCheck.isRunning(KDRunOnceChecker::ProcessList))
                Sleep::sleep(1);
        }

        const QStringList args = app.arguments();
        // from here, the "normal" installer binary is running
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
        QInstaller::Installer installer(content.magicmaker, content.performedOperations);

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
                installer.setValue(name, value);
            } else if (argument == QLatin1String("--script") || argument == QLatin1String("Script")) {
                ++i;
                if (i < args.size()) {
                    controlScript = args.at(i);
                    if (!QFileInfo(controlScript).exists())
                        return Installer::Failure;
                } else {
                    return Installer::Failure;
                }
             } else if (argument == QLatin1String("--verbose") || argument == QLatin1String("Verbose")) {
                installer.setVerbose(true);
             } else if (argument == QLatin1String("--proxy")) {
                QNetworkProxyFactory::setUseSystemConfiguration(true);
             } else if (argument == QLatin1String("--show-virtual-components")
                 || argument == QLatin1String("ShowVirtualComponents")) {
                     QInstaller::ComponentModel::setVirtualComponentsVisible(true);
                     QFont f;
                     f.setItalic(true);
                     QInstaller::ComponentModel::setVirtualComponentsFont(f);
            } else if ((argument == QLatin1String("--updater")
                || argument == QLatin1String("Updater")) && installer.isUninstaller()) {
                    installer.setUpdater();
            } else if ((argument == QLatin1String("--manage-packages")
                || argument == QLatin1String("ManagePackages")) && installer.isUninstaller()) {
                    installer.setPackageManager();
            } else if (argument == QLatin1String("--help") || argument == QLatin1String("-h")) {
                return Installer::Success;
            } else if (argument == QLatin1String("--addTempRepository")
                || argument == QLatin1String("--setTempRepository")) {
                    ++i;
                    if (i >= args.size()) {
                        std::cerr << "No repository specified" << std::endl;
                        return Installer::Failure;
                    }

                    QList<Repository> repoList;
                    QStringList items = args.at(i).split(QLatin1Char(','));
                    foreach(const QString &item, items) {
                        verbose() << "Adding custom repository:" << item << std::endl;
                        Repository rep;
                        rep.setUrl(item);
                        rep.setRequired(true);
                        repoList.append(rep);
                    }

                    // We cannot use setRemoteRepositories as that is a synchronous call which "
                    // tries to get the data from server and this isn't what we want at this point
                    const bool replace = (argument == QLatin1String("--setTempRepository"));
                    installer.setTemporaryRepositories(repoList, replace);
            } else {
                std::cerr << "Unknown option: " << argument << std::endl;
                return Installer::Failure;
            }
        }

        // Make sure we honor the system's proxy settings
        #if defined(Q_OS_UNIX) && !defined(Q_OS_MAC)
            QUrl proxyUrl(QString::fromLatin1(qgetenv("http_proxy")));
            if (proxyUrl.isValid()) {
                QNetworkProxy proxy(QNetworkProxy::HttpProxy, proxyUrl.host(),
                proxyUrl.port(), proxyUrl.userName(), proxyUrl.password());
                QNetworkProxy::setApplicationProxy(proxy);
            }
        #endif

        KDUpdater::Application updaterapp;
        const QString &productName = installer.value(QLatin1String("ProductName"));
        updaterapp.packagesInfo()->setApplicationName(productName);
        updaterapp.packagesInfo()->setApplicationVersion(installer
            .value(QLatin1String("ProductVersion")));
        updaterapp.addUpdateSource(productName, productName, QString(),
            QUrl(QLatin1String("resource://metadata/")), 0);
        installer.setUpdaterApplication(&updaterapp);

        // Create the wizard gui
        TabController controller(0);
        controller.setInstaller(&installer);
        controller.setInstallerParams(params);
        controller.setApplication(&updaterapp);
        controller.setControlScript(controlScript);

        if (installer.isInstaller()) {
            controller.setInstallerGui(new QtInstallerGui(&installer));
        } else {
            controller.setInstallerGui(new QtUninstallerGui(&installer));
        }

        int val = controller.init();
        if (val != Installer::Success)
            return val;

        const int result = app.exec();
        if (result != 0)
            return result;

        if (installer.finishedWithSuccess())
            return Installer::Success;

        const Installer::Status status = installer.status();
        const Installer::Status controllerState = Installer::Status(controller.status());

        Q_ASSERT(status == Installer::Failure || status == Installer::Success
            || status == Installer::Canceled);

        if (controllerState == Installer::Success)
            return Installer::Success;

        switch (status) {
            case Installer::Success:
                return status;

            case Installer::Canceled:
                return status;

            default:
                break;
        }
        return Installer::Failure;
    } catch(const Error &e) {
        std::cerr << qPrintable(e.message()) << std::endl;
    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
    } catch(...) {
         std::cerr << "Unknown error, aborting." << std::endl;
    }

    return Installer::Failure;
}
