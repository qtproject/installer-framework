/**************************************************************************
**
** This file is part of Qt SDK**
**
** Copyright (c) 2011 Nokia Corporation and/or its subsidiary(-ies).*
**
** Contact:  Nokia Corporation qt-info@nokia.com**
**
** No Commercial Usage
**
** This file contains pre-release code and may not be distributed.
** You may use this file in accordance with the terms and conditions
** contained in the Technology Preview License Agreement accompanying
** this package.
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
** rights. These rights are described in the Nokia Qt LGPL Exception version
** 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** If you are unsure which license is appropriate for your use, please contact
** (qt-info@nokia.com).
**
**************************************************************************/
#include "repositorygen.h"

#include <common/fileutils.h>
#include <common/errors.h>
#include <common/utils.h>
#include <common/consolepasswordprovider.h>
#include <settings.h>

#include <KDUpdater/KDUpdater>

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QDomAttr>
#include <QDomDocument>
#include <QTemporaryFile>
#include <QVector>

#include "lib7z_facade.h"

#include <cassert>

using namespace QInstaller;

QT_BEGIN_NAMESPACE
static bool operator==(const PackageInfo& lhs, const PackageInfo& rhs)
{
    return lhs.name == rhs.name && lhs.version == rhs.version;
}
QT_END_NAMESPACE

static QVector<PackageInfo> collectAvailablePackages(const QString& packagesDirectory)
{
    verbose() << "Collecting information about available packages..." << std::endl;

    QVector< PackageInfo > dict;
    const QFileInfoList entries = QDir(packagesDirectory)
        .entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (QFileInfoList::const_iterator it = entries.begin(); it != entries.end(); ++it) {
        verbose() << "  found subdirectory \"" << it->fileName() << "\"";
        //because the filter is QDir::Dirs - filename means the name of the subdirectory
        if (it->fileName().contains(QLatin1Char('-'))) {
            verbose() << ", but it contains \"-\" which is not allowed, because it is used as the seperator between the component name and the version number internally."
                << std::endl;
            throw QInstaller::Error(QObject::tr("Component %1 can't contain \"-\"").arg(it->fileName()));
        }

        QFile file(QString::fromLatin1("%1/meta/package.xml").arg(it->filePath()));
        if (!file.exists()) {
            verbose() << ", but it contains no package information (meta/package.xml missing)"
                << std::endl;
            throw QInstaller::Error(QObject::tr("Component %1 does not contain a package "
                "description.").arg(it->fileName()));
        }

        file.open(QIODevice::ReadOnly);

        QDomDocument doc;
        QString errorMessage;
        int errorLine = 0;
        int errorColumn = 0;
        if (!doc.setContent(&file, &errorMessage, &errorLine, &errorColumn)) {
            verbose() << ", but it's package description is invalid. Error at " << errorLine
                << ", " << errorColumn << ": " << errorMessage << std::endl;
            throw QInstaller::Error(QObject::tr("Component package description for %1 is invalid. "
                "Error at line: %2, column: %3 -> %4").arg(it->fileName(), QString::number(errorLine),
                QString::number(errorColumn), errorMessage));
        }

        const QString name = doc.firstChildElement(QString::fromLatin1("Package"))
            .firstChildElement(QLatin1String("Name")).text();
        if (name != it->fileName()) {
            verbose() << std::endl;
            throw QInstaller::Error(QObject::tr("Component folder name must match component name: "
                "\"%1\" in %2/").arg(name, it->fileName()));
        }

        PackageInfo info;
        info.name = name;
        info.version = doc.firstChildElement(QString::fromLatin1("Package")).
            firstChildElement(QString::fromLatin1("Version")).text();
        if (!QRegExp(QLatin1String("[0-9]+((\\.|-)[0-9]+)*")).exactMatch(info.version)) {
            verbose() << std::endl;
            throw QInstaller::Error(QObject::tr("Component version for %1 is invalid! <Version>%2</version>")
                .arg(it->fileName(), info.version));
        }
        info.dependencies = doc.firstChildElement(QString::fromLatin1("Package")).
            firstChildElement(QString::fromLatin1("Dependencies")).text().split(QRegExp(QLatin1String("\\b(,|, )\\b")),
            QString::SkipEmptyParts);
        info.directory = it->filePath();
        dict.push_back(info);

        verbose() << ", it provides the package " <<name;
        if (!info.version.isEmpty())
            verbose() << "-" << info.version;
        verbose() << std::endl;
    }

    if (dict.isEmpty())
        verbose() << "No available packages found at the specified location." << std::endl;

    return dict;
}

