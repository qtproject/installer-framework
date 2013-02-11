/**************************************************************************
**
** Copyright (C) 2012-2013 Digia Plc and/or its subsidiary(-ies).
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

#include "registertoolchainoperation.h"

#include "persistentsettings.h"
#include "packagemanagercore.h"
#include "qtcreator_constants.h"
#include "qtcreatorpersistentsettings.h"

#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QSettings>
#include <QtCore/QString>

using namespace QInstaller;

using namespace ProjectExplorer;

RegisterToolChainOperation::RegisterToolChainOperation()
{
    setName(QLatin1String("RegisterToolChain"));
}

void RegisterToolChainOperation::backup()
{
}

bool RegisterToolChainOperation::performOperation()
{
    const QStringList args = arguments();

    if (args.count() < 4) {
        setError(InvalidArguments);
        setErrorString(tr("Invalid arguments in %0: %1 arguments given, minimum 4 expected.")
            .arg(name()).arg(args.count()));
        return false;
    }

    QString toolChainsXmlFilePath;

    PackageManagerCore *const core = value(QLatin1String("installer")).value<PackageManagerCore*>();
    if (!core) {
        setError(UserDefinedError);
        setErrorString(tr("Needed installer object in \"%1\" operation is empty.").arg(name()));
        return false;
    }
    if (core->value(scQtCreatorInstallerToolchainsFile).isEmpty()) {
        setError(UserDefinedError);
        setErrorString(tr("There is no value set for %1 on the installer object.").arg(
            scQtCreatorInstallerToolchainsFile));
        return false;
    }
    toolChainsXmlFilePath = core->value(scQtCreatorInstallerToolchainsFile);

    QtCreatorToolChain toolChain;

    int argCounter = 0;
    toolChain.key = args.at(argCounter++); //Qt SDK:gccPath
    toolChain.type = args.at(argCounter++); //where this toolchain is defined in QtCreator
    toolChain.displayName = args.at(argCounter++); //nice special Toolchain (Qt SDK)
    toolChain.abiString = args.at(argCounter++); //x86-windows-msys-pe-32bit
    toolChain.compilerPath = QDir::toNativeSeparators(args.at(argCounter++)); //gccPath
    if (args.count() > argCounter)
        toolChain.debuggerPath = QDir::toNativeSeparators(args.at(argCounter++));
    if (args.count() > argCounter)
        toolChain.armVersion = args.at(argCounter++);
    if (args.count() > argCounter)
        toolChain.force32Bit = args.at(argCounter++);

    QtCreatorPersistentSettings creatorToolChainSettings;

    if (!creatorToolChainSettings.init(toolChainsXmlFilePath)) {
        setError(UserDefinedError);
        setErrorString(tr("Can't read from tool chains xml file(%1) correctly.")
            .arg(toolChainsXmlFilePath));
        return false;
    }

    if (!creatorToolChainSettings.addToolChain(toolChain)) {
        setError(InvalidArguments);
        setErrorString(tr("Some arguments are not right in %1 operation.")
            .arg(name()).arg(args.count()));
        return false;
    }
    return creatorToolChainSettings.save();
}

bool RegisterToolChainOperation::undoOperation()
{
    const QStringList args = arguments();

    if (args.count() < 4) {
        setError(InvalidArguments);
        setErrorString(tr("Invalid arguments in %0: %1 arguments given, minimum 4 expected.")
            .arg(name()).arg(args.count()));
        return false;
    }

    PackageManagerCore *const core = value(QLatin1String("installer")).value<PackageManagerCore*>();
    if (!core) {
        setError(UserDefinedError);
        setErrorString(tr("Needed installer object in \"%1\" operation is empty.").arg(name()));
        return false;
    }

    // default value is the old value to keep the possibility that old saved operations can run undo
#ifdef Q_OS_MAC
    QString toolChainsXmlFilePath = core->value(scQtCreatorInstallerToolchainsFile,
        QString::fromLatin1("%1/Qt Creator.app/Contents/Resources/QtProject/toolChains.xml").arg(
        core->value(QLatin1String("TargetDir"))));
#else
    QString toolChainsXmlFilePath = core->value(scQtCreatorInstallerToolchainsFile,
        QString::fromLatin1("%1/QtCreator/share/qtcreator/QtProject/toolChains.xml").arg(core->value(
        QLatin1String("TargetDir"))));
#endif

    QtCreatorToolChain toolChain;

    int argCounter = 0;
    toolChain.key = args.at(argCounter++); //Qt SDK:gccPath
    toolChain.type = args.at(argCounter++); //where this toolchain is defined in QtCreator
    toolChain.displayName = args.at(argCounter++); //nice special Toolchain (Qt SDK)
    toolChain.abiString = args.at(argCounter++); //x86-windows-msys-pe-32bit
    toolChain.compilerPath = QDir::toNativeSeparators(args.at(argCounter++)); //gccPath
    if (args.count() > argCounter)
        toolChain.debuggerPath = QDir::toNativeSeparators(args.at(argCounter++));
    if (args.count() > argCounter)
        toolChain.armVersion = args.at(argCounter++);
    if (args.count() > argCounter)
        toolChain.force32Bit = args.at(argCounter++);

    QtCreatorPersistentSettings creatorToolChainSettings;

    if (!creatorToolChainSettings.init(toolChainsXmlFilePath)) {
        setError(UserDefinedError);
        setErrorString(tr("Can't read from tool chains xml file(%1) correctly.")
            .arg(toolChainsXmlFilePath));
        return false;
    }

    if (!creatorToolChainSettings.removeToolChain(toolChain)) {
        setError(InvalidArguments);
        setErrorString(tr("Some arguments are not right in %1 operation.")
            .arg(name()).arg(args.count()));
        return false;
    }
    return creatorToolChainSettings.save();
}

bool RegisterToolChainOperation::testOperation()
{
    return true;
}

Operation *RegisterToolChainOperation::clone() const
{
    return new RegisterToolChainOperation();
}
