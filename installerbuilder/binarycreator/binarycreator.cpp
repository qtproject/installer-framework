/**************************************************************************
**
** This file is part of Installer Framework
**
** Copyright (c) 2011-2012 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (qt-info@nokia.com)
**
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** Other Usage
**
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
** If you have questions regarding the use of this file, please contact
** Nokia at qt-info@nokia.com.
**
**************************************************************************/
#include "common/repositorygen.h"

#include <binaryformat.h>
#include <errors.h>
#include <fileutils.h>
#include <init.h>
#include <settings.h>
#include <utils.h>

#include <kdsavefile.h>

#include <QtCore/QDirIterator>
#include <QtCore/QProcess>
#include <QtCore/QSettings>
#include <QtCore/QTemporaryFile>

#include <iostream>

using namespace QInstaller;
using namespace QInstallerCreator;

struct Input {
    QString outputPath;
    QString installerExePath;
    ComponentIndex components;
    QString binaryResourcePath;
    QStringList binaryResources;

    Range<qint64> operationsPos;
    QVector<Range<qint64> > resourcePos;
    Range<qint64> componentIndexSegment;
};

class BundleBackup
{
public:
    explicit BundleBackup(const QString &bundle = QString())
        : bundle(bundle)
    {
        if (!bundle.isEmpty() && QFileInfo(bundle).exists()) {
            backup = generateTemporaryFileName(bundle);
            QFile::rename(bundle, backup);
        }
    }

    ~BundleBackup()
    {
        if (!backup.isEmpty()) {
            removeDirectory(bundle);
            QFile::rename(backup, bundle);
        }
    }

    void release() const
    {
        if (!backup.isEmpty())
            removeDirectory(backup);
        backup.clear();
    }

private:
    const QString bundle;
    mutable QString backup;
};

static void chmod755(const QString &absolutFilePath)
{
    QFile::setPermissions(absolutFilePath, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner
        | QFile::ReadGroup | QFile::ExeGroup | QFile::ReadOther | QFile::ExeOther);
}

