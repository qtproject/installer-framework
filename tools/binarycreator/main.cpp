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

#include <binarycreator.h>
#include <init.h>
#include <utils.h>
#include <loggingutils.h>
#include <archivefactory.h>

#include <QtCore/QDebug>
#include <QMetaEnum>

#include <iostream>

using namespace QInstaller;

static void printUsage()
{
    QString suffix;
#ifdef Q_OS_WIN
    suffix = QLatin1String(".exe");
#endif
    const QString appName = QFileInfo(QCoreApplication::applicationFilePath()).fileName();
    const QString archiveFormats = ArchiveFactory::supportedTypes().join(QLatin1Char('|'));
    std::cout << "Usage: " << appName << " [options] target" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;

    std::cout << "  -t|--template file        Use file as installer template binary" << std::endl;
    std::cout << "                            If this parameter is not given, the template used" << std::endl;
    std::cout << "                            defaults to installerbase." << std::endl;

    QInstallerTools::printRepositoryGenOptions();

    std::cout << "  -c|--config file          The file containing the installer configuration" << std::endl;

    std::cout << "  -n|--online-only          Do not add any package into the installer" << std::endl;
    std::cout << "                             (for online only installers)" << std::endl;

    std::cout << "  -f|--offline-only         Forces the installer to act as an offline installer, " << std::endl;
    std::cout << "                             i.e. never access online repositories" << std::endl;

    std::cout << "  -r|--resources r1,.,rn    include the given resource files into the binary" << std::endl;

    std::cout << "  -v|--verbose              Verbose output" << std::endl;
    std::cout << "  -rcc|--compile-resource   Compiles the default resource and outputs the result into"
        << std::endl;
    std::cout << "                            'update.rcc' in the current path." << std::endl;
#ifdef Q_OS_MACOS
    std::cout << "  --mt|--create-maintenancetool " << std::endl;
    std::cout << "                            Creates maintenance tool app bundle. Target option is omitted, bundle name "<<std::endl;
    std::cout << "                            can be configured in the config.xml using element <MaintenanceToolName>." << std::endl;
    std::cout << "  -s|--sign identity        Sign generated app bundle using the given code " << std::endl;
    std::cout << "                            signing identity" << std::endl;
#endif
    std::cout << "  --af|--archive-format " << archiveFormats << std::endl;
    std::cout << "                            Set the format used when packaging new component data archives. If" << std::endl;
    std::cout << "                            you omit this option the 7z format will be used as a default." << std::endl;
    std::cout << "  --ac|--compression 0,1,3,5,7,9" << std::endl;
    std::cout << "                            Sets the compression level used when packaging new data archives." << std::endl;
    std::cout << std::endl;
    std::cout << "Packages are to be found in the current working directory and get listed as "
        "their names" << std::endl << std::endl;
    std::cout << "Example (offline installer):" << std::endl;
    char sep = QDir::separator().toLatin1();
    std::cout << "  " << appName << " --offline-only -c installer-config" << sep << "config.xml -p "
        "packages-directory -t installerbase" << suffix << " SDKInstaller" << suffix << std::endl;
    std::cout << "Creates an offline installer for the SDK, containing all dependencies." << std::endl;
    std::cout << std::endl;
    std::cout << "Example (online installer):" << std::endl;
    std::cout << "  " << appName << " -c installer-config" << sep << "config.xml -p packages-directory "
        "-e org.qt-project.sdk.qt,org.qt-project.qtcreator -t installerbase" << suffix << " SDKInstaller"
        << suffix << std::endl;
    std::cout << std::endl;
    std::cout << "Creates an installer for the SDK without qt and qt creator." << std::endl;
    std::cout << std::endl;
    std::cout << "Example update.rcc:" << std::endl;
    std::cout << "  " << appName << " -c installer-config" << sep << "config.xml -p packages-directory "
        "-rcc" << std::endl;
#ifdef Q_OS_MACOS
    std::cout << "Example (maintenance tool bundle):" << std::endl;
    std::cout << "  " << appName << " -c installer-config" << sep << "config.xml" << " --mt" << std::endl;
    std::cout << std::endl;
#endif
}

