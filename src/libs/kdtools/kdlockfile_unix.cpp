/****************************************************************************
**
** Copyright (C) 2013 Klaralvdalens Datakonsult AB (KDAB)
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
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
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "kdlockfile_p.h"

#include <QCoreApplication>

#include <cerrno>
#include <sys/file.h>
#include <unistd.h>

bool KDLockFile::Private::lock()
{
    if (locked)
        return true;

    errorString.clear();
    errno = 0;
    handle = open(filename.toLatin1().constData(), O_CREAT | O_RDWR | O_NONBLOCK, 0600);
    if (handle == -1) {
        errorString = QCoreApplication::translate("KDLockFile", "Could not create lock file '%1': "
            "%2").arg(filename, QString::fromLocal8Bit(strerror(errno)));
        return false;
    }
    const QString pid = QString::number(qApp->applicationPid());
    const QByteArray data = pid.toLatin1();
    errno = 0;
    qint64 written = 0;
    while (written < data.size()) {
        const qint64 n = write(handle, data.constData() + written, data.size() - written);
        if (n < 0) {
            errorString = QCoreApplication::translate("KDLockFile", "Could not write PID to lock "
                "file '%1': %2").arg(filename, QString::fromLocal8Bit(strerror(errno)));
            return false;
        }
        written += n;
    }
    errno = 0;
    locked = flock(handle, LOCK_NB | LOCK_EX) != -1;
    if (!locked) {
        errorString = QCoreApplication::translate("KDLockFile", "Could not obtain the lock for "
            "file '%1': %2").arg(filename, QString::fromLocal8Bit(strerror(errno)));
    }
    return locked;
}

bool KDLockFile::Private::unlock()
{
    errorString.clear();
    if (!locked)
        return true;

    errno = 0;
    locked = flock(handle, LOCK_UN | LOCK_NB) == -1;
    if (locked) {
        errorString = QCoreApplication::translate("KDLockFile", "Could not release the lock for "
            "file '%1': %2").arg(filename, QString::fromLocal8Bit(strerror(errno)));
    } else {
        unlink(filename.toLatin1());
    }
    return !locked;
}