static int assemble(Input input, const QString &configdir)
{
    const QString configfile = QFileInfo(configdir, QLatin1String("config.xml")).absoluteFilePath();
    const QInstaller::Settings &settings = QInstaller::Settings::fromFileAndPrefix(configfile, configdir);

#ifdef Q_OS_LINUX
Q_UNUSED(settings)
#endif

#ifdef Q_OS_MAC
    if (QFileInfo(input.installerExePath).isBundle()) {
        const QString bundle = input.installerExePath;
        // if the input file was a bundle
        const QSettings s(QString::fromLatin1("%1/Contents/Info.plist").arg(input.installerExePath),
            QSettings::NativeFormat);
        input.installerExePath = QString::fromLatin1("%1/Contents/MacOS/%2").arg(bundle)
            .arg(s.value(QLatin1String("CFBundleExecutable"),
            QFileInfo(input.installerExePath).completeBaseName()).toString());
    }

    const bool createDMG = input.outputPath.endsWith(QLatin1String(".dmg"));
    if (createDMG)
        input.outputPath.replace(input.outputPath.length() - 4, 4, QLatin1String(".app"));

    const bool isBundle = input.outputPath.endsWith(QLatin1String(".app"));
    const QString bundle = isBundle ? input.outputPath : QString();
    const BundleBackup bundleBackup(bundle);

    if (isBundle) {
        // output should be a bundle
        const QFileInfo fi(input.outputPath);
        QInstaller::mkpath(fi.filePath() + QLatin1String("/Contents/MacOS"));
        QInstaller::mkpath(fi.filePath() + QLatin1String("/Contents/Resources"));

        {
            QFile pkgInfo(fi.filePath() + QLatin1String("/Contents/PkgInfo"));
            pkgInfo.open(QIODevice::WriteOnly);
            QTextStream pkgInfoStream(&pkgInfo);
            pkgInfoStream << QLatin1String("APPL????") << endl;
        }

        const QString iconFile = QFile::exists(settings.icon()) ? settings.icon()
            : QString::fromLatin1(":/resources/default_icon_mac.icns");
        const QString iconTargetFile = fi.completeBaseName() + QLatin1String(".icns");
        QFile::copy(iconFile, fi.filePath() + QLatin1String("/Contents/Resources/") + iconTargetFile);

        QFile infoPList(fi.filePath() + QLatin1String("/Contents/Info.plist"));
        infoPList.open(QIODevice::WriteOnly);
        QTextStream plistStream(&infoPList);
        plistStream << QLatin1String("<?xml version=\"1.0\" encoding=\"UTF-8\"?>") << endl;
        plistStream << QLatin1String("<!DOCTYPE plist SYSTEM \"file://localhost/System/Library/DTDs"
            "/PropertyList.dtd\">") << endl;
        plistStream << QLatin1String("<plist version=\"0.9\">") << endl;
        plistStream << QLatin1String("<dict>") << endl;
        plistStream << QLatin1String("    <key>CFBundleIconFile</key>") << endl;
        plistStream << QLatin1String("    <string>") << iconTargetFile << QLatin1String("</string>")
            << endl;
        plistStream << QLatin1String("    <key>CFBundlePackageType</key>") << endl;
        plistStream << QLatin1String("    <string>APPL</string>") << endl;
        plistStream << QLatin1String("    <key>CFBundleGetInfoString</key>") << endl;
        plistStream << QLatin1String("    <string>Created by Qt/QMake</string>") << endl;
        plistStream << QLatin1String("    <key>CFBundleSignature</key>") << endl;
        plistStream << QLatin1String("    <string> ???? </string>") << endl;
        plistStream << QLatin1String("    <key>CFBundleExecutable</key>") << endl;
        plistStream << QLatin1String("    <string>") << fi.completeBaseName() << QLatin1String("</string>")
            << endl;
        plistStream << QLatin1String("    <key>CFBundleIdentifier</key>") << endl;
        plistStream << QLatin1String("    <string>com.yourcompany.installerbase</string>") << endl;
        plistStream << QLatin1String("    <key>NOTE</key>") << endl;
        plistStream << QLatin1String("    <string>This file was generated by Qt/QMake.</string>")
            << endl;
        plistStream << QLatin1String("</dict>") << endl;
        plistStream << QLatin1String("</plist>") << endl;

        input.outputPath = QString::fromLatin1("%1/Contents/MacOS/%2").arg(input.outputPath)
            .arg(fi.completeBaseName());
    }
#endif

    QTemporaryFile file(input.outputPath);
    if (!file.open()) {
        throw Error(QObject::tr("Could not copy %1 to %2: %3").arg(input.installerExePath,
            input.outputPath, file.errorString()));
    }

    const QString tempFile = file.fileName();
    file.close();
    file.remove();

    QFile instExe(input.installerExePath);
    if (!instExe.copy(tempFile)) {
        throw Error(QObject::tr("Could not copy %1 to %2: %3").arg(instExe.fileName(), tempFile,
            instExe.errorString()));
    }
    input.installerExePath = tempFile;

#if defined(Q_OS_WIN)
    // setting the windows icon must happen before we append our binary data - otherwise they get lost :-/
    if (QFile::exists(settings.icon())) {
        // no error handling as this is not fatal
        setApplicationIcon(tempFile, settings.icon());
    }
#elif defined(Q_OS_MAC)
    if (isBundle) {
        // no error handling as this is not fatal
        const QString copyscript = QDir::temp().absoluteFilePath(QLatin1String("copylibsintobundle.sh"));
        QFile::copy(QLatin1String(":/resources/copylibsintobundle.sh"), copyscript);
        QFile::rename(tempFile, input.outputPath);
        chmod755(copyscript);
        QProcess p;
        p.start(copyscript, QStringList() << bundle);
        p.waitForFinished();
        QFile::rename(input.outputPath, tempFile);
        QFile::remove(copyscript);
    }
#endif

    KDSaveFile out(input.outputPath);
    try {
        openForWrite(&out, input.outputPath);

        QFile exe(input.installerExePath);
        openForRead(&exe, exe.fileName());
        appendFileData(&out, &exe);

        const qint64 dataBlockStart = out.pos();
        qDebug() << "Data block starts at" << dataBlockStart;

        // append our self created resource file
        QFile res(input.binaryResourcePath);
        openForRead(&res, res.fileName());
        appendFileData(&out, &res);
        input.resourcePos.append(Range<qint64>::fromStartAndEnd(out.pos() - res.size(), out.pos())
            .moved(-dataBlockStart));

        // append given resource files
        foreach (const QString &resource, input.binaryResources) {
            QFile res(resource);
            openForRead(&res, res.fileName());
            appendFileData(&out, &res);
            input.resourcePos.append(Range<qint64>::fromStartAndEnd(out.pos() - res.size(), out.pos())
                .moved(-dataBlockStart));
        }

        // zero operations cause we are not the uninstaller
        const qint64 operationsStart = out.pos();
        appendInt64(&out, 0);
        appendInt64(&out, 0);
        input.operationsPos = Range<qint64>::fromStartAndEnd(operationsStart, out.pos())
            .moved(-dataBlockStart);

        // component index:
        input.components.writeComponentData(&out, -dataBlockStart);
        const qint64 compIndexStart = out.pos() - dataBlockStart;
        input.components.writeIndex(&out, -dataBlockStart);
        input.componentIndexSegment = Range<qint64>::fromStartAndEnd(compIndexStart, out.pos()
            - dataBlockStart);

        qDebug("Component index: [%llu:%llu]", input.componentIndexSegment.start(),
            input.componentIndexSegment.end());
        appendInt64Range(&out, input.componentIndexSegment);
        foreach (const Range<qint64> &range, input.resourcePos)
            appendInt64Range(&out, range);
        appendInt64Range(&out, input.operationsPos);
        appendInt64(&out, input.resourcePos.count());

        //data block size, from end of .exe to end of file
        appendInt64(&out, out.pos() + 3 * sizeof(qint64) - dataBlockStart);
        appendInt64(&out, QInstaller::MagicInstallerMarker);
        appendInt64(&out, QInstaller::MagicCookie);

    } catch (const Error &e) {
        qCritical("Error occurred while assembling the installer: %s", qPrintable(e.message()));
        QFile::remove(tempFile);
        return 1;
    }

    if (!out.commit(KDSaveFile::OverwriteExistingFile)) {
        qCritical("Could not write installer to %s: %s", qPrintable(out.fileName()),
            qPrintable(out.errorString()));
        QFile::remove(tempFile);
        return 1;
    }
#ifndef Q_OS_WIN
    chmod755(out.fileName());
#endif
    QFile::remove(tempFile);

#ifdef Q_OS_MAC
    bundleBackup.release();

    if (createDMG) {
        qDebug() << "creating a DMG disk image...";
        // no error handling as this is not fatal
        const QString mkdmgscript = QDir::temp().absoluteFilePath(QLatin1String("mkdmg.sh"));
        QFile::copy(QLatin1String(":/resources/mkdmg.sh"), mkdmgscript);
        chmod755(mkdmgscript);

        QProcess p;
        p.start(mkdmgscript, QStringList() << QFileInfo(out.fileName()).fileName() << bundle);
        p.waitForFinished();
        QFile::remove(mkdmgscript);
        qDebug() <<  "done." << mkdmgscript;
    }
#endif
    return 0;
}

