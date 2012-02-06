/**************************************************************************
**
** This file is part of Installer Framework
**
** Copyright (c) 2010-2012 Nokia Corporation and/or its subsidiary(-ies).
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

#ifndef REGISTERTOOLCHAINOPERATION_H
#define REGISTERTOOLCHAINOPERATION_H

#include "qinstallerglobal.h"

namespace QInstaller {

/*!
    Arguments:
    * SDK Path - to find the QtCreator installation
    * ToolChainKey - is the internal QtCreator settings key usually: GccToolChain
    * toolchain type - where this toolchain is defined in QtCreator
    *     ProjectExplorer.ToolChain.Gcc ProjectExplorer.ToolChain.Mingw
    *     ProjectExplorer.ToolChain.LinuxIcc ProjectExplorer.ToolChain.Msvc
    *     Qt4ProjectManager.ToolChain.GCCE Qt4ProjectManager.ToolChain.Maemo
    * display name - the name how it will be displayed in QtCreator
    * application binary interface - this is an internal creator typ as a String CPU-OS-OS_FLAVOR-BINARY_FORMAT-WORD_WIDTH
    *     CPU: arm x86 mips ppc itanium
    *     OS: linux macos symbian unix windows
    *     OS_FLAVOR: generic maemo meego generic device emulator generic msvc2005 msvc2008 msvc2010 msys ce
    *     BINARY_FORMAT: elf pe mach_o qml_rt
    *     WORD_WIDTH: 8 16 32 64
    * compiler path - the binary which is used as the compiler
    * debugger path - the binary which is used as the debugger
*/
class RegisterToolChainOperation : public Operation
{
public:
    RegisterToolChainOperation();

    void backup();
    bool performOperation();
    bool undoOperation();
    bool testOperation();
    Operation *clone() const;
};

} // namespace QInstaller

#endif // REGISTERTOOLCHAINOPERATION_H
