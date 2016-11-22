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
#include "adminauthorization.h"

#include "utils.h"

#include <QDebug>
#include <QDir>
#include <QSettings>

#include <qt_windows.h>

#include "globals.h"

#ifdef Q_CC_MINGW
# ifndef SEE_MASK_NOASYNC
#  define SEE_MASK_NOASYNC 0x00000100
# endif
#endif

namespace QInstaller {

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

bool AdminAuthorization::execute(QWidget *, const QString &program, const QStringList &arguments)
{
    DeCoInitializer _;

    // AdminAuthorization::execute uses UAC to ask for admin privileges. If the user is no
    // administrator yet and the computer's policies are set to not use UAC (which is the case
    // in some corporate networks), the call to execute() will simply succeed and not at all
    // launch the child process. To avoid this, we detect this situation here and return early.
    if (!hasAdminRights()) {
        QLatin1String key("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\"
            "Policies\\System");
        QSettings registry(key, QSettings::NativeFormat);
        const QVariant enableLUA = registry.value(QLatin1String("EnableLUA"));
        if ((enableLUA.type() == QVariant::Int) && (enableLUA.toInt() == 0))
            return false;
    }

    const QString file = QDir::toNativeSeparators(program);
    const QString args = QInstaller::createCommandline(QString(), arguments);

    SHELLEXECUTEINFOW shellExecuteInfo = { 0 };
    shellExecuteInfo.nShow = SW_HIDE;
    shellExecuteInfo.lpVerb = L"runas";
    shellExecuteInfo.lpFile = (wchar_t *)file.utf16();
    shellExecuteInfo.cbSize = sizeof(SHELLEXECUTEINFOW);
    shellExecuteInfo.lpParameters = (wchar_t *)args.utf16();
    shellExecuteInfo.fMask = SEE_MASK_NOASYNC;

    qCDebug(QInstaller::lcServer) << "Starting elevated process" << file
                                  << "with arguments" << args;

    if (ShellExecuteExW(&shellExecuteInfo)) {
        qCDebug(QInstaller::lcServer) << "Finished starting elevated process.";
        return true;
    } else {
        qCWarning(QInstaller::lcServer) << "Error while starting elevated process" << program
            << ":" << QInstaller::windowsErrorString(GetLastError());
    }
    return false;
}

} // namespace QInstaller
