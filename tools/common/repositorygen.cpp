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
#include "repositorygen.h"

#include <fileutils.h>
#include <errors.h>
#include <lib7z_facade.h>
#include <settings.h>
#include <qinstallerglobal.h>
#include <utils.h>

#include <kdupdater.h>

#include <QtCore/QDirIterator>

#include <QtScript/QScriptEngine>

#include <QtXml/QDomDocument>

#include <iostream>

using namespace QInstallerTools;

void QInstallerTools::printRepositoryGenOptions()
{
    std::cout << "  -c|--config file          The file containing the installer configuration" << std::endl;

    std::cout << "  -p|--packages dir         The directory containing the available packages." << std::endl;
    std::cout << "                            Defaults to the current working directory." << std::endl;

    std::cout << "  -e|--exclude p1,...,pn    Exclude the given packages." << std::endl;
    std::cout << "  -i|--include p1,...,pn    Include the given packages and their dependencies" << std::endl;
    std::cout << "                            from the repository." << std::endl;

    std::cout << "  --ignore-translations     Don't use any translation" << std::endl;
    std::cout << "  --ignore-invalid-packages Ignore all invalid packages instead of aborting." << std::endl;
}

void QInstallerTools::compressPaths(const QStringList &paths, const QString &archivePath)
{
    QFile archive(archivePath);
    QInstaller::openForWrite(&archive, archivePath);
    Lib7z::createArchive(&archive, paths);
}

void QInstallerTools::compressMetaDirectories(const QString &repoDir)
{
    QDir dir(repoDir);
    const QStringList sub = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    foreach (const QString &i, sub) {
        QDir sd(dir);
        sd.cd(i);
        const QString absPath = sd.absolutePath();
        const QString fn = QLatin1String("meta.7z");
        const QString tmpTarget = repoDir + QLatin1String("/") +fn;
        compressPaths(QStringList() << absPath, tmpTarget);
        QFile tmp(tmpTarget);
        const QString finalTarget = absPath + QLatin1String("/") + fn;
        if (!tmp.rename(finalTarget)) {
            throw QInstaller::Error(QString::fromLatin1("Could not move file from '%1' to '%2'").arg(tmpTarget,
                finalTarget));
        }
    }
}

