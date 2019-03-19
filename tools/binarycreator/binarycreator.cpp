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

#include <qtpatch.h>

#include <binarycontent.h>
#include <binaryformat.h>
#include <errors.h>
#include <fileio.h>
#include <fileutils.h>
#include <init.h>
#include <repository.h>
#include <settings.h>
#include <utils.h>

#include <QDateTime>
#include <QDirIterator>
#include <QDomDocument>
#include <QProcess>
#include <QRegExp>
#include <QSettings>
#include <QTemporaryFile>
#include <QTemporaryDir>

#include <iostream>

#ifdef Q_OS_MACOS
#include <QtCore/QtEndian>
#include <QtCore/QFile>
#include <QtCore/QVersionNumber>

#include <mach-o/fat.h>
#include <mach-o/loader.h>
#endif

using namespace QInstaller;

struct Input {
    QString outputPath;
    QString installerExePath;
    QInstallerTools::PackageInfoVector packages;
    QInstaller::ResourceCollectionManager manager;
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

#ifdef Q_OS_MACOS
template <typename T = uint32_t> T readInt(QIODevice *ioDevice, bool *ok,
                                           bool swap, bool peek = false) {
    const auto bytes = peek
            ? ioDevice->peek(sizeof(T))
            : ioDevice->read(sizeof(T));
    if (bytes.size() != sizeof(T)) {
        if (ok)
            *ok = false;
        return T();
    }
    if (ok)
        *ok = true;
    T n = *reinterpret_cast<const T *>(bytes.constData());
    return swap ? qbswap(n) : n;
}

static QVersionNumber readMachOMinimumSystemVersion(QIODevice *device)
{
    bool ok;
    std::vector<qint64> machoOffsets;

    qint64 pos = device->pos();
    uint32_t magic = readInt(device, &ok, false);
    if (magic == FAT_MAGIC || magic == FAT_CIGAM ||
        magic == FAT_MAGIC_64 || magic == FAT_CIGAM_64) {
        bool swap = magic == FAT_CIGAM || magic == FAT_CIGAM_64;
        uint32_t nfat = readInt(device, &ok, swap);
        if (!ok)
            return QVersionNumber();

        for (uint32_t n = 0; n < nfat; ++n) {
            const bool is64bit = magic == FAT_MAGIC_64 || magic == FAT_CIGAM_64;
            fat_arch_64 fat_arch;
            fat_arch.cputype = static_cast<cpu_type_t>(readInt(device, &ok, swap));
            if (!ok)
                return QVersionNumber();

            fat_arch.cpusubtype = static_cast<cpu_subtype_t>(readInt(device, &ok, swap));
            if (!ok)
                return QVersionNumber();

            fat_arch.offset = is64bit
                    ? readInt<uint64_t>(device, &ok, swap) : readInt(device, &ok, swap);
            if (!ok)
                return QVersionNumber();

            fat_arch.size = is64bit
                    ? readInt<uint64_t>(device, &ok, swap) : readInt(device, &ok, swap);
            if (!ok)
                return QVersionNumber();

            fat_arch.align = readInt(device, &ok, swap);
            if (!ok)
                return QVersionNumber();

            fat_arch.reserved = is64bit ? readInt(device, &ok, swap) : 0;
            if (!ok)
                return QVersionNumber();

            machoOffsets.push_back(static_cast<qint64>(fat_arch.offset));
        }
    } else if (!ok) {
        return QVersionNumber();
    }

    // Wasn't a fat file, so we just read a thin Mach-O from the original offset
    if (machoOffsets.empty())
        machoOffsets.push_back(pos);

    std::vector<QVersionNumber> versions;

    for (const auto &offset : machoOffsets) {
        if (!device->seek(offset))
            return QVersionNumber();

        bool swap = false;
        mach_header_64 header;
        header.magic = readInt(device, nullptr, swap);
        switch (header.magic) {
        case MH_CIGAM:
        case MH_CIGAM_64:
            swap = true;
            break;
        case MH_MAGIC:
        case MH_MAGIC_64:
            break;
        default:
            return QVersionNumber();
        }

        header.cputype = static_cast<cpu_type_t>(readInt(device, &ok, swap));
        if (!ok)
            return QVersionNumber();

        header.cpusubtype = static_cast<cpu_subtype_t>(readInt(device, &ok, swap));
        if (!ok)
            return QVersionNumber();

        header.filetype = readInt(device, &ok, swap);
        if (!ok)
            return QVersionNumber();

        header.ncmds = readInt(device, &ok, swap);
        if (!ok)
            return QVersionNumber();

        header.sizeofcmds = readInt(device, &ok, swap);
        if (!ok)
            return QVersionNumber();

        header.flags = readInt(device, &ok, swap);
        if (!ok)
            return QVersionNumber();

        header.reserved = header.magic == MH_MAGIC_64 || header.magic == MH_CIGAM_64
                ? readInt(device, &ok, swap) : 0;
        if (!ok)
            return QVersionNumber();

        for (uint32_t i = 0; i < header.ncmds; ++i) {
            const uint32_t cmd = readInt(device, nullptr, swap);
            const uint32_t cmdsize = readInt(device, nullptr, swap);
            if (cmd == 0 || cmdsize == 0)
                return QVersionNumber();

            switch (cmd) {
            case LC_VERSION_MIN_MACOSX:
            case LC_VERSION_MIN_IPHONEOS:
            case LC_VERSION_MIN_TVOS:
            case LC_VERSION_MIN_WATCHOS:
                const uint32_t version = readInt(device, &ok, swap, true);
                if (!ok)
                    return QVersionNumber();

                versions.push_back(QVersionNumber(
                                       static_cast<int>(version >> 16),
                                       static_cast<int>((version >> 8) & 0xff),
                                       static_cast<int>(version & 0xff)));
                break;
            }

            const qint64 remaining = static_cast<qint64>(cmdsize - sizeof(cmd) - sizeof(cmdsize));
            if (device->read(remaining).size() != remaining)
                return QVersionNumber();
        }
    }

    std::sort(versions.begin(), versions.end());
    return !versions.empty() ? versions.front() : QVersionNumber();
}
#endif

static int assemble(Input input, const QInstaller::Settings &settings, const QString &signingIdentity)
{
#ifdef Q_OS_OSX
    if (QInstaller::isInBundle(input.installerExePath)) {
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

        QString minimumSystemVersion;
        QFile file(input.installerExePath);
        if (file.open(QIODevice::ReadOnly))
            minimumSystemVersion = readMachOMinimumSystemVersion(&file).normalized().toString();

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
            iconFile = QString::fromLatin1(":/resources/default_icon_mac.icns");
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
        plistStream << QLatin1String("<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
                                     "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">") << endl;
        plistStream << QLatin1String("<plist version=\"1.0\">") << endl;
        plistStream << QLatin1String("<dict>") << endl;
        plistStream << QLatin1String("\t<key>CFBundleIconFile</key>") << endl;
        plistStream << QLatin1String("\t<string>") << iconTargetFile << QLatin1String("</string>")
            << endl;
        plistStream << QLatin1String("\t<key>CFBundlePackageType</key>") << endl;
        plistStream << QLatin1String("\t<string>APPL</string>") << endl;
        plistStream << QLatin1String("\t<key>CFBundleGetInfoString</key>") << endl;
#define QUOTE_(x) #x
#define QUOTE(x) QUOTE_(x)
        plistStream << QLatin1String("\t<string>") << QLatin1String(QUOTE(IFW_VERSION_STR)) << ("</string>")
            << endl;
#undef QUOTE
#undef QUOTE_
        plistStream << QLatin1String("\t<key>CFBundleSignature</key>") << endl;
        plistStream << QLatin1String("\t<string>\?\?\?\?</string>") << endl;
        plistStream << QLatin1String("\t<key>CFBundleExecutable</key>") << endl;
        plistStream << QLatin1String("\t<string>") << fi.completeBaseName() << QLatin1String("</string>")
            << endl;
        plistStream << QLatin1String("\t<key>CFBundleIdentifier</key>") << endl;
        plistStream << QLatin1String("\t<string>com.yourcompany.installerbase</string>") << endl;
        plistStream << QLatin1String("\t<key>NOTE</key>") << endl;
        plistStream << QLatin1String("\t<string>This file was generated by Qt Installer Framework.</string>")
            << endl;
        plistStream << QLatin1String("\t<key>NSPrincipalClass</key>") << endl;
        plistStream << QLatin1String("\t<string>NSApplication</string>") << endl;
        if (!minimumSystemVersion.isEmpty()) {
            plistStream << QLatin1String("\t<key>LSMinimumSystemVersion</key>") << endl;
            plistStream << QLatin1String("\t<string>") << minimumSystemVersion << QLatin1String("</string>") << endl;
        }
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
        throw Error(QString::fromLatin1("Cannot copy %1 to %2: %3").arg(input.installerExePath,
            input.outputPath, file.errorString()));
    }

    const QString tempFile = file.fileName();
    file.close();
    file.remove();

    QFile instExe(input.installerExePath);
    if (!instExe.copy(tempFile)) {
        throw Error(QString::fromLatin1("Cannot copy %1 to %2: %3").arg(instExe.fileName(),
            tempFile, instExe.errorString()));
    }

    QtPatch::patchBinaryFile(tempFile, QByteArray("MY_InstallerCreateDateTime_MY"),
        QDateTime::currentDateTime().toString(QLatin1String("yyyy-MM-dd - HH:mm:ss")).toLatin1());


    input.installerExePath = tempFile;

#if defined(Q_OS_WIN)
    // setting the windows icon must happen before we append our binary data - otherwise they get lost :-/
    if (QFile::exists(settings.installerApplicationIcon())) {
        // no error handling as this is not fatal
        setApplicationIcon(tempFile, settings.installerApplicationIcon());
    }
#elif defined(Q_OS_OSX)
    if (isBundle) {
        // no error handling as this is not fatal
        const QString copyscript = QDir::temp().absoluteFilePath(QLatin1String("copylibsintobundle.sh"));
        QFile::copy(QLatin1String(":/resources/copylibsintobundle.sh"), copyscript);
        QFile::rename(tempFile, input.outputPath);
        chmod755(copyscript);
        QProcess p;
        p.start(copyscript, QStringList() << bundle);
        p.waitForFinished(-1);
        QFile::rename(input.outputPath, tempFile);
        QFile::remove(copyscript);
    }
#endif

    QTemporaryFile out;
    QString targetName = input.outputPath;
#ifdef Q_OS_OSX
    QDir resourcePath(QFileInfo(input.outputPath).dir());
    resourcePath.cdUp();
    resourcePath.cd(QLatin1String("Resources"));
    targetName = resourcePath.filePath(QLatin1String("installer.dat"));
#endif

    {
        QFile target(targetName);
        if (target.exists() && !target.remove()) {
            qCritical("Cannot remove target %s: %s", qPrintable(target.fileName()),
                qPrintable(target.errorString()));
            QFile::remove(tempFile);
            return EXIT_FAILURE;
        }
    }

    try {
        QInstaller::openForWrite(&out);
        QFile exe(input.installerExePath);

#ifdef Q_OS_OSX
        if (!exe.copy(input.outputPath)) {
            throw Error(QString::fromLatin1("Cannot copy %1 to %2: %3").arg(exe.fileName(),
                input.outputPath, exe.errorString()));
        }
#else
        QInstaller::openForRead(&exe);
        QInstaller::appendData(&out, &exe, exe.size());
#endif

        foreach (const QInstallerTools::PackageInfo &info, input.packages) {
            QInstaller::ResourceCollection collection;
            collection.setName(info.name.toUtf8());

            qDebug() << "Creating resource archive for" << info.name;
            foreach (const QString &file, info.copiedFiles) {
                const QSharedPointer<Resource> resource(new Resource(file));
                qDebug().nospace() << "Appending " << file << " (" << humanReadableSize(resource->size()) << ")";
                collection.appendResource(resource);
            }
            input.manager.insertCollection(collection);
        }

        const QList<QInstaller::OperationBlob> operations;
        BinaryContent::writeBinaryContent(&out, operations, input.manager,
            BinaryContent::MagicInstallerMarker, BinaryContent::MagicCookie);
    } catch (const Error &e) {
        qCritical("Error occurred while assembling the installer: %s", qPrintable(e.message()));
        QFile::remove(tempFile);
        return EXIT_FAILURE;
    }

    if (!out.rename(targetName)) {
        qCritical("Cannot write installer to %s: %s", targetName.toUtf8().constData(),
            out.errorString().toUtf8().constData());
        QFile::remove(tempFile);
        return EXIT_FAILURE;
    }
    out.setAutoRemove(false);

#ifndef Q_OS_WIN
    chmod755(out.fileName());
#endif
    QFile::remove(tempFile);

#ifdef Q_OS_OSX
    if (isBundle && !signingIdentity.isEmpty()) {
        qDebug() << "Signing .app bundle...";

        QProcess p;
        p.start(QLatin1String("codesign"),
                QStringList() << QLatin1String("--force")
                              << QLatin1String("--deep")
                              << QLatin1String("--sign") << signingIdentity
                              << bundle);

        if (!p.waitForFinished(-1)) {
            qCritical("Failed to sign app bundle: error while running '%s %s': %s",
                      p.program().toUtf8().constData(),
                      p.arguments().join(QLatin1Char(' ')).toUtf8().constData(),
                      p.errorString().toUtf8().constData());
            return EXIT_FAILURE;
        }

        if (p.exitStatus() == QProcess::NormalExit) {
            if (p.exitCode() != 0) {
                qCritical("Failed to sign app bundle: running codesign failed "
                          "with exit code %d: %s", p.exitCode(),
                          p.readAllStandardError().constData());
                return EXIT_FAILURE;
            }
        }

        qDebug() << "done.";
    }

    bundleBackup.release();

    if (createDMG) {
        qDebug() << "creating a DMG disk image...";

        const QString volumeName = QFileInfo(input.outputPath).fileName();
        const QString imagePath = QString::fromLatin1("%1/%2.dmg")
                                    .arg(QFileInfo(bundle).path())
                                    .arg(volumeName);

        // no error handling as this is not fatal
        QProcess p;
        p.start(QLatin1String("/usr/bin/hdiutil"),
                QStringList() << QLatin1String("create")
                              << imagePath
                              << QLatin1String("-srcfolder")
                              << bundle
                              << QLatin1String("-ov")
                              << QLatin1String("-volname")
                              << volumeName
                              << QLatin1String("-fs")
                              << QLatin1String("HFS+"));
        qDebug() << "running " << p.program() << p.arguments();
        p.waitForFinished(-1);
        qDebug() << "removing" << bundle;
        QDir(bundle).removeRecursively();
        qDebug() <<  "done.";
    }
#else
    Q_UNUSED(signingIdentity)
#endif
    return EXIT_SUCCESS;
}

QT_BEGIN_NAMESPACE
int runRcc(int argc, char *argv[]);
QT_END_NAMESPACE

static int runRcc(const QStringList &args)
{
    const int argc = args.count();
    QVector<char*> argv(argc, nullptr);
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

static QSharedPointer<QInstaller::Resource> createDefaultResourceFile(const QString &directory,
    const QString &binaryName)
{
    QTemporaryFile projectFile(directory + QLatin1String("/rccprojectXXXXXX.qrc"));
    if (!projectFile.open())
        throw Error(QString::fromLatin1("Cannot create temporary file for generated rcc project file"));
    projectFile.close();

    const WorkingDirectoryChange wd(directory);
    const QString projectFileName = QFileInfo(projectFile.fileName()).absoluteFilePath();

    // 1. create the .qrc file
    if (runRcc(QStringList() << QLatin1String("rcc") << QLatin1String("-project") << QLatin1String("-o")
        << projectFileName) != EXIT_SUCCESS) {
            throw Error(QString::fromLatin1("Cannot create rcc project file."));
    }

    // 2. create the binary resource file from the .qrc file
    if (runRcc(QStringList() << QLatin1String("rcc") << QLatin1String("-binary") << QLatin1String("-o")
        << binaryName << projectFileName) != EXIT_SUCCESS) {
            throw Error(QString::fromLatin1("Cannot compile rcc project file."));
    }

    return QSharedPointer<QInstaller::Resource>(new QInstaller::Resource(binaryName, binaryName
        .toUtf8()));
}

static
QList<QSharedPointer<QInstaller::Resource> > createBinaryResourceFiles(const QStringList &resources)
{
    QList<QSharedPointer<QInstaller::Resource> > result;
    foreach (const QString &resource, resources) {
        QFile file(resource);
        if (file.exists()) {
            const QString binaryName = generateTemporaryFileName();
            const QString fileName = QFileInfo(file.fileName()).absoluteFilePath();
            const int status = runRcc(QStringList() << QLatin1String("rcc")
                << QLatin1String("-binary") << QLatin1String("-o") << binaryName << fileName);
            if (status != EXIT_SUCCESS)
                continue;

            result.append(QSharedPointer<QInstaller::Resource> (new QInstaller::Resource(binaryName,
                binaryName.toUtf8())));
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
#ifdef Q_OS_OSX
    std::cout << "  -s|--sign identity        Sign generated app bundle using the given code " << std::endl;
    std::cout << "                            signing identity" << std::endl;
#endif
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
    QInstaller::openForRead(&configXml);

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
        qDebug().noquote() << QString::fromLatin1("Read dom element: <%1>%2</%1>.").arg(tagName, elementText);

        QString newName = domElement.text().replace(QRegExp(QLatin1String("\\\\|/|\\.|:")),
            QLatin1String("_"));

        QString targetFile;
        QFileInfo elementFileInfo;
        if (tagName == QLatin1String("Icon") || tagName == QLatin1String("InstallerApplicationIcon")) {
#if defined(Q_OS_OSX)
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

    QInstaller::openForWrite(&configXml);
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
// increase maximum numbers of file descriptors
#if defined (Q_OS_MACOS)
    struct rlimit rl;
    getrlimit(RLIMIT_NOFILE, &rl);
    rl.rlim_cur = qMin(static_cast<rlim_t>(OPEN_MAX), rl.rlim_max);
    setrlimit(RLIMIT_NOFILE, &rl);
#endif
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
    QStringList repositoryDirectories;
    bool onlineOnly = false;
    bool offlineOnly = false;
    QStringList resources;
    QStringList filteredPackages;
    QInstallerTools::FilterType ftype = QInstallerTools::Exclude;
    bool compileResource = false;
    QString signingIdentity;

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
        } else if (*it == QLatin1String("--repository")) {
            ++it;
            if (it == args.end()) {
                return printErrorAndUsageAndExit(QString::fromLatin1("Error: Repository parameter missing argument."));
            }
            if (QFileInfo(*it).exists()) {
                repositoryDirectories.append(*it);
            } else {
                return printErrorAndUsageAndExit(QString::fromLatin1("Error: Only local filesystem repositories now supported."));
            }
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
#ifdef Q_OS_OSX
        } else if (*it == QLatin1String("-s") || *it == QLatin1String("--sign")) {
            ++it;
            if (it == args.end() || it->startsWith(QLatin1String("-")))
                return printErrorAndUsageAndExit(QString::fromLatin1("Error: No code signing identity specified."));
            signingIdentity = *it;
#endif
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

    if (packagesDirectories.isEmpty() && repositoryDirectories.isEmpty())
        return printErrorAndUsageAndExit(QString::fromLatin1("Error: Both Package directory and Repository parameters missing."));

    qDebug() << "Parsed arguments, ok.";

    Input input;
    int exitCode = EXIT_FAILURE;
    QTemporaryDir tmp;
    tmp.setAutoRemove(false);
    const QString tmpMetaDir = tmp.path();
    QTemporaryDir tmp2;
    tmp2.setAutoRemove(false);
    const QString tmpRepoDir = tmp2.path();
    try {
        const Settings settings = Settings::fromFileAndPrefix(configFile, QFileInfo(configFile)
            .absolutePath());

        // Note: the order here is important

        QInstallerTools::PackageInfoVector packages;

        // 1; update the list of available compressed packages
        if (!repositoryDirectories.isEmpty()) {
            // 1.1; search packages
            QInstallerTools::PackageInfoVector precompressedPackages = QInstallerTools::createListOfRepositoryPackages(repositoryDirectories,
                &filteredPackages, ftype);
            // 1.2; add to common vector
            packages.append(precompressedPackages);
        }

        // 2; update the list of available prepared packages
        if (!packagesDirectories.isEmpty()) {
            // 2.1; search packages
            QInstallerTools::PackageInfoVector preparedPackages = QInstallerTools::createListOfPackages(packagesDirectories,
                &filteredPackages, ftype);
            // 2.2; copy the packages data and setup the packages vector with the files we copied,
            //    must happen before copying meta data because files will be compressed if
            //    needed and meta data generation relies on this
            QInstallerTools::copyComponentData(packagesDirectories, tmpRepoDir, &preparedPackages);
            // 2.3; add to common vector
            packages.append(preparedPackages);
        }

        // 3; copy the meta data of the available packages, generate Updates.xml
        QInstallerTools::copyMetaData(tmpMetaDir, tmpRepoDir, packages, settings
            .applicationName(), settings.version());

        // 4; copy the configuration file and and icons etc.
        copyConfigData(configFile, tmpMetaDir + QLatin1String("/installer-config"));
        {
            QSettings confInternal(tmpMetaDir + QLatin1String("/config/config-internal.ini")
                , QSettings::IniFormat);
            // assume offline installer if there are no repositories and no
            //--online-only not set
            offlineOnly = offlineOnly | settings.repositories().isEmpty();
            if (onlineOnly)
                offlineOnly = !onlineOnly;
            confInternal.setValue(QLatin1String("offlineOnly"), offlineOnly);
        }

#ifdef Q_OS_OSX
        // on mac, we enforce building a bundle
        if (!target.endsWith(QLatin1String(".app")) && !target.endsWith(QLatin1String(".dmg")))
            target += QLatin1String(".app");
#endif
        if (!compileResource) {
            // 5; put the copied resources into a resource file
            QInstaller::ResourceCollection metaCollection("QResources");
            metaCollection.appendResource(createDefaultResourceFile(tmpMetaDir,
                generateTemporaryFileName()));
            metaCollection.appendResources(createBinaryResourceFiles(resources));
            input.manager.insertCollection(metaCollection);

            input.packages = packages;
            input.outputPath = target;
            input.installerExePath = templateBinary;

            qDebug() << "Creating the binary";
            exitCode = assemble(input, settings, signingIdentity);
        } else {
            createDefaultResourceFile(tmpMetaDir, QDir::currentPath() + QLatin1String("/update.rcc"));
            exitCode = EXIT_SUCCESS;
        }
    } catch (const Error &e) {
        QFile::remove(input.outputPath);
        std::cerr << "Caught exception: " << e.message() << std::endl;
    } catch (...) {
        QFile::remove(input.outputPath);
        std::cerr << "Unknown exception caught" << std::endl;
    }

    qDebug() << "Cleaning up...";
    const QInstaller::ResourceCollection collection = input.manager.collectionByName("QResources");
    foreach (const QSharedPointer<QInstaller::Resource> &resource, collection.resources())
        QFile::remove(QString::fromUtf8(resource->name()));
    QInstaller::removeDirectory(tmpMetaDir, true);
    QInstaller::removeDirectory(tmpRepoDir, true);

    return exitCode;
}
