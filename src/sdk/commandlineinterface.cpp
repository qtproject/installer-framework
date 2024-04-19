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

#include "commandlineinterface.h"

#include <component.h>
#include <init.h>
#include <packagemanagercore.h>
#include <globals.h>
#include <productkeycheck.h>
#include <errors.h>
#include <loggingutils.h>

#include <QDir>

CommandLineInterface::CommandLineInterface(int &argc, char *argv[])
    : SDKApp<QCoreApplication>(argc, argv)
{
    QInstaller::init(); // register custom operations
    m_parser.parse(arguments());
}

CommandLineInterface::~CommandLineInterface()
{
    delete m_core;
}

bool CommandLineInterface::initialize()
{
    QString errorMessage;
    if (!init(errorMessage)) {
        qCWarning(QInstaller::lcInstallerInstallLog) << errorMessage;
        return false;
    }
    if (m_core->settings().disableCommandLineInterface()) {
        qCWarning(QInstaller::lcInstallerInstallLog)
                << "Command line interface support disabled from installer configuration by vendor!";
        return false;
    }
    // Filter the arguments list by removing any key=value pair occurrences.
    QString command;
    m_positionalArguments = m_parser.positionalArguments();
    foreach (const QString &argument, m_positionalArguments) {
        if (argument.contains(QLatin1Char('=')))
            m_positionalArguments.removeOne(argument);
    }
    if (m_positionalArguments.isEmpty()) {
        // Special case, normally positional arguments should contain
        // at least the command invoked.
        qCDebug(QInstaller::lcInstallerInstallLog)
                << "Command line interface initialized but no command argument found.";
    } else {
        // Sanity and order of arguments already checked in main(), we should be
        // quite safe to assume that command is the first positional argument.
        command = m_positionalArguments.first();
        m_positionalArguments.removeFirst();
    }
    m_core->saveGivenArguments(QStringList() << command << m_parser.optionNames());
    QString ctrlScript = controlScript();
    if (!ctrlScript.isEmpty()) {
        m_core->controlScriptEngine()->loadInContext(
                QLatin1String("Controller"), ctrlScript);
        qCDebug(QInstaller::lcInstallerInstallLog) << "Loaded control script" << ctrlScript;
    }
    return true;
}