static PackageInfo findMatchingPackage(const QString& name, const QVector< PackageInfo >& available)
{
    const QString id = name.contains(QChar::fromLatin1('-'))
        ? name.section(QChar::fromLatin1('-'), 0, 0) : name;
    QString version = name.contains(QChar::fromLatin1('-'))
        ? name.section(QChar::fromLatin1('-'), 1, -1) : QString();

    QRegExp compEx(QLatin1String("([<=>]+)(.*)"));
    const QString comparator = compEx.exactMatch(version)
        ? compEx.cap(1) : QString::fromLatin1("=");
    version = compEx.exactMatch(version) ? compEx.cap(2) : version;

    const bool allowEqual = comparator.contains(QLatin1Char('='));
    const bool allowLess = comparator.contains(QLatin1Char('<'));
    const bool allowMore = comparator.contains(QLatin1Char('>'));

    for (QVector< PackageInfo >::const_iterator it = available.begin(); it != available.end(); ++it) {
        if (it->name != id)
            continue;

        if (allowEqual && (version.isEmpty() || it->version == version))
            return *it;
        else if (allowLess && KDUpdater::compareVersion(version, it->version) > 0)
            return *it;
        else if (allowMore && KDUpdater::compareVersion(version, it->version) < 0)
            return *it;
    }

    return PackageInfo();
}

/**
 * Returns true, when the \a package's identifier starts with, but not equals \a prefix.
 */
static bool packageHasPrefix(const PackageInfo& package, const QString& prefix)
{
    return package.name.startsWith(prefix) && package.name.mid(prefix.length(), 1)
        == QString::fromLatin1(".");
}

/**
 * Returns true, whel all \a packages start with \a prefix
 */
static bool allPackagesHavePrefix(const QVector< PackageInfo >& packages, const QString& prefix)
{
    for (QVector< PackageInfo >::const_iterator it = packages.begin(); it != packages.end(); ++it) {
        if (!packageHasPrefix(*it, prefix))
            return false;
    }
    return true;
}

/**
 * Returns all packages out of \a all starting with \a prefix.
 */
static QVector< PackageInfo > packagesWithPrefix(const QVector< PackageInfo >& all,
    const QString& prefix)
{
    QVector< PackageInfo > result;
    for (QVector< PackageInfo >::const_iterator it = all.begin(); it != all.end(); ++it) {
        if (packageHasPrefix(*it, prefix))
            result.push_back(*it);
    }
    return result;
}

static QVector< PackageInfo > calculateNeededPackages(const QStringList& components,
    const QVector< PackageInfo >& available, bool addDependencies = true)
{
    QVector< PackageInfo > result;

    for (QStringList::const_iterator it = components.begin(); it != components.end(); ++it) {
        static bool recursion = false;
        static QStringList hitComponents;

        if (!recursion)
            hitComponents.clear();

        if (hitComponents.contains(*it))
            throw Error(QObject::tr("Circular dependencies detected").arg(*it));
        hitComponents.push_back(*it);

        recursion = true;

        verbose() << "Trying to find a package for name " << *it << "... ";
        const PackageInfo info = findMatchingPackage(*it, available);
        if (info.name.isEmpty()) {
            verbose() << "Not found :-o" << std::endl;
            verbose() << "  Couldn't find package for component " << *it << " bailing out..."
                << std::endl;
            throw Error(QObject::tr("Couldn't find package for component %1").arg(*it));
        }
        verbose() << "Found." << std::endl;
        if (!result.contains(info)) {
            result.push_back(info);

            if (addDependencies) {
                QVector< PackageInfo > dependencies;

                if (!info.dependencies.isEmpty()) {
                    verbose() << "  It depends on:" << std::endl;
                    for (QStringList::const_iterator dep = info.dependencies.begin();
                        dep != info.dependencies.end(); ++dep)
                        verbose() << "    " << *dep << std::endl;
                    dependencies += calculateNeededPackages(info.dependencies, available);
                }
                // append all child items, as this package was requested explicitely
                dependencies += packagesWithPrefix(available, info.name);

                for (QVector< PackageInfo >::const_iterator dep = dependencies.begin();
                    dep != dependencies.end(); ++dep) {
                    if (result.contains(*dep))
                        continue;

                    result += *dep;
                    const QVector< PackageInfo > depdeps = calculateNeededPackages(QStringList()
                        << dep->name, available);
                    for (QVector< PackageInfo >::const_iterator dep2 = depdeps.begin();
                        dep2 != depdeps.end(); ++dep2)
                        if (!result.contains(*dep2))
                            result += *dep2;
                }
            }
        }

        recursion = false;
    }

    return result;
}

