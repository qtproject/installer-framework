/**************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
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

#include "downloader.h"
#include "domnodedebugstreamoperator.h"

#include <globals.h>
#include <init.h>
#include <fileutils.h>
#include <lib7z_facade.h>
#include <utils.h>

#include <QCoreApplication>
#include <QFile>
#include <QUrl>
#include <QString>
#include <QDomDocument>
#include <QDomElement>
#include <QDomNodeList>
#include <QStringList>
#include <QDebug>
#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#include <QDebug>

#include <iostream>

static void printUsage()
{
    const QString appName = QFileInfo( QCoreApplication::applicationFilePath() ).fileName();
    std::cout << "Usage: " << qPrintable(appName)
        << " --url <repository_url> --repository <empty_repository_dir> --packages <empty_packages_dir>" << std::endl;
    std::cout << "  --url              URL to fetch all the content from." << std::endl;
    std::cout << "  --repository       Target directory for the repository content." << std::endl;
    std::cout << "  --packages         The packages target directory where it creates the needed content to create new installers or repositories." << std::endl;
    std::cout << "  --clean            Removes all the content if there is an existing repository or packages dir" << std::endl;
    std::cout << "  --only-metacontent Download only the meta content of the components." << std::endl;

    std::cout << "Example:" << std::endl;
    std::cout << "  " << qPrintable(appName) << " --url http://www.example.com/repository/" <<
                 " --repository repository --packages packages" << std::endl;
}

// this should be a new class which uses XmlStreamReader instead of DomDocument
// should be implicit shared, see repository class
// maybe we can use some code from persistentdata in qtcreator
class ComponentData {
    public:
        ComponentData() {}
        ComponentData(const QString &/*xmlData*/) {}

        QVariant attributeValue(const QString &key, const QString &attribute, const QVariant &defaultValue = QVariant()) {
            Q_UNUSED(key)
            Q_UNUSED(attribute)
            return defaultValue;
        }

        QVariant value(const QString &key, const QVariant &defaultValue = QVariant()) {
            Q_UNUSED(defaultValue)
            // just use the quick dirty hack added members
            if (key == QLatin1String("Script"))
                return m_script;
            if (key == QLatin1String("Version"))
                return m_version;
            return QVariant();
        }
        QString textValue(const QString &key, const QString &defaultValue = QString()) {
            return value(key, defaultValue).toString();
        }

    // quick dirty hack added public members
    public:
        QDomDocument m_packageXml;
        QStringList m_downloadDownloadableArchives;
        QString m_script;
        QString m_version;
};

static void downloadFile(const QUrl &source, const QString &target)
{
    QEventLoop downloadEventLoop;
    Downloader downloader(source, target);
    QObject::connect(&downloader, SIGNAL(finished()), &downloadEventLoop, SLOT(quit()));
    downloader.run();
    downloadEventLoop.exec();
}

