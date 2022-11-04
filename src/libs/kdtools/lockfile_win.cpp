/****************************************************************************
**
** Copyright (C) 2013 Klaralvdalens Datakonsult AB (KDAB)
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
****************************************************************************/

#include "lockfile.h"
#include "lockfile_p.h"

#include "utils.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

namespace KDUpdater {

bool LockFile::Private::lock()
{
    if (locked)
        return locked;

    errorString.clear();
    handle = CreateFile(filename.toStdWString().data(), GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ, NULL, QFileInfo::exists(filename) ? OPEN_EXISTING : CREATE_NEW,
        FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, NULL);

    if (handle == INVALID_HANDLE_VALUE) {
        errorString = QCoreApplication::translate("LockFile", "Cannot create lock file \"%1\": "
            "%2").arg(QDir::toNativeSeparators(filename), QInstaller::windowsErrorString(GetLastError()));
        return false;
    }

    DWORD bytesWritten;
    const QByteArray pid = QString::number(QCoreApplication::applicationPid()).toLatin1();
    if (!WriteFile(handle, pid.data(), pid.size(), &bytesWritten, NULL)) {
        errorString = QCoreApplication::translate("LockFile", "Cannot write PID to lock file "
            "\"%1\": %2").arg(QDir::toNativeSeparators(filename), QInstaller::windowsErrorString(GetLastError()));
        return false;
    }
    FlushFileBuffers(handle);

    if (!::LockFile(handle, 0, 0, QFileInfo(filename).size(), 0)) {
        errorString = QCoreApplication::translate("LockFile", "Cannot obtain the lock for "
            "file \"%1\": %2").arg(QDir::toNativeSeparators(filename), QInstaller::windowsErrorString(GetLastError()));
    } else {
        locked = true;
    }
    return locked;
}

bool LockFile::Private::unlock()
{
    errorString.clear();
    if (!locked)
        return true;

    if (!UnlockFile(handle, 0, 0, QFileInfo(filename).size(), 0)) {
        errorString = QCoreApplication::translate("LockFile", "Cannot release the lock for "
            "file \"%1\": %2").arg(QDir::toNativeSeparators(filename), QInstaller::windowsErrorString(GetLastError()));
    } else {
        locked = false;
        CloseHandle(handle);
    }
    return !locked;
}

} // namespace KDUpdater