void QInstallerTools::generateMetaDataDirectory(const QString &outDir, const QString &dataDir,
    const PackageInfoVector &packages, const QString &appName, const QString &appVersion,
    const QString &redirectUpdateUrl)
{
    QString metapath = outDir;
    if (QFileInfo(metapath).isRelative())
        metapath = QDir::cleanPath(QDir::current().absoluteFilePath(metapath));
    qDebug() << "Generating meta data...";

    if (!QFile::exists(metapath))
        QInstaller::mkpath(metapath);

    QDomDocument doc;
    QDomElement root;
    // use existing Updates.xml, if any
    QFile existingUpdatesXml(QFileInfo(dataDir, QLatin1String("Updates.xml")).absoluteFilePath());
    if (!existingUpdatesXml.open(QIODevice::ReadOnly) || !doc.setContent(&existingUpdatesXml)) {
        root = doc.createElement(QLatin1String("Updates"));
        root.appendChild(doc.createElement(QLatin1String("ApplicationName"))).appendChild(
            doc.createTextNode(appName));
        root.appendChild(doc.createElement(QLatin1String("ApplicationVersion"))).appendChild(
            doc.createTextNode(appVersion));
        root.appendChild(doc.createElement(QLatin1String("Checksum"))).appendChild(
            doc.createTextNode(QLatin1String("true")));
        if (!redirectUpdateUrl.isEmpty()) {
            root.appendChild(doc.createElement(QLatin1String("RedirectUpdateUrl"))).appendChild(
                doc.createTextNode(redirectUpdateUrl));
        }
    } else {
        root = doc.documentElement();
    }

    for (PackageInfoVector::const_iterator it = packages.begin(); it != packages.end(); ++it) {
        const QString packageXmlPath = QString::fromLatin1("%1/meta/package.xml").arg(it->directory);
        qDebug() << QString::fromLatin1("\tGenerating meta data for package %1 using %2.").arg(
            it->name, packageXmlPath);

        // remove existing entry for this component from existing Updates.xml
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
        QInstaller::openForRead(&file, packageXmlPath);
        QString errMsg;
        int col = 0;
        int line = 0;
        if (!packageXml.setContent(&file, &errMsg, &line, &col)) {
            throw QInstaller::Error(QString::fromLatin1("Could not parse '%1': line: %2, column: %3: %4 (%5)")
                .arg(packageXmlPath, QString::number(line), QString::number(col), errMsg, it->name));
        }
        const QDomNode package = packageXml.firstChildElement(QLatin1String("Package"));

        QDomElement update = doc.createElement(QLatin1String("PackageUpdate"));
        QDomElement nameElement = doc.createElement(QLatin1String("Name"));
        nameElement.appendChild(doc.createTextNode(it->name));
        update.appendChild(nameElement);

        // list of current unused or later transformed tags
        QStringList blackList;
        blackList << QLatin1String("UserInterfaces") << QLatin1String("Translations") <<
            QLatin1String("Licenses") << QLatin1String("Name");

        bool foundDefault = false;
        bool foundVirtual = false;
        const QDomNodeList childNodes = package.childNodes();
        for (int i = 0; i < childNodes.count(); ++i) {
            const QDomNode node = childNodes.at(i);
            const QString key = node.nodeName();

            if (key == QLatin1String("Default"))
                foundDefault = true;
            if (key == QLatin1String("Virtual"))
                foundVirtual = true;
            if (node.isComment() || blackList.contains(key))
                continue;   // just skip comments and some tags...

            const QString value = node.toElement().text();
            QDomElement element = doc.createElement(key);
            for (int  i = 0; i < node.attributes().size(); i++) {
                element.setAttribute(node.attributes().item(i).toAttr().name(),
                    node.attributes().item(i).toAttr().value());
            }
            update.appendChild(element).appendChild(doc.createTextNode(value));
        }

        if (foundDefault && foundVirtual) {
            throw QInstaller::Error(QString::fromLatin1("Error: <Default> and <Virtual> elements are "
                "mutually exclusive. File: '%0'").arg(packageXmlPath));
        }

        // get the size of the data
        quint64 componentSize = 0;
        quint64 compressedComponentSize = 0;

        const QDir::Filters filters = QDir::Files | QDir::NoDotAndDotDot;
        const QDir cmpDataDir = QString::fromLatin1("%1/%2/data").arg(dataDir, it->name);
        const QFileInfoList entries = cmpDataDir.exists() ? cmpDataDir.entryInfoList(filters | QDir::Dirs)
            : QDir(QString::fromLatin1("%1/%2").arg(dataDir, it->name)).entryInfoList(filters);

        foreach (const QFileInfo &fi, entries) {
            if (fi.isHidden())
                continue;

            try {
                if (fi.isDir()) {
                    QDirIterator recursDirIt(fi.filePath(), QDirIterator::Subdirectories);
                    while (recursDirIt.hasNext()) {
                        const quint64 size = QFile(recursDirIt.next()).size();
                        componentSize += size;
                        compressedComponentSize += size;
                    }
                } else if (Lib7z::isSupportedArchive(fi.filePath())) {
                    // if it's an archive already, list its files and sum the uncompressed sizes
                    QFile archive(fi.filePath());
                    compressedComponentSize += archive.size();
                    archive.open(QIODevice::ReadOnly);
                    const QVector< Lib7z::File > files = Lib7z::listArchive(&archive);
                    for (QVector< Lib7z::File >::const_iterator fileIt = files.begin();
                        fileIt != files.end(); ++fileIt) {
                            componentSize += fileIt->uncompressedSize;
                    }
                } else {
                    // otherwise just add its size
                    const quint64 size = fi.size();
                    componentSize += size;
                    compressedComponentSize += size;
                }
            } catch(...) {
                // ignore, that's just about the sizes - and size doesn't matter, you know?
            }
        }

        QDomElement fileElement = doc.createElement(QLatin1String("UpdateFile"));
        fileElement.setAttribute(QLatin1String("UncompressedSize"), componentSize);
        fileElement.setAttribute(QLatin1String("CompressedSize"), compressedComponentSize);
        update.appendChild(fileElement);

        root.appendChild(update);

        if (!QDir(metapath).mkpath(it->name))
            throw QInstaller::Error(QString::fromLatin1("Could not create directory '%1'.").arg(it->name));

        // copy scripts
        const QString script = package.firstChildElement(QLatin1String("Script")).text();
        if (!script.isEmpty()) {
            const QString fromLocation(QString::fromLatin1("%1/meta/%2").arg(it->directory, script));

            QString scriptContent;
            QFile scriptFile(fromLocation);
            if (scriptFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QTextStream in(&scriptFile);
                scriptContent = in.readAll();
            }
            static QScriptEngine testScriptEngine;
            testScriptEngine.evaluate(scriptContent, scriptFile.fileName());
            if (testScriptEngine.hasUncaughtException()) {
                throw QInstaller::Error(QString::fromLatin1("Exception while loading the component script: '%1'")
                    .arg(QInstaller::uncaughtExceptionString(&testScriptEngine, scriptFile.fileName())));
            }

            // added the xml tag RequiresAdminRights to the xml if somewhere addElevatedOperation is used
            if (scriptContent.contains(QLatin1String("addElevatedOperation"))) {
                QDomElement requiresAdminRightsElement =
                    doc.createElement(QLatin1String("RequiresAdminRights"));
                requiresAdminRightsElement.appendChild(doc.createTextNode(QLatin1String("true")));
            }

            qDebug() << "\tCopying associated script" << script << "into the meta package...";
            QString toLocation(QString::fromLatin1("%1/%2/%3").arg(metapath, it->name, script));
            if (!scriptFile.copy(toLocation)) {
                qDebug() << "failed!";
                throw QInstaller::Error(QString::fromLatin1("Could not copy the script '%1' to its target location '%2'.")
                    .arg(fromLocation, toLocation));
            } else {
                qDebug() << "\tdone.";
            }
        }

        // copy user interfaces
        const QDomNodeList uiNodes = package.firstChildElement(QLatin1String("UserInterfaces")).childNodes();
        QStringList userinterfaces;
        for (int i = 0; i < uiNodes.count(); ++i) {
            const QDomNode node = uiNodes.at(i);
            if (node.nodeName() != QLatin1String("UserInterface"))
                continue;

            const QDir dir(QString::fromLatin1("%1/meta").arg(it->directory));
            const QStringList uis = dir.entryList(QStringList(node.toElement().text()), QDir::Files);
            if (uis.isEmpty()) {
                throw QInstaller::Error(QString::fromLatin1("Couldn't find any user interface matching '%1' while "
                    "copying user interfaces of '%2'.").arg(node.toElement().text(), it->name));
            }

            for (QStringList::const_iterator ui = uis.begin(); ui != uis.end(); ++ui) {
                qDebug() << "\tCopying associated user interface" << *ui << "into the meta package...";
                userinterfaces.push_back(*ui);
                if (!QFile::copy(QString::fromLatin1("%1/meta/%2").arg(it->directory, *ui),
                    QString::fromLatin1("%1/%2/%3").arg(metapath, it->name, *ui))) {
                        qDebug() << "failed!";
                        throw QInstaller::Error(QString::fromLatin1("Could not copy the UI file '%1' to its target "
                            "location '%2'.").arg(*ui, it->name));
                } else {
                    qDebug() << "done";
                }
            }
        }

        if (!userinterfaces.isEmpty()) {
            update.appendChild(doc.createElement(QLatin1String("UserInterfaces")))
                .appendChild(doc.createTextNode(userinterfaces.join(QChar::fromLatin1(','))));
        }

        // copy translations
        const QDomNodeList qmNodes = package.firstChildElement(QLatin1String("Translations")).childNodes();
        QStringList translations;
        if (!qApp->arguments().contains(QString::fromLatin1("--ignore-translations"))) {
            for (int i = 0; i < qmNodes.count(); ++i) {
                const QDomNode node = qmNodes.at(i);
                if (node.nodeName() != QLatin1String("Translation"))
                    continue;

                const QDir dir(QString::fromLatin1("%1/meta").arg(it->directory));
                const QStringList qms = dir.entryList(QStringList(node.toElement().text()), QDir::Files);
                if (qms.isEmpty()) {
                    throw QInstaller::Error(QString::fromLatin1("Could not find any translation file matching '%1' "
                        "while copying translations of '%2'.").arg(node.toElement().text(), it->name));
                }

                for (QStringList::const_iterator qm = qms.begin(); qm != qms.end(); ++qm) {
                    qDebug() << "\tCopying associated translation" << *qm << "into the meta package...";
                    translations.push_back(*qm);
                    if (!QFile::copy(QString::fromLatin1("%1/meta/%2").arg(it->directory, *qm),
                        QString::fromLatin1("%1/%2/%3").arg(metapath, it->name, *qm))) {
                            qDebug() << "failed!";
                            throw QInstaller::Error(QString::fromLatin1("Could not copy the translation '%1' to its "
                                "target location '%2'.").arg(*qm, it->name));
                    } else {
                        qDebug() << "done";
                    }
                }
            }

            if (!translations.isEmpty()) {
                update.appendChild(doc.createElement(QLatin1String("Translations")))
                    .appendChild(doc.createTextNode(translations.join(QChar::fromLatin1(','))));
            }

        }

        // copy license files
        const QDomNodeList licenseNodes = package.firstChildElement(QLatin1String("Licenses")).childNodes();
        for (int i = 0; i < licenseNodes.count(); ++i) {
            const QDomNode licenseNode = licenseNodes.at(i);
            if (licenseNode.nodeName() == QLatin1String("License")) {
                const QString &licenseFile =
                    licenseNode.toElement().attributeNode(QLatin1String("file")).value();
                const QString &sourceFile =
                    QString::fromLatin1("%1/meta/%2").arg(it->directory).arg(licenseFile);
                if (!QFile::exists(sourceFile)) {
                    throw QInstaller::Error(QString::fromLatin1("Could not find any license matching '%1' while "
                        "copying license files of '%2'.").arg(licenseFile, it->name));
                }

                qDebug() << "\tCopying associated license file" << licenseFile << "into the meta package...";
                if (!QFile::copy(sourceFile, QString::fromLatin1("%1/%2/%3")
                    .arg(metapath, it->name, licenseFile))) {
                        qDebug() << "failed!";
                        throw QInstaller::Error(QString::fromLatin1("Could not copy the license file '%1' to its "
                            "target location '%2'.").arg(licenseFile, it->name));
                } else {
                    qDebug() << "done.";
                }

                // Translated License files
                for (int j = 0; j < translations.size(); ++j) {
                    QFileInfo translationFile(translations.at(j));
                    QFileInfo untranslated(licenseFile);
                    const QString &translatedLicenseFile =
                        QString::fromLatin1("%2_%3.%4").arg(untranslated.baseName(),
                        translationFile.baseName(), untranslated.completeSuffix());
                    const QString &translatedSourceFile =
                        QString::fromLatin1("%1/meta/%2").arg(it->directory).arg(translatedLicenseFile);
                    if (!QFile::exists(translatedSourceFile)) {
                        qDebug() << "Could not find translated license file" << translatedSourceFile;
                        continue;
                    }

                    qDebug() << "\tCopying associated license file" << translatedLicenseFile
                        << "into the meta package...";

                    if (!QFile::copy(translatedSourceFile, QString::fromLatin1("%1/%2/%3")
                        .arg(metapath, it->name, translatedLicenseFile))) {
                            qDebug() << "\tfailed!";
                    } else {
                        qDebug() << "\tdone.";
                    }
                }
            }
        }

        if (licenseNodes.count() > 0)
            update.appendChild(package.firstChildElement(QLatin1String("Licenses")).cloneNode());
    }

    doc.appendChild(root);

    const QString updatesXmlFile = QFileInfo(metapath, QLatin1String("Updates.xml")).absoluteFilePath();
    QFile updatesXml(updatesXmlFile);

    QInstaller::openForWrite(&updatesXml, updatesXmlFile);
    QInstaller::blockingWrite(&updatesXml, doc.toByteArray());
}

