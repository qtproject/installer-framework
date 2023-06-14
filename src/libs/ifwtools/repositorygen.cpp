/**************************************************************************
**
** Copyright (C) 2023 The Qt Company Ltd.
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

#include "repositorygen.h"

#include "constants.h"
#include "fileio.h"
#include "fileutils.h"
#include "errors.h"
#include "globals.h"
#include "archivefactory.h"
#include "metadata.h"
#include "settings.h"
#include "qinstallerglobal.h"
#include "utils.h"
#include "scriptengine.h"

#include "updater.h"

#include <QtCore/QDirIterator>
#include <QtCore/QRegularExpression>

#include <QtXml/QDomDocument>
#include <QTemporaryDir>

#include <iostream>

#define QUOTE_(x) #x
#define QUOTE(x) QUOTE_(x)

using namespace QInstaller;
using namespace QInstallerTools;

void QInstallerTools::printRepositoryGenOptions()
{
    std::cout << "  -p|--packages dir         The directory containing the available packages." << std::endl;
    std::cout << "                            This entry can be given multiple times." << std::endl;
    std::cout << "  --repository dir          The directory containing the available repository." << std::endl;
    std::cout << "                            This entry can be given multiple times." << std::endl;

    std::cout << "  -e|--exclude p1,...,pn    Exclude the given packages." << std::endl;
    std::cout << "  -i|--include p1,...,pn    Include the given packages and their dependencies" << std::endl;
    std::cout << "                            from the repository." << std::endl;

    std::cout << "  --ignore-translations     Do not use any translation" << std::endl;
    std::cout << "  --ignore-invalid-packages Ignore all invalid packages instead of aborting." << std::endl;
    std::cout << "  --ignore-invalid-repositories Ignore all invalid repositories instead of aborting." << std::endl;
    std::cout << "  -s|--sha-update p1,...,pn List of packages which are updated using" <<std::endl;
    std::cout << "                            content sha1 instead of version number." << std::endl;
}

QString QInstallerTools::makePathAbsolute(const QString &path)
{
    if (QFileInfo(path).isRelative())
        return QDir::current().absoluteFilePath(path);
    return path;
}

void QInstallerTools::copyWithException(const QString &source, const QString &target, const QString &kind)
{
    qDebug() << "Copying associated" << kind << "file" << source;

    const QFileInfo targetFileInfo(target);
    if (!targetFileInfo.dir().exists())
        QInstaller::mkpath(targetFileInfo.absolutePath());

    QFile sourceFile(source);
    if (!sourceFile.copy(target)) {
        qDebug() << "failed!\n";
        throw QInstaller::Error(QString::fromLatin1("Cannot copy the %1 file from \"%2\" to \"%3\": "
            "%4").arg(kind, QDir::toNativeSeparators(source), QDir::toNativeSeparators(target),
            /* in case of an existing target the error String does not show the file */
            (targetFileInfo.exists() ? QLatin1String("Target already exist.") : sourceFile.errorString())));
    }

    qDebug() << "done.";
}

static QStringList copyFilesFromNode(const QString &parentNode, const QString &childNode, const QString &attr,
    const QString &kind, const QDomNode &package, const PackageInfo &info, const QString &targetDir)
{
    QStringList copiedFiles;
    const QDomNodeList nodes = package.firstChildElement(parentNode).childNodes();
    for (int i = 0; i < nodes.count(); ++i) {
        const QDomNode node = nodes.at(i);
        if (node.nodeName() != childNode)
            continue;

        const QDir dir(QString::fromLatin1("%1/meta").arg(info.directory));
        const QString filter = attr.isEmpty() ? node.toElement().text() : node.toElement().attribute(attr);
        const QStringList files = dir.entryList(QStringList(filter), QDir::Files);
        if (files.isEmpty()) {
            throw QInstaller::Error(QString::fromLatin1("Cannot find any %1 matching \"%2\" "
                "while copying %1 of \"%3\".").arg(kind, filter, info.name));
        }

        foreach (const QString &file, files) {
            const QString source(QString::fromLatin1("%1/meta/%2").arg(info.directory, file));
            const QString target(QString::fromLatin1("%1/%2/%3").arg(targetDir, info.name, file));
            copyWithException(source, target, kind);
            copiedFiles.append(file);
        }
    }
    return copiedFiles;
}

/*
    Returns \c true if the \a file is an archive or an SHA1 checksum
    file for an archive, /c false otherwise.
*/
static bool isArchiveOrChecksum(const QString &file)
{
    if (file.endsWith(QLatin1String(".sha1")))
        return true;

    for (auto &supportedSuffix : ArchiveFactory::supportedTypes()) {
        if (file.endsWith(supportedSuffix))
            return true;
    }
    return false;
}

/*
    Fills the package \a info with the name of the metadata archive when applicable. Returns
    \c true if the component has metadata compressed in an archive or uncompressed to cache, or
    if the metadata archive is redundant. Returns \c false if the component should have metadata
    but none was found.
*/
static bool findMetaFile(const QString &repositoryDir, const QDomElement &packageUpdate, PackageInfo &info)
{
    // Note: the order here is important, when updating from an existing
    // repository we shouldn't drop the empty metadata archives.

    // 1. First, try with normal repository structure
    QString metaFile = QString::fromLatin1("%1/%3%2").arg(info.directory,
        QString::fromLatin1("meta.7z"), info.version);

    if (QFileInfo::exists(metaFile)) {
        info.metaFile = metaFile;
        return true;
    }

    // 2. If that does not work, check for fetched temporary repository structure
    metaFile = QString::fromLatin1("%1/%2-%3-%4").arg(repositoryDir,
        info.name, info.version, QString::fromLatin1("meta.7z"));

    if (QFileInfo::exists(metaFile)) {
        info.metaFile = metaFile;
        return true;
    }

    // 3. Try with the cached metadata directory structure
    const QDir packageDir(info.directory);
    const QStringList cachedMetaFiles = packageDir.entryList(QDir::Files);
    for (auto &file : cachedMetaFiles) {
        if (!isArchiveOrChecksum(file))
            return true; // Return for first non-archive file
    }

    // 4. The meta archive may be redundant, skip in that case (cached item from a
    // repository that has empty meta archive)
    bool metaElementFound = false;
    const QDomNodeList c1 = packageUpdate.childNodes();
    for (int i = 0; i < c1.count(); ++i) {
        const QDomElement e1 = c1.at(i).toElement();
        for (const QString &meta : scMetaElements) {
            if (e1.tagName() == meta) {
                metaElementFound = true;
                break;
            }
        }
    }
    return !metaElementFound;
}

