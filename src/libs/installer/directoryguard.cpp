/**************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
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

#include "directoryguard.h"

#include "fileutils.h"
#include "globals.h"
#include "errors.h"

#include <QCoreApplication>
#include <QDir>

namespace QInstaller {

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::DirectoryGuard
    \brief RAII class to create a directory and delete it on destruction unless released.
*/

/*!
    Constructs a new guard object for \a path.
*/
DirectoryGuard::DirectoryGuard(const QString &path)
    : m_path(path)
    , m_created(false)
    , m_released(false)
{
    m_path.replace(QLatin1Char('\\'), QLatin1Char('/'));
}

/*!
    Destroys the directory guard instance and removes the
    guarded directory unless released.
*/
DirectoryGuard::~DirectoryGuard()
{
    if (!m_created || m_released)
        return;
    QDir dir(m_path);
    if (!dir.rmdir(m_path))
        qCWarning(lcInstallerInstallLog) << "Cannot delete directory" << m_path;
}

/*!
    Tries to create the directory structure.
    Returns a list of every directory created.
*/
QStringList DirectoryGuard::tryCreate()
{
    if (m_path.isEmpty())
        return QStringList();

    const QFileInfo fi(m_path);
    if (fi.exists() && fi.isDir())
        return QStringList();
    if (fi.exists() && !fi.isDir()) {
        throw Error(QCoreApplication::translate("DirectoryGuard",
            "Path \"%1\" exists but is not a directory.").arg(QDir::toNativeSeparators(m_path)));
    }
    QStringList created;

    QDir toCreate(m_path);
    while (!toCreate.exists()) {
        QString p = toCreate.absolutePath();
        created.push_front(p);
        p = p.section(QLatin1Char('/'), 0, -2);
        toCreate = QDir(p);
    }

    m_created = QInstaller::createDirectoryWithParents(m_path);
    if (!m_created) {
        throw Error(QCoreApplication::translate("DirectoryGuard",
            "Cannot create directory \"%1\".").arg(QDir::toNativeSeparators(m_path)));
    }
    return created;
}

/*!
    Marks the directory as released.
*/
void DirectoryGuard::release()
{
    m_released = true;
}

} // namespace QInstaller
