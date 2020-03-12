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

#include <QDir>
#include <QDomDocument>

#include <iostream>

CommandLineInterface::CommandLineInterface(int &argc, char *argv[])
    : SDKApp<QCoreApplication>(argc, argv)
{
    QInstaller::init(); // register custom operations
    m_parser.parse(arguments());
}

bool CommandLineInterface::initialize()
{
    QString errorMessage;
    if (!init(errorMessage)) {
        qCWarning(QInstaller::lcInstallerInstallLog) << errorMessage;
        return false;
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

int CommandLineInterface::silentUpdate()
{
    if (!initialize())
        return EXIT_FAILURE;
    if (m_core->isInstaller()) {
        qCWarning(QInstaller::lcInstallerInstallLog) << "Cannot perform update with installer.";
        return EXIT_FAILURE;
    }
    if (!checkLicense())
        return EXIT_FAILURE;
    m_core->updateComponentsSilently(QStringList());
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

int CommandLineInterface::listAvailablePackages()
{
    if (!initialize())
        return EXIT_FAILURE;
    if (!m_core->isInstaller())
        m_core->setPackageManager();
    if (!checkLicense())
        return EXIT_FAILURE;
    QString regexp = m_parser.value(QLatin1String(CommandLineOptions::ListPackages));
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
    QStringList packages;
    const QString &value = m_parser.value(QLatin1String(CommandLineOptions::UpdatePackages));
    if (!value.isEmpty())
        packages = value.split(QLatin1Char(','), QString::SkipEmptyParts);
    m_core->updateComponentsSilently(packages);
    return EXIT_SUCCESS;
}

int CommandLineInterface::install()
{
    if (!initialize() || (m_core->isInstaller() && !setTargetDir()) || !checkLicense())
        return EXIT_FAILURE;
    QStringList packages;
    const QString &value = m_parser.value(QLatin1String(CommandLineOptions::InstallPackages));
    if (!value.isEmpty())
        packages = value.split(QLatin1Char(','), QString::SkipEmptyParts);
    m_core->installSelectedComponentsSilently(packages);
    return EXIT_SUCCESS;
}

int CommandLineInterface::installDefault()
{
    if (!initialize())
        return EXIT_FAILURE;
    if (!m_core->isInstaller()) {
        qCWarning(QInstaller::lcInstallerInstallLog) << "Cannot perform default installation with maintenance tool.";
        return EXIT_FAILURE;
    }
    if (!setTargetDir() || !checkLicense())
        return EXIT_FAILURE;
    m_core->installDefaultComponentsSilently();
    return EXIT_SUCCESS;
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
    QStringList packages;
    const QString &value = m_parser.value(QLatin1String(CommandLineOptions::UninstallSelectedPackages));
    if (!value.isEmpty())
        packages = value.split(QLatin1Char(','), QString::SkipEmptyParts);
    m_core->uninstallComponentsSilently(packages);
    return EXIT_SUCCESS;
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
    if (m_parser.isSet(QLatin1String(CommandLineOptions::TargetDir))) {
        targetDir = m_parser.value(QLatin1String(CommandLineOptions::TargetDir));
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
