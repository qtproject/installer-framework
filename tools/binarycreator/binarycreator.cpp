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

#include <qtpatch.h>

#include <binaryformat.h>
#include <errors.h>
#include <fileutils.h>
#include <init.h>
#include <repository.h>
#include <settings.h>
#include <utils.h>

#include <kdsavefile.h>

#include <QtCore/QDirIterator>
#include <QtCore/QProcess>
#include <QtCore/QSettings>
#include <QtCore/QTemporaryFile>
#include <QTemporaryDir>

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

#ifndef Q_OS_WIN
static void chmod755(const QString &absolutFilePath)
{
    QFile::setPermissions(absolutFilePath, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner
        | QFile::ReadGroup | QFile::ExeGroup | QFile::ReadOther | QFile::ExeOther);
}
#endif

static int assemble(Input input, const QInstaller::Settings &settings)
{
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

        const QString contentsResourcesPath = fi.filePath() + QLatin1String("/Contents/Resources/");

        QInstaller::mkpath(fi.filePath() + QLatin1String("/Contents/MacOS"));
        QInstaller::mkpath(contentsResourcesPath);

        {
            QFile pkgInfo(fi.filePath() + QLatin1String("/Contents/PkgInfo"));
            pkgInfo.open(QIODevice::WriteOnly);
            QTextStream pkgInfoStream(&pkgInfo);
            pkgInfoStream << QLatin1String("APPL????") << endl;
        }

        QString iconFile;
        if (QFile::exists(settings.installerApplicationIcon())) {
            iconFile = settings.installerApplicationIcon();
        } else {
            iconFile = QFile::exists(settings.icon()) ? settings.icon()
                : QString::fromLatin1(":/resources/default_icon_mac.icns");
        }

        const QString iconTargetFile = fi.completeBaseName() + QLatin1String(".icns");
        QFile::copy(iconFile, contentsResourcesPath + iconTargetFile);
        if (QDir(qApp->applicationDirPath() + QLatin1String("/qt_menu.nib")).exists()) {
            copyDirectoryContents(qApp->applicationDirPath() + QLatin1String("/qt_menu.nib"),
                contentsResourcesPath + QLatin1String("/qt_menu.nib"));
        }

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
#define QUOTE_(x) #x
#define QUOTE(x) QUOTE_(x)
        plistStream << QLatin1String("    <string>") << QLatin1String(QUOTE(IFW_VERSION)) << ("</string>")
            << endl;
#undef QUOTE
#undef QUOTE_
        plistStream << QLatin1String("    <key>CFBundleSignature</key>") << endl;
        plistStream << QLatin1String("    <string> ???? </string>") << endl;
        plistStream << QLatin1String("    <key>CFBundleExecutable</key>") << endl;
        plistStream << QLatin1String("    <string>") << fi.completeBaseName() << QLatin1String("</string>")
            << endl;
        plistStream << QLatin1String("    <key>CFBundleIdentifier</key>") << endl;
        plistStream << QLatin1String("    <string>com.yourcompany.installerbase</string>") << endl;
        plistStream << QLatin1String("    <key>NOTE</key>") << endl;
        plistStream << QLatin1String("    <string>This file was generated by Qt Installer Framework.</string>")
            << endl;
        plistStream << QLatin1String("    <key>NSPrincipalClass</key>") << endl;
        plistStream << QLatin1String("    <string>NSApplication</string>") << endl;
        plistStream << QLatin1String("</dict>") << endl;
        plistStream << QLatin1String("</plist>") << endl;

        input.outputPath = QString::fromLatin1("%1/Contents/MacOS/%2").arg(input.outputPath)
            .arg(fi.completeBaseName());
    }
#elif defined(Q_OS_LINUX)
    Q_UNUSED(settings)
