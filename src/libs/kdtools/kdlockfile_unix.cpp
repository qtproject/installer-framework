/****************************************************************************
**
** Copyright (C) 2013 Klaralvdalens Datakonsult AB (KDAB)
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
****************************************************************************/

#include "kdlockfile_p.h"

#include <QtCore/QCoreApplication>

#include <cerrno>

#include <sys/file.h>

#include <unistd.h>

KDLockFile::Private::~Private()
{
    unlock();
}

bool KDLockFile::Private::lock()
{
    if (locked)
        return true;

    errorString.clear();
    errno = 0;
    handle = open(filename.toLatin1().constData(), O_CREAT | O_RDWR | O_NONBLOCK, 0600);
    if (handle == -1) {
        errorString = QObject::tr("Could not create lock file %1: %2").arg(filename, QLatin1String(strerror(errno)));
        return false;
    }
    const QString pid = QString::number(qApp->applicationPid());
    const QByteArray data = pid.toLatin1();
    errno = 0;
    qint64 written = 0;
    while (written < data.size()) {
        const qint64 n = write(handle, data.constData() + written, data.size() - written);
        if (n < 0) {
            errorString = QObject::tr("Could not write PID to lock file %1: %2").arg( filename, QLatin1String( strerror( errno ) ) );
            return false;
        }
        written += n;
    }
    errno = 0;
    locked = flock(handle, LOCK_NB | LOCK_EX) != -1;
    if (!locked)
        errorString = QObject::tr("Could not lock lock file %1: %2").arg(filename, QLatin1String(strerror(errno)));
    return locked;
}

bool KDLockFile::Private::unlock()
{
    errorString.clear();
    if (!locked)
        return true;
    errno = 0;
    locked = flock(handle, LOCK_UN | LOCK_NB) == -1;
    if (locked)
        errorString = QObject::tr("Could not unlock lock file %1: %2").arg(filename, QLatin1String(strerror(errno)));
    return !locked;
}
