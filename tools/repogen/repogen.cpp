/**************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
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
#include "common/repositorygen.h"

#include <errors.h>
#include <fileutils.h>
#include <init.h>
#include <settings.h>
#include <utils.h>
#include <lib7z_facade.h>

#include <QtCore/QDir>
#include <QtCore/QDirIterator>
#include <QtCore/QFileInfo>

#include <iostream>

#define QUOTE_(x) #x
#define QUOTE(x) QUOTE_(x)

using namespace Lib7z;
using namespace QInstaller;

static void printUsage()
{
    const QString appName = QFileInfo(QCoreApplication::applicationFilePath()).fileName();
    std::cout << "Usage: " << appName << " [options] repository-dir" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;

    QInstallerTools::printRepositoryGenOptions();

    std::cout << "  -r|--remove               Force removing target directory if existent." << std::endl;
    std::cout << "  -u|--updateurl            URL instructs clients to receive updates from a " << std::endl;
    std::cout << "                            different location" << std::endl;

    std::cout << "  --update                  Update a set of existing components (defined by " << std::endl;
    std::cout << "                            --include or --exclude) in the repository" << std::endl;

    std::cout << "  -v|--verbose              Verbose output" << std::endl;

    std::cout << std::endl;
    std::cout << "Example:" << std::endl;
    std::cout << "  " << appName << " -p ../examples/packages -u "
        "http://www.example.com:8080 repository/" << std::endl;
}

static int printErrorAndUsageAndExit(const QString &err)
{
    std::cerr << qPrintable(err) << std::endl << std::endl;
    printUsage();
    return 1;
}

int main(int argc, char** argv)
{
    QString tmpMetaDir;
    int exitCode = EXIT_FAILURE;
    try {
        QCoreApplication app(argc, argv);

        QInstaller::init();

        QStringList args = app.arguments().mid(1);

        QStringList filteredPackages;
        bool updateExistingRepository = false;
        QStringList packagesDirectories;
        QString redirectUpdateUrl;
        QInstallerTools::FilterType filterType = QInstallerTools::Exclude;
        bool remove = false;

        //TODO: use a for loop without removing values from args like it is in binarycreator.cpp
        //for (QStringList::const_iterator it = args.begin(); it != args.end(); ++it) {
        while (!args.isEmpty() && args.first().startsWith(QLatin1Char('-'))) {
            if (args.first() == QLatin1String("--verbose") || args.first() == QLatin1String("-v")) {
                args.removeFirst();
                setVerbose(true);
            } else if (args.first() == QLatin1String("--exclude") || args.first() == QLatin1String("-e")) {
                args.removeFirst();
                if (!filteredPackages.isEmpty())
                    return printErrorAndUsageAndExit(QObject::tr("Error: --include and --exclude are mutually "
                                                                 "exclusive. Use either one or the other."));
                if (args.isEmpty() || args.first().startsWith(QLatin1Char('-')))
                    return printErrorAndUsageAndExit(QObject::tr("Error: Package to exclude missing"));
                filteredPackages = args.first().split(QLatin1Char(','));
                args.removeFirst();
            } else if (args.first() == QLatin1String("--include") || args.first() == QLatin1String("-i")) {
                args.removeFirst();
                if (!filteredPackages.isEmpty())
                    return printErrorAndUsageAndExit(QObject::tr("Error: --include and --exclude are mutual "
                                                                 "exclusive options. Use either one or the other."));
                if (args.isEmpty() || args.first().startsWith(QLatin1Char('-')))
                    return printErrorAndUsageAndExit(QObject::tr("Error: Package to include missing"));
                filteredPackages = args.first().split(QLatin1Char(','));
                args.removeFirst();
                filterType = QInstallerTools::Include;
            } else if (args.first() == QLatin1String("--single") || args.first() == QLatin1String("--update")) {
                args.removeFirst();
                updateExistingRepository = true;
            } else if (args.first() == QLatin1String("-p") || args.first() == QLatin1String("--packages")) {
                args.removeFirst();
                if (args.isEmpty()) {
                    return printErrorAndUsageAndExit(QObject::tr("Error: Packages parameter missing "
                        "argument"));
                }
                if (!QFileInfo(args.first()).exists()) {
                    return printErrorAndUsageAndExit(QObject::tr("Error: Package directory not found "
                        "at the specified location"));
                }
                packagesDirectories.append(args.first());
                args.removeFirst();
            } else if (args.first() == QLatin1String("-c") || args.first() == QLatin1String("--config")) {
                args.removeFirst();
                if (args.isEmpty())
                    return printErrorAndUsageAndExit(QObject::tr("Error: Config parameter missing argument"));
                args.removeFirst();
                std::cout << "Config file parameter is deprecated and ignored." << std::endl;
            } else if (args.first() == QLatin1String("-u") || args.first() == QLatin1String("--updateurl")) {
                args.removeFirst();
                if (args.isEmpty())
                    return printErrorAndUsageAndExit(QObject::tr("Error: Config parameter missing argument"));
                redirectUpdateUrl = args.first();
                args.removeFirst();
            } else if (args.first() == QLatin1String("--ignore-translations")
                || args.first() == QLatin1String("--ignore-invalid-packages")) {
                    args.removeFirst();
            } else if (args.first() == QLatin1String("-r") || args.first() == QLatin1String("--remove")) {
                remove = true;
                args.removeFirst();
            } else {
                printUsage();
                return 1;
            }
        }

        if (packagesDirectories.isEmpty() || (args.count() != 1)) {
                printUsage();
                return 1;
        }

        if (remove && updateExistingRepository) {
            throw QInstaller::Error(QObject::tr("Argument -r|--remove and --single|--update are mutually "
                "exclusive!"));
        }

        const QString repositoryDir = QInstallerTools::makePathAbsolute(args.first());
        if (remove)
            QInstaller::removeDirectory(repositoryDir);

        if (!updateExistingRepository && QFile::exists(repositoryDir)) {
            throw QInstaller::Error(QObject::tr("Repository target folder %1 already exists!")
                .arg(repositoryDir));
        }

        QInstallerTools::PackageInfoVector packages = QInstallerTools::createListOfPackages(packagesDirectories,
            filteredPackages, filterType);
        QHash<QString, QString> pathToVersionMapping = QInstallerTools::buildPathToVersionMapping(packages);

        foreach (const QInstallerTools::PackageInfo &package, packages) {
            const QFileInfo fi(repositoryDir, package.name);
            if (fi.exists())
                removeDirectory(fi.absoluteFilePath());
        }

        tmpMetaDir = QInstaller::createTemporaryDirectory();
        QInstallerTools::copyComponentData(packagesDirectories, repositoryDir, &packages);
        QInstallerTools::copyMetaData(tmpMetaDir, repositoryDir, packages, QLatin1String("{AnyApplication}"),
            QLatin1String(QUOTE(IFW_REPOSITORY_FORMAT_VERSION)), redirectUpdateUrl);
        QInstallerTools::compressMetaDirectories(tmpMetaDir, tmpMetaDir, pathToVersionMapping);

        QDirIterator it(repositoryDir, QStringList(QLatin1String("Updates*.xml")), QDir::Files | QDir::CaseSensitive);
        while (it.hasNext()) {
            it.next();
            QFile::remove(it.fileInfo().absoluteFilePath());
        }
        QInstaller::moveDirectoryContents(tmpMetaDir, repositoryDir);
        exitCode = EXIT_SUCCESS;
    } catch (const Lib7z::SevenZipException &e) {
        std::cerr << "Caught 7zip exception: " << e.message() << std::endl;
    } catch (const QInstaller::Error &e) {
        std::cerr << "Caught exception: " << e.message() << std::endl;
    } catch (...) {
        std::cerr << "Unknown exception caught" << std::endl;
    }

    QInstaller::removeDirectory(tmpMetaDir, true);
    return exitCode;
}