PackageInfoVector QInstallerTools::createListOfPackages(const QString &packagesDirectory,
    const QStringList &filteredPackages, FilterType filterType)
{
    qDebug() << "Collecting information about available packages...";

    bool ignoreInvalidPackages = qApp->arguments().contains(QString::fromLatin1("--ignore-invalid-packages"));

    PackageInfoVector dict;
    const QFileInfoList entries = QDir(packagesDirectory)
        .entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (QFileInfoList::const_iterator it = entries.begin(); it != entries.end(); ++it) {
        if (filterType == Exclude) {
            if (filteredPackages.contains(it->fileName()))
                continue;
        } else {
            if (!filteredPackages.contains(it->fileName()))
                continue;
        }
        qDebug() << QString::fromLatin1("\tfound subdirectory '%1'").arg(it->fileName());
        // because the filter is QDir::Dirs - filename means the name of the subdirectory
        if (it->fileName().contains(QLatin1Char('-'))) {
            if (ignoreInvalidPackages)
                continue;
            throw QInstaller::Error(QString::fromLatin1("Component '%1' mustn't contain '-'. This is not allowed, because "
                "dashes are used as the separator between the component name and the version number internally.")
                .arg(it->fileName()));
        }

        QFile file(QString::fromLatin1("%1/meta/package.xml").arg(it->filePath()));
        QFileInfo fileInfo(file);
        if (!fileInfo.exists()) {
            if (ignoreInvalidPackages)
                continue;
            throw QInstaller::Error(QString::fromLatin1("Component '%1' does not contain a package "
                "description (meta/package.xml is missing).").arg(it->fileName()));
        }

        file.open(QIODevice::ReadOnly);

        QDomDocument doc;
        QString error;
        int errorLine = 0;
        int errorColumn = 0;
        if (!doc.setContent(&file, &error, &errorLine, &errorColumn)) {
            if (ignoreInvalidPackages)
                continue;
            throw QInstaller::Error(QString::fromLatin1("Component package description in '%1' is invalid. "
                "Error at line: %2, column: %3 -> %4").arg(fileInfo.absoluteFilePath(), QString::number(errorLine),
                QString::number(errorColumn), error));
        }

        const QDomElement packageElement = doc.firstChildElement(QLatin1String("Package"));
        const QString name = packageElement.firstChildElement(QLatin1String("Name")).text();
        if (!name.isEmpty() && name != it->fileName()) {
            qWarning() << QString::fromLatin1("The <Name> tag in the '%1' is ignored - the installer uses the "
                "path element right before the 'meta' ('%2').").arg(fileInfo.absoluteFilePath(), it->fileName());
        }

        const QString releaseDate = packageElement.firstChildElement(QLatin1String("ReleaseDate")).text();
        if (releaseDate.isEmpty() || (!QDate::fromString(releaseDate, Qt::ISODate).isValid())) {
            if (ignoreInvalidPackages)
                continue;
            throw QInstaller::Error(QString::fromLatin1("Release date for '%1' is invalid! <ReleaseDate>%2"
                "</ReleaseDate>. Supported format: YYYY-MM-DD").arg(fileInfo.absoluteFilePath(), releaseDate));
        }

        PackageInfo info;
        info.name = it->fileName();
        info.version = packageElement.firstChildElement(QLatin1String("Version")).text();
        if (!QRegExp(QLatin1String("[0-9]+((\\.|-)[0-9]+)*")).exactMatch(info.version)) {
            if (ignoreInvalidPackages)
                continue;
            throw QInstaller::Error(QString::fromLatin1("Component version for '%1' is invalid! <Version>%2</Version>")
                .arg(fileInfo.absoluteFilePath(), info.version));
        }
        info.dependencies = packageElement.firstChildElement(QLatin1String("Dependencies")).text()
            .split(QInstaller::scCommaRegExp, QString::SkipEmptyParts);
        info.directory = it->filePath();
        dict.push_back(info);

        qDebug() << QString::fromLatin1("\t- it provides the package %1 - %2").arg(name, info.version);
    }

    if (dict.isEmpty())
        qDebug() << "No available packages found at the specified location.";

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
    const QString &nodename)
{
    qDebug() << "searching sha1sum node for" << nodename;
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

void QInstallerTools::compressMetaDirectories(const QString &repoDir, const QString &baseDir,
    const QHash<QString, QString> &versionMapping)
{
    QDomDocument doc;
    QDomElement root;
    // use existing Updates.xml, if any
    QFile existingUpdatesXml(QFileInfo(QDir(repoDir), QLatin1String("Updates.xml")).absoluteFilePath());
    if (!existingUpdatesXml.open(QIODevice::ReadOnly) || !doc.setContent(&existingUpdatesXml)) {
        qDebug() << "Could not find Updates.xml";
    } else {
        root = doc.documentElement();
    }
    existingUpdatesXml.close();

    QDir dir(repoDir);
    const QStringList sub = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    QDomNodeList elements =  doc.elementsByTagName(QLatin1String("PackageUpdate"));
    foreach (const QString &i, sub) {
        QDir sd(dir);
        sd.cd(i);
        const QString path = QString(i).remove(baseDir);
        const QString versionPrefix = versionMapping[path];
        if (path.isNull())
            continue;
        const QString absPath = sd.absolutePath();
        const QString fn = QLatin1String(versionPrefix.toLatin1() + "meta.7z");
        const QString tmpTarget = repoDir + QLatin1String("/") +fn;
        compressPaths(QStringList() << absPath, tmpTarget);

        // remove the files that got compressed
        QInstaller::removeFiles(absPath, true);

        QFile tmp(tmpTarget);
        tmp.open(QFile::ReadOnly);
        const QByteArray sha1Sum = QInstaller::calculateHash(&tmp, QCryptographicHash::Sha1);
        writeSHA1ToNodeWithName(doc, elements, sha1Sum, path);
        const QString finalTarget = absPath + QLatin1String("/") + fn;
        if (!tmp.rename(finalTarget))
            throw QInstaller::Error(QString::fromLatin1("Could not move '%1' to '%2'").arg(tmpTarget, finalTarget));
    }

    QInstaller::openForWrite(&existingUpdatesXml, existingUpdatesXml.fileName());
    QInstaller::blockingWrite(&existingUpdatesXml, doc.toByteArray());
    existingUpdatesXml.close();
}

void QInstallerTools::copyComponentData(const QString &packageDir, const QString &repoDir,
    PackageInfoVector &infos)
{
    for (int i = 0; i < infos.count(); ++i) {
        const PackageInfo info = infos.at(i);
        const QString name = info.name;
        qDebug() << "Copying component data for" << name;

        const QString namedRepoDir = QString::fromLatin1("%1/%2").arg(repoDir, name);
        if (!QDir().mkpath(namedRepoDir)) {
            throw QInstaller::Error(QString::fromLatin1("Could not create repository folder for component '%1'")
                .arg(name));
        }

        QStringList compressedFiles;
        QStringList filesToCompress;
        const QDir dataDir(QString::fromLatin1("%1/%2/data").arg(packageDir, name));
        foreach (const QString &entry, dataDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Files)) {
            QFileInfo fileInfo(dataDir.absoluteFilePath(entry));
            if (fileInfo.isFile() && !fileInfo.isSymLink()) {
                const QString absoluteEntryFilePath = dataDir.absoluteFilePath(entry);
                if (Lib7z::isSupportedArchive(absoluteEntryFilePath)) {
                    QFile tmp(absoluteEntryFilePath);
                    QString target = QString::fromLatin1("%1/%3%2").arg(namedRepoDir, entry, info.version);
                    qDebug() << QString::fromLatin1("Copying archive from '%1' to '%2'").arg(tmp.fileName(),
                        target);
                    if (!tmp.copy(target)) {
                        throw QInstaller::Error(QString::fromLatin1("Could not copy '%1' to '%2': %3").arg(tmp.fileName(),
                            target, tmp.errorString()));
                    }
                    compressedFiles.append(target);
                } else {
                    filesToCompress.append(absoluteEntryFilePath);
                }
            } else if (fileInfo.isDir()) {
                qDebug() << "Compressing data directory" << entry;
                QString target = QString::fromLatin1("%1/%3%2.7z").arg(namedRepoDir, entry, info.version);
                QInstallerTools::compressPaths(QStringList() << dataDir.absoluteFilePath(entry), target);
                compressedFiles.append(target);
            } else if (fileInfo.isSymLink()) {
                filesToCompress.append(dataDir.absoluteFilePath(entry));
            }
        }

        if (!filesToCompress.isEmpty()) {
            qDebug() << "Compressing files found in data directory:" << filesToCompress;
            QString target = QString::fromLatin1("%1/%3%2").arg(namedRepoDir, QLatin1String("content.7z"),
                info.version);
            QInstallerTools::compressPaths(filesToCompress, target);
            compressedFiles.append(target);
        }

        foreach (const QString &target, compressedFiles) {
            infos[i].copiedArchives.append(target);

            QFile archiveFile(target);
            QFile archiveHashFile(archiveFile.fileName() + QLatin1String(".sha1"));

            qDebug() << "Hash is stored in" << archiveHashFile.fileName();
            qDebug() << "Creating hash of archive" << archiveFile.fileName();

            try {
                QInstaller::openForRead(&archiveFile, archiveFile.fileName());
                const QByteArray hashOfArchiveData = QInstaller::calculateHash(&archiveFile,
                    QCryptographicHash::Sha1).toHex();
                archiveFile.close();

                QInstaller::openForWrite(&archiveHashFile, archiveHashFile.fileName());
                archiveHashFile.write(hashOfArchiveData);
                qDebug() << "Generated sha1 hash:" << hashOfArchiveData;
                infos[i].copiedArchives.append(archiveHashFile.fileName());
                archiveHashFile.close();
            } catch (const QInstaller::Error &/*e*/) {
                archiveFile.close();
                archiveHashFile.close();
                throw;
            }
        }
    }
}
