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
#include <common/utils.h>
#include <common/repositorygen.h>
#include <settings.h>

#include "lib7z_facade.h"
#include "init.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QString>
#include <QStringList>

#include <iostream>

using namespace QInstaller;
using namespace Lib7z;

static void printUsage()
{
    const QString appName = QFileInfo( QCoreApplication::applicationFilePath() ).fileName();
    std::cout << "Usage: " << appName << " directory archive.7z" << std::endl;
    std::cout << std::endl;
    std::cout << "Example:" << std::endl;
    std::cout << "  " << appName << " someDirectory foobar.7z" << std::endl;
}

int main(int argc, char **argv)
{
    try {
        QCoreApplication app(argc, argv);
        init();

        setVerbose( true );

        if( app.arguments().count() < 3 )
        {
            printUsage();
            return 1;
        }

        QInstaller::compressDirectory(app.arguments().at(1), app.arguments().at(2));
        return 0;
    } catch ( const Lib7z::SevenZipException& e ) {
        std::cerr << e.message() << std::endl;
    } catch ( const QInstaller::Error& e ) {
        std::cerr << e.message() << std::endl;
    }
    return 1;
}