QT_BEGIN_NAMESPACE
int runRcc(int argc, char *argv[]);
QT_END_NAMESPACE

static int runRcc(const QStringList &args)
{
    const int argc = args.count();
    QVector<char*> argv(argc, 0);
    for (int i = 0; i < argc; ++i)
        argv[i] = qstrdup(qPrintable(args[i]));

    const int result = runRcc(argc, argv.data());

    foreach (char *arg, argv)
        delete [] arg;

    return result;
}

class WorkingDirectoryChange
{
public:
    explicit WorkingDirectoryChange(const QString &path)
        : oldPath(QDir::currentPath())
    {
        QDir::setCurrent(path);
    }

    virtual ~WorkingDirectoryChange()
    {
        QDir::setCurrent(oldPath);
    }

private:
    const QString oldPath;
};

static QString createBinaryResourceFile(const QString &directory)
{
    QTemporaryFile projectFile(directory + QLatin1String("/rccprojectXXXXXX.qrc"));
    if (!projectFile.open())
        throw Error(QObject::tr("Could not create temporary file for generated rcc project file"));
    projectFile.close();

    const WorkingDirectoryChange wd(directory);
    const QString binaryName = generateTemporaryFileName();
    const QString projectFileName = QFileInfo(projectFile.fileName()).absoluteFilePath();

    // 1. create the .qrc file
    runRcc(QStringList() << QLatin1String("rcc") << QLatin1String("-project")
        << QLatin1String("-o") << projectFileName);

    // 2. create the binary resource file from the .qrc file
    runRcc(QStringList() << QLatin1String("rcc") << QLatin1String("-binary")
        << QLatin1String("-o") << binaryName << projectFileName);

    return binaryName;
}