namespace {
    struct ArchiveFile {
        ArchiveFile() : uncompressedSize(0) {}
        quint64 uncompressedSize;
        QByteArray sha1sum;
        QString fileName;
    };
}

void QInstaller::compressDirectory(const QStringList& paths, const QString& archivePath)
{
    foreach (QString path, paths) {
        if (!QFileInfo(path).exists())
            throw QInstaller::Error(QObject::tr("Folder %1 does not exist").arg(path));
    }

    QFile archive(archivePath);
    openForWrite(&archive, archivePath);
    Lib7z::createArchive(&archive, paths);
}

void QInstaller::compressMetaDirectories(const QString& configDir, const QString& repoDir)
{
    const QString configfile = QFileInfo(configDir, QLatin1String("config.xml")).absoluteFilePath();
    const QInstaller::Settings &settings = QInstaller::Settings::fromFileAndPrefix(configfile, configDir);

    KDUpdaterCrypto crypto;
    crypto.setPrivateKey(settings.privateKey());
    ConsolePasswordProvider passwordProvider;
    crypto.setPrivatePasswordProvider(&passwordProvider);

    QDir dir(repoDir);
    const QStringList sub = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    Q_FOREACH (const QString& i, sub) {
        QDir sd(dir);
        sd.cd(i);
        const QString absPath = sd.absolutePath();
        const QString fn = QLatin1String("meta.7z");
        const QString tmpTarget = repoDir + QLatin1String("/") +fn;
        compressDirectory(QStringList() << absPath, tmpTarget);
        QFile tmp(tmpTarget);
        const QString finalTarget = absPath + QLatin1String("/") + fn;
        if (!tmp.rename(finalTarget)) {
            throw QInstaller::Error(QObject::tr("Could not move %1 to %2").arg(tmpTarget,
                finalTarget));
        }

        // if we have a private key, sign the meta.7z file
        if (!settings.privateKey().isEmpty()) {
            verbose() << "Adding a RSA signature to " << finalTarget << std::endl;
            const QByteArray signature = crypto.sign(finalTarget);
            QFile sigFile(finalTarget + QLatin1String(".sig"));
            if (!sigFile.open(QIODevice::WriteOnly)) {
                throw QInstaller::Error(QObject::tr("Could not open %1 for writing")
                    .arg(finalTarget));
            }
            sigFile.write(signature);
        }
    }
}

