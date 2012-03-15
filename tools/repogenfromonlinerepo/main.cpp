/**************************************************************************
**
** This file is part of Installer Framework**
**
** Copyright (c) 2011-2012 Nokia Corporation and/or its subsidiary(-ies).*
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
#include "downloadmanager.h"

#include <QtCore/QCoreApplication>
#include <QFile>
#include <QString>
#include <QDomDocument>
#include <QDomElement>
#include <QDomNodeList>
#include <QStringList>
#include <QDebug>
#include <QFileInfo>
#include <QDir>
#include <iostream>

static void printUsage()
{
    const QString appName = QFileInfo( QCoreApplication::applicationFilePath() ).fileName();
    std::cout << "Usage: " << qPrintable(appName) << "--url <repository_url>" << std::endl;
    std::cout << std::endl;
    std::cout << "Example:" << std::endl;
    std::cout << "  " << qPrintable(appName) << " someDirectory foobar.7z" << std::endl;
}


int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    QString repoUrl = QLatin1String("http://www.forum.nokia.com/nokiaqtsdkrepository/oppdatering/windows/"
        "online_ndk_repo");

    QStringList args = app.arguments();
    for( QStringList::const_iterator it = args.constBegin(); it != args.constEnd(); ++it )
    {
        if( *it == QString::fromLatin1( "-h" ) || *it == QString::fromLatin1( "--help" ) )
        {
            printUsage();
            return 0;
        }
        else if( *it == QString::fromLatin1( "-u" ) || *it == QString::fromLatin1( "--url" ) )
        {
            ++it;
            if( it == args.end() ) {
//                printUsage();
//                return -1;
            } else {
                repoUrl = *it;
            }
        }
    }

    QEventLoop downloadEventLoop;

    DownloadManager downloadManager;

// get Updates.xml to get to know what we can download
    downloadManager.append(QUrl(repoUrl + QLatin1String("/Updates.xml")));
    QObject::connect( &downloadManager, SIGNAL( finished() ), &downloadEventLoop, SLOT( quit() ) );
    downloadEventLoop.exec();
// END - get Updates.xml to get to know what we can download

    QFile batchFile(QLatin1String("download.bat"));
    if (!batchFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qDebug() << "can not open " << QFileInfo(batchFile).absoluteFilePath();
        return app.exec();
    }

    QTextStream batchFileOut(&batchFile);

    const QString updatesXmlPath = QLatin1String("Updates.xml");

    Q_ASSERT( !updatesXmlPath.isEmpty() );
    Q_ASSERT( QFile::exists( updatesXmlPath ) );

    QFile updatesFile( updatesXmlPath );
    if ( !updatesFile.open( QIODevice::ReadOnly ) ) {
        //qDebug() << QString::fromLatin1("Could not open Updates.xml for reading: %1").arg( updatesFile
        //    .errorString() ) ;
        return app.exec();
    }

    QDomDocument doc;
    QString err;
    int line = 0;
    int col = 0;
    if ( !doc.setContent( &updatesFile, &err, &line, &col ) ) {
        //qDebug() << QString::fromLatin1("Could not parse component index: %1:%2: %3")
        //    .arg(QString::number(line), QString::number( col ), err );
        return app.exec();
    }

    const QDomElement root = doc.documentElement();
    const QDomNodeList children = root.childNodes();
    for ( int i = 0; i < children.count(); ++i ) {
        //qDebug() << children.count();
        QString packageName;
        QString packageDisplayName;
        QString packageDescription;
        QString packageUpdateText;
        QString packageVersion;
        QString packageReleaseDate;
        QString packageHash;
        QString packageUserinterfacesAsString;
        QString packageInstallPriority;
        QString packageScript;
        QString packageDependencies;
        QString packageForcedInstallation;
        bool packageIsVirtual = false;
        QString sevenZString;
        const QDomElement el = children.at( i ).toElement();
        if ( el.isNull() )
            continue;
        if ( el.tagName() == QLatin1String("PackageUpdate") ) {
            const QDomNodeList c2 = el.childNodes();

            for ( int j = 0; j < c2.count(); ++j ) {
                if ( c2.at( j ).toElement().tagName() == QLatin1String("Name") )
                    packageName = c2.at( j ).toElement().text();
                else if ( c2.at( j ).toElement().tagName() == QLatin1String("DisplayName") )
                    packageDisplayName = c2.at( j ).toElement().text();
                else if ( c2.at( j ).toElement().tagName() == QLatin1String("Description") )
                    packageDescription = c2.at( j ).toElement().text();
                else if ( c2.at( j ).toElement().tagName() == QLatin1String("UpdateText") )
                    packageUpdateText = c2.at( j ).toElement().text();
                else if ( c2.at( j ).toElement().tagName() == QLatin1String("Version") )
                    packageVersion = c2.at( j ).toElement().text();
                else if ( c2.at( j ).toElement().tagName() == QLatin1String("ReleaseDate") )
                    packageReleaseDate = c2.at( j ).toElement().text();
                else if ( c2.at( j ).toElement().tagName() == QLatin1String("SHA1") )
                    packageHash = c2.at( j ).toElement().text();
                else if ( c2.at( j ).toElement().tagName() == QLatin1String("UserInterfaces") )
                    packageUserinterfacesAsString = c2.at( j ).toElement().text();
                else if ( c2.at( j ).toElement().tagName() == QLatin1String("Script") )
                    packageScript = c2.at( j ).toElement().text();
                else if ( c2.at( j ).toElement().tagName() == QLatin1String("Dependencies") )
                    packageDependencies = c2.at( j ).toElement().text();
                else if ( c2.at( j ).toElement().tagName() == QLatin1String("ForcedInstallation") )
                    packageForcedInstallation = c2.at( j ).toElement().text();
                else if ( c2.at( j ).toElement().tagName() == QLatin1String("InstallPriority") )
                    packageInstallPriority = c2.at( j ).toElement().text();
                else if ( c2.at( j ).toElement().tagName() == QLatin1String("Virtual") && c2.at( j )
                    .toElement().text() == QLatin1String("true")) {
                        packageIsVirtual = true;
                }
            }
        }
        if (packageName.isEmpty()) {
            continue;
        }

        if ( !packageScript.isEmpty() ) {
        // get Updates.xml to get to know what we can download
            downloadManager.append(QUrl(repoUrl + QLatin1String("/") + packageName + QLatin1String("/")
                + packageScript));
            QObject::connect( &downloadManager, SIGNAL( finished() ), &downloadEventLoop, SLOT( quit() ) );
            downloadEventLoop.exec();
        // END - get Updates.xml to get to know what we can download

            QString localScriptFileName = packageScript;
            Q_ASSERT( QFile::exists( localScriptFileName ) );

            QFile file(localScriptFileName);
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                //qDebug() << localScriptFileName << " was not readable";
                continue;
            }

            QTextStream in(&file);
            while (!in.atEnd()) {
                QString line = in.readLine();
                if (line.contains(QLatin1String(".7z"))) {
                    int firstPosition = line.indexOf(QLatin1String("\""));
                    QString subString = line.right(line.count() - firstPosition - 1); //-1 means "
                    //qDebug() << subString;
                    int secondPosition = subString.indexOf(QLatin1String("\""));
                    sevenZString = subString.left(secondPosition);
                    //qDebug() << sevenZString;
                    break;
                }
            }
            file.remove();
        }
        QStringList packageUserinterfaces = packageUserinterfacesAsString.split(QLatin1String(","));
        packageUserinterfaces.removeAll(QString());
        packageUserinterfaces.removeAll(QLatin1String(""));

        QStringList fileList;

        //fileList << packageVersion + sevenZString;
        foreach(const QString file, packageUserinterfaces) {
            if(!file.isEmpty()) {
                fileList << file;
            }/* else {
                qDebug() << "There is something wrong with the userinterface string list.";
                return a.exec();
            }*/
        }
        if(!packageScript.isEmpty()) {
            fileList << packageScript;
        }

        QFile packagesXml( QString( QCoreApplication::applicationDirPath() + QLatin1String("/")
            + packageName + QLatin1String(".xml")));
        packagesXml.open( QIODevice::WriteOnly );
        QTextStream packageAsXmlStream( &packagesXml );
        packageAsXmlStream << QLatin1String("<?xml version=\"1.0\"?>" ) << endl;
        packageAsXmlStream << QLatin1String("<Package>" ) << endl;
        packageAsXmlStream << QString::fromLatin1("    <DisplayName>%1</DisplayName>").arg(packageDisplayName)
            << endl;

        if (!packageDescription.isEmpty()) {
            packageAsXmlStream << QString::fromLatin1("    <Description>%1</Description>" )
                .arg(packageDescription) << endl;
        }

        if (!packageUpdateText.isEmpty()) {
            packageAsXmlStream << QString::fromLatin1("    <UpdateText>%1</UpdateText>" )
                .arg(packageUpdateText) << endl;
        }

        if (!packageVersion.isEmpty()) {
            packageAsXmlStream << QString::fromLatin1("    <Version>%1</Version>" )
                .arg(packageVersion) << endl;
        }

        if (!packageReleaseDate.isEmpty()) {
            packageAsXmlStream << QString::fromLatin1("    <ReleaseDate>%1</ReleaseDate>" )
                .arg(packageReleaseDate) << endl;
        }
        packageAsXmlStream << QString::fromLatin1("    <Name>%1</Name>" ).arg(packageName) << endl;

        if (!packageScript.isEmpty()) {
            packageAsXmlStream << QString::fromLatin1("    <Script>%1</Script>" ).arg(packageScript) << endl;
        }

        if (packageIsVirtual) {
            packageAsXmlStream << QString::fromLatin1("    <Virtual>true</Virtual>" ) << endl;
        }

        if (!packageInstallPriority.isEmpty()) {
            packageAsXmlStream << QString::fromLatin1("    <InstallPriority>%1</InstallPriority>" )
                .arg(packageInstallPriority) << endl;
        }
        if (!packageDependencies.isEmpty()) {
            packageAsXmlStream << QString::fromLatin1("    <Dependencies>%1</Dependencies>" )
                .arg(packageDependencies) << endl;
        }

        if (!packageForcedInstallation.isEmpty()) {
            packageAsXmlStream << QString::fromLatin1("    <ForcedInstallation>%1</ForcedInstallation>" )
                .arg(packageForcedInstallation) << endl;
        }

        if (!packageUserinterfaces.isEmpty()) {
            packageAsXmlStream << QString::fromLatin1("    <UserInterfaces>" ) << endl;
            foreach(const QString userInterfaceFile, packageUserinterfaces) {
                packageAsXmlStream << QString::fromLatin1("        <UserInterface>%1</UserInterface>" )
                    .arg(userInterfaceFile) << endl;
            }
            packageAsXmlStream << QString::fromLatin1("    </UserInterfaces>" ) << endl;
        }
        packageAsXmlStream << QString::fromLatin1("</Package>" ) << endl;

        batchFileOut << "rem download line BEGIN =============================================\n";

        batchFileOut << "mkdir " << packageName << "\\meta\n";
        batchFileOut << "move " << QDir::toNativeSeparators(QFileInfo(packagesXml).absoluteFilePath()) << " " << packageName << "\\meta\\package.xml\n";
        if (!sevenZString.isEmpty()) {
            batchFileOut << "mkdir " << packageName << "\\data\n";
            batchFileOut << "cd " << packageName << "\\data\n";
            batchFileOut << "wget " << repoUrl << "/" << packageName << "/" << QString(packageVersion + sevenZString) << " -O " << sevenZString << "\n";
            batchFileOut << "cd ..\\..\n";
        }
        batchFileOut << "cd " << packageName << "\\meta\n";
        foreach(const QString file, fileList) {
            batchFileOut << "wget " << repoUrl << "/" << packageName << "/" << file << "\n";
        }
        batchFileOut << "cd ..\\..\n";

        batchFileOut << "rem download line END   =============================================\n";
    } //for ( int i = 0; i < children.count(); ++i ) {

    if ( children.count() == 0 ) {
        qDebug() << "no packages found";
        return app.exec();
    } else {
        qDebug() << "found packages and wrote batch file";
    }


    return 0;
}