QHash<QString, ComponentData> downLoadRepository(const QString &repositoryUrl, const QString &repositoryTarget)
{
    const QString updatesXmlFileName = QLatin1String("Updates.xml");
    QHash<QString, ComponentData> componentDataHash;

    const QUrl updatesXmlUrl(QString::fromLatin1("%1/%2").arg(repositoryUrl, updatesXmlFileName));

    downloadFile(updatesXmlUrl, QDir(repositoryTarget).filePath(updatesXmlFileName));

    QFile updatesFile(QDir(repositoryTarget).filePath(updatesXmlFileName));
    if (!updatesFile.exists()) {
        qDebug() << "could not download the file:" << updatesXmlUrl.toString();
        return componentDataHash;
    } else
        qDebug() << "file downloaded to location:" << QDir(repositoryTarget).filePath(updatesXmlFileName);
    if (!updatesFile.open(QIODevice::ReadOnly)) {
        qDebug() << QString::fromLatin1("Could not open Updates.xml for reading: %1").arg(updatesFile
            .errorString()) ;
        return componentDataHash;
    }

    QStringList ignoreTagList;
    ignoreTagList << QLatin1String("Name");
    ignoreTagList << QLatin1String("ReleaseDate");
    ignoreTagList << QLatin1String("SHA1");
    ignoreTagList << QLatin1String("UpdateFile");
    QStringList fileTagList;
    fileTagList << QLatin1String("DownloadableArchives");


    QDomDocument updatesXml;
    QString error;
    int line = 0;
    int column = 0;
    if (!updatesXml.setContent( &updatesFile, &error, &line, &column)) {
        qWarning() << QString::fromLatin1("Could not parse component index: %1:%2: %3")
            .arg(QString::number(line), QString::number(column), error);
        return componentDataHash;
    }

    QDomNode packageUpdateDomNode = updatesXml.firstChildElement(QLatin1String("Updates")).firstChildElement(
        QLatin1String("PackageUpdate"));
    while (!packageUpdateDomNode.isNull()) {

        if (packageUpdateDomNode.nodeName() == QLatin1String("PackageUpdate")) {
            QDomNode packageUpdateEntry = packageUpdateDomNode.firstChild();
            QString currentPackageName;
            ComponentData currentComponentData;
            // creating the package.xml for later use
            QDomElement currentNewPackageElement = currentComponentData.m_packageXml.createElement(
                    QLatin1String("Package"));
            while (!packageUpdateEntry.isNull()) {
                // do name first before ignore filters the name out
                if (packageUpdateEntry.nodeName() == QLatin1String("Name")) {
                    currentPackageName = packageUpdateEntry.toElement().text();
                }
                if (ignoreTagList.contains(packageUpdateEntry.nodeName())) {
                    packageUpdateEntry = packageUpdateEntry.nextSibling();
                    continue;
                }
                if (packageUpdateEntry.nodeName() == QLatin1String("Script")) {
                    currentComponentData.m_script = packageUpdateEntry.toElement().text();
                }
                if (packageUpdateEntry.nodeName() == QLatin1String("Version")) {
                    currentComponentData.m_version = packageUpdateEntry.toElement().text();
                    currentComponentData.m_downloadDownloadableArchives.append(
                        currentComponentData.m_version + QLatin1String("meta.7z"));
                }

                if (packageUpdateEntry.nodeName() == QLatin1String("DownloadableArchives")) {
                    QStringList tDownloadList = packageUpdateEntry.toElement().text().split(
                        QInstaller::commaRegExp(), QString::SkipEmptyParts);
                    foreach (const QString &download, tDownloadList) {
                        if (qApp->arguments().contains(QLatin1String("--only-metacontent"))) {
                            qDebug() << "Skip download: <url> + " << currentPackageName + QLatin1String("/") + currentComponentData.m_version + download;
                        } else {
                            currentComponentData.m_downloadDownloadableArchives.append(
                                currentComponentData.m_version + download);
                        }
                        currentComponentData.m_downloadDownloadableArchives.append(
                            currentComponentData.m_version + download + QLatin1String(".sha1"));
                    }
                }

                currentNewPackageElement.appendChild(packageUpdateEntry.cloneNode(true));
                packageUpdateEntry = packageUpdateEntry.nextSibling();
            }
            currentComponentData.m_packageXml.appendChild(currentNewPackageElement);
            Q_ASSERT(!currentComponentData.m_packageXml.toString().isEmpty());
            componentDataHash.insert(currentPackageName, currentComponentData);
        } else {
            qWarning() << QString::fromLatin1("Unknown elment '%1'").arg(packageUpdateDomNode.nodeName(),
                QFileInfo(updatesXmlFileName).absoluteFilePath());
        }
        packageUpdateDomNode = packageUpdateDomNode.nextSibling();
    }

    QHashIterator<QString, ComponentData> itComponentData(componentDataHash);
    while (itComponentData.hasNext()) {
        itComponentData.next();
        QString componentDirectory = QDir(repositoryTarget).filePath(itComponentData.key());
        if (!QDir().mkpath(componentDirectory))
            qWarning() << "could not create:" << componentDirectory;

        foreach (const QString &download, itComponentData.value().m_downloadDownloadableArchives) {
            const QString fileTarget(componentDirectory + QDir::separator() + download);
            const QUrl downloadUrl(repositoryUrl + QLatin1String("/") + itComponentData.key() + QLatin1String("/") + download);
            downloadFile(downloadUrl, fileTarget);
        }
    }
    return componentDataHash;
}

