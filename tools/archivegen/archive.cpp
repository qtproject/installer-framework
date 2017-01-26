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
        << " directory.7z [files | directories]" << std::endl;
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
        QInstallerTools::compressPaths(sourceDirectories, app.arguments().at(1));
        return EXIT_SUCCESS;
    } catch (const Lib7z::SevenZipException &e) {
        std::cerr << "caught 7zip exception: " << e.message() << std::endl;
    } catch (const QInstaller::Error &e) {
        std::cerr << "caught exception: " << e.message() << std::endl;
    }
    return EXIT_FAILURE;
}
