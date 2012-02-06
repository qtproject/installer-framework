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

#include "registerdefaultdebuggeroperation.h"

#include "constants.h"
#include "persistentsettings.h"
#include "packagemanagercore.h"
#include "qtcreator_constants.h"
#include "qtcreatorpersistentsettings.h"

#include <QString>
#include <QFileInfo>
#include <QDir>
#include <QSettings>
#include <QDebug>

using namespace QInstaller;

using namespace ProjectExplorer;

//TODO move this to a general location it is used on some classes
static QString fromNativeSeparatorsAllOS(const QString &pathName)
{
    QString n = pathName;
    for (int i = 0; i < n.size(); ++i) {
        if (n.at(i) == QLatin1Char('\\'))
            n[i] = QLatin1Char('/');
    }
    return n;
}

RegisterDefaultDebuggerOperation::RegisterDefaultDebuggerOperation()
{
    setName(QLatin1String("RegisterDefaultDebugger"));
}

void RegisterDefaultDebuggerOperation::backup()
{
}

/** application binary interface - this is an internal creator typ as a String CPU-OS-OS_FLAVOR-BINARY_FORMAT-WORD_WIDTH
  *     CPU: arm x86 mips ppc itanium
  *     OS: linux macos symbian unix windows
  *     OS_FLAVOR: generic maemo meego generic device emulator generic msvc2005 msvc2008 msvc2010 msys ce
  *     BINARY_FORMAT: elf pe mach_o qml_rt
  *     WORD_WIDTH: 8 16 32 64
  */

bool RegisterDefaultDebuggerOperation::performOperation()
{
    const QStringList args = arguments();

    if (args.count() != 2) {
        setError(InvalidArguments);
        setErrorString(tr("Invalid arguments in %0: %1 arguments given, 2 expected.")
            .arg(name()).arg(args.count()));
        return false;
    }

    QString toolChainsXmlFilePath;

    PackageManagerCore *const core = qVariantValue<PackageManagerCore *>(value(QLatin1String("installer")));
    if (!core) {
        setError(UserDefinedError);
        setErrorString(tr("Needed installer object in \"%1\" operation is empty.").arg(name()));
        return false;
    }
    const QString &rootInstallPath = core->value(scTargetDir);
    toolChainsXmlFilePath = rootInstallPath + QLatin1String(ToolChainSettingsSuffixPath);

    int argCounter = 0;
    const QString &abiString = args.at(argCounter++); //for example x86-windows-msys-pe-32bit
    const QString &debuggerPath = fromNativeSeparatorsAllOS(args.at(argCounter++));

    QtCreatorPersistentSettings creatorToolChainSettings;

    if (!creatorToolChainSettings.init(toolChainsXmlFilePath)) {
        setError(UserDefinedError);
        setErrorString(tr("Can't read from tool chains xml file(%1) correctly.")
            .arg(toolChainsXmlFilePath));
        return false;
    }

    creatorToolChainSettings.addDefaultDebugger(abiString, debuggerPath);
    return creatorToolChainSettings.save();
}

bool RegisterDefaultDebuggerOperation::undoOperation()
{
    const QStringList args = arguments();

    if (args.count() != 2) {
        setError(InvalidArguments);
        setErrorString(tr("Invalid arguments in %0: %1 arguments given, 2 expected.")
            .arg(name()).arg(args.count()));
        return false;
    }

    QString toolChainsXmlFilePath;

    PackageManagerCore *const core = qVariantValue<PackageManagerCore *>(value(QLatin1String("installer")));
    if (!core) {
        setError(UserDefinedError);
        setErrorString(tr("Needed installer object in \"%1\" operation is empty.").arg(name()));
        return false;
    }
    const QString &rootInstallPath = core->value(scTargetDir);
    toolChainsXmlFilePath = rootInstallPath + QLatin1String(ToolChainSettingsSuffixPath);

    int argCounter = 0;
    const QString &abiString = args.at(argCounter++); //for example x86-windows-msys-pe-32bit
    const QString &debuggerPath = fromNativeSeparatorsAllOS(args.at(argCounter++));
    Q_UNUSED(debuggerPath)

    QtCreatorPersistentSettings creatorToolChainSettings;

    creatorToolChainSettings.init(toolChainsXmlFilePath);
    creatorToolChainSettings.removeDefaultDebugger(abiString);
    return creatorToolChainSettings.save();
}

bool RegisterDefaultDebuggerOperation::testOperation()
{
    return true;
}

Operation *RegisterDefaultDebuggerOperation::clone() const
{
    return new RegisterDefaultDebuggerOperation();
}
