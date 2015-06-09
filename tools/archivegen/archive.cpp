/**************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
**
** $QT_END_LICENSE$
**
**************************************************************************/
#include "common/repositorygen.h"

#include <errors.h>
#include <init.h>
#include <lib7z_facade.h>
#include <utils.h>

#include <QCoreApplication>
#include <QFileInfo>

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
        Lib7z::createArchive(app.arguments().at(1), sourceDirectories, Lib7z::QTmpFile::No);
        return EXIT_SUCCESS;
    } catch (const QInstaller::Error &e) {
        std::cerr << "Caught exception: " << e.message() << std::endl;
    }
    return EXIT_FAILURE;
}