void QInstallerTools::copyMetaData(const QString &_targetDir, const QString &metaDataDir,
    const PackageInfoVector &packages, const QString &appName, const QString &appVersion,
    const QStringList &uniteMetadatas)
{
    const QString targetDir = makePathAbsolute(_targetDir);
    if (!QFile::exists(targetDir))
        QInstaller::mkpath(targetDir);

    QDomDocument doc;
    QDomElement root;
    QFile existingUpdatesXml(QFileInfo(metaDataDir, QLatin1String("Updates.xml")).absoluteFilePath());
    if (existingUpdatesXml.open(QIODevice::ReadOnly) && doc.setContent(&existingUpdatesXml)) {
        root = doc.documentElement();
        // remove entry for this component from existing Updates.xml, if found
        foreach (const PackageInfo &info, packages) {
            const QDomNodeList packageNodes = root.childNodes();
            for (int i = packageNodes.count() - 1; i >= 0; --i) {
                const QDomNode node = packageNodes.at(i);
                if (node.nodeName() != QLatin1String("PackageUpdate"))
                    continue;
                if (node.firstChildElement(QLatin1String("Name")).text() != info.name)
                    continue;
                root.removeChild(node);
            }
        }
        existingUpdatesXml.close();
    } else {
        root = doc.createElement(QLatin1String("Updates"));
        root.appendChild(doc.createElement(QLatin1String("ApplicationName"))).appendChild(doc
            .createTextNode(appName));
        root.appendChild(doc.createElement(QLatin1String("ApplicationVersion"))).appendChild(doc
            .createTextNode(appVersion));
        root.appendChild(doc.createElement(QLatin1String("Checksum"))).appendChild(doc
            .createTextNode(QLatin1String("true")));
    }

    foreach (const PackageInfo &info, packages) {
        if (info.metaFile.isEmpty() && info.metaNode.isEmpty()) {
            if (!QDir(targetDir).mkpath(info.name))
                throw QInstaller::Error(QString::fromLatin1("Cannot create directory \"%1\".").arg(info.name));

            const QString packageXmlPath = QString::fromLatin1("%1/meta/package.xml").arg(info.directory);
            qDebug() << "Copy meta data for package" << info.name << "using" << packageXmlPath;

            QFile file(packageXmlPath);
            QInstaller::openForRead(&file);

            QString errMsg;
            int line = 0;
            int column = 0;
            QDomDocument packageXml;
            if (!packageXml.setContent(&file, &errMsg, &line, &column)) {
                throw QInstaller::Error(QString::fromLatin1("Cannot parse \"%1\": line: %2, column: %3: %4 (%5)")
                                        .arg(QDir::toNativeSeparators(packageXmlPath)).arg(line).arg(column).arg(errMsg, info.name));
            }

            QDomElement update = doc.createElement(QLatin1String("PackageUpdate"));
            QDomNode nameElement = update.appendChild(doc.createElement(QLatin1String("Name")));
            nameElement.appendChild(doc.createTextNode(info.name));

            // list of current unused or later transformed tags
            QStringList blackList;
            blackList << QLatin1String("UserInterfaces") << QLatin1String("Translations") <<
                         QLatin1String("Licenses") << QLatin1String("Name") << QLatin1String("Operations");

            bool foundDefault = false;
            bool foundVirtual = false;
            bool foundDisplayName = false;
            bool foundDownloadableArchives = false;
            bool foundCheckable = false;
            const QDomNode package = packageXml.firstChildElement(QLatin1String("Package"));
            const QDomNodeList childNodes = package.childNodes();
            for (int i = 0; i < childNodes.count(); ++i) {
                const QDomNode node = childNodes.at(i);
                const QString key = node.nodeName();

                if (key == QLatin1String("Default"))
                    foundDefault = true;
                if (key == QLatin1String("Virtual"))
                    foundVirtual = true;
                if (key == QLatin1String("DisplayName"))
                    foundDisplayName = true;
                if (key == QLatin1String("DownloadableArchives"))
                    foundDownloadableArchives = true;
                if (key == QLatin1String("Checkable"))
                    foundCheckable = true;
                if (node.isComment() || blackList.contains(key))
                    continue;   // just skip comments and some tags...

                QDomElement element = doc.createElement(key);
                for (int j = 0; j < node.attributes().size(); ++j) {
                    element.setAttribute(node.attributes().item(j).toAttr().name(),
                                         node.attributes().item(j).toAttr().value());
                }
                update.appendChild(element).appendChild(doc.createTextNode(node.toElement().text()));
            }

            if (foundDefault && foundVirtual) {
                throw QInstaller::Error(QString::fromLatin1("Error: <Default> and <Virtual> elements are "
                                                            "mutually exclusive in file \"%1\".").arg(QDir::toNativeSeparators(packageXmlPath)));
            }

            if (foundDefault && foundCheckable) {
                throw QInstaller::Error(QString::fromLatin1("Error: <Default> and <Checkable>"
                                                            "elements are mutually exclusive in file \"%1\".")
                                        .arg(QDir::toNativeSeparators(packageXmlPath)));
            }

            if (!foundDisplayName) {
                qWarning() << "No DisplayName tag found at" << info.name << ", using component Name instead.";
                QDomElement displayNameElement = doc.createElement(QLatin1String("DisplayName"));
                update.appendChild(displayNameElement).appendChild(doc.createTextNode(info.name));
            }

            // get the size of the data
            quint64 componentSize = 0;
            quint64 compressedComponentSize = 0;

            const QDir::Filters filters = QDir::Files | QDir::NoDotAndDotDot;
            const QDir dataDir = QString::fromLatin1("%1/%2/data").arg(metaDataDir, info.name);
            const QFileInfoList entries = dataDir.exists() ? dataDir.entryInfoList(filters | QDir::Dirs)
                                                           : QDir(QString::fromLatin1("%1/%2").arg(metaDataDir, info.name)).entryInfoList(filters);
            qDebug() << "calculate size of directory" << dataDir.absolutePath();
            foreach (const QFileInfo &fi, entries) {
                try {
                    QScopedPointer<AbstractArchive> archive(ArchiveFactory::instance().create(fi.filePath()));
                    if (fi.isDir()) {
                        QDirIterator recursDirIt(fi.filePath(), QDirIterator::Subdirectories);
                        while (recursDirIt.hasNext()) {
                            recursDirIt.next();
                            const quint64 size = QInstaller::fileSize(recursDirIt.fileInfo());
                            componentSize += size;
                            compressedComponentSize += size;
                        }
                    } else if (archive && archive->open(QIODevice::ReadOnly) && archive->isSupported()) {
                        // if it's an archive already, list its files and sum the uncompressed sizes
                        compressedComponentSize += fi.size();

                        QVector<ArchiveEntry>::const_iterator fileIt;
                        const QVector<ArchiveEntry> files = archive->list();
                        for (fileIt = files.begin(); fileIt != files.end(); ++fileIt)
                            componentSize += fileIt->uncompressedSize;
                    } else {
                        // otherwise just add its size
                        const quint64 size = QInstaller::fileSize(fi);
                        componentSize += size;
                        compressedComponentSize += size;
                    }
                } catch (const QInstaller::Error &error) {
                    qDebug().noquote() << error.message();
                } catch(...) {
                    // ignore, that's just about the sizes - and size doesn't matter, you know?
                }
            }

            QDomElement fileElement = doc.createElement(QLatin1String("UpdateFile"));
            fileElement.setAttribute(QLatin1String("UncompressedSize"), componentSize);
            fileElement.setAttribute(QLatin1String("CompressedSize"), compressedComponentSize);
            // adding the OS attribute to be compatible with old sdks
            fileElement.setAttribute(QLatin1String("OS"), QLatin1String("Any"));
            update.appendChild(fileElement);

            if (info.createContentSha1Node) {
                QDomNode contentSha1Element = update.appendChild(doc.createElement(QLatin1String("ContentSha1")));
                contentSha1Element.appendChild(doc.createTextNode(info.contentSha1));
            }

            root.appendChild(update);

            // copy script files
            copyScriptFiles(childNodes, info, foundDownloadableArchives, targetDir);

            // write DownloadableArchives tag if that is missed by the user
            if (!foundDownloadableArchives && !info.copiedFiles.isEmpty()) {
                QStringList realContentFiles;
                foreach (const QString &filePath, info.copiedFiles) {
                    if (!filePath.endsWith(QLatin1String(".sha1"), Qt::CaseInsensitive)) {
                        const QString fileName = QFileInfo(filePath).fileName();
                        // remove unnecessary version string from filename and add it to the list
                        realContentFiles.append(fileName.mid(info.version.size()));
                    }
                }

                update.appendChild(doc.createElement(QLatin1String("DownloadableArchives"))).appendChild(doc
                                                                                                         .createTextNode(realContentFiles.join(QChar::fromLatin1(','))));
            }

            // copy user interfaces
            const QStringList uiFiles = copyFilesFromNode(QLatin1String("UserInterfaces"),
                                                          QLatin1String("UserInterface"), QString(), QLatin1String("user interface"), package, info,
                                                          targetDir);
            if (!uiFiles.isEmpty()) {
                update.appendChild(doc.createElement(QLatin1String("UserInterfaces"))).appendChild(doc
                                                                                                   .createTextNode(uiFiles.join(QChar::fromLatin1(','))));
            }

            // copy translations
            QStringList trFiles;
            if (!qApp->arguments().contains(QString::fromLatin1("--ignore-translations"))) {
                trFiles = copyFilesFromNode(QLatin1String("Translations"), QLatin1String("Translation"),
                                            QString(), QLatin1String("translation"), package, info, targetDir);
                if (!trFiles.isEmpty()) {
                    update.appendChild(doc.createElement(QLatin1String("Translations"))).appendChild(doc
                                                                                                     .createTextNode(trFiles.join(QChar::fromLatin1(','))));
                }
            }

            // copy license files
            const QStringList licenses = copyFilesFromNode(QLatin1String("Licenses"), QLatin1String("License"),
                                                           QLatin1String("file"), QLatin1String("license"), package, info, targetDir);
            if (!licenses.isEmpty()) {
                foreach (const QString &trFile, trFiles) {
                    // Copy translated license file based on the assumption that it will have the same base name
                    // as the original license plus the file name of an existing translation file without suffix.
                    foreach (const QString &license, licenses) {
                        const QFileInfo untranslated(license);
                        const QString translatedLicense = QString::fromLatin1("%2_%3.%4").arg(untranslated
                                                                                              .baseName(), QFileInfo(trFile).baseName(), untranslated.completeSuffix());
                        // ignore copy failure, that's just about the translations
                        QFile::copy(QString::fromLatin1("%1/meta/%2").arg(info.directory).arg(translatedLicense),
                                    QString::fromLatin1("%1/%2/%3").arg(targetDir, info.name, translatedLicense));
                    }
                }
                update.appendChild(package.firstChildElement(QLatin1String("Licenses")).cloneNode());
            }
            // operations
            update.appendChild(package.firstChildElement(QLatin1String("Operations")).cloneNode());
        } else {
            // Extract metadata from archive
            if (!info.metaFile.isEmpty()){
                QScopedPointer<AbstractArchive> metaFile(ArchiveFactory::instance().create(info.metaFile));
                if (!metaFile) {
                    throw QInstaller::Error(QString::fromLatin1("Could not create handler "
                        "object for archive \"%1\": \"%2\".").arg(info.metaFile, QLatin1String(Q_FUNC_INFO)));
                }
                if (!(metaFile->open(QIODevice::ReadOnly) && metaFile->extract(targetDir))) {
                    throw Error(QString::fromLatin1("Could not extract archive \"%1\": %2").arg(
                        QDir::toNativeSeparators(info.metaFile), metaFile->errorString()));
                }
            } else {
                // The metadata may have been already extracted, i.e. when reading from a
                // local repository cache.
                const QDir packageDir(info.directory);
                const QStringList metaFiles = packageDir.entryList(QDir::Files);
                for (auto &file : metaFiles) {
                    if (isArchiveOrChecksum(file))
                        continue; // Skip data archives

                    const QString source(QString::fromLatin1("%1/%2").arg(info.directory, file));
                    const QString target(QString::fromLatin1("%1/%2/%3").arg(targetDir, info.name, file));
                    copyWithException(source, target, QLatin1String("cached metadata"));
                }
            }

            // Restore "PackageUpdate" node;
            QDomDocument update;
            if (!update.setContent(info.metaNode)) {
                throw QInstaller::Error(QString::fromLatin1("Cannot restore \"PackageUpdate\" description for node %1").arg(info.name));
            }

            root.appendChild(update.documentElement());
        }
    }

    // Packages can be in repositories using different meta formats,
    // always extract unified meta if given as argument.
    foreach (const QString uniteMetadata, uniteMetadatas) {
        const QString metaFilePath = QFileInfo(metaDataDir, uniteMetadata).absoluteFilePath();
        QScopedPointer<AbstractArchive> metaFile(ArchiveFactory::instance().create(metaFilePath));
        if (!metaFile) {
            throw QInstaller::Error(QString::fromLatin1("Could not create handler "
                "object for archive \"%1\": \"%2\".").arg(metaFilePath, QLatin1String(Q_FUNC_INFO)));
        }
        if (!(metaFile->open(QIODevice::ReadOnly) && metaFile->extract(targetDir))) {
            throw Error(QString::fromLatin1("Could not extract archive \"%1\": %2").arg(
                QDir::toNativeSeparators(metaFilePath), metaFile->errorString()));
        }
    }

    doc.appendChild(root);

    QFile targetUpdatesXml(targetDir + QLatin1String("/Updates.xml"));
    QInstaller::openForWrite(&targetUpdatesXml);
    QInstaller::blockingWrite(&targetUpdatesXml, doc.toByteArray());
}

