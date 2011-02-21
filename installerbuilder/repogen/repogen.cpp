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
#include <common/errors.h>
#include <common/fileutils.h>
#include <common/utils.h>
#include <common/repositorygen.h>
#include <common/installersettings.h>

#include "lib7z_facade.h"
#include "init.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QString>
#include <QStringList>

#include <iostream>

using namespace QInstaller;
using namespace Lib7z;

static void printUsage()
{
    const QString appName = QFileInfo( QCoreApplication::applicationFilePath() ).fileName();
    std::cout << "Usage: " << appName << " packages-dir config-dir repository-dir component1 [component2 ...]" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -e|--exclude p1,...,pn exclude the given packages and their dependencies from the repository" << std::endl;
    std::cout << "  -v|--verbose           Verbose output" << std::endl;
    std::cout << "     --single            Put only the given components (not their dependencies) into the (already existing) repository" << std::endl;
    std::cout << std::endl;
    std::cout << "Example:" << std::endl;
    std::cout << "  " << appName << " ../examples/packages ../examples/config repository/ com.nokia.sdk" << std::endl;
}

static int printErrorAndUsageAndExit( const QString& err ) 
{
    std::cerr << qPrintable( err ) << std::endl << std::endl;
    printUsage();
    return 1;
}

static QString makeAbsolute( const QString& path ) {
    QFileInfo fi( path );
    if ( fi.isAbsolute() )
        return path;
    return QDir::current().absoluteFilePath( path );
}

static QVector<PackageInfo> filterBlacklisted( QVector< PackageInfo > packages, const QStringList& blacklist ) 
{
    for( int i = packages.size() - 1; i >= 0; --i )
        if ( blacklist.contains( packages[i].name ) )
            packages.remove( i );
    return packages;
}

int main( int argc, char** argv ) {
    try {
        QCoreApplication app( argc, argv );

        init();

        QStringList args = app.arguments().mid( 1 );

        QStringList excludedPackages;
        bool replaceSingleComponent = false;

        while( !args.isEmpty() && args.first().startsWith( QLatin1String( "-" ) ) )
        {
            if( args.first() == QLatin1String( "--verbose" ) || args.first() == QLatin1String( "-v" ) )
            {
                args.pop_front();
                setVerbose( true );
            }
            else if( args.first() == QLatin1String( "--exclude" ) || args.first() == QLatin1String( "-e" ) )
            {
                args.pop_front();
                if( args.isEmpty() || args.first().startsWith( QLatin1String( "-" ) ) )
                    return printErrorAndUsageAndExit( QObject::tr("Error: Package to exclude missing") );
                excludedPackages = args.first().split( QLatin1Char( ',' ) );
                args.pop_front();
            }
            else if( args.first() == QLatin1String( "--single" ) )
            {
                replaceSingleComponent = true;
                args.pop_front();
            }
            else
            {
                printUsage();
                return 1;
            }
        }

        if( args.count() < 4 ) 
        {
            printUsage();
            return 1;
        }
 
        const QString packageDir = makeAbsolute( args[0] );
        const QString configDir = makeAbsolute( args[1] );
        const QString repoDir = makeAbsolute( args[2] );
        const QStringList components = args.mid( 3 );
        const InstallerSettings cfg = InstallerSettings::fromFileAndPrefix( configDir + QLatin1String("/config.xml"), configDir );
        const QString appName = cfg.applicationName();
        const QString appVersion = cfg.applicationVersion();

        if( !replaceSingleComponent && QFile::exists( repoDir ) )
            throw QInstaller::Error( QObject::tr("Repository target folder %1 already exists!").arg( repoDir ) );

        const QVector< PackageInfo > packages = filterBlacklisted( createListOfPackages( components, packageDir, !replaceSingleComponent ), excludedPackages );

        QMap<QString, QString> pathToVersionMapping = buildPathToVersionMap(packages);

        for( QVector< PackageInfo >::const_iterator it = packages.begin(); it != packages.end(); ++it )
        {
            const QFileInfo fi( repoDir, it->name );
            if( fi.exists() )
                removeDirectory( fi.absoluteFilePath() );
        }

        copyComponentData( packageDir, configDir, repoDir, packages );
        const QString metaTmp = createTemporaryDirectory();
        const TempDirDeleter tmpDeleter( metaTmp );
        generateMetaDataDirectory( metaTmp, repoDir, packages, appName, appVersion );
        compressMetaDirectories( configDir, metaTmp, metaTmp, pathToVersionMapping );
        
        QFile::remove( QFileInfo( repoDir, QLatin1String( "Updates.xml" ) ).absoluteFilePath() );
        moveDirectoryContents( metaTmp, repoDir );
        return 0;
    } catch ( const Lib7z::SevenZipException& e ) {
        std::cerr << e.message() << std::endl;
    } catch ( const QInstaller::Error& e ) {
        std::cerr << e.message() << std::endl;
    }
    return 1;
}

