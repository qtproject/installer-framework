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

#include "registerdefaultdebuggeroperation.h"

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

    PackageManagerCore *const core = value(QLatin1String("installer")).value<PackageManagerCore*>();
    if (!core) {
        setError(UserDefinedError);
        setErrorString(tr("Needed installer object in \"%1\" operation is empty.").arg(name()));
        return false;
    }

    // default value is the old value to keep the possibility that old saved operations can run undo
#ifdef Q_OS_MAC
    QString toolChainsXmlFilePath = core->value(scQtCreatorInstallerToolchainsFile,
        QString::fromLatin1("%1/Qt Creator.app/Contents/Resources/Nokia/toolChains.xml").arg(
        core->value(QLatin1String("TargetDir"))));
#else
    QString toolChainsXmlFilePath = core->value(scQtCreatorInstallerToolchainsFile,
        QString::fromLatin1("%1/QtCreator/share/qtcreator/Nokia/toolChains.xml").arg(core->value(
        QLatin1String("TargetDir"))));
#endif

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
