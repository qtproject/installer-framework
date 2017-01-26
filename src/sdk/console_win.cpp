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

#include "console.h"

# include <qt_windows.h>
# include <wincon.h>


# ifndef ENABLE_INSERT_MODE
#  define ENABLE_INSERT_MODE 0x0020
# endif

# ifndef ENABLE_QUICK_EDIT_MODE
#  define ENABLE_QUICK_EDIT_MODE 0x0040
# endif

# ifndef ENABLE_EXTENDED_FLAGS
#  define ENABLE_EXTENDED_FLAGS 0x0080
# endif

static bool isRedirected(HANDLE stdHandle)
{
    if (stdHandle == NULL) // launched from GUI
        return false;
    DWORD fileType = GetFileType(stdHandle);
    if (fileType == FILE_TYPE_UNKNOWN) {
        // launched from console, but no redirection
        return false;
    }
    // redirected into file, pipe ...
    return true;
}

/**
 * Redirects stdout, stderr output to console
 *
 * Console is a RAII class that ensures stdout, stderr output is visible
 * for GUI applications on Windows.
 *
 * If the application is launched from the explorer, startup menu etc
 * a new console window is created.
 *
 * If the application is launched from the console (cmd.exe), output is
 * printed there.
 *
 * If the application is launched from the console, but stdout is redirected
 * (e.g. into a file), Console does not interfere.
 */
Console::Console() :
    m_oldCout(0),
    m_oldCerr(0)
{
    bool isCoutRedirected = isRedirected(GetStdHandle(STD_OUTPUT_HANDLE));
    bool isCerrRedirected = isRedirected(GetStdHandle(STD_ERROR_HANDLE));

    if (!isCoutRedirected) { // verbose output only ends up in cout
        // try to use parent console. else launch & set up new console
        parentConsole = AttachConsole(ATTACH_PARENT_PROCESS);
        if (!parentConsole) {
            AllocConsole();
            HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
            if (handle != INVALID_HANDLE_VALUE) {
                COORD largestConsoleWindowSize = GetLargestConsoleWindowSize(handle);
                largestConsoleWindowSize.X -= 3;
                largestConsoleWindowSize.Y = 5000;
                SetConsoleScreenBufferSize(handle, largestConsoleWindowSize);
            }
            handle = GetStdHandle(STD_INPUT_HANDLE);
            if (handle != INVALID_HANDLE_VALUE)
                SetConsoleMode(handle, ENABLE_INSERT_MODE | ENABLE_QUICK_EDIT_MODE
                               | ENABLE_EXTENDED_FLAGS);
# ifndef Q_CC_MINGW
            HMENU systemMenu = GetSystemMenu(GetConsoleWindow(), FALSE);
            if (systemMenu != NULL)
                RemoveMenu(systemMenu, SC_CLOSE, MF_BYCOMMAND);
            DrawMenuBar(GetConsoleWindow());
# endif
        }
    }

    if (!isCoutRedirected) {
        m_oldCout = std::cout.rdbuf();
        m_newCout.open("CONOUT$");
        std::cout.rdbuf(m_newCout.rdbuf());
    }

    if (!isCerrRedirected) {
        m_oldCerr = std::cerr.rdbuf();
        m_newCerr.open("CONOUT$");
        std::cerr.rdbuf(m_newCerr.rdbuf());
    }
}

Console::~Console()
{
    if (!parentConsole) {
        system("PAUSE");
    } else {
        // simulate enter key to switch to boot prompt
        PostMessage(GetConsoleWindow(), WM_KEYDOWN, 0x0D, 0);
    }

    if (m_oldCerr)
        std::cerr.rdbuf(m_oldCerr);
    if (m_oldCout)
        std::cout.rdbuf(m_oldCout);

    if (m_oldCout)
        FreeConsole();
}
