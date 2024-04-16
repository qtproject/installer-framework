/**************************************************************************
**
** Copyright (C) 2024 The Qt Company Ltd.
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

#ifndef SDKAPP_H
#define SDKAPP_H

#include "commandlineparser.h"

#include <binarycontent.h>
#include <binaryformat.h>
#include <fileio.h>
#include <fileutils.h>
#include <constants.h>
#include <packagemanagercore.h>
#include <settings.h>
#include <productkeycheck.h>
#include <binaryformatenginehandler.h>
#include <filedownloaderfactory.h>
#include <packagemanagerproxyfactory.h>
#include <utils.h>
#include <runoncechecker.h>
#include <globals.h>
#include <errors.h>
#include <loggingutils.h>
#include <scriptengine.h>

#include <QApplication>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QResource>
#include <QLoggingCategory>
#include <QUuid>
#include <QMessageBox>
#include <QMetaEnum>
#include <QTranslator>

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QNetworkInformation>
#endif

template<class T>
class SDKApp : public T
{
public:
    SDKApp(int& argc, char** argv)
        : T(argc, argv)
        , m_runCheck(QDir::tempPath() + QLatin1Char('/')
            + qApp->applicationName() + QLatin1String("1234865.lock"))
        , m_core(nullptr)
    {
        m_parser.parse(QCoreApplication::arguments());
#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
        QNetworkInformation::loadDefaultBackend();
#endif
    }

    virtual ~SDKApp()
    {
        foreach (const QByteArray &ba, m_resourceMappings)
            QResource::unregisterResource((const uchar*) ba.data(), QLatin1String(":/metadata"));
    }

    bool notify(QObject *receiver, QEvent *event)
    {
        try {
            return T::notify(receiver, event);
        } catch (QInstaller::Error &e) {
            qFatal("Exception thrown: %s", qPrintable(e.message()));
        } catch (std::exception &e) {
            qFatal("Exception thrown: %s", e.what());
        } catch (...) {
            qFatal("Unknown exception caught.");
        }
        return false;
    }

    bool init(QString &errorMessage) {
        QString appname = qApp->applicationName();

        QFile binary(binaryFile());
    #ifdef Q_OS_WIN
        // On some admin user installations it is possible that the installer.dat
        // file is left without reading permissions for non-administrator users,
        // we should check this and prompt the user to run the executable as admin if needed.
        if (!binary.open(QIODevice::ReadOnly)) {
            QFileInfo binaryInfo(binary.fileName());
            errorMessage = QObject::tr("Please make sure that the current user has read access "
                "to file \"%1\" or try running %2 as an administrator.").arg(binaryInfo.fileName(), qAppName());
            return false;
        }
        binary.close();
    #endif
        QString datFileName = datFile(binaryFile());
        quint64 cookie = datFileName.isEmpty() ? QInstaller::BinaryContent::MagicCookie : QInstaller::BinaryContent::MagicCookieDat;
        binary.setFileName(!datFileName.isEmpty() ? datFileName : binaryFile());
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

        bool isCommandLineInterface = false;
        foreach (const QString &option, CommandLineOptions::scCommandLineInterfaceOptions) {
           if (m_parser.positionalArguments().contains(option)) {
               isCommandLineInterface = true;
               break;
           }
        }
        if (m_parser.isSet(CommandLineOptions::scDeprecatedCheckUpdates))
            isCommandLineInterface = true;

        QString loggingRules;
        if (m_parser.isSet(CommandLineOptions::scLoggingRulesLong)) {
            loggingRules = QLatin1String("ifw.* = false\n");
            loggingRules += m_parser.value(CommandLineOptions::scLoggingRulesLong)
                          .split(QLatin1Char(','), Qt::SkipEmptyParts)
                          .join(QLatin1Char('\n')); // take rules from command line
        } else if (isCommandLineInterface) {
            loggingRules = QLatin1String("ifw.* = false\n"
                                        "ifw.installer.* = true\n"
                                        "ifw.server = true\n"
                                        "ifw.progress.indicator = true\n");
        } else {
            // enable all except detailed package information and developer specific logging
            loggingRules = QLatin1String("ifw.* = true\n"
                                        "ifw.developer.build = false\n");
        }

        if (QInstaller::LoggingHandler::instance().verboseLevel() == QInstaller::LoggingHandler::Detailed) {
            loggingRules += QLatin1String("\nifw.developer.build = true\n");
        }
        QLoggingCategory::setFilterRules(loggingRules);
        qCDebug(QInstaller::lcInstallerInstallLog).noquote() << "Arguments:" <<
                m_parser.arguments().join(QLatin1String(", "));

        for (auto &optionName : m_parser.optionNames()) {
            if (isCommandLineInterface)
                break;

            if (m_parser.optionContextFlags(optionName) & CommandLineParser::CommandLineOnly) {
                qCWarning(QInstaller::lcInstallerInstallLog).nospace() << "Found command line only option "
                    << optionName << ". This will not have any effect when running in graphical mode.";
            }
        }

        dumpResourceTree();

        SDKApp::registerMetaResources(manager.collectionByName("QResources"));
        QInstaller::BinaryFormatEngineHandler::instance()->registerResources(manager.collections());

        const QHash<QString, QString> userArgs = userArguments();
        if (m_parser.isSet(CommandLineOptions::scStartClientLong)) {
            const QStringList arguments = m_parser.value(CommandLineOptions::scStartClientLong)
                .split(QLatin1Char(','), Qt::SkipEmptyParts);
            m_core = new QInstaller::PackageManagerCore(
                magicMarker, oldOperations, datFileName,
                arguments.value(0, QLatin1String(QInstaller::Protocol::DefaultSocket)),
                arguments.value(1, QLatin1String(QInstaller::Protocol::DefaultAuthorizationKey)),
                QInstaller::Protocol::Mode::Debug, userArgs, isCommandLineInterface);
        } else {
            m_core = new QInstaller::PackageManagerCore(magicMarker, oldOperations, datFileName,
                QUuid::createUuid().toString(), QUuid::createUuid().toString(),
                QInstaller::Protocol::Mode::Production, userArgs, isCommandLineInterface);
        }

        QLocale lang = QLocale::English;
#ifndef IFW_DISABLE_TRANSLATIONS
        if (!isCommandLineInterface) {
            const QString directory = QLatin1String(":/translations");
            // Check if there is a modified translation first to enable people
            // to easily provide corrected translations to Qt/IFW for their installers
            const QString newDirectory = QLatin1String(":/translations_new");
            const QStringList translations = m_core->settings().translations();

            if (translations.isEmpty()) {
                for (const QString &language : QLocale().uiLanguages()) {
                    const QLocale locale(language);
                    std::unique_ptr<QTranslator> qtTranslator(new QTranslator(QCoreApplication::instance()));
                    bool qtLoaded = qtTranslator->load(locale, QLatin1String("qt"),
                                                      QLatin1String("_"), newDirectory);
                    if (!qtLoaded)
                        qtLoaded = qtTranslator->load(locale, QLatin1String("qt"),
                                                      QLatin1String("_"), directory);

                    if (qtLoaded || locale.language() == QLocale::English) {
                        if (qtLoaded)
                            QCoreApplication::instance()->installTranslator(qtTranslator.release());

                        std::unique_ptr <QTranslator> ifwTranslator(new QTranslator(QCoreApplication::instance()));
                        bool ifwLoaded = ifwTranslator->load(locale, QLatin1String("ifw"), QLatin1String("_"), newDirectory);
                        if (!ifwLoaded)
                            ifwLoaded = ifwTranslator->load(locale, QLatin1String("ifw"), QLatin1String("_"), directory);
                        if (ifwLoaded) {
                            QCoreApplication::instance()->installTranslator(ifwTranslator.release());
                        } else {
                            qCWarning(QInstaller::lcDeveloperBuild) << "Could not load IFW translation for language"
                                << QLocale::languageToString(locale.language());
                        }

                        // To stop loading other translations it's sufficient that
                        // qt was loaded successfully or we hit English as system language
                        lang = locale;
                        break;
                    } else {
                        qCWarning(QInstaller::lcDeveloperBuild) << "Could not load Qt translation for language"
                            << QLocale::languageToString(locale.language());
                    }
                }
            } else {
                foreach (const QString &translation, translations) {
                    std::unique_ptr<QTranslator> translator(new QTranslator(QCoreApplication::instance()));
                    if (translator->load(translation, QLatin1String(":/translations")))
                        QCoreApplication::instance()->installTranslator(translator.release());
                }
                QLocale currentLocale(translations.at(0).section(QLatin1Char('_'), 1));
                lang = currentLocale;
            }
        }
#endif

        if (m_runCheck.isRunning(RunOnceChecker::ConditionFlag::Lockfile)) {
            // It is possible to install an application and thus the maintenance tool into a
            // directory that requires elevated permission to create a lock file. Since this
            // cannot be done without requesting credentials from the user, we silently ignore
            // the fact that we could not create the lock file and check the running processes.
            if (m_runCheck.isRunning(RunOnceChecker::ConditionFlag::ProcessList)) {
                errorMessage = QObject::tr("Another %1 instance is already running. Wait "
                        "until it finishes, close it, or restart your system.").arg(qAppName());
                return false;
            }
        }

        // From Qt5.8 onwards system proxy is used by default. If Qt is built with
        // QT_USE_SYSTEM_PROXIES false then system proxies are not used by default.
        if (m_parser.isSet(CommandLineOptions::scNoProxyLong)) {
            m_core->settings().setProxyType(QInstaller::Settings::NoProxy);
            KDUpdater::FileDownloaderFactory::instance().setProxyFactory(m_core->proxyFactory());
        } else if (QNetworkProxyFactory::usesSystemConfiguration()) {
            m_core->settings().setProxyType(QInstaller::Settings::SystemProxy);
            KDUpdater::FileDownloaderFactory::instance().setProxyFactory(m_core->proxyFactory());
        }

        if (m_parser.isSet(CommandLineOptions::scLocalCachePathLong)) {
            const QString cachePath = m_parser.value(CommandLineOptions::scLocalCachePathLong);
            if (cachePath.isEmpty()) {
                errorMessage = QObject::tr("Empty value for option 'cache-path'.");
                return false;
            }
            m_core->settings().setLocalCachePath(cachePath);
        }
        m_core->resetLocalCache(true);

        if (m_parser.isSet(CommandLineOptions::scShowVirtualComponentsLong))
            QInstaller::PackageManagerCore::setVirtualComponentsVisible(true);

        // IFW 3.x.x style --updater option support provided for backward compatibility
        if (m_parser.isSet(CommandLineOptions::scStartUpdaterLong)
                || m_parser.isSet(CommandLineOptions::scDeprecatedUpdater)) {
            if (m_core->isInstaller()) {
                errorMessage = QObject::tr("Cannot start installer binary as updater.");
                return false;
            }
            m_core->setUserSetBinaryMarker(QInstaller::BinaryContent::MagicUpdaterMarker);
        }

        if (m_parser.isSet(CommandLineOptions::scStartPackageManagerLong)) {
            if (m_core->isInstaller()) {
                errorMessage = QObject::tr("Cannot start installer binary as package manager.");
                return false;
            }
            m_core->setUserSetBinaryMarker(QInstaller::BinaryContent::MagicPackageManagerMarker);
        }

        if (m_parser.isSet(CommandLineOptions::scStartUninstallerLong)) {
            if (m_core->isInstaller()) {
                errorMessage = QObject::tr("Cannot start installer binary as uninstaller.");
                return false;
            }
            m_core->setUserSetBinaryMarker(QInstaller::BinaryContent::MagicUninstallerMarker);
        }

        if (m_parser.isSet(CommandLineOptions::scAddRepositoryLong)) {
            const QStringList repoList = repositories(m_parser.value(CommandLineOptions::scAddRepositoryLong));
            if (repoList.isEmpty()) {
                errorMessage = QObject::tr("Empty repository list for option 'addRepository'.");
                return false;
            }
            m_core->addUserRepositories(repoList);
        }

        if (m_parser.isSet(CommandLineOptions::scAddTmpRepositoryLong)) {
            const QStringList repoList = repositories(m_parser.value(CommandLineOptions::scAddTmpRepositoryLong));
            if (repoList.isEmpty()) {
                errorMessage = QObject::tr("Empty repository list for option 'addTempRepository'.");
                return false;
            }
            m_core->setTemporaryRepositories(repoList, false);
        }

        if (m_parser.isSet(CommandLineOptions::scSetTmpRepositoryLong)) {
            const QStringList repoList = repositories(m_parser.value(CommandLineOptions::scSetTmpRepositoryLong));
            if (repoList.isEmpty()) {
                errorMessage = QObject::tr("Empty repository list for option 'setTempRepository'.");
                return false;
            }
            m_core->setTemporaryRepositories(repoList, true);
        }

        if (m_parser.isSet(CommandLineOptions::scInstallCompressedRepositoryLong)) {
            const QStringList repoList = repositories(m_parser.value(CommandLineOptions::scInstallCompressedRepositoryLong));
            if (repoList.isEmpty()) {
                errorMessage = QObject::tr("Empty repository list for option 'installCompressedRepository'.");
                return false;
            }
            foreach (QString repository, repoList) {
                if (!QFileInfo::exists(repository)) {
                    errorMessage = QObject::tr("The file %1 does not exist.").arg(repository);
                    return false;
                }
            }
            m_core->setTemporaryRepositories(repoList, false, true);
        }
        // Disable checking for free space on target
        if (m_parser.isSet(CommandLineOptions::scNoSizeCheckingLong))
            m_core->setCheckAvailableSpace(false);

        QInstaller::PackageManagerCore::setNoForceInstallation(m_parser
            .isSet(CommandLineOptions::scNoForceInstallationLong));
        QInstaller::PackageManagerCore::setNoDefaultInstallation(m_parser
            .isSet(CommandLineOptions::scNoDefaultInstallationLong));
        QInstaller::PackageManagerCore::setCreateLocalRepositoryFromBinary(m_parser
            .isSet(CommandLineOptions::scCreateLocalRepositoryLong)
            || m_core->settings().createLocalRepository());

        if (m_parser.isSet(CommandLineOptions::scMaxConcurrentOperationsLong)) {
            bool isValid;
            const int count = m_parser.value(CommandLineOptions::scMaxConcurrentOperationsLong).toInt(&isValid);
            if (!isValid) {
                errorMessage = QObject::tr("Invalid value for 'max-concurrent-operations'.");
                return false;
            }
            QInstaller::PackageManagerCore::setMaxConcurrentOperations(count);
        }

        if (m_parser.isSet(CommandLineOptions::scAcceptLicensesLong))
            m_core->setAutoAcceptLicenses();

        if (m_parser.isSet(CommandLineOptions::scConfirmCommandLong))
            m_core->setAutoConfirmCommand();

        // Ignore message acceptance options when running the installer with GUI
        if (m_core->isCommandLineInstance()) {
            if (m_parser.isSet(CommandLineOptions::scAcceptMessageQueryLong))
                m_core->autoAcceptMessageBoxes();

            if (m_parser.isSet(CommandLineOptions::scRejectMessageQueryLong))
                m_core->autoRejectMessageBoxes();

            if (m_parser.isSet(CommandLineOptions::scMessageDefaultAnswerLong))
                m_core->acceptMessageBoxDefaultButton();

            if (m_parser.isSet(CommandLineOptions::scMessageAutomaticAnswerLong)) {
                const QString positionalArguments = m_parser.value(CommandLineOptions::scMessageAutomaticAnswerLong);
                const QStringList items = positionalArguments.split(QLatin1Char(','), Qt::SkipEmptyParts);
                if (items.count() > 0) {
                    errorMessage = setMessageBoxAutomaticAnswers(items);
                    if (!errorMessage.isEmpty())
                        return false;
                } else {
                    errorMessage = QObject::tr("Arguments missing for option %1")
                            .arg(CommandLineOptions::scMessageAutomaticAnswerLong);
                    return false;
                }
            }
            if (m_parser.isSet(CommandLineOptions::scFileDialogAutomaticAnswer)) {
                const QString positionalArguments = m_parser.value(CommandLineOptions::scFileDialogAutomaticAnswer);
                const QStringList items = positionalArguments.split(QLatin1Char(','), Qt::SkipEmptyParts);

                foreach (const QString &item, items) {
                    if (item.contains(QLatin1Char('='))) {
                        const QString name = item.section(QLatin1Char('='), 0, 0);
                        QString value = item.section(QLatin1Char('='), 1, 1);
                        m_core->setFileDialogAutomaticAnswer(name, value);
                    }
                }
            }
        }

        try {
            ProductKeyCheck::instance()->init(m_core);
        } catch (const QInstaller::Error &e) {
            errorMessage = e.message();
            return false;
        }

        m_core->setValue(QInstaller::scUILanguage, lang.name());
        emit m_core->defaultTranslationsLoadedForLanguage(lang);
        ProductKeyCheck::instance()->addPackagesFromXml(QLatin1String(":/metadata/Updates.xml"));

        return true;
    }

    /*!
        Returns the installer binary or installer.dat. In case of an installer this will be the
        installer binary itself, which contains the binary layout and the binary content. In case
        of an maintenance tool, it will return a .dat file that has just a binary layout
        as the binary layout cannot be appended to the actual maintenance tool binary
        itself because of signing.

        On macOS: This function will return always the .dat file
        .dat file is located inside the resource folder in the application
        bundle in macOS.
    */
    QString binaryFile() const
    {
        QString binaryFile = QCoreApplication::applicationFilePath();

        // The installer binary on macOS and Windows does not contain the binary
        // content, it's put into the resources folder as separate file.
        // Adjust the actual binary path. No error checking here since we
        // will fail later while reading the binary content.
        QDir resourcePath(QFileInfo(binaryFile).dir());

#ifdef Q_OS_MACOS
        resourcePath.cdUp();
        resourcePath.cd(QLatin1String("Resources"));
#endif
        QString datFilePath = resourcePath.filePath(QLatin1String("installer.dat"));
        if (QFileInfo::exists(datFilePath))
            return datFilePath;
        return binaryFile;
    }

    /*!
        Returns the corresponding .dat file for a given installer / maintenance tool binary or an
        empty string if it fails to find one.
    */
    QString datFile(const QString &binaryFile) const
    {
        QFile file(binaryFile);
        QInstaller::openForRead(&file);
        const quint64 cookiePos = QInstaller::BinaryContent::findMagicCookie(&file,
            QInstaller::BinaryContent::MagicCookie);
        if (!file.seek(cookiePos - sizeof(qint64)))    // seek to read the marker
            return QString(); // ignore error, we will fail later

        const qint64 magicMarker = QInstaller::retrieveInt64(&file);
        if (magicMarker == QInstaller::BinaryContent::MagicUninstallerMarker) {
            QFileInfo fi(binaryFile);
            QString bundlePath;
            QString datFileName;
            if (QInstaller::isInBundle(fi.absoluteFilePath(), &bundlePath))
                fi.setFile(bundlePath);
#ifdef Q_OS_MACOS
                datFileName = fi.absoluteDir().filePath(fi.baseName() + QLatin1String(".dat"));
#else
                datFileName = fi.absoluteDir().filePath(qApp->applicationName() + QLatin1String(".dat"));
#endif
            // When running maintenance tool, datFile name should be the same as the application name.
            // In case we have updated maintenance tool in previous maintenance tool run, the datFile
            // name may not match if the maintenance tool name has changed. In that case try to
            // look for the dat file from the root folder of the install.
            if (!QFileInfo::exists(datFileName)) {
                QFileInfo fi(datFileName);
                QDirIterator it(fi.absolutePath(),
                        QStringList() << QLatin1String("*.dat"),
                        QDir::Files | QDir::NoSymLinks | QDir::NoDotAndDotDot);
                while (it.hasNext()) {
                    try {
                        QFile f(it.next());
                        f.open(QIODevice::ReadOnly);
                        if (f.fileName().endsWith(QLatin1String("installer.dat")))
                             continue;
                        QInstaller::BinaryContent::findMagicCookie(&f, magicMarker);
                        datFileName = f.fileName();
                        break;
                    } catch (const QInstaller::Error &error) {
                        Q_UNUSED(error)
                        continue;
                    }
                }
            }
            return datFileName;
        }
        return QString();
    }

    void registerMetaResources(const QInstaller::ResourceCollection &collection)
    {
        foreach (const QSharedPointer<QInstaller::Resource> &resource, collection.resources()) {
            const bool isOpen = resource->isOpen();
            if ((!isOpen) && (!resource->open()))
                continue;

            if (!resource->seek(0))
                continue;

            const QByteArray ba = resource->readAll();
            if (ba.isEmpty())
                continue;

            if (QResource::registerResource((const uchar*) ba.data(), QLatin1String(":/metadata")))
                m_resourceMappings.append(ba);

            if (!isOpen) // If we reach that point, either the resource was opened already...
                resource->close();           // or we did open it and have to close it again.
        }
    }

    QStringList repositories(const QString &list) const
    {
        const QStringList items = list.split(QLatin1Char(','), Qt::SkipEmptyParts);
        foreach (const QString &item, items)
            qCDebug(QInstaller::lcInstallerInstallLog) << "Adding custom repository:" << item;
        return items;
    }

    QHash<QString, QString> userArguments()
    {
        QHash<QString, QString> params;
        const QStringList positionalArguments = m_parser.positionalArguments();
        foreach (const QString &argument, positionalArguments) {
            if (argument.contains(QLatin1Char('='))) {
                const QString name = argument.section(QLatin1Char('='), 0, 0);
                const QString value = argument.section(QLatin1Char('='), 1);
                params.insert(name, value);
            }
        }
        return params;
    }

    QString setMessageBoxAutomaticAnswers(const QStringList &items)
    {
        foreach (const QString &item, items) {
            if (item.contains(QLatin1Char('='))) {
                const QMetaObject metaObject = QMessageBox::staticMetaObject;
                int enumIndex = metaObject.indexOfEnumerator("StandardButton");
                if (enumIndex != -1) {
                    QMetaEnum en = metaObject.enumerator(enumIndex);
                    const QString name = item.section(QLatin1Char('='), 0, 0);
                    QString value = item.section(QLatin1Char('='), 1, 1);
                    value.prepend(QLatin1String("QMessageBox::"));
                    bool ok = false;
                    int buttonValue = en.keyToValue(value.toLocal8Bit().data(), &ok);
                    if (ok)
                        m_core->setMessageBoxAutomaticAnswer(name, buttonValue);
                    else
                        return QObject::tr("Invalid button value %1 ").arg(value);
                }
            } else {
                return QObject::tr("Incorrect arguments for %1")
                        .arg(CommandLineOptions::scMessageAutomaticAnswerLong);
            }
        }
        return QString();
    }
    void dumpResourceTree() const
    {
        qCDebug(QInstaller::lcDeveloperBuild) << "Resource tree:";
        QDirIterator it(QLatin1String(":/"), QDir::NoDotAndDotDot | QDir::AllEntries | QDir::Hidden,
            QDirIterator::Subdirectories);
        while (it.hasNext()) {
            if (it.next().startsWith(QLatin1String(":/qt-project.org")))
                continue;
            qCDebug(QInstaller::lcDeveloperBuild) << "    " << it.filePath().toUtf8().constData();
        }
    }

    QString controlScript()
    {
        QString controlScript = QString();
        if (m_parser.isSet(CommandLineOptions::scScriptLong)) {
            controlScript = m_parser.value(CommandLineOptions::scScriptLong);
            if (!QFileInfo::exists(controlScript))
                qCDebug(QInstaller::lcInstallerInstallLog) << "Script file does not exist.";

        } else if (!m_core->settings().controlScript().isEmpty()) {
            controlScript = QLatin1String(":/metadata/installer-config/")
                + m_core->settings().controlScript();
        }
        return controlScript;
    }

private:
    QList<QByteArray> m_resourceMappings;

public:
    RunOnceChecker m_runCheck;
    QInstaller::PackageManagerCore *m_core;
    CommandLineParser m_parser;
};

#endif  // SDKAPP_H
