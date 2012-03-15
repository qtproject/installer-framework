/**************************************************************************
**
** This file is part of Installer Framework
**
** Copyright (c) 2011-2012 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (qt-info@nokia.com)
**
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
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** Other Usage
**
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
** If you have questions regarding the use of this file, please contact
** Nokia at qt-info@nokia.com.
**
**************************************************************************/
#include "common/repositorygen.h"

#include <errors.h>
#include <init.h>
#include <lib7z_facade.h>
#include <utils.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QFileInfo>
#include <QtCore/QStringList>

#include <iostream>

using namespace Lib7z;
using namespace QInstaller;

static void printUsage()
{
    std::cout << "Usage: " << QFileInfo(QCoreApplication::applicationFilePath()).fileName()
        << " directory.7z directories" << std::endl;
}

int main(int argc, char *argv[])
{
    try {
        QCoreApplication app(argc, argv);

        if (app.arguments().count() < 3) {
            printUsage();
            return EXIT_FAILURE;
        }

        QInstaller::init();
        QInstaller::setVerbose(true);
        const QStringList sourceDirectories = app.arguments().mid(2);
        QInstallerTools::compressDirectory(sourceDirectories, app.arguments().at(1));
        return EXIT_SUCCESS;
    } catch (const Lib7z::SevenZipException &e) {
        std::cerr << e.message() << std::endl;
    } catch (const QInstaller::Error &e) {
        std::cerr << e.message() << std::endl;
    }
    return EXIT_FAILURE;
}