#endif

    QTemporaryFile file(input.outputPath);
    if (!file.open()) {
        throw Error(QString::fromLatin1("Could not copy %1 to %2: %3").arg(input.installerExePath,
            input.outputPath, file.errorString()));
    }

    const QString tempFile = file.fileName();
    file.close();
    file.remove();

    QFile instExe(input.installerExePath);
    if (!instExe.copy(tempFile)) {
        throw Error(QString::fromLatin1("Could not copy %1 to %2: %3").arg(instExe.fileName(), tempFile,
            instExe.errorString()));
    }

    QtPatch::patchBinaryFile(tempFile, QByteArray("MY_InstallerCreateDateTime_MY"),
        QDateTime::currentDateTime().toString(QLatin1String("yyyy-MM-dd - HH:mm:ss")).toLatin1());


    input.installerExePath = tempFile;

#if defined(Q_OS_WIN)
    // setting the windows icon must happen before we append our binary data - otherwise they get lost :-/
    if (QFile::exists(settings.installerApplicationIcon())) {
        // no error handling as this is not fatal
        setApplicationIcon(tempFile, settings.installerApplicationIcon());
    } else {
        if (QFile::exists(settings.icon())) {
            setApplicationIcon(tempFile, settings.icon());
        }
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

#ifdef Q_OS_MAC
    QDir resourcePath(QFileInfo(input.outputPath).dir());
    resourcePath.cdUp();
    resourcePath.cd(QLatin1String("Resources"));
    KDSaveFile out(resourcePath.filePath(QLatin1String("installer.dat")));
#else
    KDSaveFile out(input.outputPath);
#endif
    try {
#ifdef Q_OS_MAC
        openForWrite(&out, out.fileName());

        QFile exe(input.installerExePath);
        if (!exe.copy(input.outputPath)) {
            throw Error(QString::fromLatin1("Could not copy %1 to %2: %3").arg(exe.fileName(), input.outputPath,
                exe.errorString()));
        }
#else
        openForWrite(&out, input.outputPath);

        QFile exe(input.installerExePath);
        openForRead(&exe, exe.fileName());
        appendFileData(&out, &exe);
#endif

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
        return EXIT_FAILURE;
    }

    if (!out.commit(KDSaveFile::OverwriteExistingFile)) {
        qCritical("Could not write installer to %s: %s", qPrintable(out.fileName()),
            qPrintable(out.errorString()));
        QFile::remove(tempFile);
        return EXIT_FAILURE;
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
        p.start(mkdmgscript, QStringList() << QFileInfo(input.outputPath).fileName() << bundle);
        p.waitForFinished();
        QFile::remove(mkdmgscript);
        qDebug() <<  "done." << mkdmgscript;
    }
#endif
    return EXIT_SUCCESS;
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

    // Note: this does not run the rcc provided by Qt, this one is using the compiled in binarycreator
    //       version. If it happens that resource mapping fails, we might need to adapt the code here...
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

static QString createBinaryResourceFile(const QString &directory, const QString &binaryName)
{
    QTemporaryFile projectFile(directory + QLatin1String("/rccprojectXXXXXX.qrc"));
    if (!projectFile.open())
        throw Error(QString::fromLatin1("Could not create temporary file for generated rcc project file"));
    projectFile.close();

    const WorkingDirectoryChange wd(directory);
    const QString projectFileName = QFileInfo(projectFile.fileName()).absoluteFilePath();

    // 1. create the .qrc file
    if (runRcc(QStringList() << QLatin1String("rcc") << QLatin1String("-project") << QLatin1String("-o")
        << projectFileName) != EXIT_SUCCESS) {
            throw Error(QString::fromLatin1("Could not create rcc project file."));
    }

    // 2. create the binary resource file from the .qrc file
    if (runRcc(QStringList() << QLatin1String("rcc") << QLatin1String("-binary") << QLatin1String("-o")
        << binaryName << projectFileName) != EXIT_SUCCESS) {
            throw Error(QString::fromLatin1("Could not compile rcc project file."));
    }

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
    QString suffix;
#ifdef Q_OS_WIN
    suffix = QLatin1String(".exe");
#endif
    const QString appName = QFileInfo(QCoreApplication::applicationFilePath()).fileName();
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
}

void copyConfigData(const QString &configFile, const QString &targetDir)
{
    qDebug() << "Begin to copy configuration file and data.";

    const QString sourceConfigFile = QFileInfo(configFile).absoluteFilePath();
    const QString targetConfigFile = targetDir + QLatin1String("/config.xml");
    QInstallerTools::copyWithException(sourceConfigFile, targetConfigFile, QLatin1String("configuration"));

    QFile configXml(targetConfigFile);
    QInstaller::openForRead(&configXml, configXml.fileName());

    QDomDocument dom;
    dom.setContent(&configXml);
    configXml.close();

    // iterate over all child elements, searching for relative file names
    const QDomNodeList children = dom.documentElement().childNodes();
    const QString sourceConfigFilePath = QFileInfo(sourceConfigFile).absolutePath();
    for (int i = 0; i < children.count(); ++i) {
        QDomElement domElement = children.at(i).toElement();
        if (domElement.isNull())
            continue;

        const QString tagName = domElement.tagName();
        const QString elementText = domElement.text();
        qDebug() << QString::fromLatin1("Read dom element: <%1>%2</%1>.").arg(tagName, elementText);

        QString newName = domElement.text().replace(QRegExp(QLatin1String("\\\\|/|\\.|:")),
            QLatin1String("_"));

        QString targetFile;
        QFileInfo elementFileInfo;
        if (tagName == QLatin1String("Icon") || tagName == QLatin1String("InstallerApplicationIcon")) {
#if defined(Q_OS_MAC)
            const QString suffix = QLatin1String(".icns");
#elif defined(Q_OS_WIN)
            const QString suffix = QLatin1String(".ico");
#else
            const QString suffix = QLatin1String(".png");
#endif
            elementFileInfo = QFileInfo(sourceConfigFilePath, elementText + suffix);
            targetFile = targetDir + QLatin1Char('/') + newName + suffix;
        } else {
            elementFileInfo = QFileInfo(sourceConfigFilePath, elementText);
            const QString suffix = elementFileInfo.completeSuffix();
            if (!suffix.isEmpty())
                newName.append(QLatin1Char('.') + suffix);
            targetFile = targetDir + QLatin1Char('/') + newName;
        }
        if (!elementFileInfo.exists() || elementFileInfo.isDir())
            continue;

        domElement.replaceChild(dom.createTextNode(newName), domElement.firstChild());
        QInstallerTools::copyWithException(elementFileInfo.absoluteFilePath(), targetFile, tagName);
    }

    QInstaller::openForWrite(&configXml, configXml.fileName());
    QTextStream stream(&configXml);
    dom.save(stream, 4);

    qDebug() << "done.\n";
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

    QString templateBinary = QLatin1String("installerbase");
    QString suffix;
#ifdef Q_OS_WIN
    suffix = QLatin1String(".exe");
    templateBinary = templateBinary + suffix;
#endif
    if (!QFileInfo(templateBinary).exists())
        templateBinary = QString::fromLatin1("%1/%2").arg(qApp->applicationDirPath(), templateBinary);

    QString target;
    QString configFile;
    QStringList packagesDirectories;
    bool onlineOnly = false;
    bool offlineOnly = false;
    QStringList resources;
    QStringList filteredPackages;
    QInstallerTools::FilterType ftype = QInstallerTools::Exclude;
    bool compileResource = false;

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
            if (!QFileInfo(*it).exists()) {
                return printErrorAndUsageAndExit(QString::fromLatin1("Error: Package directory not found at the "
                    "specified location."));
            }
            packagesDirectories.append(*it);
        } else if (*it == QLatin1String("-e") || *it == QLatin1String("--exclude")) {
            ++it;
            if (!filteredPackages.isEmpty())
                return printErrorAndUsageAndExit(QString::fromLatin1("Error: --include and --exclude are mutually "
                                                             "exclusive. Use either one or the other."));
            if (it == args.end() || it->startsWith(QLatin1String("-")))
                return printErrorAndUsageAndExit(QString::fromLatin1("Error: Package to exclude missing."));
            filteredPackages = it->split(QLatin1Char(','));
        } else if (*it == QLatin1String("-i") || *it == QLatin1String("--include")) {
            ++it;
            if (!filteredPackages.isEmpty())
                return printErrorAndUsageAndExit(QString::fromLatin1("Error: --include and --exclude are mutually "
                                                             "exclusive. Use either one or the other."));
            if (it == args.end() || it->startsWith(QLatin1String("-")))
                return printErrorAndUsageAndExit(QString::fromLatin1("Error: Package to include missing."));
            filteredPackages = it->split(QLatin1Char(','));
            ftype = QInstallerTools::Include;
        }
        else if (*it == QLatin1String("-v") || *it == QLatin1String("--verbose")) {
            QInstaller::setVerbose(true);
        } else if (*it == QLatin1String("-n") || *it == QLatin1String("--online-only")) {
            if (!filteredPackages.isEmpty()) {
                return printErrorAndUsageAndExit(QString::fromLatin1("Error: 'online-only' option cannot be used "
                    "in conjunction with the 'include' or 'exclude' option. An 'online-only' installer will never "
                    "contain any components apart from the root component."));
            }
            onlineOnly = true;
        } else if (*it == QLatin1String("-f") || *it == QLatin1String("--offline-only")) {
            offlineOnly = true;
        } else if (*it == QLatin1String("-t") || *it == QLatin1String("--template")) {
            ++it;
            if (it == args.end()) {
                return printErrorAndUsageAndExit(QString::fromLatin1("Error: Template parameter missing argument."));
            }
            templateBinary = *it;
#ifdef Q_OS_WIN
            if (!templateBinary.endsWith(suffix))
                templateBinary = templateBinary + suffix;
#endif
            if (!QFileInfo(templateBinary).exists()) {
                return printErrorAndUsageAndExit(QString::fromLatin1("Error: Template not found at the specified "
                    "location."));
            }
        } else if (*it == QLatin1String("-c") || *it == QLatin1String("--config")) {
            ++it;
            if (it == args.end())
                return printErrorAndUsageAndExit(QString::fromLatin1("Error: Config parameter missing argument."));
            const QFileInfo fi(*it);
            if (!fi.exists()) {
                return printErrorAndUsageAndExit(QString::fromLatin1("Error: Config file %1 not found at the "
                    "specified location.").arg(*it));
            }
            if (!fi.isFile()) {
                return printErrorAndUsageAndExit(QString::fromLatin1("Error: Configuration %1 is not a file.")
                    .arg(*it));
            }
            if (!fi.isReadable()) {
                return printErrorAndUsageAndExit(QString::fromLatin1("Error: Config file %1 is not readable.")
                    .arg(*it));
            }
            configFile = *it;
        } else if (*it == QLatin1String("-r") || *it == QLatin1String("--resources")) {
            ++it;
            if (it == args.end() || it->startsWith(QLatin1String("-")))
                return printErrorAndUsageAndExit(QString::fromLatin1("Error: Resource files to include are missing."));
            resources = it->split(QLatin1Char(','));
        } else if (*it == QLatin1String("--ignore-translations")
            || *it == QLatin1String("--ignore-invalid-packages")) {
                continue;
        } else if (*it == QLatin1String("-rcc") || *it == QLatin1String("--compile-resource")) {
            compileResource = true;
        } else {
            if (it->startsWith(QLatin1String("-"))) {
                return printErrorAndUsageAndExit(QString::fromLatin1("Error: Unknown option \"%1\" used. Maybe you "
                    "are using an old syntax.").arg(*it));
            } else if (target.isEmpty()) {
                target = *it;
#ifdef Q_OS_WIN
                if (!target.endsWith(suffix))
                    target = target + suffix;
#endif
            } else {
                return printErrorAndUsageAndExit(QString::fromLatin1("Error: You are using an old syntax please add the "
                    "component name with the include option")
                    .arg(*it));
            }
        }
    }

    if (onlineOnly && offlineOnly) {
        return printErrorAndUsageAndExit(QString::fromLatin1("You cannot use --online-only and "
            "--offline-only at the same time."));
    }

    if (onlineOnly) {
        filteredPackages.append(QLatin1String("X_fake_filter_component_for_online_only_installer_X"));
        ftype = QInstallerTools::Include;
    }

    if (target.isEmpty() && !compileResource)
        return printErrorAndUsageAndExit(QString::fromLatin1("Error: Target parameter missing."));

    if (configFile.isEmpty())
        return printErrorAndUsageAndExit(QString::fromLatin1("Error: No configuration file selected."));

    if (packagesDirectories.isEmpty())
        return printErrorAndUsageAndExit(QString::fromLatin1("Error: Package directory parameter missing."));

    qDebug() << "Parsed arguments, ok.";

    int exitCode = EXIT_FAILURE;
    QTemporaryDir tmp;
    tmp.setAutoRemove(false);
    const QString tmpMetaDir = tmp.path();
    try {
        const Settings settings = Settings::fromFileAndPrefix(configFile, QFileInfo(configFile).absolutePath());
        QInstallerTools::PackageInfoVector packages = QInstallerTools::createListOfPackages(packagesDirectories,
            &filteredPackages, ftype);
        QInstallerTools::copyMetaData(tmpMetaDir, packagesDirectories.first(), packages, settings.applicationName(),
            settings.applicationVersion());

        copyConfigData(configFile, tmpMetaDir + QLatin1String("/installer-config"));
        {
            QSettings confInternal(tmpMetaDir + QLatin1String("/config/config-internal.ini")
                , QSettings::IniFormat);
            // assume offline installer if there are no repositories
            offlineOnly |= settings.repositories().isEmpty();
            confInternal.setValue(QLatin1String("offlineOnly"), offlineOnly);
        }

#if defined(Q_OS_MAC)
        // on mac, we enforce building a bundle
        if (!target.endsWith(QLatin1String(".app")) && !target.endsWith(QLatin1String(".dmg")))
            target += QLatin1String(".app");
#endif
        if (!compileResource) {
            Input input;
            input.outputPath = target;
            input.installerExePath = templateBinary;
            input.binaryResourcePath = createBinaryResourceFile(tmpMetaDir, generateTemporaryFileName());
            input.binaryResources = createBinaryResourceFiles(resources);

            QInstallerTools::copyComponentData(packagesDirectories, tmpMetaDir, &packages);

            // now put the packages into the components section of the binary
            foreach (const QInstallerTools::PackageInfo &info, packages) {
                Component comp;
                comp.setName(info.name.toUtf8());

                qDebug() << "Creating component info for" << info.name;
                foreach (const QString &archive, info.copiedFiles) {
                    const QSharedPointer<Archive> arch(new Archive(archive));
                    qDebug() << QString::fromLatin1("Appending %1 (%2)").arg(archive,
                        humanReadableSize(arch->size()));
                    comp.appendArchive(arch);
                }
                input.components.insertComponent(comp);
            }

            qDebug() << "Creating the binary";
            exitCode = assemble(input, settings);

            // cleanup
            qDebug() << "Cleaning up...";
            QFile::remove(input.binaryResourcePath);
            foreach (const QString &resource, input.binaryResources)
                QFile::remove(resource);
        } else {
            createBinaryResourceFile(tmpMetaDir, QDir::currentPath() + QLatin1String("/update.rcc"));
            exitCode = EXIT_SUCCESS;
        }
    } catch (const Error &e) {
        std::cerr << "Caught exception: " << e.message() << std::endl;
    } catch (...) {
        std::cerr << "Unknown exception caught" << std::endl;
    }

    QInstaller::removeDirectory(tmpMetaDir, true);
    return exitCode;
}
