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

#include "constants.h"
#include "commandlineparser.h"
#include "installerbase.h"
#include "installerbasecommons.h"
#include "tabcontroller.h"

#include <binaryformatenginehandler.h>
#include <copydirectoryoperation.h>
#include <errors.h>
#include <init.h>
#include <updateoperations.h>
#include <messageboxhandler.h>
#include <packagemanagercore.h>
#include <packagemanagerproxyfactory.h>
#include <qprocesswrapper.h>
#include <protocol.h>
#include <productkeycheck.h>
#include <settings.h>
#include <utils.h>
#include <globals.h>

#include <runoncechecker.h>
#include <filedownloaderfactory.h>

#include <QDir>
#include <QDirIterator>
#include <QFontDatabase>
#include <QTemporaryFile>
#include <QTranslator>
#include <QUuid>
#include <QLoggingCategory>

#ifdef ENABLE_SQUISH
#include <qtbuiltinhook.h>
#endif

InstallerBase::InstallerBase(int &argc, char *argv[])
    : SDKApp<QApplication>(argc, argv)
    , m_core(nullptr)
{
    QInstaller::init(); // register custom operations
}

InstallerBase::~InstallerBase()
{
    delete m_core;
}

int InstallerBase::run()
{
    RunOnceChecker runCheck(QDir::tempPath()
                            + QLatin1Char('/')
                            + qApp->applicationName()
                            + QLatin1String("1234865.lock"));
    if (runCheck.isRunning(RunOnceChecker::ConditionFlag::Lockfile)) {
        // It is possible that two installers with the same name get executed
        // concurrently and thus try to access the same lock file. This causes
        // a warning to be shown (when verbose output is enabled) but let's
        // just silently ignore the fact that we could not create the lock file
        // and check the running processes.
        if (runCheck.isRunning(RunOnceChecker::ConditionFlag::ProcessList)) {
            QInstaller::MessageBoxHandler::information(nullptr, QLatin1String("AlreadyRunning"),
                tr("Waiting for %1").arg(qAppName()),
                tr("Another %1 instance is already running. Wait "
                "until it finishes, close it, or restart your system.").arg(qAppName()));
            return EXIT_FAILURE;
        }
    }

    QString fileName = datFile(binaryFile());
    quint64 cookie = QInstaller::BinaryContent::MagicCookieDat;
    if (fileName.isEmpty()) {
        fileName = binaryFile();
        cookie = QInstaller::BinaryContent::MagicCookie;
    }

    QFile binary(fileName);
    QInstaller::openForRead(&binary);

    qint64 magicMarker;
    QInstaller::ResourceCollectionManager manager;
    QList<QInstaller::OperationBlob> oldOperations;
    QInstaller::BinaryContent::readBinaryContent(&binary, &oldOperations, &manager, &magicMarker,
        cookie);

    // Usually resources simply get mapped into memory and therefore the file does not need to be
    // kept open during application runtime. Though in case of offline installers we need to access
    // the appended binary content (packages etc.), so we close only in maintenance mode.
    if (magicMarker != QInstaller::BinaryContent::MagicInstallerMarker)
        binary.close();

    CommandLineParser parser;
    parser.parse(arguments());

    QString loggingRules(QLatin1String("ifw.* = false")); // disable all by default
    if (QInstaller::isVerbose()) {
        loggingRules = QString(); // enable all in verbose mode
        if (parser.isSet(QLatin1String(CommandLineOptions::LoggingRules))) {
            loggingRules = parser.value(QLatin1String(CommandLineOptions::LoggingRules))
                           .split(QLatin1Char(','), QString::SkipEmptyParts)
                           .join(QLatin1Char('\n')); // take rules from command line
        }
    }
    QLoggingCategory::setFilterRules(loggingRules);

    qCDebug(QInstaller::lcTranslations) << "Language:" << QLocale().uiLanguages()
        .value(0, QLatin1String("No UI language set")).toUtf8().constData();
    qDebug().noquote() << "Arguments:" << arguments().join(QLatin1String(", "));

    SDKApp::registerMetaResources(manager.collectionByName("QResources"));
    if (parser.isSet(QLatin1String(CommandLineOptions::StartClient))) {
        const QStringList arguments = parser.value(QLatin1String(CommandLineOptions::StartClient))
            .split(QLatin1Char(','), QString::SkipEmptyParts);
        m_core = new QInstaller::PackageManagerCore(
            magicMarker, oldOperations,
            arguments.value(0, QLatin1String(QInstaller::Protocol::DefaultSocket)),
            arguments.value(1, QLatin1String(QInstaller::Protocol::DefaultAuthorizationKey)),
            QInstaller::Protocol::Mode::Debug);
    } else {
        m_core = new QInstaller::PackageManagerCore(magicMarker, oldOperations,
            QUuid::createUuid().toString(), QUuid::createUuid().toString());
    }

    {
        using namespace QInstaller;
        ProductKeyCheck::instance()->init(m_core);
        ProductKeyCheck::instance()->addPackagesFromXml(QLatin1String(":/metadata/Updates.xml"));
        BinaryFormatEngineHandler::instance()->registerResources(manager.collections());
    }

    dumpResourceTree();

    QString controlScript;
    if (parser.isSet(QLatin1String(CommandLineOptions::Script))) {
        controlScript = parser.value(QLatin1String(CommandLineOptions::Script));
        if (!QFileInfo(controlScript).exists())
            throw QInstaller::Error(QLatin1String("Script file does not exist."));
    } else if (!m_core->settings().controlScript().isEmpty()) {
        controlScript = QLatin1String(":/metadata/installer-config/")
            + m_core->settings().controlScript();
    }

    // From Qt5.8 onwards a separate command line option --proxy is not needed as system
    // proxy is used by default. If Qt is built with QT_USE_SYSTEM_PROXIES false
    // then system proxies are not used by default.
    if ((parser.isSet(QLatin1String(CommandLineOptions::Proxy))
#if QT_VERSION > 0x050800
            || QNetworkProxyFactory::usesSystemConfiguration()
#endif
            ) && !parser.isSet(QLatin1String(CommandLineOptions::NoProxy))) {
        m_core->settings().setProxyType(QInstaller::Settings::SystemProxy);
        KDUpdater::FileDownloaderFactory::instance().setProxyFactory(m_core->proxyFactory());
    }

    if (parser.isSet(QLatin1String(CommandLineOptions::ShowVirtualComponents))) {
        QFont f;
        f.setItalic(true);
        QInstaller::PackageManagerCore::setVirtualComponentsFont(f);
        QInstaller::PackageManagerCore::setVirtualComponentsVisible(true);
    }

    if (parser.isSet(QLatin1String(CommandLineOptions::Updater))) {
        if (m_core->isInstaller())
            throw QInstaller::Error(QLatin1String("Cannot start installer binary as updater."));
        m_core->setUpdater();
    }

    if (parser.isSet(QLatin1String(CommandLineOptions::ManagePackages))) {
        if (m_core->isInstaller())
            throw QInstaller::Error(QLatin1String("Cannot start installer binary as package manager."));
        m_core->setPackageManager();
    }

    if (parser.isSet(QLatin1String(CommandLineOptions::AddRepository))) {
        const QStringList repoList = repositories(parser
            .value(QLatin1String(CommandLineOptions::AddRepository)));
        if (repoList.isEmpty())
            throw QInstaller::Error(QLatin1String("Empty repository list for option 'addRepository'."));
        m_core->addUserRepositories(repoList);
    }

    if (parser.isSet(QLatin1String(CommandLineOptions::AddTmpRepository))) {
        const QStringList repoList = repositories(parser
            .value(QLatin1String(CommandLineOptions::AddTmpRepository)));
        if (repoList.isEmpty())
            throw QInstaller::Error(QLatin1String("Empty repository list for option 'addTempRepository'."));
        m_core->setTemporaryRepositories(repoList, false);
    }

    if (parser.isSet(QLatin1String(CommandLineOptions::SetTmpRepository))) {
        const QStringList repoList = repositories(parser
            .value(QLatin1String(CommandLineOptions::SetTmpRepository)));
        if (repoList.isEmpty())
            throw QInstaller::Error(QLatin1String("Empty repository list for option 'setTempRepository'."));
        m_core->setTemporaryRepositories(repoList, true);
    }

    if (parser.isSet(QLatin1String(CommandLineOptions::InstallCompressedRepository))) {
        const QStringList repoList = repositories(parser
            .value(QLatin1String(CommandLineOptions::InstallCompressedRepository)));
        if (repoList.isEmpty())
            throw QInstaller::Error(QLatin1String("Empty repository list for option 'installCompressedRepository'."));
        foreach (QString repository, repoList) {
            if (!QFileInfo::exists(repository)) {
                qDebug() << "The file " << repository << "does not exist.";
                return EXIT_FAILURE;
            }
        }
        m_core->setTemporaryRepositories(repoList, false, true);
    }

    QInstaller::PackageManagerCore::setNoForceInstallation(parser
        .isSet(QLatin1String(CommandLineOptions::NoForceInstallation)));
    QInstaller::PackageManagerCore::setCreateLocalRepositoryFromBinary(parser
        .isSet(QLatin1String(CommandLineOptions::CreateLocalRepository))
        || m_core->settings().createLocalRepository());

    const QStringList positionalArguments = parser.positionalArguments();
    foreach (const QString &argument, positionalArguments) {
        if (argument.contains(QLatin1Char('='))) {
            const QString name = argument.section(QLatin1Char('='), 0, 0);
            const QString value = argument.section(QLatin1Char('='), 1, 1);
            m_core->setValue(name, value);
        }
    }

    const QString directory = QLatin1String(":/translations");
    const QStringList translations = m_core->settings().translations();

    if (translations.isEmpty()) {
        foreach (const QLocale locale, QLocale().uiLanguages()) {
            QScopedPointer<QTranslator> qtTranslator(new QTranslator(QCoreApplication::instance()));
            const bool qtLoaded = qtTranslator->load(locale, QLatin1String("qt"),
                                              QLatin1String("_"), directory);

            if (qtLoaded || locale.language() == QLocale::English) {
                if (qtLoaded)
                    QCoreApplication::instance()->installTranslator(qtTranslator.take());

                QScopedPointer<QTranslator> ifwTranslator(new QTranslator(QCoreApplication::instance()));
                if (ifwTranslator->load(locale, QLatin1String("ifw"), QLatin1String("_"), directory))
                    QCoreApplication::instance()->installTranslator(ifwTranslator.take());

                // To stop loading other translations it's sufficient that
                // qt was loaded successfully or we hit English as system language
                break;
            }
        }
    } else {
        foreach (const QString &translation, translations) {
            QScopedPointer<QTranslator> translator(new QTranslator(QCoreApplication::instance()));
            if (translator->load(translation, QLatin1String(":/translations")))
                QCoreApplication::instance()->installTranslator(translator.take());
        }
    }

    {
        QDirIterator fontIt(QStringLiteral(":/fonts"));
        while (fontIt.hasNext()) {
            const QString path = fontIt.next();
            qCDebug(QInstaller::lcResources) << "Registering custom font" << path;
            if (QFontDatabase::addApplicationFont(path) == -1)
                qWarning() << "Failed to register font!";
        }
    }

    //Do not show gui with --silentUpdate, instead update components silently
    if (parser.isSet(QLatin1String(CommandLineOptions::SilentUpdate))) {
        if (m_core->isInstaller())
            throw QInstaller::Error(QLatin1String("Cannot start installer binary as updater."));
        const ProductKeyCheck *const productKeyCheck = ProductKeyCheck::instance();
        if (!productKeyCheck->hasValidLicense())
            throw QInstaller::Error(QLatin1String("Silent update not allowed."));
        m_core->setUpdater();
        m_core->updateComponentsSilently();
    }
    else {
        //create the wizard GUI
        TabController controller(nullptr);
        controller.setManager(m_core);
        controller.setControlScript(controlScript);
        if (m_core->isInstaller())
            controller.setGui(new InstallerGui(m_core));
        else
            controller.setGui(new MaintenanceGui(m_core));

        QInstaller::PackageManagerCore::Status status =
            QInstaller::PackageManagerCore::Status(controller.init());
        if (status != QInstaller::PackageManagerCore::Success)
            return status;

#ifdef ENABLE_SQUISH
        int squishPort = 11233;
        if (parser.isSet(QLatin1String(CommandLineOptions::SquishPort))) {
            squishPort = parser.value(QLatin1String(CommandLineOptions::SquishPort)).toInt();
        }
        if (squishPort != 0) {
            if (Squish::allowAttaching(squishPort))
                qDebug() << "Attaching to squish port " << squishPort << " succeeded";
            else
                qDebug() << "Attaching to squish failed.";
        } else {
            qWarning() << "Invalid squish port number: " << squishPort;
        }
#endif
        const int result = QCoreApplication::instance()->exec();
        if (result != 0)
            return result;

        if (m_core->finishedWithSuccess())
            return QInstaller::PackageManagerCore::Success;

        status = m_core->status();
        switch (status) {
            case QInstaller::PackageManagerCore::Success:
                return status;

            case QInstaller::PackageManagerCore::Canceled:
                return status;

            default:
                break;
        }
        return QInstaller::PackageManagerCore::Failure;
    }
    return QInstaller::PackageManagerCore::Success;
}


// -- private

void InstallerBase::dumpResourceTree() const
{
    qCDebug(QInstaller::lcResources) << "Resource tree:";
    QDirIterator it(QLatin1String(":/"), QDir::NoDotAndDotDot | QDir::AllEntries | QDir::Hidden,
        QDirIterator::Subdirectories);
    while (it.hasNext()) {
        if (it.next().startsWith(QLatin1String(":/qt-project.org")))
            continue;
        qCDebug(QInstaller::lcResources) << "    " << it.filePath().toUtf8().constData();
    }
}

QStringList InstallerBase::repositories(const QString &list) const
{
    const QStringList items = list.split(QLatin1Char(','), QString::SkipEmptyParts);
    foreach (const QString &item, items)
        qDebug().noquote() << "Adding custom repository:" << item;
    return items;
}
