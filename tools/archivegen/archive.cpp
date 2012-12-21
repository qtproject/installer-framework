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
        QInstallerTools::compressPaths(sourceDirectories, app.arguments().at(1));
        return EXIT_SUCCESS;
    } catch (const Lib7z::SevenZipException &e) {
        std::cerr << "caught 7zip exception: " << e.message() << std::endl;
    } catch (const QInstaller::Error &e) {
        std::cerr << "caught exception: " << e.message() << std::endl;
    }
    return EXIT_FAILURE;
}
