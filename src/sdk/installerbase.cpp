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
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
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
#include <kdupdaterupdateoperations.h>
#include <messageboxhandler.h>
#include <packagemanagercore.h>
#include <packagemanagerproxyfactory.h>
#include <qprocesswrapper.h>
#include <protocol.h>
#include <productkeycheck.h>
#include <settings.h>
#include <utils.h>

#include <kdrunoncechecker.h>
#include <kdupdaterfiledownloaderfactory.h>

#include <QDirIterator>
#include <QTemporaryFile>
#include <QTranslator>
#include <QUuid>

InstallerBase::InstallerBase(int &argc, char *argv[])
    : SDKApp<QApplication>(argc, argv)
    , m_core(0)
{
    QInstaller::init(); // register custom operations
}

InstallerBase::~InstallerBase()
{
    delete m_core;
}

int InstallerBase::run()
{
    KDRunOnceChecker runCheck(qApp->applicationDirPath() + QLatin1String("/lockmyApp1234865.lock"));
    if (runCheck.isRunning(KDRunOnceChecker::ConditionFlag::Lockfile)) {
        // It is possible to install an application and thus the maintenance tool into a
        // directory that requires elevated permission to create a lock file. Since this
        // cannot be done without requesting credentials from the user, we silently ignore
        // the fact that we could not create the lock file and check the running processes.
        if (runCheck.isRunning(KDRunOnceChecker::ConditionFlag::ProcessList)) {
            QInstaller::MessageBoxHandler::information(0, QLatin1String("AlreadyRunning"),
                QString::fromLatin1("Waiting for %1").arg(qAppName()),
                QString::fromLatin1("Another %1 instance is already running. Wait "
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

    if (QInstaller::isVerbose()) {
        qDebug() << "Language:" << QLocale().uiLanguages().value(0,
            QLatin1String("No UI language set")).toUtf8().constData();
        qDebug() << "Arguments: " << arguments().join(QLatin1String(", ")).toUtf8().constData();
    }

    CommandLineParser parser;
    parser.parse(arguments());

    SDKApp::registerMetaResources(manager.collectionByName("QResources"));
    if (parser.isSet(QLatin1String(CommandLineOptions::StartClient))) {
        const QStringList arguments = parser.value(QLatin1String(CommandLineOptions::StartServer))
            .split(QLatin1Char(','), QString::SkipEmptyParts);
        m_core = new QInstaller::PackageManagerCore(magicMarker, oldOperations, QString(arguments
            .value(0, QString::number(QInstaller::Protocol::DefaultPort))).toInt(),
            arguments.value(1, QLatin1String(QInstaller::Protocol::DefaultAuthorizationKey)),
            QInstaller::Protocol::Mode::Debug);
    } else {
        m_core = new QInstaller::PackageManagerCore(magicMarker, oldOperations,
            30000 + qrand() % 100, QUuid::createUuid().toString());
    }

    {
        using namespace QInstaller;
        ProductKeyCheck::instance()->init(m_core);
        ProductKeyCheck::instance()->addPackagesFromXml(QLatin1String(":/metadata/Updates.xml"));
        BinaryFormatEngineHandler::instance()->registerResources(manager.collections());
    }
    if (QInstaller::isVerbose())
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

    if (parser.isSet(QLatin1String(CommandLineOptions::Proxy))) {
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

    QInstaller::PackageManagerCore::setNoForceInstallation(parser
        .isSet(QLatin1String(CommandLineOptions::NoForceInstallation)));
    QInstaller::PackageManagerCore::setCreateLocalRepositoryFromBinary(parser
        .isSet(QLatin1String(CommandLineOptions::CreateLocalRepository))
        || m_core->settings().createLocalRepository());

    QHash<QString, QString> params;
    const QStringList positionalArguments = parser.positionalArguments();
    foreach (const QString &argument, positionalArguments) {
        if (argument.contains(QLatin1Char('='))) {
            const QString name = argument.section(QLatin1Char('='), 0, 0);
            const QString value = argument.section(QLatin1Char('='), 1, 1);
            params.insert(name, value);
            m_core->setValue(name, value);
        }
    }

    const QString directory = QLatin1String(":/translations");
    const QStringList translations = m_core->settings().translations();

    // install the default Qt translator
    QScopedPointer<QTranslator> translator(new QTranslator(QCoreApplication::instance()));
    foreach (const QLocale locale, QLocale().uiLanguages()) {
        // As there is no qt_en.qm, we simply end the search when the next
        // preferred language is English.
        if (locale.language() == QLocale::English)
            break;
        if (translator->load(locale, QLatin1String("qt"), QString::fromLatin1("_"), directory)) {
            QCoreApplication::instance()->installTranslator(translator.take());
            break;
        }
    }

    translator.reset(new QTranslator(QCoreApplication::instance()));
    // install English translation as fallback so that correct license button text is used
    if (translator->load(QLatin1String("en"), directory))
        QCoreApplication::instance()->installTranslator(translator.take());

    if (translations.isEmpty()) {
        translator.reset(new QTranslator(QCoreApplication::instance()));
        foreach (const QLocale locale, QLocale().uiLanguages()) {
            if (translator->load(locale, QLatin1String(""), QLatin1String(""), directory)) {
                QCoreApplication::instance()->installTranslator(translator.take());
                break;
            }
        }
    } else {
        foreach (const QString &translation, translations) {
            translator.reset(new QTranslator(QCoreApplication::instance()));
            if (translator->load(translation, QLatin1String(":/translations")))
                QCoreApplication::instance()->installTranslator(translator.take());
        }
    }

    //create the wizard GUI
    TabController controller(0);
    controller.setManager(m_core);
    controller.setManagerParams(params);
    controller.setControlScript(controlScript);

    if (m_core->isInstaller())
        controller.setGui(new InstallerGui(m_core));
    else
        controller.setGui(new MaintenanceGui(m_core));

    QInstaller::PackageManagerCore::Status status =
        QInstaller::PackageManagerCore::Status(controller.init());
    if (status != QInstaller::PackageManagerCore::Success)
        return status;

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


// -- private

void InstallerBase::dumpResourceTree() const
{
    qDebug() << "Resource tree:";
    QDirIterator it(QLatin1String(":/"), QDir::NoDotAndDotDot | QDir::AllEntries | QDir::Hidden,
        QDirIterator::Subdirectories);
    while (it.hasNext()) {
        if (it.next().startsWith(QLatin1String(":/qt-project.org")))
            continue;
        qDebug() << "    " << it.filePath().toUtf8().constData();
    }
}

QStringList InstallerBase::repositories(const QString &list) const
{
    const QStringList items = list.split(QLatin1Char(','), QString::SkipEmptyParts);
    foreach (const QString &item, items)
        qDebug() << "Adding custom repository:" << item.toUtf8().constData();
    return items;
}