void QInstaller::generateMetaDataDirectory(const QString& metapath_, const QString& dataDir,
    const QVector< PackageInfo >& packages, const QString& appName, const QString& appVersion)
{
    QString metapath = metapath_;
    if (QFileInfo(metapath).isRelative())
        metapath = QDir::cleanPath(QDir::current().absoluteFilePath(metapath));
    verbose() << "Generating meta data..." << std::endl;

    if (!QFile::exists(metapath))
        QInstaller::mkpath(metapath);

    QDomDocument doc;
    QDomElement root;
    // use existing Updates.xml, if any
    QFile existingUpdatesXml(QFileInfo(dataDir, QLatin1String("Updates.xml")).absoluteFilePath());
    if (!existingUpdatesXml.open(QIODevice::ReadOnly) || !doc.setContent(&existingUpdatesXml)) {
        root = doc.createElement("Updates");
        root.appendChild(doc.createElement("ApplicationName")).appendChild(
            doc.createTextNode(appName));
        root.appendChild(doc.createElement("ApplicationVersion")).appendChild(
            doc.createTextNode(appVersion));
        root.appendChild(doc.createElement("Checksum")).appendChild(
            doc.createTextNode(QLatin1String("true")));
    } else {
        root = doc.documentElement();
    }

    for (QVector< PackageInfo >::const_iterator it = packages.begin(); it != packages.end(); ++it) {
        const QString packageXmlPath = QString::fromLatin1("%1/meta/package.xml").arg(it->directory);
        verbose() << "  Generating meta data for package " << it->name << " using "
            << packageXmlPath << std::endl;;

        // remove existing entry for thes component from existing Updates.xml
        const QDomNodeList packageNodes = root.childNodes();
        for (int i = 0; i < packageNodes.count(); ++i) {
            const QDomNode node = packageNodes.at(i);
            if (node.nodeName() != QLatin1String("PackageUpdate"))
                continue;
            if (node.firstChildElement(QLatin1String("Name")).text() != it->name)
                continue;
            root.removeChild(node);
            --i;
        }

        QDomDocument packageXml;
        QFile file(packageXmlPath);
        openForRead(&file, packageXmlPath);
        QString errMsg;
        int col = 0;
        int line = 0;
        if (!packageXml.setContent(&file, &errMsg, &line, &col)) {
            throw Error(QObject::tr("Could not parse %1: %2:%3: %4 (%5)").arg(packageXmlPath,
                QString::number(line), QString::number(col), errMsg, it->name));
        }
        const QDomNode package = packageXml.firstChildElement("Package");

        QDomElement update = doc.createElement("PackageUpdate");

        const QDomNodeList childNodes = package.childNodes();
        for (int i = 0; i < childNodes.count(); ++i) {
            const QDomNode node = childNodes.at(i);
            // just skip the comments...
            if (node.isComment())
                continue;
            const QString key = node.nodeName();
            if (key == QString::fromLatin1("UserInterfaces"))
                continue;
            if (key == QString::fromLatin1("Translations"))
                continue;
            if (key == QString::fromLatin1("Licenses"))
                continue;
            const QString value = node.toElement().text();
            QDomElement element = doc.createElement(key);
            for (int  i = 0; i < node.attributes().size(); i++) {
                element.setAttribute(node.attributes().item(i).toAttr().name(),
                                     node.attributes().item(i).toAttr().value());
            }
            update.appendChild(element).appendChild(doc.createTextNode(value));
        }

        // get the size of the data
        quint64 componentSize = 0;
        quint64 compressedComponentSize = 0;

        const QString cmpDataDir = QString::fromLatin1("%1/%2").arg(dataDir, it->name);
        const QFileInfoList entries = !QDir(cmpDataDir + QLatin1String("/data")).exists()
            ? QDir(cmpDataDir).entryInfoList(QDir::Files | QDir::NoDotAndDotDot)
            : QDir(cmpDataDir + QLatin1String("/data")).entryInfoList(QDir::Files
                | QDir::Dirs | QDir::NoDotAndDotDot);
        QVector<ArchiveFile> archiveFiles;

        Q_FOREACH (const QFileInfo& fi, entries) {
            if (fi.isHidden())
                continue;

            try {
                if (fi.isDir()) {
                    QDirIterator recursDirIt(fi.filePath(), QDirIterator::Subdirectories);
                    while (recursDirIt.hasNext()) {
                        componentSize += QFile(recursDirIt.next()).size();
                        compressedComponentSize += QFile(recursDirIt.next()).size();
                    }
                } else if (Lib7z::isSupportedArchive(fi.filePath())) {
                    // if it's an archive already, list it's files and sum the uncompressed sizes
                    QFile archive(fi.filePath());
                    compressedComponentSize += archive.size();
                    archive.open(QIODevice::ReadOnly);
                    const QVector< Lib7z::File > files = Lib7z::listArchive(&archive);
                    for (QVector< Lib7z::File >::const_iterator fileIt = files.begin();
                        fileIt != files.end(); ++fileIt) {
                            componentSize += fileIt->uncompressedSize;
                    }
                } else {
                    // otherwise just add it's size
                    componentSize += fi.size();
                    compressedComponentSize += fi.size();
                }
            } catch(...) {
                // ignore, that's just about the sizes - and size doesn't matter, you know?
            }
        }

        // add fake update files
        const QStringList platforms = QStringList() << "Windows" << "MacOSX" << "Linux";
        Q_FOREACH (const QString& platform, platforms) {
            QDomElement file = doc.createElement("UpdateFile");
            file.setAttribute("OS", platform);
            file.setAttribute("UncompressedSize", componentSize);
            file.setAttribute("CompressedSize", compressedComponentSize);
            file.appendChild(doc.createTextNode(QLatin1String("(null)")));
            update.appendChild(file);
        }

        root.appendChild(update);

        if (!QDir(metapath).mkpath(it->name))
            throw Error(QObject::tr("Could not create directory %1").arg(it->name));

        // copy scripts
        const QString script = package.firstChildElement("Script").text();
        if (!script.isEmpty()) {
            verbose() << "    Copying associated script " << script << " into the meta package...";
            QString fromLocation(QString::fromLatin1("%1/meta/%2").arg(it->directory, script));
            QString toLocation(QString::fromLatin1("%1/%2/%3").arg(metapath, it->name, script));
            if (!QFile::copy(fromLocation, toLocation)) {
                verbose() << "failed!" << std::endl;
                throw Error(QObject::tr("Could not copy the script (%1) to its target location (%2)")
                    .arg(fromLocation, toLocation));
            } else {
                verbose() << std::endl;
            }
        }

        // copy user interfaces
        const QDomNodeList uiNodes = package.firstChildElement("UserInterfaces").childNodes();
        QStringList userinterfaces;
        for (int i = 0; i < uiNodes.count(); ++i) {
            const QDomNode node = uiNodes.at(i);
            if (node.nodeName() != QString::fromLatin1("UserInterface"))
                continue;

            const QDir dir(QString::fromLatin1("%1/meta").arg(it->directory));
            const QStringList uis = dir.entryList(QStringList(node.toElement().text()), QDir::Files);
            if (uis.isEmpty()) {
                throw Error(QObject::tr("Couldn't find any user interface matching %1 while copying "
                    "user interfaces of %2").arg(node.toElement().text(), it->name));
            }

            for (QStringList::const_iterator ui = uis.begin(); ui != uis.end(); ++ui) {
                verbose() << "    Copying associated user interface " << *ui << " into the meta "
                    "package...";
                userinterfaces.push_back(*ui);
                if (!QFile::copy(QString::fromLatin1("%1/meta/%2").arg(it->directory, *ui),
                    QString::fromLatin1("%1/%2/%3").arg(metapath, it->name, *ui))) {
                        verbose() << "failed!" << std::endl;
                        throw Error(QObject::tr("Could not copy the UI file %1 to its target location "
                            "(%2)").arg(*ui, it->name));
                } else {
                    verbose() << std::endl;
                }
            }
        }

        if (!userinterfaces.isEmpty()) {
            update.appendChild(doc.createElement(QString::fromLatin1("UserInterfaces")))
                .appendChild(doc.createTextNode(userinterfaces.join(QChar::fromLatin1(','))));
        }

        // copy translations
        const QDomNodeList qmNodes = package.firstChildElement("Translations").childNodes();
        QStringList translations;
        for (int i = 0; i < qmNodes.count(); ++i) {
            const QDomNode node = qmNodes.at(i);
            if (node.nodeName() != QString::fromLatin1("Translation"))
                continue;

            const QDir dir(QString::fromLatin1("%1/meta").arg(it->directory));
            const QStringList qms = dir.entryList(QStringList(node.toElement().text()), QDir::Files);
            if (qms.isEmpty()) {
                throw Error(QObject::tr("Could not find any user interface matching %1 while "
                    "copying user interfaces of %2").arg(node.toElement().text(), it->name));
            }

            for (QStringList::const_iterator qm = qms.begin(); qm != qms.end(); ++qm) {
                verbose() << "    Copying associated translation " << *qm << " into the meta "
                    "package...";
                translations.push_back(*qm);
                if (!QFile::copy(QString::fromLatin1("%1/meta/%2").arg(it->directory, *qm),
                    QString::fromLatin1("%1/%2/%3").arg(metapath, it->name, *qm))) {
                        verbose() << "failed!" << std::endl;
                        throw Error(QObject::tr("Could not copy the translation %1 to its target "
                            "location (%2)").arg(*qm, it->name));
                } else {
                    verbose() << std::endl;
                }
            }
        }

        if (!translations.isEmpty()) {
            update.appendChild(doc.createElement(QString::fromLatin1("Translations")))
                .appendChild(doc.createTextNode(translations.join(QChar::fromLatin1(','))));
        }

        // copy license files
        const QDomNodeList licenseNodes = package.firstChildElement("Licenses").childNodes();
        for (int i = 0; i < licenseNodes.count(); ++i) {
            const QDomNode licenseNode = licenseNodes.at(i);
            if (licenseNode.nodeName() == QLatin1String("License")) {
                const QString &licenseFile =
                    licenseNode.toElement().attributeNode(QLatin1String("file")).value();
                const QString &sourceFile =
                    QString::fromLatin1("%1/meta/%2").arg(it->directory).arg(licenseFile);
                if (!QFile::exists(sourceFile)) {
                    throw Error(QObject::tr("Could not find any license matching %1 while "
                        "copying license files of %2").arg(licenseFile, it->name));
                }

                verbose() << "    Copying associated license file " << licenseFile << " into "
                    "the meta package...";
                if (!QFile::copy(sourceFile, QString::fromLatin1("%1/%2/%3")
                    .arg(metapath, it->name, licenseFile))) {
                        verbose() << "failed!" << std::endl;
                        throw Error(QObject::tr("Could not copy the license file %1 to its "
                            "target location (%2)").arg(licenseFile, it->name));
                } else {
                    verbose() << std::endl;
                }
            }
        }

        if (licenseNodes.count() > 0)
            update.appendChild(package.firstChildElement("Licenses").cloneNode());
    }

    doc.appendChild(root);

    const QString updatesXmlFile = QFileInfo(metapath, "Updates.xml").absoluteFilePath();
    QFile updatesXml(updatesXmlFile);

    openForWrite(&updatesXml, updatesXmlFile);
    blockingWrite(&updatesXml, doc.toByteArray());
}

