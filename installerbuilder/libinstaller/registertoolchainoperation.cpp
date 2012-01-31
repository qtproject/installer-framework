/**************************************************************************
**
** This file is part of Qt SDK**
**
** Copyright (c) 2012 Nokia Corporation and/or its subsidiary(-ies).*
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
#include "registertoolchainoperation.h"

#include "constants.h"
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

    PackageManagerCore *const core = qVariantValue<PackageManagerCore*>(value(QLatin1String("installer")));
    if (!core) {
        setError(UserDefinedError);
        setErrorString(tr("Needed installer object in \"%1\" operation is empty.").arg(name()));
        return false;
    }
    const QString &rootInstallPath = core->value(scTargetDir);
    toolChainsXmlFilePath = rootInstallPath + QLatin1String(ToolChainSettingsSuffixPath);

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

    QString toolChainsXmlFilePath;

    PackageManagerCore *const core = qVariantValue<PackageManagerCore*>(value(QLatin1String("installer")));
    if (!core) {
        setError(UserDefinedError);
        setErrorString(tr("Needed installer object in \"%1\" operation is empty.").arg(name()));
        return false;
    }
    const QString &rootInstallPath = core->value(scTargetDir);
    toolChainsXmlFilePath = rootInstallPath + QLatin1String(ToolChainSettingsSuffixPath);

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