PackageInfoVector QInstallerTools::createListOfPackages(const QStringList &packagesDirectories,
    QStringList *packagesToFilter, FilterType filterType, QStringList packagesUpdatedWithSha)
{
    qDebug() << "Collecting information about available packages...";

    bool ignoreInvalidPackages = qApp->arguments().contains(QString::fromLatin1("--ignore-invalid-packages"));

    PackageInfoVector dict;
    QFileInfoList entries;
    foreach (const QString &packagesDirectory, packagesDirectories)
        entries.append(QDir(packagesDirectory).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot));
    for (QFileInfoList::const_iterator it = entries.constBegin(); it != entries.constEnd(); ++it) {
        if (filterType == Exclude) {
            // Check for current file in exclude list, if found, skip it and remove it from exclude list
            if (packagesToFilter->contains(it->fileName())) {
                packagesToFilter->removeAll(it->fileName());
                continue;
            }
        } else {
            // Check for current file in include list, if not found, skip it; if found, remove it from include list
            if (!packagesToFilter->contains(it->fileName()))
                continue;
            packagesToFilter->removeAll(it->fileName());
        }
        qDebug() << "Found subdirectory" << it->fileName();
        // because the filter is QDir::Dirs - filename means the name of the subdirectory
        if (it->fileName().contains(QLatin1Char('-'))) {
            qDebug("When using the component \"%s\" as a dependency, "
                "to ensure backward compatibility, you must add a colon symbol at the end, "
                "even if you do not specify a version.",
                qUtf8Printable(it->fileName()));
        }

        QFile file(QString::fromLatin1("%1/meta/package.xml").arg(it->filePath()));
        QFileInfo fileInfo(file);
        if (!fileInfo.exists()) {
            if (ignoreInvalidPackages)
                continue;
            throw QInstaller::Error(QString::fromLatin1("Component \"%1\" does not contain a package "
                "description (meta/package.xml is missing).").arg(QDir::toNativeSeparators(it->fileName())));
        }

        file.open(QIODevice::ReadOnly);

        QDomDocument doc;
        QString error;
        int errorLine = 0;
        int errorColumn = 0;
        if (!doc.setContent(&file, &error, &errorLine, &errorColumn)) {
            if (ignoreInvalidPackages)
                continue;
            throw QInstaller::Error(QString::fromLatin1("Component package description in \"%1\" is invalid. "
                "Error at line: %2, column: %3 -> %4").arg(QDir::toNativeSeparators(fileInfo.absoluteFilePath()),
                                                           QString::number(errorLine),
                                                           QString::number(errorColumn), error));
        }

        const QDomElement packageElement = doc.firstChildElement(QLatin1String("Package"));
        const QString name = packageElement.firstChildElement(QLatin1String("Name")).text();
        if (!name.isEmpty() && name != it->fileName()) {
            qWarning().nospace() << "The <Name> tag in the file " << fileInfo.absoluteFilePath()
                       << " is ignored - the installer uses the path element right before the 'meta'"
                       << " (" << it->fileName() << ")";
        }

        QString releaseDate = packageElement.firstChildElement(QLatin1String("ReleaseDate")).text();
        if (releaseDate.isEmpty()) {
            qWarning("Release date for \"%s\" is empty! Using the current date instead.",
                qPrintable(fileInfo.absoluteFilePath()));
            releaseDate = QDate::currentDate().toString(Qt::ISODate);
        }

        if (!QDate::fromString(releaseDate, Qt::ISODate).isValid()) {
            if (ignoreInvalidPackages)
                continue;
            throw QInstaller::Error(QString::fromLatin1("Release date for \"%1\" is invalid! <ReleaseDate>%2"
                "</ReleaseDate>. Supported format: YYYY-MM-DD").arg(QDir::toNativeSeparators(fileInfo.absoluteFilePath()),
                                                                    releaseDate));
        }

        PackageInfo info;
        info.name = it->fileName();
        info.version = packageElement.firstChildElement(QLatin1String("Version")).text();
        // Version cannot start with comparison characters, be an empty string
        // or have whitespaces at the beginning or at the end
        static const QRegularExpression regex(QLatin1String("^(?![<=>\\s]+)(.+)$"));
        if (!regex.match(info.version).hasMatch() || (info.version != info.version.trimmed())) {
            if (ignoreInvalidPackages)
                continue;
            throw QInstaller::Error(QString::fromLatin1("Component version for \"%1\" is invalid! <Version>%2</Version>")
                .arg(QDir::toNativeSeparators(fileInfo.absoluteFilePath()), info.version));
        }
        info.dependencies = packageElement.firstChildElement(QLatin1String("Dependencies")).text()
            .split(QInstaller::commaRegExp(), Qt::SkipEmptyParts);
        info.directory = it->filePath();
        if (packagesUpdatedWithSha.contains(info.name)) {
            info.createContentSha1Node = true;
            packagesUpdatedWithSha.removeOne(info.name);
        } else {
            info.createContentSha1Node = false;
        }

        dict.push_back(info);

        qDebug() << "- it provides the package" << info.name << " - " << info.version;
    }

    if (!packagesToFilter->isEmpty() && packagesToFilter->at(0) != QString::fromLatin1(
        "X_fake_filter_component_for_online_only_installer_X")) {
        qWarning() << "The following explicitly given packages could not be found\n in package directory:" << *packagesToFilter;
    }

    if (dict.isEmpty())
        qDebug() << "No available packages found at the specified location.";

    if (!packagesUpdatedWithSha.isEmpty()) {
        throw QInstaller::Error(QString::fromLatin1("The following packages could not be found in "
            "package directory:  %1").arg(packagesUpdatedWithSha.join(QLatin1String(", "))));
    }

    return dict;
}