static int printErrorAndUsageAndExit(const QString &err)
{
    std::cerr << qPrintable(err) << std::endl << std::endl;
    printUsage();
    return EXIT_FAILURE;
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    QInstaller::init();

    QInstallerTools::BinaryCreatorArgs parsedArgs;

    parsedArgs.templateBinary = QLatin1String("installerbase");
    QString suffix;
#ifdef Q_OS_WIN
    suffix = QLatin1String(".exe");
    parsedArgs.templateBinary = parsedArgs.templateBinary + suffix;
#endif
    if (!QFileInfo(parsedArgs.templateBinary).exists())
        parsedArgs.templateBinary = QString::fromLatin1("%1/%2").arg(qApp->applicationDirPath(), parsedArgs.templateBinary);

    const QStringList args = app.arguments().mid(1);
    for (QStringList::const_iterator it = args.begin(); it != args.end(); ++it) {
        if (*it == QLatin1String("-h") || *it == QLatin1String("--help")) {
            printUsage();
            return 0;
        } else if (*it == QLatin1String("-p") || *it == QLatin1String("--packages")) {
            ++it;
            if (it == args.end()) {
                return printErrorAndUsageAndExit(QString::fromLatin1("Error: Packages parameter missing argument."));
            }
            parsedArgs.packagesDirectories.append(*it);
        } else if (*it == QLatin1String("--repository")) {
            ++it;
            if (it == args.end()) {
                return printErrorAndUsageAndExit(QString::fromLatin1("Error: Repository parameter missing argument."));
            }
            parsedArgs.repositoryDirectories.append(*it);
        } else if (*it == QLatin1String("-e") || *it == QLatin1String("--exclude")) {
            ++it;
            if (!parsedArgs.filteredPackages.isEmpty())
                return printErrorAndUsageAndExit(QString::fromLatin1("Error: --include and --exclude are mutually "
                                                             "exclusive. Use either one or the other."));
            if (it == args.end() || it->startsWith(QLatin1String("-")))
                return printErrorAndUsageAndExit(QString::fromLatin1("Error: Package to exclude missing."));
            parsedArgs.filteredPackages = it->split(QLatin1Char(','));
        } else if (*it == QLatin1String("-i") || *it == QLatin1String("--include")) {
            ++it;
            if (!parsedArgs.filteredPackages.isEmpty())
                return printErrorAndUsageAndExit(QString::fromLatin1("Error: --include and --exclude are mutually "
                                                             "exclusive. Use either one or the other."));
            if (it == args.end() || it->startsWith(QLatin1String("-")))
                return printErrorAndUsageAndExit(QString::fromLatin1("Error: Package to include missing."));
            parsedArgs.filteredPackages = it->split(QLatin1Char(','));
            parsedArgs.ftype = QInstallerTools::Include;
        }
        else if (*it == QLatin1String("-v") || *it == QLatin1String("--verbose")) {
            LoggingHandler::instance().setVerbose(true);
        } else if (*it == QLatin1String("-n") || *it == QLatin1String("--online-only")) {
            parsedArgs.onlineOnly = true;
        } else if (*it == QLatin1String("-f") || *it == QLatin1String("--offline-only")) {
            parsedArgs.offlineOnly = true;
        } else if (*it == QLatin1String("-t") || *it == QLatin1String("--template")) {
            ++it;
            if (it == args.end()) {
                return printErrorAndUsageAndExit(QString::fromLatin1("Error: Template parameter missing argument."));
            }
            parsedArgs.templateBinary = *it;
        } else if (*it == QLatin1String("-c") || *it == QLatin1String("--config")) {
            ++it;
            if (it == args.end())
                return printErrorAndUsageAndExit(QString::fromLatin1("Error: Config parameter missing argument."));
            parsedArgs.configFile = *it;
        } else if (*it == QLatin1String("-r") || *it == QLatin1String("--resources")) {
            ++it;
            if (it == args.end() || it->startsWith(QLatin1String("-")))
                return printErrorAndUsageAndExit(QString::fromLatin1("Error: Resource files to include are missing."));
            parsedArgs.resources = it->split(QLatin1Char(','));
        } else if (*it == QLatin1String("--ignore-translations")
            || *it == QLatin1String("--ignore-invalid-packages")) {
                continue;
        } else if (*it == QLatin1String("-rcc") || *it == QLatin1String("--compile-resource")) {
            parsedArgs.compileResource = true;
        } else if (*it == QLatin1String("--af") || *it == QLatin1String("--archive-format")) {
            ++it;
            if (it == args.end())
                return printErrorAndUsageAndExit(QString::fromLatin1("Error: Archive format parameter missing argument."));
            parsedArgs.archiveSuffix = *it;
        } else if (*it == QLatin1String("--ac") || *it == QLatin1String("--compression")) {
            ++it;
            if (it == args.end())
                return printErrorAndUsageAndExit(QString::fromLatin1("Error: Compression parameter missing argument"));

            bool ok = false;
            QMetaEnum levels = QMetaEnum::fromType<AbstractArchive::CompressionLevel>();
            const int value = it->toInt(&ok);
            if (!ok || !levels.valueToKey(value)) {
                return printErrorAndUsageAndExit(QString::fromLatin1(
                    "Error: Unknown compression level \"%1\".").arg(value));
            }
            parsedArgs.compression = static_cast<AbstractArchive::CompressionLevel>(value);
#ifdef Q_OS_MACOS
        } else if (*it == QLatin1String("--mt") || *it == QLatin1String("--create-maintenancetool")) {
            parsedArgs.createMaintenanceTool = true;

        } else if (*it == QLatin1String("-s") || *it == QLatin1String("--sign")) {
            ++it;
            if (it == args.end() || it->startsWith(QLatin1String("-")))
                return printErrorAndUsageAndExit(QString::fromLatin1("Error: No code signing identity specified."));
            parsedArgs.signingIdentity = *it;
#endif
        } else {
            if (it->startsWith(QLatin1String("-"))) {
                return printErrorAndUsageAndExit(QString::fromLatin1("Error: Unknown option \"%1\" used. Maybe you "
                    "are using an old syntax.").arg(*it));
            } else if (parsedArgs.target.isEmpty()) {
                parsedArgs.target = *it;
            } else {
                return printErrorAndUsageAndExit(QString::fromLatin1("Error: You are using an old syntax please add the "
                    "component name with the include option")
                    .arg(*it));
            }
        }
    }

    QString errorMsg;
    if (QInstallerTools::createBinary(parsedArgs, errorMsg) == EXIT_FAILURE) {
        if (!errorMsg.isEmpty())
            printErrorAndUsageAndExit(errorMsg);

        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