static QStringList createBinaryResourceFiles(const QStringList &resources)
{
    QStringList result;
    foreach (const QString &resource, resources) {
        QFile file(resource);
        if (file.exists()) {
            const QString binaryName = generateTemporaryFileName();
            const QString fileName = QFileInfo(file.fileName()).absoluteFilePath();
            const int status = runRcc(QStringList() << QLatin1String("rcc")
                << QLatin1String("-binary") << QLatin1String("-o") << binaryName << fileName);
            if (status == EXIT_SUCCESS)
                result.append(binaryName);
        }
    }
    return result;
}

static void printUsage()
{
    const QString appName = QFileInfo(QCoreApplication::applicationFilePath()).fileName();
    std::cout << "Usage: " << appName << " [options] target" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;

    std::cout << "  -t|--template file        Use file as installer template binary" << std::endl;
    std::cout << "                            If this parameter is not given, the template used" << std::endl;
    std::cout << "                            defaults to installerbase." << std::endl;

    QInstallerTools::printRepositoryGenOptions();

    std::cout << "  -n|--nodeps               Don't add dependencies of package1...n into the " << std::endl;
    std::cout << "                            installer (for online installers)" << std::endl;

    std::cout << "  --offline-only            Forces the installer to act as an offline installer, " << std::endl;
    std::cout << "                            i.e. never access online repositories" << std::endl;

    std::cout << "  -r|--resources r1,.,rn    include the given resource files into the binary" << std::endl;
    std::cout << std::endl;
    std::cout << "  -v|--verbose              Verbose output" << std::endl;
    std::cout << "Packages are to be found in the current working directory and get listed as "
        "their names" << std::endl << std::endl;
    std::cout << "Example (offline installer):" << std::endl;
    std::cout << "  " << appName << " --offline-only -c installer-config -p packages-directory -t "
        "installerbase SDKInstaller.exe" << std::endl;
    std::cout << "Creates an offline installer for the SDK, containing all dependencies." << std::endl;
    std::cout << std::endl;
    std::cout << "Example (online installer):" << std::endl;
    std::cout << "  " << appName << " -c installer-config -p packages-directory -e com.nokia.sdk.qt,"
        "com.nokia.qtcreator -t installerbase SDKInstaller.exe" << std::endl;
    std::cout << std::endl;
    std::cout << "Creates an installer for the SDK without qt and qt creator." << std::endl;
    std::cout << std::endl;
}

