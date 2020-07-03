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

#include "runoncechecker.h"
#include "lockfile.h"
#include "sysinfo.h"
#include "globals.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QList>

#include <algorithm>

using namespace KDUpdater;

RunOnceChecker::RunOnceChecker(const QString &filename)
    : m_lockfile(filename)
{
}

RunOnceChecker::~RunOnceChecker()
{
    if (!m_lockfile.unlock())
        qCWarning(QInstaller::lcInstallerInstallLog).noquote() << m_lockfile.errorString();
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

bool RunOnceChecker::isRunning(RunOnceChecker::ConditionFlags flags)
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
            qCWarning(QInstaller::lcInstallerInstallLog).noquote() << m_lockfile.errorString();
        return !locked;
    }
    return false;
}