PackageInfoVector QInstallerTools::createListOfRepositoryPackages(const QStringList &repositoryDirectories,
    QStringList *packagesToFilter, FilterType filterType)
{
    qDebug() << "Collecting information about available repository packages...";

    bool ignoreInvalidRepositories = qApp->arguments().contains(QString::fromLatin1("--ignore-invalid-repositories"));

    PackageInfoVector dict;
    QFileInfoList entries;
    foreach (const QString &repositoryDirectory, repositoryDirectories)
        entries.append(QFileInfo(repositoryDirectory));
    for (QFileInfoList::const_iterator it = entries.constBegin(); it != entries.constEnd(); ++it) {

        qDebug() << "Process repository" << it->fileName();

        QFile file(QString::fromLatin1("%1/Updates.xml").arg(it->filePath()));

        QFileInfo fileInfo(file);
        if (!fileInfo.exists()) {
            if (ignoreInvalidRepositories) {
                qDebug() << "- skip invalid repository";
                continue;
            }
            throw QInstaller::Error(QString::fromLatin1("Repository \"%1\" does not contain a update "
                "description (Updates.xml is missing).").arg(QDir::toNativeSeparators(it->fileName())));
        }
        if (!file.open(QIODevice::ReadOnly)) {
            qDebug() << "Cannot open Updates.xml for reading:" << file.errorString();
            continue;
        }

        QString error;
        QDomDocument doc;
        if (!doc.setContent(&file, &error)) {
            qDebug().nospace() << "Cannot fetch a valid version of Updates.xml from repository "
                               << it->fileName() << ": " << error;
            continue;
        }
        file.close();

        const QDomElement root = doc.documentElement();
        if (root.tagName() != QLatin1String("Updates")) {
            throw QInstaller::Error(QCoreApplication::translate("QInstaller",
                "Invalid content in \"%1\".").arg(QDir::toNativeSeparators(file.fileName())));
        }

        bool hasUnifiedMetaFile = false;
        const QDomElement unifiedSha1 = root.firstChildElement(scSHA1);
        const QDomElement unifiedMetaName = root.firstChildElement(QLatin1String("MetadataName"));

        // Unified metadata takes priority over component metadata
        if (!unifiedSha1.isNull() && !unifiedMetaName.isNull())
            hasUnifiedMetaFile = true;

        const QDomNodeList children = root.childNodes();
        for (int i = 0; i < children.count(); ++i) {
            const QDomElement el = children.at(i).toElement();
            if ((!el.isNull()) && (el.tagName() == QLatin1String("PackageUpdate"))) {
                QInstallerTools::PackageInfo info;

                QDomElement c1 = el.firstChildElement(QInstaller::scName);
                if (!c1.isNull())
                    info.name = c1.text();
                else
                    continue;
                if (filterType == Exclude) {
                    // Check for current package in exclude list, if found, skip it
                    if (packagesToFilter->contains(info.name)) {
                        continue;
                    }
                } else {
                    // Check for current package in include list, if not found, skip it
                    if (!packagesToFilter->contains(info.name))
                        continue;
                }
                c1 = el.firstChildElement(QInstaller::scVersion);
                if (!c1.isNull())
                    info.version = c1.text();
                else
                    continue;

                info.directory = QString::fromLatin1("%1/%2").arg(it->filePath(), info.name);
                if (!hasUnifiedMetaFile) {
                    const QDomElement sha1 = el.firstChildElement(QInstaller::scSHA1);
                    if (!sha1.isNull() && !findMetaFile(it->filePath(), el, info)) {
                        throw QInstaller::Error(QString::fromLatin1("Could not find metadata archive for component "
                            "%1 %2 in repository %3.").arg(info.name, info.version, it->filePath()));
                    }
                }

                const QDomNodeList c2 = el.childNodes();
                for (int j = 0; j < c2.count(); ++j) {
                    const QDomElement c2Element = c2.at(j).toElement();
                    if (c2Element.tagName() == QInstaller::scDependencies)
                        info.dependencies = c2Element.text()
                            .split(QInstaller::commaRegExp(), Qt::SkipEmptyParts);
                    else if (c2Element.tagName() == QInstaller::scDownloadableArchives) {
                        QStringList names = c2Element.text()
                            .split(QInstaller::commaRegExp(), Qt::SkipEmptyParts);
                        foreach (const QString &name, names) {
                            info.copiedFiles.append(QString::fromLatin1("%1/%3%2").arg(info.directory,
                                name, info.version));
                            info.copiedFiles.append(QString::fromLatin1("%1/%3%2.sha1").arg(info.directory,
                                name, info.version));
                        }
                    }
                }
                QString metaString;
                {
                    QTextStream metaStream(&metaString);
                    el.save(metaStream, 0);
                }
                info.metaNode = metaString;

                bool pushToDict = true;
                bool replacement = false;
                // Check whether this package already exists in vector:
                for (int i = 0; i < dict.size(); ++i) {
                    const QInstallerTools::PackageInfo oldInfo = dict.at(i);
                    if (oldInfo.name != info.name)
                        continue;

                    if (KDUpdater::compareVersion(info.version, oldInfo.version) > 0) {
                        // A package with newer version, it will replace the existing one.
                        dict.remove(i);
                        replacement = true;
                    } else {
                        // A package with older or same version, do not add it again.
                        pushToDict = false;
                    }
                    break;
                }

                if (pushToDict) {
                    replacement ? qDebug() << "- it provides a new version of the package" << info.name << " - " << info.version << "- replaced"
                                : qDebug() << "- it provides the package" << info.name << " - " << info.version;
                    dict.push_back(info);
                } else {
                    qDebug() << "- it provides an old version of the package" << info.name << " - " << info.version << "- ignored";
                }
            }
        }
    }

    return dict;
}