static QString createMetaDataDirectory(const QInstallerTools::PackageInfoVector &packages,
    const QString &packagesDir, const QString &configdir)
{
    const QString configfile = QFileInfo(configdir, "config.xml").absoluteFilePath();
    const QInstaller::Settings &settings = QInstaller::Settings::fromFileAndPrefix(configfile, QString());

    const QString metapath = createTemporaryDirectory();
    generateMetaDataDirectory(metapath, packagesDir, packages, settings.applicationName(),
        settings.applicationVersion());

    const QString configCopy = metapath + QLatin1String("/installer-config");
    QInstaller::mkdir(configCopy);
    QString absoluteConfigPath = QFileInfo(configdir).absoluteFilePath();

    QDirIterator it(absoluteConfigPath, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString next = it.next();
        if (next.contains("/.")) // skip files that are in directories starting with a point
            continue;

        qDebug() << "\tFound configuration file: " << next;
        const QFileInfo sourceFileInfo(next);
        const QString source = sourceFileInfo.absoluteFilePath();
        const QFileInfo targetFileInfo(configCopy, QFileInfo(next).fileName());
        const QDir targetDir = targetFileInfo.dir();
        if (!targetDir.exists())
            QInstaller::mkpath(targetFileInfo.absolutePath());
        const QString target = targetFileInfo.absoluteFilePath();

        if (!QFile::copy(source, target))
            throw Error(QObject::tr("Could not copy %1.").arg(source));

        if (sourceFileInfo.fileName().toLower() == QLatin1String("config.xml")) {
            // if we just copied the config.xml, make sure to remove the RSA private key from it :-o
            QFile configXml(targetDir.filePath(QLatin1String("config.xml")));
            configXml.open(QIODevice::ReadOnly);
            QDomDocument dom;
            dom.setContent(&configXml);
            configXml.close();

            // iterate over all child elements, searching for relative file names
            const QDomNodeList children = dom.documentElement().childNodes();
            for (int i = 0; i < children.count(); ++i) {
                QDomElement el = children.at(i).toElement();
                if (el.isNull())
                    continue;

                QFileInfo fi(absoluteConfigPath, el.text());
#if defined(Q_OS_MAC)
                const QFileInfo fiIcon(absoluteConfigPath, el.text() + QLatin1String(".icns"));
#elif defined(Q_OS_WIN)
                const QFileInfo fiIcon(absoluteConfigPath, el.text() + QLatin1String(".ico"));
#else
                const QFileInfo fiIcon(absoluteConfigPath, el.text() + QLatin1String(".png"));
#endif
                if (!fi.exists() && fiIcon.exists())
                    fi = fiIcon;

                if (!fi.exists() || fi.absolutePath() == QFileInfo(configdir).dir().absolutePath())
                    continue;

                if (fi.isDir())
                    continue;

                const QString newName = el.text().replace(QRegExp(QLatin1String("\\\\|/|\\.")),
                    QLatin1String("_"));

                if (!QFile::exists(targetDir.absoluteFilePath(newName))) {
                    if (!QFile::copy(fi.absoluteFilePath(), targetDir.absoluteFilePath(newName)))
                        throw Error(QObject::tr("Could not copy %1.").arg(el.text()));
                }
                el.removeChild(el.firstChild());
                el.appendChild(dom.createTextNode(newName));
            }

            openForWrite(&configXml, configXml.fileName());
            QTextStream stream(&configXml);
            dom.save(stream, 4);
            qDebug() << "\tdone.";
        }
    }
    return metapath;
}

static int printErrorAndUsageAndExit(const QString &err)
{
    std::cerr << qPrintable(err) << std::endl << std::endl;
    printUsage();
    return EXIT_FAILURE;
}