int CommandLineInterface::checkUpdates()
{
    if (!initialize())
        return EXIT_FAILURE;
    if (m_core->isInstaller()) {
        qCWarning(QInstaller::lcInstallerInstallLog) << "Cannot check updates with installer.";
        return EXIT_FAILURE;
    }
    try {
        if (m_core->searchAvailableUpdates() != QInstaller::PackageManagerCore::Success) {
            return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
    } catch (const QInstaller::Error &err) {
        qCCritical(QInstaller::lcInstallerInstallLog) << err.message();
        return EXIT_FAILURE;
    }
}

int CommandLineInterface::listInstalledPackages()
{
    if (!initialize())
        return EXIT_FAILURE;
    if (m_core->isInstaller()) {
        qCWarning(QInstaller::lcInstallerInstallLog) << "Cannot list installed packages with installer.";
        return EXIT_FAILURE;
    }
    m_core->setPackageManager();
    QString regexp;
    if (!m_positionalArguments.isEmpty())
        regexp = m_positionalArguments.first();
    m_core->listInstalledPackages(regexp);
    return EXIT_SUCCESS;
}

int CommandLineInterface::searchAvailablePackages()
{
    if (!initialize())
        return EXIT_FAILURE;
    if (!m_core->isInstaller())
        m_core->setPackageManager();
    if (!checkLicense())
        return EXIT_FAILURE;
    QString regexp;
    if (!m_positionalArguments.isEmpty())
        regexp = m_positionalArguments.first();

    if (m_parser.isSet(CommandLineOptions::scTypeLong)) {
        // If type is specified, only list relevant contents
        if (m_parser.value(CommandLineOptions::scTypeLong) == QLatin1String("package"))
            m_core->listAvailablePackages(regexp, parsePackageFilters());
        else if (m_parser.value(CommandLineOptions::scTypeLong) == QLatin1String("alias"))
            m_core->listAvailableAliases(regexp);
    } else {
         // No type - we can try again with packages search if there were no matching aliases
        if (!m_core->listAvailableAliases(regexp))
            m_core->listAvailablePackages(regexp, parsePackageFilters());
    }

    return EXIT_SUCCESS;
}

int CommandLineInterface::updatePackages()
{
    if (!initialize())
        return EXIT_FAILURE;
    if (m_core->isInstaller()) {
        qCWarning(QInstaller::lcInstallerInstallLog) << "Cannot update packages with installer.";
        return EXIT_FAILURE;
    }
    if (!checkLicense())
        return EXIT_FAILURE;
    try {
        return m_core->updateComponentsSilently(m_positionalArguments);
    } catch (const QInstaller::Error &err) {
        qCCritical(QInstaller::lcInstallerInstallLog) << err.message();
        return EXIT_FAILURE;
    }
}

int CommandLineInterface::installPackages()
{
    if (!initialize() || (m_core->isInstaller() && !setTargetDir()) || !checkLicense())
        return EXIT_FAILURE;
    try {
        if (m_positionalArguments.isEmpty()) {
            if (!m_core->isInstaller()) {
                qCWarning(QInstaller::lcInstallerInstallLog)
                    << "Cannot perform default installation with maintenance tool.";
                return EXIT_FAILURE;
            }
            // No packages provided, install default components
            return m_core->installDefaultComponentsSilently();
        }
        // Normal installation
        return m_core->installSelectedComponentsSilently(m_positionalArguments);
    } catch (const QInstaller::Error &err) {
        qCCritical(QInstaller::lcInstallerInstallLog) << err.message();
        return EXIT_FAILURE;
    }
}

int CommandLineInterface::uninstallPackages()
{
    if (!initialize())
        return EXIT_FAILURE;
    if (m_core->isInstaller()) {
        qCWarning(QInstaller::lcInstallerInstallLog) << "Cannot uninstall packages with installer.";
        return EXIT_FAILURE;
    }
    m_core->setPackageManager();
    try {
        return m_core->uninstallComponentsSilently(m_positionalArguments);
    } catch (const QInstaller::Error &err) {
        qCCritical(QInstaller::lcInstallerInstallLog) << err.message();
        return EXIT_FAILURE;
    }
}

int CommandLineInterface::removeInstallation()
{
    if (!initialize())
        return EXIT_FAILURE;
    if (m_core->isInstaller()) {
        qCWarning(QInstaller::lcInstallerInstallLog) << "Cannot uninstall packages with installer.";
        return EXIT_FAILURE;
    }
    m_core->setUninstaller();
    try {
        return m_core->removeInstallationSilently();
    } catch (const QInstaller::Error &err) {
        qCCritical(QInstaller::lcInstallerInstallLog) << err.message();
        return EXIT_FAILURE;
    }
}

int CommandLineInterface::createOfflineInstaller()
{
    if (!initialize())
        return EXIT_FAILURE;
    if (!m_core->isInstaller() || m_core->isOfflineOnly()) {
        qCWarning(QInstaller::lcInstallerInstallLog)
            << "Offline installer can only be created with an online installer.";
        return EXIT_FAILURE;
    }
    if (m_positionalArguments.isEmpty()) {
        qCWarning(QInstaller::lcInstallerInstallLog) << "Missing components argument.";
        return EXIT_FAILURE;
    }
    if (!(setTargetDir() && checkLicense()))
        return EXIT_FAILURE;

    if (m_parser.isSet(CommandLineOptions::scOfflineInstallerNameLong)) {
        m_core->setOfflineBinaryName(m_parser.value(CommandLineOptions::scOfflineInstallerNameLong));
    } else {
        const QString offlineName = m_core->offlineBinaryName();
        qCDebug(QInstaller::lcInstallerInstallLog) << "No filename specified for "
            "the generated installer, using default name:" << offlineName;
    }
    try {
        return m_core->createOfflineInstaller(m_positionalArguments);
    } catch (const QInstaller::Error &err) {
        qCCritical(QInstaller::lcInstallerInstallLog) << err.message();
        return EXIT_FAILURE;
    }
}

int CommandLineInterface::clearLocalCache()
{
    if (!initialize())
        return EXIT_FAILURE;

    if (!m_core->clearLocalCache())
        return EXIT_FAILURE;

    qCDebug(QInstaller::lcInstallerInstallLog) << "Cache cleared successfully!";
    return EXIT_SUCCESS;
}

bool CommandLineInterface::checkLicense()
{
    const ProductKeyCheck *const productKeyCheck = ProductKeyCheck::instance();
    if (!productKeyCheck->hasValidLicense()) {
        qCWarning(QInstaller::lcInstallerInstallLog) << "No valid license found.";
        return false;
    }
    return true;
}

bool CommandLineInterface::setTargetDir()
{
    QString targetDir;
    if (m_parser.isSet(CommandLineOptions::scRootLong)) {
        targetDir = m_parser.value(CommandLineOptions::scRootLong);
    } else {
        targetDir = m_core->value(QInstaller::scTargetDir);
        qCDebug(QInstaller::lcInstallerInstallLog) << "No target directory specified, using default value:" << targetDir;
    }
    if (m_core->installationAllowedToDirectory(targetDir)) {
        QString targetDirWarning = m_core->targetDirWarning(targetDir);
        if (!targetDirWarning.isEmpty()) {
            qCWarning(QInstaller::lcInstallerInstallLog) << m_core->targetDirWarning(targetDir);
        } else {
            m_core->setValue(QInstaller::scTargetDir, targetDir);
            return true;
        }
    }
    return false;
}

QHash<QString, QString> CommandLineInterface::parsePackageFilters()
{
    QHash<QString, QString> filterHash;
    if (m_parser.isSet(CommandLineOptions::scFilterPackagesLong)) {
        const QStringList filterList = m_parser.value(CommandLineOptions::scFilterPackagesLong)
            .split(QLatin1Char(','));

        for (auto &filter : filterList) {
            const int i = filter.indexOf(QLatin1Char('='));
            const QString element = filter.left(i).trimmed();
            const QString value = filter.mid(i + 1).trimmed();

            if ((i == -1) || (filter.count(QLatin1Char('=')) > 1)
                    || element.isEmpty() || value.isEmpty()) {
                qCWarning(QInstaller::lcInstallerInstallLog).nospace() << "Ignoring unknown entry "
                    << filter << "in package filter arguments. Please use syntax \"element=regex,...\".";
                continue;
            }
            filterHash.insert(element, value);
        }
    }
    return filterHash;
}
