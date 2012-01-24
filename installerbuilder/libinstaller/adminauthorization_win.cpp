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
#include "adminauthorization.h"

#include <common/utils.h>

#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QStringList>
#include <QtCore/QVector>

#include <windows.h>

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
    SID_IDENTIFIER_AUTHORITY authority = SECURITY_NT_AUTHORITY;
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

bool AdminAuthorization::execute(QWidget *, const QString &program, const QStringList &arguments)
{
    const QString file = QDir::toNativeSeparators(program);
    const QString args = QInstaller::createCommandline(QString(), arguments);

    const int len = GetShortPathNameW((wchar_t *)file.utf16(), 0, 0);
    if (len == 0)
        return false;
    wchar_t *const buffer = new wchar_t[len];
    GetShortPathName((wchar_t *)file.utf16(), buffer, len);

    SHELLEXECUTEINFOW TempInfo = { 0 };
    TempInfo.cbSize = sizeof(SHELLEXECUTEINFOW);
    TempInfo.fMask = 0;
    TempInfo.hwnd = 0;
    TempInfo.lpVerb = L"runas";
    TempInfo.lpFile = buffer;
    TempInfo.lpParameters = (wchar_t *)args.utf16();
    TempInfo.lpDirectory = 0;
    TempInfo.nShow = SW_NORMAL;

    
    qDebug() << QString::fromLatin1(" starting elevated process %1 %2 with ::ShellExecuteExW( &TempInfo );"
        ).arg(program, arguments.join(QLatin1String(" ")));
    const bool result = ::ShellExecuteExW(&TempInfo);
    qDebug() << QLatin1String("after starting elevated process");
    delete[] buffer;
    return result;
}
