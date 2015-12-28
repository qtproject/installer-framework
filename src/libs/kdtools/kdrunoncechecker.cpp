/****************************************************************************
**
** Copyright (C) 2013 Klaralvdalens Datakonsult AB (KDAB)
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
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
** $QT_END_LICENSE$
**
****************************************************************************/

#include "kdrunoncechecker.h"
#include "kdlockfile.h"
#include "kdsysinfo.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QList>

#include <algorithm>

using namespace KDUpdater;

KDRunOnceChecker::KDRunOnceChecker(const QString &filename)
    : m_lockfile(filename)
{
}

KDRunOnceChecker::~KDRunOnceChecker()
{
    if (!m_lockfile.unlock())
        qWarning() << m_lockfile.errorString().toUtf8().constData();
}

class ProcessnameEquals
{
public:
    ProcessnameEquals(const QString &name)
#ifdef Q_OS_WIN
        : m_name(name.toLower())
#else
        : m_name(name)
#endif
    {}

    bool operator()(const ProcessInfo &info)
    {
#ifdef Q_OS_WIN
        const QString infoName = info.name.toLower();
        if (infoName == QDir::toNativeSeparators(m_name))
            return true;
#else
        const QString infoName = info.name;
#endif
        if (infoName == m_name)
            return true;

        const QFileInfo fi(infoName);
        if (fi.fileName() == m_name || fi.baseName() == m_name)
            return true;
        return false;
    }

private:
    QString m_name;
};

bool KDRunOnceChecker::isRunning(KDRunOnceChecker::ConditionFlags flags)
{
    if (flags.testFlag(ConditionFlag::ProcessList)) {
        const QList<ProcessInfo> allProcesses = runningProcesses();
        const int count = std::count_if(allProcesses.constBegin(), allProcesses.constEnd(),
            ProcessnameEquals(QCoreApplication::applicationFilePath()));
        return (count > 1);
    }

    if (flags.testFlag(ConditionFlag::Lockfile)) {
        const bool locked = m_lockfile.lock();
        if (!locked)
            qWarning() << m_lockfile.errorString().toUtf8().constData();
        return !locked;
    }
    return false;
}
