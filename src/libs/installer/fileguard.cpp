/**************************************************************************
**
** Copyright (C) 2022 The Qt Company Ltd.
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

#include "fileguard.h"

#include <QTimer>

using namespace QInstaller;

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::FileGuard
    \brief The \c FileGuard class provides basic access serialization for file paths.

    This class keeps a list of file paths that are locked from mutual
    access. Attempting to lock them from another thread will fail until the
    the locked path name is released.
*/

Q_GLOBAL_STATIC(FileGuard, globalFileGuard)

/*!
    Attempts to lock \a path. Returns \c true if the lock could be
    acquired, \c false if another thread has already locked the path.
*/
bool FileGuard::tryLock(const QString &path)
{
    QMutexLocker _(&m_mutex);
    if (path.isEmpty())
        return false;

    if (m_paths.contains(path))
        return false;

    m_paths.append(path);
    return true;
}

/*!
    Unlocks \a path.
*/
void FileGuard::release(const QString &path)
{
    QMutexLocker _(&m_mutex);
    m_paths.removeOne(path);
}

/*!
    Returns the application global instance.
*/
FileGuard *FileGuard::globalObject()
{
    return globalFileGuard;
}

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::FileGuardLocker
    \brief The \c FileGuardLocker class locks a file path and releases it on destruction.

    A convenience class for locking a file path using the resource acquisition
    is initialization (RAII) programming idiom.
*/

/*!
    Constructs the object and attempts to lock \a path with \a guard. If the lock is already
    held by another thread, this method will wait for it to become available.
*/
FileGuardLocker::FileGuardLocker(const QString &path, FileGuard *guard)
    : m_path(path)
    , m_guard(guard)
{
    if (!m_guard->tryLock(m_path)) {
        QTimer timer;
        QEventLoop loop;

        QObject::connect(&timer, &QTimer::timeout, [&]() {
            if (m_guard->tryLock(m_path))
                loop.quit();
        });

        timer.start(100);
        loop.exec();
    }
}

/*!
    Destructs the object and unlocks the locked file path.
*/
FileGuardLocker::~FileGuardLocker()
{
    m_guard->release(m_path);
}