QVector<PackageInfo> QInstaller::createListOfPackages(const QStringList& components,
    const QString& packagesDirectory, bool addDependencies)
{
    const QVector< PackageInfo > available = collectAvailablePackages(packagesDirectory);
    return available;

    //we don't want to have two different dependency checking codes (installer itself and repgen here)
    //so because they have two different behaviours we deactivate it here for now

    verbose() << "Calculating dependencies for selected packages..." << std::endl;
    QVector<PackageInfo> needed = calculateNeededPackages(components, available, addDependencies);

    verbose() << "The following packages will be placed in the installer:" << std::endl;
    Q_FOREACH (const PackageInfo& i, needed) {
        verbose() << "  " << i.name;
        if (!i.version.isEmpty())
            verbose() << "-" << i.version;
        verbose() << std::endl;
    }

    // now just append the virtual parents (not including all their descendants!)
    // like... if com.nokia.sdk.qt.qtcore was passed, even com.nokia.sdk.qt will show up in the tree
    if (addDependencies) {
        for (int i = 0; i < needed.count(); ++i) {
            const PackageInfo& package = needed[ i ];
            const QString name = package.name;
            const QString version = package.version;
            QString id = name.section(QChar::fromLatin1('.'), 0, -2);
            while (!id.isEmpty()) {
                PackageInfo info;
                if (!version.isEmpty())
                    info = findMatchingPackage(QString::fromLatin1("%1-%2").arg(id, version), available);
                if (info.name.isEmpty())
                    info = findMatchingPackage(id, available);
                if (!info.name.isEmpty() && !allPackagesHavePrefix(needed, id) && !needed.contains(info)) {
                    verbose() << "Adding " << info.name << " as it is the virtual parent item of "
                        << name << std::endl;
                    needed.push_back(info);
                }
                id = id.section(QChar::fromLatin1('.'), 0, -2);
            }
        }
    }

    return needed;
}

