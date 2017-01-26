/**************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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
#include "common/repositorygen.h"

#include <errors.h>
#include <fileutils.h>
#include <init.h>
#include <kdupdater.h>
#include <settings.h>
#include <utils.h>
#include <lib7z_facade.h>

#include <QDomDocument>
#include <QtCore/QDir>
#include <QtCore/QDirIterator>
#include <QtCore/QFileInfo>
#include <QTemporaryDir>

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

    std::cout << "  --update                  Update a set of existing components (defined by " << std::endl;
    std::cout << "                            --include or --exclude) in the repository" << std::endl;

    std::cout << "  --update-new-components   Update a set of existing components (defined by " << std::endl;
    std::cout << "                            --include or --exclude) in the repository with all new components"
        << std::endl;

    std::cout << "  -v|--verbose              Verbose output" << std::endl;

    std::cout << std::endl;
    std::cout << "Example:" << std::endl;
    std::cout << "  " << appName << " -p ../examples/packages repository/"
        << std::endl;
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
        QInstallerTools::FilterType filterType = QInstallerTools::Exclude;
        bool remove = false;
        bool updateExistingRepositoryWithNewComponents = false;

        //TODO: use a for loop without removing values from args like it is in binarycreator.cpp
        //for (QStringList::const_iterator it = args.begin(); it != args.end(); ++it) {
        while (!args.isEmpty() && args.first().startsWith(QLatin1Char('-'))) {
            if (args.first() == QLatin1String("--verbose") || args.first() == QLatin1String("-v")) {
                args.removeFirst();
                setVerbose(true);
            } else if (args.first() == QLatin1String("--exclude") || args.first() == QLatin1String("-e")) {
                args.removeFirst();
                if (!filteredPackages.isEmpty()) {
                    return printErrorAndUsageAndExit(QCoreApplication::translate("QInstaller",
                        "Error: --include and --exclude are mutually exclusive. Use either one or "
                        "the other."));
                }

                if (args.isEmpty() || args.first().startsWith(QLatin1Char('-'))) {
                    return printErrorAndUsageAndExit(QCoreApplication::translate("QInstaller",
                        "Error: Package to exclude missing"));
                }

                filteredPackages = args.first().split(QLatin1Char(','));
                args.removeFirst();
            } else if (args.first() == QLatin1String("--include") || args.first() == QLatin1String("-i")) {
                args.removeFirst();
                if (!filteredPackages.isEmpty()) {
                    return printErrorAndUsageAndExit(QCoreApplication::translate("QInstaller",
                        "Error: --include and --exclude are mutual exclusive options. Use either "
                        "one or the other."));
                }

                if (args.isEmpty() || args.first().startsWith(QLatin1Char('-'))) {
                    return printErrorAndUsageAndExit(QCoreApplication::translate("QInstaller",
                        "Error: Package to include missing"));
                }

                filteredPackages = args.first().split(QLatin1Char(','));
                args.removeFirst();
                filterType = QInstallerTools::Include;
            } else if (args.first() == QLatin1String("--update")) {
                args.removeFirst();
                updateExistingRepository = true;
            } else if (args.first() == QLatin1String("--update-new-components")) {
                args.removeFirst();
                updateExistingRepositoryWithNewComponents = true;
            } else if (args.first() == QLatin1String("-p") || args.first() == QLatin1String("--packages")) {
                args.removeFirst();
                if (args.isEmpty()) {
                    return printErrorAndUsageAndExit(QCoreApplication::translate("QInstaller",
                        "Error: Packages parameter missing argument"));
                }

                if (!QFileInfo(args.first()).exists()) {
                    return printErrorAndUsageAndExit(QCoreApplication::translate("QInstaller",
                        "Error: Package directory not found at the specified location"));
                }

                packagesDirectories.append(args.first());
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

        const bool update = updateExistingRepository || updateExistingRepositoryWithNewComponents;
        if (remove && update) {
            throw QInstaller::Error(QCoreApplication::translate("QInstaller",
                "Argument -r|--remove and --update|--update-new-components are mutually exclusive!"));
        }

        const QString repositoryDir = QInstallerTools::makePathAbsolute(args.first());
        if (remove)
            QInstaller::removeDirectory(repositoryDir);

        if (!update && QFile::exists(repositoryDir) && !QDir(repositoryDir).entryList(
            QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty()) {

            throw QInstaller::Error(QCoreApplication::translate("QInstaller",
                "Repository target folder %1 already exists!").arg(repositoryDir));
        }

        QInstallerTools::PackageInfoVector packages = QInstallerTools::createListOfPackages(packagesDirectories,
            &filteredPackages, filterType);

        if (updateExistingRepositoryWithNewComponents) {
            QDomDocument doc;
            QFile file(repositoryDir + QLatin1String("/Updates.xml"));
            if (file.open(QFile::ReadOnly) && doc.setContent(&file)) {
                const QDomElement root = doc.documentElement();
                if (root.tagName() != QLatin1String("Updates")) {
                    throw QInstaller::Error(QCoreApplication::translate("QInstaller",
                        "Invalid content in '%1'.").arg(file.fileName()));
                }
                file.close(); // close the file, we read the content already

                // read the already existing updates xml content
                const QDomNodeList children = root.childNodes();
                QHash <QString, QInstallerTools::PackageInfo> hash;
                for (int i = 0; i < children.count(); ++i) {
                    const QDomElement el = children.at(i).toElement();
                    if ((!el.isNull()) && (el.tagName() == QLatin1String("PackageUpdate"))) {
                        QInstallerTools::PackageInfo info;
                        const QDomNodeList c2 = el.childNodes();
                        for (int j = 0; j < c2.count(); ++j) {
                            if (c2.at(j).toElement().tagName() == scName)
                                info.name = c2.at(j).toElement().text();
                            else if (c2.at(j).toElement().tagName() == scRemoteVersion)
                                info.version = c2.at(j).toElement().text();
                        }
                        hash.insert(info.name, info);
                    }
                }

                // remove all components that have no update (decision based on the version tag)
                for (int i = packages.count() - 1; i >= 0; --i) {
                    const QInstallerTools::PackageInfo info = packages.at(i);
                    if (!hash.contains(info.name))
                        continue;   // the component is not there, keep it

                    if (KDUpdater::compareVersion(info.version, hash.value(info.name).version) < 1)
                        packages.remove(i); // the version did not change, no need to update the component
                }

                if (packages.isEmpty()) {
                    std::cout << QString::fromLatin1("Could not find new components to update '%1'.")
                        .arg(repositoryDir) << std::endl;
                    return EXIT_SUCCESS;
                }
            }
        }

        QHash<QString, QString> pathToVersionMapping = QInstallerTools::buildPathToVersionMapping(packages);

        foreach (const QInstallerTools::PackageInfo &package, packages) {
            const QFileInfo fi(repositoryDir, package.name);
            if (fi.exists())
                removeDirectory(fi.absoluteFilePath());
        }

        QTemporaryDir tmp;
        tmp.setAutoRemove(false);
        tmpMetaDir = tmp.path();
        QInstallerTools::copyComponentData(packagesDirectories, repositoryDir, &packages);
        QInstallerTools::copyMetaData(tmpMetaDir, repositoryDir, packages, QLatin1String("{AnyApplication}"),
            QLatin1String(QUOTE(IFW_REPOSITORY_FORMAT_VERSION)));
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