bool extractFile(const QString &source, const QString &target)
{
    if (!Lib7z::isSupportedArchive(source)) {
        qWarning() << source << "is not a supported archive";
    }

    QFile archive(source);
    if (archive.open(QIODevice::ReadOnly)) {
        try {
            Lib7z::extractArchive(&archive, target);
        } catch (const Lib7z::SevenZipException& e) {
            qWarning() << QString::fromLatin1("Error while extracting %1: %2.").arg(source, e.message());
            return false;
        } catch (...) {
            qWarning() << QString::fromLatin1("Unknown exception caught while extracting %1.").arg(source);
            return false;
        }
    } else {
        qWarning() << QString::fromLatin1("Could not open %1 for reading: %2.").arg(
            target, archive.errorString());
        return false;
    }
    return true;
}


int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    // init installer to have the 7z lib initialized
    QInstaller::init();
    // with the installer messagehandler we need to enable verbose to see QDebugs
    QInstaller::setVerbose(true);

    QString repositoryUrl;
    QString repositoryTarget;
    QString packageDirectoryTarget;
    bool clean = false;
    QStringList args = app.arguments();
    QStringList::const_iterator itArgument = args.constBegin();
    itArgument++; // ignore the first one
    if (itArgument == args.constEnd()) {
        printUsage();
        return 0;
    }

    for (; itArgument != args.constEnd(); ++itArgument) {
        if (*itArgument == QString::fromLatin1("-h") || *itArgument == QString::fromLatin1("--help")) {
            printUsage();
            return 0;
        } else if (*itArgument == QString::fromLatin1("--only-metacontent")) {
            // just consume that argument, it will be used later via qApp->arguments
        } else if (*itArgument == QString::fromLatin1("--clean")) {
            clean = true;
        } else if (*itArgument == QString::fromLatin1("-u") || *itArgument == QString::fromLatin1("--url")) {
            ++itArgument;
            if (itArgument == args.end()) {
                printUsage();
                return -1;
            } else {
                repositoryUrl = *itArgument;
            }
        } else if (*itArgument == QString::fromLatin1("-r") || *itArgument == QString::fromLatin1("--repository")) {
            ++itArgument;
            if (itArgument == args.end()) {
                printUsage();
                return -1;
            } else {
                repositoryTarget = *itArgument;
            }
        } else if (*itArgument == QString::fromLatin1("-p") || *itArgument == QString::fromLatin1("--packages")) {
            ++itArgument;
            if (itArgument == args.end()) {
                printUsage();
                return -1;
            } else {
                packageDirectoryTarget = *itArgument;
            }
        } else {
            qWarning() << QString::fromLatin1("Argument '%1' is unknown").arg(*itArgument);
            printUsage();
            return 0;
        }
    }
    if (repositoryTarget.isEmpty() || packageDirectoryTarget.isEmpty()) {
        printUsage();
        return 0;
    } else {
        // resolve pathes
        repositoryTarget = QFileInfo(repositoryTarget).absoluteFilePath();
        packageDirectoryTarget = QFileInfo(packageDirectoryTarget).absoluteFilePath();
    }

    foreach (const QString &target, QStringList() << repositoryTarget << packageDirectoryTarget) {
        if (QFileInfo(target).exists()) {
            if (clean) {
                qDebug() << "removing directory:" << target;
                QInstaller::removeDirectory(target, true);
            } else if (!QDir(target).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty()){
                qWarning() << QString::fromLatin1("The directory '%1' needs to be empty or just "
                    "add the --clean argument.").arg(target);
                return EXIT_FAILURE;
            }
        }

        while (!QDir().mkpath(target)) {
            qWarning() << QString::fromLatin1("Could not create %1").arg(target);
        }
    }

    QHash<QString, ComponentData> componentDataHash;
    componentDataHash = downLoadRepository(repositoryUrl, repositoryTarget);

    // maybe in that case we should download the meta data to temp and
    // get the downdloadable archives information from there later
    if (packageDirectoryTarget.isEmpty())
        return EXIT_SUCCESS;

    QDirIterator itRepositoryFile(repositoryTarget, QDir::Files, QDirIterator::Subdirectories);
    while (itRepositoryFile.hasNext()) {
        QString currentFile = itRepositoryFile.next();

        QString componentSubdirectoryName = itRepositoryFile.filePath();

        QString normalizedRepositoryTarget = repositoryTarget;
        normalizedRepositoryTarget.replace(QLatin1Char('\\'), QLatin1Char('/'));

        componentSubdirectoryName.remove(repositoryTarget);
        componentSubdirectoryName.remove(normalizedRepositoryTarget);

        QString componentPackageDir = QFileInfo(packageDirectoryTarget + QDir::separator() + componentSubdirectoryName).absolutePath();
        QString absoluteSourceFilePath = itRepositoryFile.filePath();

        if (currentFile.endsWith(QLatin1String("meta.7z"))) {
            QString absolutTargetPath = QFileInfo(
                packageDirectoryTarget + QDir::separator()).absolutePath();

            extractFile(absoluteSourceFilePath, absolutTargetPath);
            QInstaller::moveDirectoryContents(componentPackageDir,
                componentPackageDir + QDir::separator() + QLatin1String("meta"));
        } else if (!currentFile.endsWith(QLatin1String("Updates.xml")) && !currentFile.endsWith(QLatin1String(".sha1"))){
            QString pathToTarget = componentPackageDir + QDir::separator() + QLatin1String("data");
            QString target = QDir(pathToTarget).absoluteFilePath(itRepositoryFile.fileName());
            QDir().mkpath(pathToTarget);
            QFile file;
            if (!file.copy(absoluteSourceFilePath, target)) {
                qWarning() << QString::fromLatin1("copy file %1 to %2 was not working %3").arg(
                    absoluteSourceFilePath, target, file.errorString());
            }
        }
    }


    QHashIterator<QString, ComponentData> itComponentData(componentDataHash);
    while (itComponentData.hasNext()) {
        itComponentData.next();
        if (itComponentData.value().m_script.isEmpty())
            continue;

        QString componentScript = QFileInfo(QString::fromLatin1("%1/%2/meta/%3").arg(
            packageDirectoryTarget, itComponentData.key(), itComponentData.value().m_script)).absoluteFilePath();

        QString packagesXml = QFileInfo(QString::fromLatin1("%1/%2/meta/packages.xml").arg(
            packageDirectoryTarget, itComponentData.key())).absoluteFilePath();

        QFile packagesXmlFile(packagesXml);

        if (!packagesXmlFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            qWarning() << QString::fromLatin1("Failed to open '%1' for writing. %2").arg(
                packagesXml, packagesXmlFile.errorString());
            return EXIT_FAILURE;
        }

        if (itComponentData.value().m_packageXml.toString().isEmpty()) {
            qWarning() << "No xml data found in component:" << itComponentData.key();
            return EXIT_FAILURE;
        }

        QTextStream stream(&packagesXmlFile);
        stream << itComponentData.value().m_packageXml.toString(4);
        packagesXmlFile.close();

        QString dataPackagesPath = QFileInfo(QString::fromLatin1("%1/%2/data").arg(
            packageDirectoryTarget, itComponentData.key())).absoluteFilePath();
        QDir().mkpath(dataPackagesPath);

        QString dataRepositoryPath = QFileInfo(repositoryTarget + QDir::separator() + itComponentData.key()).absoluteFilePath();
        QDir().mkpath(dataRepositoryPath);

        QFile componentScriptFile(componentScript);
        if (!componentScriptFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qDebug() << QString::fromLatin1("Can not read %1 %2").arg(componentScript, componentScriptFile.errorString());
            continue;
        }

        QString sevenZString;
        QTextStream in(&componentScriptFile);
        in.setCodec("UTF-8");
        while (!in.atEnd()) {
            QString line = in.readLine();
            if (line.contains(QLatin1String(".7z"))) {
                int firstPosition = line.indexOf(QLatin1String("\""));
                QString subString = line.right(line.count() - firstPosition - 1); //-1 means "
                //qDebug() << subString;
                int secondPosition = subString.indexOf(QLatin1String("\""));
                sevenZString = subString.left(secondPosition);
                QUrl downloadUrl((QStringList() << repositoryUrl << itComponentData.key() << itComponentData.value().m_version + sevenZString).join(QLatin1String("/")));
                QString localRepositoryTarget = dataRepositoryPath + QLatin1Char('/') + itComponentData.value().m_version + sevenZString;
                downloadFile(downloadUrl, localRepositoryTarget);
                downloadFile(downloadUrl.toString() + QLatin1String(".sha1"), localRepositoryTarget + QLatin1String(".sha1"));
                QFile::copy(localRepositoryTarget, dataPackagesPath + QLatin1Char('/') + sevenZString);
            }
        }
    }

    return 0;
}