QMap<QString, QString> QInstaller::buildPathToVersionMap(const QVector<PackageInfo>& info)
{
    QMap<QString, QString> map;
    Q_FOREACH (PackageInfo inf, info) {
        map[inf.name] = inf.version;
    }
    return map;
}

static void writeSHA1ToNodeWithName(QDomDocument& doc, QDomNodeList& list, const QByteArray& sha1sum,
    const QString& nodename)
{
    verbose() << "searching sha1sum node for " << nodename << std::endl;
    for (int i = 0; i < list.size(); ++i) {
        QDomNode curNode = list.at(i);
        QDomNode nameTag = curNode.firstChildElement(QLatin1String("Name"));
        if (!nameTag.isNull() && nameTag.toElement().text() == nodename) {
            QDomNode sha1Node = doc.createElement(QLatin1String("SHA1"));
            sha1Node.appendChild(doc.createTextNode(QString::fromLatin1(sha1sum.toHex().constData())));
            curNode.appendChild(sha1Node);
        }
    }
}

void QInstaller::compressMetaDirectories(const QString& configDir, const QString& repoDir,
    const QString& baseDir, const QMap<QString, QString>& versionMapping)
{
    const QString configfile = QFileInfo(configDir, QLatin1String("config.xml")).absoluteFilePath();
    const QInstaller::Settings &settings = QInstaller::Settings::fromFileAndPrefix(configfile, configDir);

    KDUpdaterCrypto crypto;
    crypto.setPrivateKey(settings.privateKey());
    ConsolePasswordProvider passwordProvider;
    crypto.setPrivatePasswordProvider(&passwordProvider);
    QDomDocument doc;
    QDomElement root;
    // use existing Updates.xml, if any
    QFile existingUpdatesXml(QFileInfo(QDir(repoDir), QLatin1String("Updates.xml")).absoluteFilePath());
    if (!existingUpdatesXml.open(QIODevice::ReadOnly) || !doc.setContent(&existingUpdatesXml)) {
        verbose() << "Could not find Updates.xml" << std::endl;
    } else {
        root = doc.documentElement();
    }
    existingUpdatesXml.close();

    QDir dir(repoDir);
    const QStringList sub = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    QDomNodeList elements =  doc.elementsByTagName(QLatin1String("PackageUpdate"));
    Q_FOREACH (const QString& i, sub) {
        QDir sd(dir);
        sd.cd(i);
        const QString path = QString(i).remove(baseDir);
        const QString versionPrefix = versionMapping[path];
        if (path.isNull())
            continue;
        const QString absPath = sd.absolutePath();
        const QString fn = QLatin1String(versionPrefix.toLatin1() + "meta.7z");
        const QString tmpTarget = repoDir + QLatin1String("/") +fn;
        compressDirectory(QStringList() << absPath, tmpTarget);
        QFile tmp(tmpTarget);
        tmp.open(QFile::ReadOnly);
        QByteArray fileToCheck = tmp.readAll();
        QByteArray sha1Sum = QCryptographicHash::hash(fileToCheck, QCryptographicHash::Sha1);
        writeSHA1ToNodeWithName(doc, elements, sha1Sum, path);
        const QString finalTarget = absPath + QLatin1String("/") + fn;
        if (!tmp.rename(finalTarget))
            throw QInstaller::Error(QObject::tr("Could not move %1 to %2").arg(tmpTarget, finalTarget));

        // if we have a private key, sign the meta.7z file
        if (!settings.privateKey().isEmpty()) {
            verbose() << "Adding a RSA signature to " << finalTarget << std::endl;
            const QByteArray signature = crypto.sign(finalTarget);
            QFile sigFile(finalTarget + QLatin1String(".sig"));
            if (!sigFile.open(QIODevice::WriteOnly))
                throw QInstaller::Error(QObject::tr("Could not open %1 for writing").arg(finalTarget));

            sigFile.write(signature);
        }
    }
    openForWrite(&existingUpdatesXml, existingUpdatesXml.fileName());
    blockingWrite(&existingUpdatesXml, doc.toByteArray());
    existingUpdatesXml.close();
}

