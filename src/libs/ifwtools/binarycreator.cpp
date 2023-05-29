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

#include "binarycreator.h"

#include "qtpatch.h"
#include "repositorygen.h"
#include "binarycontent.h"
#include "errors.h"
#include "fileio.h"
#include "init.h"
#include "repository.h"
#include "settings.h"
#include "utils.h"
#include "fileutils.h"

#include <QDateTime>
#include <QDirIterator>
#include <QDomDocument>
#include <QProcess>
#include <QRegularExpression>
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
using namespace QInstallerTools;

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

static int assemble(Input input, const QInstaller::Settings &settings, const BinaryCreatorArgs &args)
{
#ifdef Q_OS_MACOS
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
            pkgInfoStream << QLatin1String("APPL????") << Qt::endl;
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
        plistStream << QLatin1String("<?xml version=\"1.0\" encoding=\"UTF-8\"?>") << Qt::endl;
        plistStream << QLatin1String("<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
                                     "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">") << Qt::endl;
        plistStream << QLatin1String("<plist version=\"1.0\">") << Qt::endl;
        plistStream << QLatin1String("<dict>") << Qt::endl;
        plistStream << QLatin1String("\t<key>CFBundleIconFile</key>") << Qt::endl;
        plistStream << QLatin1String("\t<string>") << iconTargetFile << QLatin1String("</string>")
            << Qt::endl;
        plistStream << QLatin1String("\t<key>CFBundlePackageType</key>") << Qt::endl;
        plistStream << QLatin1String("\t<string>APPL</string>") << Qt::endl;
#define QUOTE_(x) #x
#define QUOTE(x) QUOTE_(x)
        plistStream << QLatin1String("\t<key>CFBundleShortVersionString</key>") << Qt::endl;
        plistStream << QLatin1String("\t<string>") << QLatin1String(QUOTE(IFW_VERSION_STR)) << ("</string>")
            << Qt::endl;
        plistStream << QLatin1String("\t<key>CFBundleVersion</key>") << Qt::endl;
        plistStream << QLatin1String("\t<string>") << QLatin1String(QUOTE(IFW_VERSION_STR)) << ("</string>")
            << Qt::endl;
#undef QUOTE
#undef QUOTE_
        plistStream << QLatin1String("\t<key>CFBundleSignature</key>") << Qt::endl;
        plistStream << QLatin1String("\t<string>\?\?\?\?</string>") << Qt::endl;
        plistStream << QLatin1String("\t<key>CFBundleExecutable</key>") << Qt::endl;
        plistStream << QLatin1String("\t<string>") << fi.completeBaseName() << QLatin1String("</string>")
            << Qt::endl;
        plistStream << QLatin1String("\t<key>CFBundleIdentifier</key>") << Qt::endl;
        plistStream << QLatin1String("\t<string>com.yourcompany.installerbase</string>") << Qt::endl;
        plistStream << QLatin1String("\t<key>NOTE</key>") << Qt::endl;
        plistStream << QLatin1String("\t<string>This file was generated by Qt Installer Framework.</string>")
            << Qt::endl;
        plistStream << QLatin1String("\t<key>NSPrincipalClass</key>") << Qt::endl;
        plistStream << QLatin1String("\t<string>NSApplication</string>") << Qt::endl;
        if (!minimumSystemVersion.isEmpty()) {
            plistStream << QLatin1String("\t<key>LSMinimumSystemVersion</key>") << Qt::endl;
            plistStream << QLatin1String("\t<string>") << minimumSystemVersion << QLatin1String("</string>") << Qt::endl;
        }
        plistStream << QLatin1String("</dict>") << Qt::endl;
        plistStream << QLatin1String("</plist>") << Qt::endl;

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
#elif defined(Q_OS_MACOS)
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

    QFile out(generateTemporaryFileName());

    QString targetName = input.outputPath;
#ifdef Q_OS_MACOS
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

#ifdef Q_OS_MACOS
        if (!exe.copy(input.outputPath)) {
            throw Error(QString::fromLatin1("Cannot copy %1 to %2: %3").arg(exe.fileName(),
                input.outputPath, exe.errorString()));
        }
#else
        QInstaller::openForRead(&exe);
        QInstaller::appendData(&out, &exe, exe.size());
#endif

        if (!args.createMaintenanceTool) {
            foreach (const QInstallerTools::PackageInfo &info, input.packages) {
                QInstaller::ResourceCollection collection;
                collection.setName(info.name.toUtf8());
                qDebug() << "Creating resource archive for" << info.name;
                foreach (const QString &copiedFile, info.copiedFiles) {
                    const QSharedPointer<Resource> resource(new Resource(copiedFile));
                    qDebug().nospace() << "Appending " << copiedFile << " (" << humanReadableSize(resource->size()) << ")";
                    collection.appendResource(resource);
                }
                input.manager.insertCollection(collection);
            }

            const QList<QInstaller::OperationBlob> operations;
            BinaryContent::writeBinaryContent(&out, operations, input.manager,
                BinaryContent::MagicInstallerMarker, BinaryContent::MagicCookie);
        } else {
            createMTDatFile(out);
        }

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

#ifdef Q_OS_MACOS
    // installer executable
    chmod755(input.outputPath);
#endif
#ifndef Q_OS_WIN
    // installer executable on linux, installer.dat on macOS
    chmod755(targetName);
#endif
    QFile::remove(tempFile);

#ifdef Q_OS_MACOS
    if (isBundle && !args.signingIdentity.isEmpty()) {
        qDebug() << "Signing .app bundle...";

        QProcess p;
        p.start(QLatin1String("codesign"),
                QStringList() << QLatin1String("--force")
                              << QLatin1String("--deep")
                              << QLatin1String("--sign") << args.signingIdentity
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

void QInstallerTools::copyConfigData(const QString &configFile, const QString &targetDir)
{
    qDebug() << "Begin to copy configuration file and data.";

    const QString sourceConfigFile = QFileInfo(configFile).absoluteFilePath();
    const QString targetConfigFile = targetDir + QLatin1String("/config.xml");
    QInstallerTools::copyWithException(sourceConfigFile, targetConfigFile, QLatin1String("configuration"));

    // Permissions might be set to bogus values
    QInstaller::setDefaultFilePermissions(targetConfigFile, DefaultFilePermissions::NonExecutable);

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

        if (tagName == QLatin1String("ProductImages")) {
            const QDomNodeList productImageNode = domElement.childNodes();
            for (int j = 0; j < productImageNode.count(); ++j) {
                QDomElement productImagesElement = productImageNode.at(j).toElement();
                if (productImagesElement.isNull())
                    continue;
                const QString childName = productImagesElement.tagName();
                if (childName != QLatin1String("ProductImage"))
                    continue;
                const QDomNodeList imageNode = productImagesElement.childNodes();
                for (int k = 0; k < imageNode.count(); ++k) {
                    QDomElement productImageElement = imageNode.at(k).toElement();
                    if (productImageElement.isNull())
                        continue;
                    const QString imageChildName = productImageElement.tagName();
                    if (imageChildName != QLatin1String("Image"))
                        continue;
                    const QString targetFile = targetDir + QLatin1Char('/') + productImageElement.text();
                    const QFileInfo childFileInfo = QFileInfo(sourceConfigFilePath, productImageElement.text());
                    QInstallerTools::copyWithException(childFileInfo.absoluteFilePath(), targetFile, imageChildName);
                    copyHighDPIImage(childFileInfo, imageChildName, targetFile);
                }

            }
            continue;
        }

        static const QRegularExpression regex(QLatin1String("\\\\|/|\\.|:"));
        QString newName = domElement.text().replace(regex, QLatin1String("_"));

        QString targetFile;
        QFileInfo elementFileInfo;
        if (tagName == QLatin1String("InstallerApplicationIcon")) {
#if defined(Q_OS_MACOS)
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
        copyHighDPIImage(elementFileInfo, tagName, targetFile);
    }

    QInstaller::openForWrite(&configXml);
    QTextStream stream(&configXml);
    dom.save(stream, 4);

    qDebug() << "done.\n";
}

void QInstallerTools::copyHighDPIImage(const QFileInfo &childFileInfo,
            const QString &childName, const QString &targetFile)
{
    //Copy also highdpi image if present
    const QFileInfo childFileInfoHighDpi = QFileInfo(childFileInfo.absolutePath(), childFileInfo.baseName() + scHighDpi + childFileInfo.suffix());
    if (childFileInfoHighDpi.exists()) {
        const QFileInfo tf(targetFile);
        const QString highDpiTarget = tf.absolutePath() + QLatin1Char('/') + tf.baseName() + scHighDpi + tf.suffix();
        QInstallerTools::copyWithException(childFileInfoHighDpi.absoluteFilePath(), highDpiTarget, childName);
    }
}

int QInstallerTools::createBinary(BinaryCreatorArgs args, QString &argumentError)
{
    // increase maximum numbers of file descriptors
#if defined (Q_OS_MACOS)
    struct rlimit rl;
    getrlimit(RLIMIT_NOFILE, &rl);
    rl.rlim_cur = qMin(static_cast<rlim_t>(OPEN_MAX), rl.rlim_max);
    setrlimit(RLIMIT_NOFILE, &rl);
#endif
    QString suffix;
#ifdef Q_OS_WIN
    suffix = QLatin1String(".exe");
    if (!args.target.endsWith(suffix))
        args.target = args.target + suffix;
#endif

    // Begin check arguments
    foreach (const QString &packageDir, args.packagesDirectories) {
        if (!QFileInfo::exists(packageDir)) {
            argumentError = QString::fromLatin1("Error: Package directory not found at the specified location.");
            return EXIT_FAILURE;
        }
    }
    foreach (const QString &repositoryDir, args.repositoryDirectories) {
        if (!QFileInfo::exists(repositoryDir)) {
            argumentError = QString::fromLatin1("Error: Only local filesystem repositories now supported.");
            return EXIT_FAILURE;
        }
    }
    if (!args.filteredPackages.isEmpty() && args.onlineOnly) {
        argumentError = QString::fromLatin1("Error: 'online-only' option cannot be used "
            "in conjunction with the 'include' or 'exclude' option. An 'online-only' installer will never "
            "contain any components apart from the root component.");
        return EXIT_FAILURE;
    }
    if (!QFileInfo::exists(args.templateBinary)) {
#ifdef Q_OS_WIN
        if (!args.templateBinary.endsWith(suffix))
            args.templateBinary = args.templateBinary + suffix;
        // Try again with added executable suffix
        if (!QFileInfo::exists(args.templateBinary)) {
            argumentError = QString::fromLatin1("Error: Template base binary not found at the specified location.");
            return EXIT_FAILURE;
        }
#else
        argumentError = QString::fromLatin1("Error: Template not found at the specified location.");
        return EXIT_FAILURE;
#endif
    }
    const QFileInfo fi(args.configFile);
    if (!fi.exists()) {
        argumentError = QString::fromLatin1("Error: Config file %1 not found at the "
            "specified location.").arg(fi.absoluteFilePath());
        return EXIT_FAILURE;
    }
    if (!fi.isFile()) {
        argumentError = QString::fromLatin1("Error: Configuration %1 is not a file.")
            .arg(fi.absoluteFilePath());
        return EXIT_FAILURE;
    }
    if (!fi.isReadable()) {
        argumentError = QString::fromLatin1("Error: Config file %1 is not readable.")
            .arg(fi.absoluteFilePath());
        return EXIT_FAILURE;
    }
    if (args.onlineOnly && args.offlineOnly) {
        argumentError = QString::fromLatin1("You cannot use --online-only and "
            "--offline-only at the same time.");
        return EXIT_FAILURE;
    }
    if (args.target.isEmpty() && !args.compileResource && !args.createMaintenanceTool) {
        argumentError = QString::fromLatin1("Error: Target parameter missing.");
        return EXIT_FAILURE;
    }
    if (args.configFile.isEmpty()) {
        argumentError = QString::fromLatin1("Error: No configuration file selected.");
        return EXIT_FAILURE;
    }
    if (args.packagesDirectories.isEmpty() && args.repositoryDirectories.isEmpty()
            && !args.compileResource
            && !args.createMaintenanceTool) {
        argumentError = QString::fromLatin1("Error: Both Package directory and Repository parameters missing.");
        return EXIT_FAILURE;
    }
    if (args.onlineOnly) {
        args.filteredPackages.append(QLatin1String("X_fake_filter_component_for_online_only_installer_X"));
        args.ftype = QInstallerTools::Include;
    }
    // End check arguments
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
        const Settings settings = Settings::fromFileAndPrefix(args.configFile, QFileInfo(args.configFile)
            .absolutePath());

        // Note: the order here is important

        PackageInfoVector packages;
        QStringList unite7zFiles;

        // 1; update the list of available compressed packages
        if (!args.repositoryDirectories.isEmpty()) {
            // 1.1; search packages
            PackageInfoVector precompressedPackages = createListOfRepositoryPackages(args.repositoryDirectories,
                &args.filteredPackages, args.ftype);
            // 1.2; add to common vector
            packages.append(precompressedPackages);
            // 1.3; create list of unified metadata archives
            foreach (const QString &dir, args.repositoryDirectories) {
                QDirIterator it(dir, QStringList(QLatin1String("*_meta.7z")), QDir::Files | QDir::CaseSensitive);
                while (it.hasNext()) {
                    it.next();
                    unite7zFiles.append(it.fileInfo().absoluteFilePath());
                }
            }
        }

        // 2; update the list of available prepared packages
        if (!args.packagesDirectories.isEmpty()) {
            // 2.1; search packages
            PackageInfoVector preparedPackages = createListOfPackages(args.packagesDirectories,
                &args.filteredPackages, args.ftype);
            // 2.2; copy the packages data and setup the packages vector with the files we copied,
            //    must happen before copying meta data because files will be compressed if
            //    needed and meta data generation relies on this
            copyComponentData(args.packagesDirectories, tmpRepoDir, &preparedPackages,
                args.archiveSuffix, args.compression);
            // 2.3; add to common vector
            packages.append(preparedPackages);
        }

        // 3; copy the meta data of the available packages, generate Updates.xml
        copyMetaData(tmpMetaDir, tmpRepoDir, packages, settings
            .applicationName(), settings.version(), unite7zFiles);

        // 4; copy the configuration file and and icons etc.
        copyConfigData(args.configFile, tmpMetaDir + QLatin1String("/installer-config"));
        {
            QSettings confInternal(tmpMetaDir + QLatin1String("/config/config-internal.ini")
                , QSettings::IniFormat);
            // assume offline installer if there are no repositories and no
            //--online-only not set
            args.offlineOnly = args.offlineOnly | settings.repositories().isEmpty();
            if (args.onlineOnly)
                args.offlineOnly = !args.onlineOnly;
            confInternal.setValue(QLatin1String("offlineOnly"), args.offlineOnly);
        }

        if (!args.compileResource) {
            // 5; put the copied resources into a resource file
            ResourceCollection metaCollection("QResources");
            metaCollection.appendResource(createDefaultResourceFile(tmpMetaDir,
                generateTemporaryFileName()));
            metaCollection.appendResources(createBinaryResourceFiles(args.resources));
            input.manager.insertCollection(metaCollection);

            input.packages = packages;
            if (args.createMaintenanceTool)
                input.outputPath = settings.maintenanceToolName();
            else
                input.outputPath = args.target;
            input.installerExePath = args.templateBinary;

#ifdef Q_OS_MACOS
        // on mac, we enforce building a bundle
        if (!input.outputPath.endsWith(QLatin1String(".app")) && !input.outputPath.endsWith(QLatin1String(".dmg")))
            input.outputPath += QLatin1String(".app");
#endif

            qDebug() << "Creating the binary";
            exitCode = assemble(input, settings, args);
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
    const ResourceCollection collection = input.manager.collectionByName("QResources");
    foreach (const QSharedPointer<QInstaller::Resource> &resource, collection.resources())
        QFile::remove(QString::fromUtf8(resource->name()));
    QInstaller::removeDirectory(tmpMetaDir, true);
    QInstaller::removeDirectory(tmpRepoDir, true);

    return exitCode;
}

void QInstallerTools::createMTDatFile(QFile &datFile)
{
    QInstaller::appendInt64(&datFile, 0);   // operations start
    QInstaller::appendInt64(&datFile, 0);   // operations end
    QInstaller::appendInt64(&datFile, 0);   // resource count
    QInstaller::appendInt64(&datFile, 4 * sizeof(qint64));   // data block size
    QInstaller::appendInt64(&datFile, BinaryContent::MagicUninstallerMarker);
    QInstaller::appendInt64(&datFile, BinaryContent::MagicCookie);
}
