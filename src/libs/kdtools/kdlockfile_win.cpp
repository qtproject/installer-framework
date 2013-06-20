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

#include "kdlockfile.h"
#include "kdlockfile_p.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QFileInfo>

KDLockFile::Private::~Private()
{
    unlock();
}

bool KDLockFile::Private::lock()
{
    const QFileInfo fi(filename);
    handle = CreateFile(filename.toStdWString().data(),
                        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ,
                        NULL, fi.exists() ? OPEN_EXISTING : CREATE_NEW,
                        FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, NULL);

    if (!handle)
        return false;
    QString pid = QString::number(qApp->applicationPid());
    QByteArray data = pid.toLatin1();
    DWORD bytesWritten;
    const bool wrotePid = WriteFile(handle, data.data(), data.size(), &bytesWritten, NULL);
    if (!wrotePid)
        return false;
    FlushFileBuffers(handle);

    const bool locked = LockFile(handle, 0, 0, fi.size(), 0);

    this->locked = locked;
    return locked;
}

bool KDLockFile::Private::unlock()
{
    const QFileInfo fi(filename);
    if (locked) {
        const bool success = UnlockFile(handle, 0, 0, 0, fi.size());
        this->locked = !success;
        CloseHandle(handle);
        return success;
    }
    return true;
}