QHash<QString, QString> QInstallerTools::buildPathToVersionMapping(const PackageInfoVector &info)
{
    QHash<QString, QString> map;
    foreach (const PackageInfo &inf, info)
        map[inf.name] = inf.version;
    return map;
}

static void writeSHA1ToNodeWithName(QDomDocument &doc, QDomNodeList &list, const QByteArray &sha1sum,
    const QString &nodename = QString())
{
    if (nodename.isEmpty())
        qDebug() << "Writing sha1sum node.";
    else
        qDebug() << "Searching sha1sum node for" << nodename;
    QString sha1Value = QString::fromLatin1(sha1sum.toHex().constData());
    for (int i = 0; i < list.size(); ++i) {
        QDomNode curNode = list.at(i);
        QDomNode nameTag = curNode.firstChildElement(scName);
        if ((!nameTag.isNull() && nameTag.toElement().text() == nodename) || nodename.isEmpty()) {
            QDomNode sha1Node = curNode.firstChildElement(scSHA1);
            QDomNode newSha1Node = doc.createElement(scSHA1);
            newSha1Node.appendChild(doc.createTextNode(sha1Value));

            if (!sha1Node.isNull() && sha1Node.hasChildNodes()) {
                QDomNode sha1NodeChild = sha1Node.firstChild();
                QString sha1OldValue = sha1NodeChild.nodeValue();
                if (sha1Value == sha1OldValue) {
                    qDebug() << "- keeping the existing sha1sum" << sha1OldValue;
                    continue;
                } else {
                    qDebug() << "- clearing the old sha1sum" << sha1OldValue;
                    sha1Node.removeChild(sha1NodeChild);
                }
            }
            if (sha1Node.isNull())
                curNode.appendChild(newSha1Node);
            else
                curNode.replaceChild(newSha1Node, sha1Node);
            qDebug() << "- writing the sha1sum" << sha1Value;
        }
    }
}

