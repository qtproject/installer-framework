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
#include "macrelocateqt.h"
#include "common/utils.h"
#include "fsengineclient.h"
#include "macreplaceinstallnamesoperation.h"

#include <QtCore/QDirIterator>
#include <QtCore/QDebug>
#include <QtCore/QBuffer>
#include <QtCore/QProcess>

using namespace QInstaller;

Relocator::Relocator()
{
}

bool Relocator::apply(const QString &qtInstallDir, const QString &targetDir)
{
//    Relocator::apply(/Users/rakeller/QtSDKtest2/Simulator/Qt/gcc)
//    Relocator uses indicator: /QtSDKtest2operation 'QtPatch' with arguments: 'mac; /Users/rakeller/QtSDKtest2/Simulator/Qt/gcc' failed: Error while relocating Qt: "ReplaceInsta

    verbose() << "Relocator::apply(" << qtInstallDir << ")" << std::endl;

    mErrorMessage.clear();
    mOriginalInstallDir.clear();
    mInstallDir.clear();

    QFile buildRootFile(qtInstallDir + QLatin1String("/.orig_build_root"));
    if (buildRootFile.exists() && buildRootFile.open(QFile::ReadOnly)) {
        mOriginalInstallDir = QString::fromLocal8Bit(buildRootFile.readAll()).trimmed();
        if (!mOriginalInstallDir.endsWith(QLatin1Char('/')))
            mOriginalInstallDir += QLatin1Char('/');
    }

    mInstallDir = targetDir;
    if (!mInstallDir.endsWith(QLatin1Char('/')))
        mInstallDir.append(QLatin1Char('/'));
    if (!QFile::exists(qtInstallDir + QLatin1String("/bin/qmake"))) {
        mErrorMessage = QLatin1String("This is not a Qt installation directory.");
        return false;
    }

    QString indicator;
    //if mInstallDir = /Users/rakeller/QtSDKtest2/Simulator/Qt/gcc/
    //and if mOriginalInstallDir = /Users/berlin/Installer/______BUILD______PADDED______/ndk/Simulator/Qt/gcc/
    //then indicator should be "Simulator/Qt/gcc"
    for(int i = 0; i < mInstallDir.count(); ++i) {
        QString endWithString = mInstallDir.right(i);
        if (mOrginalInstallDir.endsWith(endWithString)) {
            indicator = endWithString;
        } else {
            break;
        }
    }
    indicator.chop(1); //removes the last "/"

    verbose() << "Relocator uses indicator: " << indicator << std::endl;
    QString replacement = targetDir;


    MacReplaceInstallNamesOperation operation;
    QStringList arguments;
    arguments << indicator
              << replacement
              << qtInstallDir + QLatin1String("/plugins")
              << qtInstallDir + QLatin1String("/lib")
              << qtInstallDir + QLatin1String("/imports")
              << qtInstallDir + QLatin1String("/bin");

    operation.setArguments(arguments);
    operation.performOperation();

    mErrorMessage = operation.errorString();


    return mErrorMessage.isNull();
}