/*!
    Usage:
    binarycreator: [--help|-h] [-p|--packages packages directory] [-t|--template binary]
        -c|--config confdir target component ...
    template defaults to installerbase[.exe] in the same directory
*/
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    QInstaller::init();

    QString templateBinary = QLatin1String("installerbase");
#ifdef Q_OS_WIN
    templateBinary += QLatin1String(".exe");
#endif
    if (!QFileInfo(templateBinary).exists())
        templateBinary = QString::fromLatin1("%1/%2").arg(qApp->applicationDirPath(), templateBinary);

    QString target;
    QString configDir;
    QString packagesDirectory = QDir::currentPath();
    bool nodeps = false;
    bool offlineOnly = false;
    QStringList resources;
    QStringList components;
    QStringList filteredPackages;
    QInstallerTools::FilterType ftype = QInstallerTools::Exclude;

    const QStringList args = app.arguments().mid(1);
    for (QStringList::const_iterator it = args.begin(); it != args.end(); ++it) {
        if (*it == QLatin1String("-h") || *it == QLatin1String("--help")) {
            printUsage();
            return 0;
        } else if (*it == QLatin1String("-p") || *it == QLatin1String("--packages")) {
            ++it;
            if (it == args.end()) {
                return printErrorAndUsageAndExit(QObject::tr("Error: Packages parameter missing argument."));
            }
            if (!QFileInfo(*it).exists()) {
                return printErrorAndUsageAndExit(QObject::tr("Error: Package directory not found at the "
                    "specified location."));
            }
            packagesDirectory = *it;
        } else if (*it == QLatin1String("-e") || *it == QLatin1String("--exclude")) {
            ++it;
            if (!filteredPackages.isEmpty())
                return printErrorAndUsageAndExit(QObject::tr("Error: --include and --exclude are mutually "
                                                             "exclusive. Use either one or the other."));
            if (it == args.end() || it->startsWith(QLatin1String("-")))
                return printErrorAndUsageAndExit(QObject::tr("Error: Package to exclude missing."));
            filteredPackages = it->split(QLatin1Char(','));
        } else if (*it == QLatin1String("-i") || *it == QLatin1String("--include")) {
            ++it;
            if (!filteredPackages.isEmpty())
                return printErrorAndUsageAndExit(QObject::tr("Error: --include and --exclude are mutually "
                                                             "exclusive. Use either one or the other."));
            if (it == args.end() || it->startsWith(QLatin1String("-")))
                return printErrorAndUsageAndExit(QObject::tr("Error: Package to include missing."));
            filteredPackages = it->split(QLatin1Char(','));
            ftype = QInstallerTools::Include;
        }
        else if (*it == QLatin1String("-v") || *it == QLatin1String("--verbose")) {
            QInstaller::setVerbose(true);
        } else if (*it == QLatin1String("-n") || *it == QLatin1String("--nodeps")) {
            if (!filteredPackages.isEmpty())
                return printErrorAndUsageAndExit(QObject::tr("for the --include and --exclude case you also "
                                                             "have to ensure that nopdeps==false"));
            nodeps = true;
        } else if (*it == QLatin1String("--offline-only")) {
            offlineOnly = true;
        } else if (*it == QLatin1String("-t") || *it == QLatin1String("--template")) {
            ++it;
            if (it == args.end()) {
                return printErrorAndUsageAndExit(QObject::tr("Error: Template parameter missing argument."));
            }
            if (!QFileInfo(*it).exists()) {
                return printErrorAndUsageAndExit(QObject::tr("Error: Template not found at the specified "
                    "location."));
            }
            templateBinary = *it;
        } else if (*it == QLatin1String("-c") || *it == QLatin1String("--config")) {
            ++it;
            if (it == args.end())
                return printErrorAndUsageAndExit(QObject::tr("Error: Config parameter missing argument."));
            const QFileInfo fi(*it);
            if (!fi.exists()) {
                return printErrorAndUsageAndExit(QObject::tr("Error: Config directory %1 not found at the "
                    "specified location.").arg(*it));
            }
            if (!fi.isDir()) {
                return printErrorAndUsageAndExit(QObject::tr("Error: Configuration %1 is not a directory.")
                    .arg(*it));
            }
            if (!fi.isReadable()) {
                return printErrorAndUsageAndExit(QObject::tr("Error: Config directory %1 is not readable.")
                    .arg(*it));
            }
            configDir = *it;
        } else if (*it == QLatin1String("-r") || *it == QLatin1String("--resources")) {
            ++it;
            if (it == args.end() || it->startsWith(QLatin1String("-")))
                return printErrorAndUsageAndExit(QObject::tr("Error: Resource files to include missing."));
            resources = it->split(QLatin1Char(','));
        } else if (*it == QLatin1String("--ignore-translations")
            || *it == QLatin1String("--ignore-invalid-packages")) {
                continue;
        } else {
            if (target.isEmpty())
                target = *it;
            else
                components.append(*it);
        }
    }

    if (!components.isEmpty()) {
        std::cout << "Package names at the end of the command are deprecated"
                      " - please use --include or --exclude" << std::endl;
        if (nodeps) {
            filteredPackages.append(components);
            ftype = QInstallerTools::Include;
        }
    }

    if (target.isEmpty())
        return printErrorAndUsageAndExit(QObject::tr("Error: Target parameter missing."));

    if (configDir.isEmpty())
        return printErrorAndUsageAndExit(QObject::tr("Error: No configuration directory selected."));

    qDebug() << "Parsed arguments, ok.";

    try {
        QInstallerTools::PackageInfoVector packages = createListOfPackages(packagesDirectory,
            filteredPackages, ftype);
        const QString metaDir = createMetaDataDirectory(packages, packagesDirectory, configDir);
        {
            QSettings confInternal(metaDir + "/config/config-internal.ini", QSettings::IniFormat);
            confInternal.setValue(QLatin1String("offlineOnly"), offlineOnly);
        }

#if defined(Q_OS_MAC)
        // on mac, we enforce building a bundle
        if (!target.endsWith(QLatin1String(".app")) && !target.endsWith(QLatin1String(".dmg"))) {
            target += QLatin1String(".app");
        }
#elif defined(Q_OS_WIN)
        // on windows, we add .exe
        if (!target.endsWith(QLatin1String(".exe")))
            target += QLatin1String(".exe");
#endif
        int result = EXIT_FAILURE;
        {
            Input input;
            input.outputPath = target;
            input.installerExePath = templateBinary;
            input.binaryResourcePath = createBinaryResourceFile(metaDir);
            input.binaryResources = createBinaryResourceFiles(resources);

            QInstallerTools::copyComponentData(packagesDirectory, metaDir, packages);

            // now put the packages into the components section of the binary
            foreach (const QInstallerTools::PackageInfo &info, packages) {
                Component comp;
                comp.setName(info.name.toUtf8());

                qDebug() << "Creating component info for" << info.name;
                foreach (const QString &archive, info.copiedArchives) {
                    const QSharedPointer<Archive> arch(new Archive(archive));
                    qDebug() << QString::fromLatin1("\tAppending %1 (%2 bytes)").arg(archive,
                        QString::number(arch->size()));
                    comp.appendArchive(arch);
                }
                input.components.insertComponent(comp);
            }

            qDebug() << "Creating the binary";
            result = assemble(input, configDir);

            // cleanup
            qDebug() << "Cleaning up...";
            QFile::remove(input.binaryResourcePath);
            foreach (const QString &resource, input.binaryResources)
                QFile::remove(resource);
        }
        removeDirectory(metaDir);
        return result;
    } catch (const Error &e) {
        std::cerr << e.message() << std::endl;
        return EXIT_FAILURE;
    } catch (...) {
        std::cerr << "Unknown exception caught" << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_FAILURE;
}