void QInstallerTools::createArchive(const QString &filename, const QStringList &data, Compression compression)
{
    QScopedPointer<AbstractArchive> targetArchive(ArchiveFactory::instance().create(filename));
    if (!targetArchive) {
        throw QInstaller::Error(QString::fromLatin1("Could not create handler "
            "object for archive \"%1\": \"%2\".").arg(filename, QLatin1String(Q_FUNC_INFO)));
    }
    targetArchive->setCompressionLevel(compression);
    if (!(targetArchive->open(QIODevice::WriteOnly) && targetArchive->create(data))) {
        throw Error(QString::fromLatin1("Could not create archive \"%1\": %2").arg(
            QDir::toNativeSeparators(filename), targetArchive->errorString()));
    }
}

void QInstallerTools::compressMetaDirectories(const QString &repoDir, const QString &existingUnite7zUrl,
    const QHash<QString, QString> &versionMapping, bool createSplitMetadata, bool createUnifiedMetadata)
{
    QDomDocument doc;
    // use existing Updates.xml, if any
    QFile existingUpdatesXml(QFileInfo(QDir(repoDir), QLatin1String("Updates.xml")).absoluteFilePath());
    if (!existingUpdatesXml.open(QIODevice::ReadOnly) || !doc.setContent(&existingUpdatesXml)) {
        qDebug() << "Cannot find Updates.xml";
    }
    existingUpdatesXml.close();

    QDir dir(repoDir);
    const QStringList entryList = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

    QStringList absPaths;
    if (createUnifiedMetadata) {
        absPaths = unifyMetadata(repoDir, existingUnite7zUrl, doc);
    }

    if (createSplitMetadata) {
        splitMetadata(entryList, repoDir, doc, versionMapping);
    } else {
        // remove the files that got compressed
        foreach (const QString path, absPaths)
            QInstaller::removeFiles(path, true);
    }

    QInstaller::openForWrite(&existingUpdatesXml);
    QInstaller::blockingWrite(&existingUpdatesXml, doc.toByteArray());
    existingUpdatesXml.close();
}

QStringList QInstallerTools::unifyMetadata(const QString &repoDir, const QString &existingRepoDir, QDomDocument doc)
{
    QStringList absPaths;
    QDir dir(repoDir);
    const QStringList entryList = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

    foreach (const QString &i, entryList) {
        dir.cd(i);
        const QString absPath = dir.absolutePath();
        absPaths.append(absPath);
        dir.cdUp();
    }

    QTemporaryDir existingRepoTempDir;
    QString existingRepoTemp = existingRepoTempDir.path();
    if (!existingRepoDir.isEmpty()) {
        existingRepoTempDir.setAutoRemove(false);
        QScopedPointer<AbstractArchive> archiveFile(ArchiveFactory::instance().create(existingRepoDir));
        if (!archiveFile) {
            throw QInstaller::Error(QString::fromLatin1("Could not create handler "
                "object for archive \"%1\": \"%2\".").arg(existingRepoDir, QLatin1String(Q_FUNC_INFO)));
        }
        if (!(archiveFile->open(QIODevice::ReadOnly) && archiveFile->extract(existingRepoTemp))) {
            throw Error(QString::fromLatin1("Could not extract archive \"%1\": %2").arg(
                QDir::toNativeSeparators(existingRepoDir), archiveFile->errorString()));
        }
        QDir dir2(existingRepoTemp);
        QStringList existingRepoEntries = dir2.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        foreach (const QString existingRepoEntry, existingRepoEntries) {
            if (entryList.contains(existingRepoEntry)) {
                continue;
            } else {
                dir2.cd(existingRepoEntry);
                const QString absPath = dir2.absolutePath();
                absPaths.append(absPath);
                dir2.cdUp();
            }
        }
    }

    // Compress all metadata from repository to one single 7z
    const QString metadataFilename = QDateTime::currentDateTime().
            toString(QLatin1String("yyyy-MM-dd-hhmm")) + QLatin1String("_meta.7z");
    const QString tmpTarget = repoDir + QDir::separator() + metadataFilename;
    createArchive(tmpTarget, absPaths);

    QFile tmp(tmpTarget);
    tmp.open(QFile::ReadOnly);
    const QByteArray sha1Sum = QInstaller::calculateHash(&tmp, QCryptographicHash::Sha1);
    QDomNodeList elements =  doc.elementsByTagName(QLatin1String("Updates"));
    writeSHA1ToNodeWithName(doc, elements, sha1Sum, QString());

    qDebug() << "Updating the metadata node with name " << metadataFilename;
    if (elements.count() > 0) {
        QDomNode node = elements.at(0);
        QDomNode nameTag = node.firstChildElement(QLatin1String("MetadataName"));

        QDomNode newNodeTag = doc.createElement(QLatin1String("MetadataName"));
        newNodeTag.appendChild(doc.createTextNode(metadataFilename));

        if (nameTag.isNull())
            node.appendChild(newNodeTag);
        else
            node.replaceChild(newNodeTag, nameTag);
    }
    QInstaller::removeDirectory(existingRepoTemp, true);
    return absPaths;
}

