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

#ifndef LOCKFILE_P_H
#define LOCKFILE_P_H

#include "lockfile.h"

#include <QString>

#ifdef Q_OS_WIN
# include <qt_windows.h>
#endif

namespace KDUpdater {

class LockFile::Private
{
public:
    explicit Private(const QString& name)
        : filename(name)
        , handle(0)
        , locked(false)
    {}

    bool lock();
    bool unlock();

    QString errorString;

private:
    QString filename;
#ifdef Q_OS_WIN
    HANDLE handle;
#else
    int handle;
#endif
    bool locked;
};

} // namespace KDUpdater

#endif // LOCKFILE_P_H
