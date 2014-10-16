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
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
**
** $QT_END_LICENSE$
**
**************************************************************************/

#ifndef QTCREATOR_CONSTANTS_H
#define QTCREATOR_CONSTANTS_H

// Begin - copied from Creator src\plugins\projectexplorer\toolchainmanager.cpp
static const char TOOLCHAIN_DATA_KEY[] = "ToolChain.";
static const char TOOLCHAIN_COUNT_KEY[] = "ToolChain.Count";
static const char TOOLCHAIN_FILE_VERSION_KEY[] = "Version";
static const char DEFAULT_DEBUGGER_COUNT_KEY[] = "DefaultDebugger.Count";
static const char DEFAULT_DEBUGGER_ABI_KEY[] = "DefaultDebugger.Abi.";
static const char DEFAULT_DEBUGGER_PATH_KEY[] = "DefaultDebugger.Path.";

static const char ID_KEY[] = "ProjectExplorer.ToolChain.Id";
static const char DISPLAY_NAME_KEY[] = "ProjectExplorer.ToolChain.DisplayName";
// End - copied from Creator

// Begin - copied from Creator src\plugins\qt4projectmanager\qtversionmanager.cpp
static const char QtVersionsSectionName[] = "QtVersions";
static const char newQtVersionsKey[] = "NewQtVersions";
// End - copied from Creator

//the values for these keys are built in packagemanagercore->value() on the fly
//so it is possible that the installer creator can choose the location for these settings files
static const QLatin1String scQtCreatorInstallerSettingsFile("QtCreatorInstallerSettingsFile");
static const QLatin1String scQtCreatorInstallerToolchainsFile("QtCreatorInstallerToolchainsFile");
static const QLatin1String scQtCreatorInstallerQtVersionFile("QtCreatorInstallerQtVersionFile");


#endif // QTCREATOR_CONSTANTS_H
