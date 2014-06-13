/**************************************************************************
**
** Copyright (C) 2012-2014 Digia Plc and/or its subsidiary(-ies).
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

#include <Security/Authorization.h>
#include <Security/AuthorizationTags.h>

#include <QStringList>
#include <QVector>

#include <unistd.h>

namespace QInstaller {

bool AdminAuthorization::execute(QWidget *, const QString &program, const QStringList &arguments)
{
    AuthorizationRef authorizationRef;
    OSStatus status = AuthorizationCreate(NULL, kAuthorizationEmptyEnvironment,
        kAuthorizationFlagDefaults, &authorizationRef);
    if (status != errAuthorizationSuccess)
        return false;

    AuthorizationItem item = { kAuthorizationRightExecute, 0, 0, 0 };
    AuthorizationRights rights = { 1, &item };
    const AuthorizationFlags flags = kAuthorizationFlagDefaults | kAuthorizationFlagInteractionAllowed
        | kAuthorizationFlagPreAuthorize | kAuthorizationFlagExtendRights;

    status = AuthorizationCopyRights(authorizationRef, &rights, kAuthorizationEmptyEnvironment,
        flags, 0);
    if (status != errAuthorizationSuccess)
        return false;

    QVector<char *> args;
    QVector<QByteArray> utf8Args;
    foreach (const QString &argument, arguments) {
        utf8Args.push_back(argument.toUtf8());
        args.push_back(utf8Args.last().data());
    }
    args.push_back(0);

    const QByteArray utf8Program = program.toUtf8();
    status = AuthorizationExecuteWithPrivileges(authorizationRef, utf8Program.data(),
        kAuthorizationFlagDefaults, args.data(), 0);

    AuthorizationFree(authorizationRef, kAuthorizationFlagDestroyRights);
    return status == errAuthorizationSuccess;
}

bool AdminAuthorization::hasAdminRights()
{
    return geteuid() == 0;
}

} // namespace QInstaller
