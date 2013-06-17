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
#include "adminauthorization.h"

#include "utils.h"

#include <QDebug>
#include <QDir>

#ifdef Q_CC_MINGW
# ifndef _WIN32_WINNT
#  define _WIN32_WINNT 0x0501
# endif
#endif

#include <windows.h>

struct DeCoInitializer
{
    DeCoInitializer()
        : neededCoInit(CoInitialize(NULL) == S_OK)
    {
    }
    ~DeCoInitializer()
    {
        if (neededCoInit)
            CoUninitialize();
    }
    bool neededCoInit;
};

AdminAuthorization::AdminAuthorization()
{
}

bool AdminAuthorization::authorize()
{
    setAuthorized();
    emit authorized();
    return true;
}

bool AdminAuthorization::hasAdminRights()
{
    SID_IDENTIFIER_AUTHORITY authority = { SECURITY_NT_AUTHORITY };
    PSID adminGroup;
    // Initialize SID.
    if (!AllocateAndInitializeSid(&authority,
                                  2,
                                  SECURITY_BUILTIN_DOMAIN_RID,
                                  DOMAIN_ALIAS_RID_ADMINS,
                                  0, 0, 0, 0, 0, 0,
                                  &adminGroup))
        return false;

    BOOL isInAdminGroup = FALSE;
    if (!CheckTokenMembership(0, adminGroup, &isInAdminGroup))
        isInAdminGroup = FALSE;

    FreeSid(adminGroup);
    return isInAdminGroup;
}

//copied from qsystemerror.cpp in Qt
static QString windowsErrorString(int errorCode)
{
    QString ret;
    wchar_t *string = 0;
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM,
                  NULL,
                  errorCode,
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                  (LPWSTR)&string,
                  0,
                  NULL);
    ret = QString::fromWCharArray(string);
    LocalFree((HLOCAL)string);

    if (ret.isEmpty() && errorCode == ERROR_MOD_NOT_FOUND)
        ret = QString::fromLatin1("The specified module could not be found.");

    ret.append(QLatin1String(" (0x"));
    ret.append(QString::number(uint(errorCode), 16).rightJustified(8, QLatin1Char('0')));
    ret.append(QLatin1String(")"));

    return ret;
}

bool AdminAuthorization::execute(QWidget *, const QString &program, const QStringList &arguments)
{
    DeCoInitializer _;

    const QString file = QDir::toNativeSeparators(program);
    const QString args = QInstaller::createCommandline(QString(), arguments);

    SHELLEXECUTEINFOW shellExecuteInfo = { 0 };
    shellExecuteInfo.nShow = SW_HIDE;
    shellExecuteInfo.lpVerb = L"runas";
    shellExecuteInfo.lpFile = (wchar_t *)file.utf16();
    shellExecuteInfo.cbSize = sizeof(SHELLEXECUTEINFOW);
    shellExecuteInfo.lpParameters = (wchar_t *)args.utf16();
    shellExecuteInfo.fMask = SEE_MASK_NOASYNC;

    qDebug() << QString::fromLatin1("Starting elevated process %1 with arguments: %2.").arg(file, args);

    if (ShellExecuteExW(&shellExecuteInfo)) {
        qDebug() << "Finished starting elevated process.";
        return true;
    } else {
        qWarning() << QString::fromLatin1("Error while starting elevated process: %1, "
            "Error: %2").arg(program, windowsErrorString(GetLastError()));
    }
    return false;
}
