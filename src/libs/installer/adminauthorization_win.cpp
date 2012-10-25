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
#include "adminauthorization.h"

#include "utils.h"

#include <QDebug>
#include <QDir>

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
    DeCoInitializer _;

    const QString file = QDir::toNativeSeparators(program);
    const QString args = QInstaller::createCommandline(QString(), arguments);

    SHELLEXECUTEINFOW shellExecuteInfo = { 0 };
    shellExecuteInfo.nShow = SW_HIDE;
    shellExecuteInfo.lpVerb = L"runas";
    shellExecuteInfo.lpFile = (wchar_t *)file.utf16();
    shellExecuteInfo.cbSize = sizeof(SHELLEXECUTEINFOW);
    shellExecuteInfo.lpParameters = (wchar_t *)args.utf16();

    qDebug() << QString::fromLatin1("Starting elevated process %1 with arguments: %2.").arg(file, args);
    ShellExecuteExW(&shellExecuteInfo);
    qDebug() << "Finished starting elevated process.";

    return GetLastError() == ERROR_SUCCESS;
}
