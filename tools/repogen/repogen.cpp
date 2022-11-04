/**************************************************************************
**
** Copyright (C) 2022 The Qt Company Ltd.
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

#include <repositorygen.h>
#include <errors.h>
#include <fileutils.h>
#include <init.h>
#include <updater.h>
#include <settings.h>
#include <utils.h>
#include <loggingutils.h>
#include <archivefactory.h>
#include <abstractarchive.h>

#include <QDomDocument>
#include <QtCore/QDir>
#include <QtCore/QDirIterator>
#include <QtCore/QFileInfo>
#include <QTemporaryDir>
#include <QMetaEnum>

#include <iostream>


using namespace QInstaller;

static void printUsage()
{
    const QString appName = QFileInfo(QCoreApplication::applicationFilePath()).fileName();
    const QString archiveFormats = ArchiveFactory::supportedTypes().join(QLatin1Char('|'));

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

    std::cout << "  --unite-metadata          Combine all metadata into one 7z. This speeds up metadata " << std::endl;
    std::cout << "                            download phase." << std::endl;

    std::cout << "  --component-metadata      Creates one metadata 7z per component. " << std::endl;
    std::cout << "  --af|--archive-format " << archiveFormats << std::endl;
    std::cout << "                            Set the format used when packaging new component data archives. If" << std::endl;
    std::cout << "                            you omit this option the 7z format will be used as a default." << std::endl;
    std::cout << "  --ac|--compression 0,1,3,5,7,9" << std::endl;
    std::cout << "                            Sets the compression level used when packaging new data archives." << std::endl;

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
        QInstallerTools::RepositoryInfo repoInfo;
        QStringList packagesUpdatedWithSha;
        QInstallerTools::FilterType filterType = QInstallerTools::Exclude;
        bool remove = false;
        bool updateExistingRepositoryWithNewComponents = false;
        bool createUnifiedMetadata = true;
        bool createComponentMetadata = true;
        QString archiveSuffix = QLatin1String("7z");
        AbstractArchive::CompressionLevel compression = AbstractArchive::Normal;

        //TODO: use a for loop without removing values from args like it is in binarycreator.cpp
        //for (QStringList::const_iterator it = args.begin(); it != args.end(); ++it) {
        while (!args.isEmpty() && args.first().startsWith(QLatin1Char('-'))) {
            if (args.first() == QLatin1String("--verbose") || args.first() == QLatin1String("-v")) {
                args.removeFirst();
                LoggingHandler::instance().setVerbose(true);
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
                        "Error: --include and --exclude are mutually exclusive. Use either one or "
                        "the other."));
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
                const QDir dir(args.first());
                if (!dir.exists()) {
                    return printErrorAndUsageAndExit(QCoreApplication::translate("QInstaller",
                        "Error: Package directory not found at the specified location"));
                }
                if (dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot).isEmpty()) {
                    return printErrorAndUsageAndExit(QCoreApplication::translate("QInstaller",
                        "Error: Package directory is empty"));
                }

                repoInfo.packages.append(args.first());
                args.removeFirst();
            } else if (args.first() == QLatin1String("--repository")) {
                args.removeFirst();
                if (args.isEmpty()) {
                    return printErrorAndUsageAndExit(QCoreApplication::translate("QInstaller",
                        "Error: Repository parameter missing argument"));
                }

                if (!QFileInfo::exists(args.first())) {
                    return printErrorAndUsageAndExit(QCoreApplication::translate("QInstaller",
                        "Error: Only local filesystem repositories now supported"));
                }
                repoInfo.repositoryPackages.append(args.first());
                args.removeFirst();
            } else if (args.first() == QLatin1String("--ignore-translations")
                || args.first() == QLatin1String("--ignore-invalid-packages")) {
                    args.removeFirst();
            } else if (args.first() == QLatin1String("-r") || args.first() == QLatin1String("--remove")) {
                remove = true;
                args.removeFirst();
            } else if (args.first() == QLatin1String("--unite-metadata")) {
                createComponentMetadata = false;
                args.removeFirst();
            } else if (args.first() == QLatin1String("--component-metadata")) {
                createUnifiedMetadata = false;
                args.removeFirst();
            } else if (args.first() == QLatin1String("--sha-update") || args.first() == QLatin1String("-s")) {
                args.removeFirst();
                packagesUpdatedWithSha = args.first().split(QLatin1Char(','));
                args.removeFirst();
            } else if (args.first() == QLatin1String("--af") || args.first() == QLatin1String("--archive-format")) {
                args.removeFirst();
                if (args.isEmpty()) {
                    return printErrorAndUsageAndExit(QCoreApplication::translate("QInstaller",
                        "Error: Archive format parameter missing argument"));
                }
                // TODO: do we need early check for supported formats?
                archiveSuffix = args.first();
                args.removeFirst();
            } else if (args.first() == QLatin1String("--ac") || args.first() == QLatin1String("--compression")) {
                args.removeFirst();
                if (args.isEmpty()) {
                    return printErrorAndUsageAndExit(QCoreApplication::translate("QInstaller",
                        "Error: Compression parameter missing argument"));
                }
                bool ok = false;
                QMetaEnum levels = QMetaEnum::fromType<AbstractArchive::CompressionLevel>();
                const int value = args.first().toInt(&ok);
                if (!ok || !levels.valueToKey(value)) {
                    return printErrorAndUsageAndExit(QCoreApplication::translate("QInstaller",
                        "Error: Unknown compression level \"%1\".").arg(value));
                }
                compression = static_cast<AbstractArchive::CompressionLevel>(value);
                args.removeFirst();
            } else {
                printUsage();
                return 1;
            }
        }

        if ((repoInfo.packages.isEmpty() && repoInfo.repositoryPackages.isEmpty()) || (args.count() != 1)) {
                printUsage();
                return 1;
        }

        const bool update = updateExistingRepository || updateExistingRepositoryWithNewComponents;
        if (remove && update) {
            throw QInstaller::Error(QCoreApplication::translate("QInstaller",
                "Argument -r|--remove and --update|--update-new-components are mutually exclusive!"));
        }

        repoInfo.repositoryDir = QInstallerTools::makePathAbsolute(args.first());
        if (remove)
            QInstaller::removeDirectory(repoInfo.repositoryDir);

        if (!update && QFile::exists(repoInfo.repositoryDir) && !QDir(repoInfo.repositoryDir).entryList(
            QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty()) {

            throw QInstaller::Error(QCoreApplication::translate("QInstaller",
                "Repository target directory \"%1\" already exists.").arg(QDir::toNativeSeparators(repoInfo.repositoryDir)));
        }

        QInstallerTools::PackageInfoVector packages = QInstallerTools::collectPackages(repoInfo,
            &filteredPackages, filterType, updateExistingRepositoryWithNewComponents, packagesUpdatedWithSha);
        if (packages.isEmpty()) {
            std::cout << QString::fromLatin1("Cannot find components to update \"%1\".")
                .arg(repoInfo.repositoryDir) << std::endl;
            return EXIT_SUCCESS;
        }

        QTemporaryDir tmp;
        tmp.setAutoRemove(false);
        tmpMetaDir = tmp.path();
        QInstallerTools::createRepository(repoInfo, &packages, tmpMetaDir,
            createComponentMetadata, createUnifiedMetadata, archiveSuffix, compression);

        exitCode = EXIT_SUCCESS;
    } catch (const QInstaller::Error &e) {
        std::cerr << "Caught exception: " << e.message() << std::endl;
    } catch (...) {
        std::cerr << "Unknown exception caught" << std::endl;
    }

    QInstaller::removeDirectory(tmpMetaDir, true);
    return exitCode;
}