void QInstallerTools::splitMetadata(const QStringList &entryList, const QString &repoDir,
                                    QDomDocument doc, const QHash<QString, QString> &versionMapping)
{
    QStringList absPaths;
    QDomNodeList elements =  doc.elementsByTagName(QLatin1String("PackageUpdate"));
    QDir dir(repoDir);
    foreach (const QString &i, entryList) {
        dir.cd(i);
        const QString absPath = dir.absolutePath();
        const QString path = QString(i).remove(repoDir);
        if (path.isNull())
            continue;
        const QString versionPrefix = versionMapping[path];
        const QString fn = QLatin1String(versionPrefix.toLatin1() + "meta.7z");
        const QString tmpTarget = repoDir + QLatin1String("/") + fn;

        createArchive(tmpTarget, QStringList() << absPath);

        // remove the files that got compressed
        QInstaller::removeFiles(absPath, true);
        QFile tmp(tmpTarget);
        tmp.open(QFile::ReadOnly);
        const QByteArray sha1Sum = QInstaller::calculateHash(&tmp, QCryptographicHash::Sha1);
        writeSHA1ToNodeWithName(doc, elements, sha1Sum, path);
        const QString finalTarget = absPath + QLatin1String("/") + fn;
        if (!tmp.rename(finalTarget)) {
            throw QInstaller::Error(QString::fromLatin1("Cannot move file \"%1\" to \"%2\".").arg(
                                        QDir::toNativeSeparators(tmpTarget), QDir::toNativeSeparators(finalTarget)));
        }
        dir.cdUp();
    }
}

void QInstallerTools::copyScriptFiles(const QDomNodeList &childNodes, const PackageInfo &info, bool &foundDownloadableArchives, const QString &targetDir)
{
    for (int i = 0; i < childNodes.count(); ++i) {
        const QDomNode node = childNodes.at(i);
        const QString key = node.nodeName();

        if (key != QLatin1String("Script"))
            continue;
        const QString script = node.toElement().text();
        if (script.isEmpty())
            continue;

        QFile scriptFile(QString::fromLatin1("%1/meta/%2").arg(info.directory, script));
        if (!scriptFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            throw QInstaller::Error(QString::fromLatin1("Cannot open component script at \"%1\".")
                                        .arg(QDir::toNativeSeparators(scriptFile.fileName())));
        }

        const QString scriptContent = QLatin1String("(function() {")
                                      + QString::fromUtf8(scriptFile.readAll())
                                      + QLatin1String(";"
                                                      "    if (typeof Component == \"undefined\")"
                                                      "        throw \"Missing Component constructor. Please check your script.\";"
                                                      "})();");

        // if the user isn't aware of the downloadable archives value we will add it automatically later
        foundDownloadableArchives |= scriptContent.contains(QLatin1String("addDownloadableArchive"))
                                     || scriptContent.contains(QLatin1String("removeDownloadableArchive"));

        static QInstaller::ScriptEngine testScriptEngine;
        const QJSValue value = testScriptEngine.evaluate(scriptContent, scriptFile.fileName());
        if (value.isError()) {
            throw QInstaller::Error(QString::fromLatin1("Exception while loading component "
                        "script at \"%1\": %2").arg(QDir::toNativeSeparators(scriptFile.fileName()),
                        value.toString().isEmpty() ? QString::fromLatin1("Unknown error.") :
                        value.toString() + QStringLiteral(" on line number: ") +
                        value.property(QStringLiteral("lineNumber")).toString()));
        }

        const QString toLocation(QString::fromLatin1("%1/%2/%3").arg(targetDir, info.name, script));
        copyWithException(scriptFile.fileName(), toLocation, QInstaller::scScript);
    }
}

void QInstallerTools::copyComponentData(const QStringList &packageDirs, const QString &repoDir,
    PackageInfoVector *const infos, const QString &archiveSuffix, Compression compression)
{
    for (int i = 0; i < infos->count(); ++i) {
        const PackageInfo info = infos->at(i);
        const QString name = info.name;
        qDebug() << "Copying component data for" << name;

        const QString namedRepoDir = QString::fromLatin1("%1/%2").arg(repoDir, name);
        if (!QDir().mkpath(namedRepoDir)) {
            throw QInstaller::Error(QString::fromLatin1("Cannot create repository directory for component \"%1\".")
                .arg(name));
        }

        if (info.copiedFiles.isEmpty()) {
            QStringList compressedFiles;
            QStringList filesToCompress;
            foreach (const QString &packageDir, packageDirs) {
                const QDir dataDir(QString::fromLatin1("%1/%2/data").arg(packageDir, name));
                foreach (const QString &entry, dataDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Files)) {
                    QFileInfo fileInfo(dataDir.absoluteFilePath(entry));
                    if (fileInfo.isFile() && !fileInfo.isSymLink()) {
                        const QString absoluteEntryFilePath = dataDir.absoluteFilePath(entry);
                        QScopedPointer<AbstractArchive> archive(ArchiveFactory::instance()
                            .create(absoluteEntryFilePath));
                        if (archive && archive->open(QIODevice::ReadOnly) && archive->isSupported()) {
                            QFile tmp(absoluteEntryFilePath);
                            QString target = QString::fromLatin1("%1/%3%2").arg(namedRepoDir, entry, info.version);
                            qDebug() << "Copying archive from" << tmp.fileName() << "to" << target;
                            if (!tmp.copy(target)) {
                                throw QInstaller::Error(QString::fromLatin1("Cannot copy file \"%1\" to \"%2\": %3")
                                    .arg(QDir::toNativeSeparators(tmp.fileName()), QDir::toNativeSeparators(target), tmp.errorString()));
                            }
                            compressedFiles.append(target);
                        } else {
                            filesToCompress.append(absoluteEntryFilePath);
                        }
                    } else if (fileInfo.isDir()) {
                        qDebug() << "Compressing data directory" << entry;
                        QString target = QString::fromLatin1("%1/%3%2.%4").arg(namedRepoDir, entry, info.version, archiveSuffix);
                        createArchive(target, QStringList() << dataDir.absoluteFilePath(entry), compression);
                        compressedFiles.append(target);
                    } else if (fileInfo.isSymLink()) {
                        filesToCompress.append(dataDir.absoluteFilePath(entry));
                    }
                }
            }

            if (!filesToCompress.isEmpty()) {
                qDebug() << "Compressing files found in data directory:" << filesToCompress;
                QString target = QString::fromLatin1("%1/%2content.%3").arg(namedRepoDir, info.version, archiveSuffix);
                createArchive(target, filesToCompress, compression);
                compressedFiles.append(target);
            }

            foreach (const QString &target, compressedFiles) {
                (*infos)[i].copiedFiles.append(target);

                QFile archiveFile(target);
                QFile archiveHashFile(archiveFile.fileName() + QLatin1String(".sha1"));

                qDebug() << "Hash is stored in" << archiveHashFile.fileName();
                qDebug() << "Creating hash of archive" << archiveFile.fileName();

                try {
                    QInstaller::openForRead(&archiveFile);
                    const QByteArray hashOfArchiveData = QInstaller::calculateHash(&archiveFile,
                        QCryptographicHash::Sha1).toHex();
                    archiveFile.close();

                    QInstaller::openForWrite(&archiveHashFile);
                    archiveHashFile.write(hashOfArchiveData);
                    qDebug() << "Generated sha1 hash:" << hashOfArchiveData;
                    (*infos)[i].copiedFiles.append(archiveHashFile.fileName());
                    if ((*infos)[i].createContentSha1Node)
                        (*infos)[i].contentSha1 = QLatin1String(hashOfArchiveData);
                    archiveHashFile.close();
                } catch (const QInstaller::Error &/*e*/) {
                    archiveFile.close();
                    archiveHashFile.close();
                    throw;
                }
            }
        } else {
            foreach (const QString &file, (*infos)[i].copiedFiles) {
                QFileInfo fromInfo(file);
                QFile from(file);
                QString target = QString::fromLatin1("%1/%2").arg(namedRepoDir, fromInfo.fileName());
                qDebug() << "Copying file from" << from.fileName() << "to" << target;
                if (!from.copy(target)) {
                    throw QInstaller::Error(QString::fromLatin1("Cannot copy file \"%1\" to \"%2\": %3")
                        .arg(QDir::toNativeSeparators(from.fileName()), QDir::toNativeSeparators(target), from.errorString()));
                }
            }
        }
    }
}

