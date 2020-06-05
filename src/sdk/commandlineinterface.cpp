/**************************************************************************
**
** Copyright (C) 2020 The Qt Company Ltd.
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

#include <QDir>
#include <QDomDocument>

#include <iostream>

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
        m_positionalArguments.removeFirst();
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
    m_core->setUpdater();
    if (!m_core->fetchRemotePackagesTree()) {
        qCWarning(QInstaller::lcInstallerInstallLog) << m_core->error();
        return EXIT_FAILURE;
    }

    const QList<QInstaller::Component *> components =
        m_core->components(QInstaller::PackageManagerCore::ComponentType::Root);
    if (components.isEmpty()) {
        qCWarning(QInstaller::lcInstallerInstallLog) << "There are currently no updates available.";
        return EXIT_SUCCESS;
    }

    QDomDocument doc;
    QDomElement root = doc.createElement(QLatin1String("updates"));
    doc.appendChild(root);

    foreach (QInstaller::Component *component, components) {
        QDomElement update = doc.createElement(QLatin1String("update"));
        update.setAttribute(QLatin1String("name"), component->value(QInstaller::scDisplayName));
        update.setAttribute(QLatin1String("version"), component->value(QInstaller::scVersion));
        update.setAttribute(QLatin1String("size"), component->value(QInstaller::scUncompressedSize));
        root.appendChild(update);
    }

    std::cout << qPrintable(doc.toString(4)) << std::endl;
    return EXIT_SUCCESS;
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
    m_core->listInstalledPackages();
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
    m_core->listAvailablePackages(regexp);
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
        return m_core->updateComponentsSilently(m_positionalArguments)
            ? EXIT_SUCCESS : EXIT_FAILURE;
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
            return m_core->installDefaultComponentsSilently()
                ? EXIT_SUCCESS : EXIT_FAILURE;
        }
        // Normal installation
        return m_core->installSelectedComponentsSilently(m_positionalArguments)
            ? EXIT_SUCCESS : EXIT_FAILURE;
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
        return m_core->uninstallComponentsSilently(m_positionalArguments)
            ? EXIT_SUCCESS : EXIT_FAILURE;
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
        return m_core->removeInstallationSilently() ? EXIT_SUCCESS : EXIT_FAILURE;
    } catch (const QInstaller::Error &err) {
        qCCritical(QInstaller::lcInstallerInstallLog) << err.message();
        return EXIT_FAILURE;
    }
}

bool CommandLineInterface::checkLicense()
{
    const ProductKeyCheck *const productKeyCheck = ProductKeyCheck::instance();
    if (!productKeyCheck->hasValidLicense()) {
        qCWarning(QInstaller::lcPackageLicenses) << "No valid license found.";
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
        targetDir = m_core->value(QLatin1String("TargetDir"));
        qCDebug(QInstaller::lcInstallerInstallLog) << "No target directory specified, using default value:" << targetDir;
    }
    if (m_core->checkTargetDir(targetDir)) {
        QString targetDirWarning = m_core->targetDirWarning(targetDir);
        if (!targetDirWarning.isEmpty()) {
            qCWarning(QInstaller::lcInstallerInstallLog) << m_core->targetDirWarning(targetDir);
        } else {
            m_core->setValue(QLatin1String("TargetDir"), targetDir);
            return true;
        }
    }
    return false;
}