void QInstaller::copyComponentData(const QString& packageDir, const QString& configDir,
    const QString& repoDir, const QVector<PackageInfo>& infos)
{
    const QString configfile = QFileInfo(configDir, QLatin1String("config.xml")).absoluteFilePath();
    const QInstaller::Settings &settings = QInstaller::Settings::fromFileAndPrefix(configfile, configDir);

    KDUpdaterCrypto crypto;
    crypto.setPrivateKey(settings.privateKey());
    ConsolePasswordProvider passwordProvider;
    crypto.setPrivatePasswordProvider(&passwordProvider);

    Q_FOREACH (const PackageInfo& info, infos) {
        const QString i = info.name;
        verbose() << "Copying component data for " << i << std::endl;
        const QString dataDirPath = QString::fromLatin1("%1/%2/data").arg(packageDir, i);
        const QDir dataDir(dataDirPath);
        if (!QDir().mkpath(QString::fromLatin1("%1/%2").arg(repoDir, i))) {
                throw QInstaller::Error(QObject::tr("Could not create repository folder for "
                    "component %1").arg(i));
        }

        const QStringList files = dataDir.entryList(QDir::Files);
        Q_FOREACH (const QString& file, files) {
            QFile tmp(dataDir.absoluteFilePath(file));
            openForRead(&tmp, tmp.fileName());

            const QString target = QString::fromLatin1("%1/%2/%4%3").arg(repoDir, i, file,
                info.version);
            verbose() << QString::fromLatin1("Copying archive from %1 to %2").arg(tmp.fileName(),
                target) << std::endl;
            if (!tmp.copy(target)) {
                throw QInstaller::Error(QObject::tr("Could not copy %1 to %2: %3")
                    .arg(tmp.fileName(), target, tmp.errorString()));
            }
            QFile archiveFile(target);
            QString archiveHashFileName = archiveFile.fileName();
            archiveHashFileName += QLatin1String(".sha1");
            verbose() << "Hash is stored in "<< archiveHashFileName << std::endl;
            QFile archiveHashFile(archiveHashFileName);
            try {
                openForRead(&archiveFile, archiveFile.fileName());
                openForWrite(&archiveHashFile, archiveHashFile.fileName());
                const QByteArray archiveData = archiveFile.readAll();
                archiveFile.close();
                const QByteArray hashOfArchiveData = QCryptographicHash::hash(archiveData,
                    QCryptographicHash::Sha1).toHex();
                archiveHashFile.write(hashOfArchiveData);
                archiveHashFile.close();

            } catch(const Error& /*e*/) {
                //verbose() << e.message() << std::endl;
                archiveHashFile.close();
                archiveFile.close();
                throw;
            }
        }

        const QStringList dirs = dataDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        Q_FOREACH (const QString& dir, dirs) {
            verbose() << "Compressing data directory " << dir << std::endl;
            const QString archiveName = QString::fromLatin1("%1/%2/%4%3.7z").arg(repoDir, i, dir,
                info.version);
            compressDirectory(QStringList() << dataDir.absoluteFilePath(dir), archiveName);
            verbose() << "Creating hash of archive "<< archiveName << std::endl;
            QFile archiveFile(archiveName);
            QString archiveHashFileName = archiveFile.fileName();
            archiveHashFileName += QLatin1String(".sha1");
            verbose() << "Hash is stored in "<< archiveHashFileName << std::endl;
            QFile archiveHashFile(archiveHashFileName);
            try {
                openForRead(&archiveFile, archiveFile.fileName());
                openForWrite(&archiveHashFile, archiveHashFile.fileName());
                const QByteArray archiveData = archiveFile.readAll();
                archiveFile.close();
                const QByteArray hashOfArchiveData = QCryptographicHash::hash(archiveData,
                    QCryptographicHash::Sha1).toHex();
                archiveHashFile.write(hashOfArchiveData);
                archiveHashFile.close();
            } catch(const Error& /*e*/) {
                //std::cerr << e.message() << std::endl;
                archiveHashFile.close();
                archiveFile.close();
                throw;
            }
        }

        // if we have a private key, sign all target files - including those we compressed ourself
        if (!settings.privateKey().isEmpty()) {
            const QDir compDataDir(QString::fromLatin1("%1/%2").arg(repoDir, i));
            const QStringList targetFiles = compDataDir.entryList(QDir::Files);
            for (QStringList::const_iterator it = targetFiles.begin(); it != targetFiles.end(); ++it) {
                verbose() << "Adding a RSA signature to " << *it << std::endl;
                const QByteArray signature = crypto.sign(compDataDir.absoluteFilePath(*it));
                QFile sigFile(compDataDir.absoluteFilePath(*it) + QLatin1String(".sig"));
                if (!sigFile.open(QIODevice::WriteOnly)) {
                    throw QInstaller::Error(QObject::tr("Could not open %1 for writing: %2")
                        .arg(sigFile.fileName(), sigFile.errorString()));
                }
                sigFile.write(signature);
            }
        }
    }
}
