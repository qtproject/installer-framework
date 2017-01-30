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

#include "lockfile_p.h"

#include <QCoreApplication>
#include <QDir>

#include <cerrno>
#include <sys/file.h>
#include <unistd.h>

namespace KDUpdater {

bool LockFile::Private::lock()
{
    if (locked)
        return true;

    errorString.clear();
    errno = 0;
    handle = open(filename.toLatin1().constData(), O_CREAT | O_RDWR | O_NONBLOCK, 0600);
    if (handle == -1) {
        errorString = QCoreApplication::translate("LockFile", "Cannot create lock file \"%1\": "
            "%2").arg(QDir::toNativeSeparators(filename), QString::fromLocal8Bit(strerror(errno)));
        return false;
    }
    const QString pid = QString::number(qApp->applicationPid());
    const QByteArray data = pid.toLatin1();
    errno = 0;
    qint64 written = 0;
    while (written < data.size()) {
        const qint64 n = write(handle, data.constData() + written, data.size() - written);
        if (n < 0) {
            errorString = QCoreApplication::translate("LockFile", "Cannot write PID to lock "
                "file \"%1\": %2").arg(QDir::toNativeSeparators(filename), QString::fromLocal8Bit(strerror(errno)));
            return false;
        }
        written += n;
    }
    errno = 0;
    locked = flock(handle, LOCK_NB | LOCK_EX) != -1;
    if (!locked) {
        errorString = QCoreApplication::translate("LockFile", "Cannot obtain the lock for "
            "file \"%1\": %2").arg(QDir::toNativeSeparators(filename), QString::fromLocal8Bit(strerror(errno)));
    }
    return locked;
}

bool LockFile::Private::unlock()
{
    errorString.clear();
    if (!locked)
        return true;

    errno = 0;
    locked = flock(handle, LOCK_UN | LOCK_NB) == -1;
    if (locked) {
        errorString = QCoreApplication::translate("LockFile", "Cannot release the lock for "
            "file \"%1\": %2").arg(QDir::toNativeSeparators(filename), QString::fromLocal8Bit(strerror(errno)));
    } else {
        unlink(filename.toLatin1());
    }
    return !locked;
}

} // namespace KDUpdater
