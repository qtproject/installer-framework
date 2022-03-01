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

#ifndef FILEGUARD_H
#define FILEGUARD_H

#include "qinstallerglobal.h"

#include <QMutex>

namespace QInstaller {

class INSTALLER_EXPORT FileGuard
{
public:
    FileGuard() = default;

    bool tryLock(const QString &path);
    void release(const QString &path);

    static FileGuard *globalObject();

private:
    QMutex m_mutex;
    QStringList m_paths;
};

class INSTALLER_EXPORT FileGuardLocker
{
public:
    explicit FileGuardLocker(const QString &path, FileGuard *guard);
    ~FileGuardLocker();

private:
    QString m_path;
    FileGuard *m_guard;
};

} // namespace QInstaller

#endif // FILEGUARD_H