void QInstallerTools::filterNewComponents(const QString &repositoryDir, QInstallerTools::PackageInfoVector &packages)
{
    QDomDocument doc;
    QFile file(repositoryDir + QLatin1String("/Updates.xml"));
    if (file.open(QFile::ReadOnly) && doc.setContent(&file)) {
        const QDomElement root = doc.documentElement();
        if (root.tagName() != QLatin1String("Updates")) {
            throw QInstaller::Error(QCoreApplication::translate("QInstaller",
                "Invalid content in \"%1\".").arg(QDir::toNativeSeparators(file.fileName())));
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
                    const QDomElement c2Element = c2.at(j).toElement();
                    if (c2Element.tagName() == scName)
                        info.name = c2Element.text();
                    else if (c2Element.tagName() == scVersion)
                        info.version = c2Element.text();
                }
                hash.insert(info.name, info);
            }
        }

        // remove all components that have no update (decision based on the version tag)
        for (int i = packages.count() - 1; i >= 0; --i) {
            const QInstallerTools::PackageInfo info = packages.at(i);

            // check if component already exists & version did not change
            if (hash.contains(info.name) && KDUpdater::compareVersion(info.version, hash.value(info.name).version) < 1) {
                packages.remove(i); // the version did not change, no need to update the component
                continue;
            }
            qDebug() << "Update component" << info.name << "in"<< repositoryDir << ".";
        }
    }
}

QString QInstallerTools::existingUniteMeta7z(const QString &repositoryDir)
{
    QString uniteMeta7z = QString();
    QFile file(repositoryDir + QLatin1String("/Updates.xml"));
    QDomDocument doc;
    if (file.open(QFile::ReadOnly) && doc.setContent(&file)) {
        QDomNodeList elements =  doc.elementsByTagName(QLatin1String("MetadataName"));
        if (elements.count() > 0 && elements.at(0).isElement()) {
            uniteMeta7z = elements.at(0).toElement().text();
            QFile metaFile(repositoryDir + QDir::separator() + uniteMeta7z);
            if (!metaFile.exists()) {
                throw QInstaller::Error(QString::fromLatin1("Unite meta7z \"%1\" does not exist in repository \"%2\"")
                    .arg(QDir::toNativeSeparators(metaFile.fileName()), repositoryDir));
            }
        }
    }
    return uniteMeta7z;
}

PackageInfoVector QInstallerTools::collectPackages(RepositoryInfo info, QStringList *filteredPackages, FilterType filterType, bool updateNewComponents, QStringList packagesUpdatedWithSha)
{
    PackageInfoVector packages;
    PackageInfoVector precompressedPackages = QInstallerTools::createListOfRepositoryPackages(info.repositoryPackages,
        filteredPackages, filterType);
    packages.append(precompressedPackages);

    PackageInfoVector preparedPackages = QInstallerTools::createListOfPackages(info.packages,
        filteredPackages, filterType, packagesUpdatedWithSha);
    packages.append(preparedPackages);
    if (updateNewComponents) {
         filterNewComponents(info.repositoryDir, packages);
    }
    foreach (const QInstallerTools::PackageInfo &package, packages) {
        const QFileInfo fi(info.repositoryDir, package.name);
        if (fi.exists())
            removeDirectory(fi.absoluteFilePath());
    }
    return packages;
}

void QInstallerTools::createRepository(RepositoryInfo info, PackageInfoVector *packages,
        const QString &tmpMetaDir, bool createComponentMetadata, bool createUnifiedMetadata,
        const QString &archiveSuffix, Compression compression)
{
    QHash<QString, QString> pathToVersionMapping = QInstallerTools::buildPathToVersionMapping(*packages);

    QStringList directories;
    directories.append(info.packages);
    directories.append(info.repositoryPackages);
    QStringList unite7zFiles;
    foreach (const QString &repositoryDirectory, info.repositoryPackages) {
        QDirIterator it(repositoryDirectory, QStringList(QLatin1String("*_meta.7z"))
                        , QDir::Files | QDir::CaseSensitive);
        while (it.hasNext()) {
            it.next();
            unite7zFiles.append(it.fileInfo().absoluteFilePath());
        }
    }
    QInstallerTools::copyComponentData(directories, info.repositoryDir, packages, archiveSuffix, compression);
    QInstallerTools::copyMetaData(tmpMetaDir, info.repositoryDir, *packages, QLatin1String("{AnyApplication}"),
        QLatin1String(QUOTE(IFW_REPOSITORY_FORMAT_VERSION)), unite7zFiles);

    QString existing7z = QInstallerTools::existingUniteMeta7z(info.repositoryDir);
    if (!existing7z.isEmpty())
        existing7z = info.repositoryDir + QDir::separator() + existing7z;
    QInstallerTools::compressMetaDirectories(tmpMetaDir, existing7z, pathToVersionMapping,
                                             createComponentMetadata, createUnifiedMetadata);

    QDirIterator it(info.repositoryDir, QStringList(QLatin1String("Updates*.xml"))
                    << QLatin1String("*_meta.7z"), QDir::Files | QDir::CaseSensitive);
    while (it.hasNext()) {
        it.next();
        QFile::remove(it.fileInfo().absoluteFilePath());
    }
    QInstaller::moveDirectoryContents(tmpMetaDir, info.repositoryDir);
}
