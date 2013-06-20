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

#include "kdrunoncechecker.h"
#include "kdlockfile.h"
#include "kdsysinfo.h"

#include <QtCore/QList>
#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>

#include <algorithm>

using namespace KDUpdater;

class KDRunOnceChecker::Private
{
public:
    Private(const QString &filename);

    KDLockFile m_lockfile;
    bool m_hasLock;
};

KDRunOnceChecker::Private::Private(const QString &filename)
    : m_lockfile(filename)
    , m_hasLock(false)
{}

KDRunOnceChecker::KDRunOnceChecker(const QString &filename)
    :d(new Private(filename))
{}

KDRunOnceChecker::~KDRunOnceChecker()
{
    delete d;
}

class ProcessnameEquals
{
public:
    ProcessnameEquals(const QString &name): m_name(name) {}

    bool operator()(const ProcessInfo &info)
    {
#ifndef Q_OS_WIN
        if (info.name == m_name)
            return true;
        const QFileInfo fi(info.name);
        if (fi.fileName() == m_name || fi.baseName() == m_name)
            return true;
        return false;
#else
        if (info.name.toLower() == m_name.toLower())
            return true;
        if (info.name.toLower() == QDir::toNativeSeparators(m_name.toLower()))
            return true;
        const QFileInfo fi(info.name);
        if (fi.fileName().toLower() == m_name.toLower() || fi.baseName().toLower() == m_name.toLower())
            return true;
        return info.name == m_name;
#endif
    }

private:
    QString m_name;
};

bool KDRunOnceChecker::isRunning(Dependencies depends)
{
    bool running = false;
    switch (depends) {
        case Lockfile: {
            const bool locked = d->m_hasLock || d->m_lockfile.lock();
            if (locked)
                d->m_hasLock = true;
            running = running || ! locked;
        }
        break;
        case ProcessList: {
            const QList<ProcessInfo> allProcesses = runningProcesses();
            const QString appName = qApp->applicationFilePath();
            //QList< ProcessInfo >::const_iterator it = std::find_if(allProcesses.constBegin(), allProcesses.constEnd(), ProcessnameEquals(appName));
            const int count = std::count_if(allProcesses.constBegin(), allProcesses.constEnd(), ProcessnameEquals(appName));
            running = running || /*it != allProcesses.constEnd()*/count > 1;
        }
        break;
        case Both: {
            const QList<ProcessInfo> allProcesses = runningProcesses();
            const QString appName = qApp->applicationFilePath();
            //QList<ProcessInfo>::const_iterator it = std::find_if(allProcesses.constBegin(), allProcesses.constEnd(), ProcessnameEquals(appName));
            const int count = std::count_if(allProcesses.constBegin(), allProcesses.constEnd(), ProcessnameEquals(appName));
            const bool locked = d->m_hasLock || d->m_lockfile.lock();
            if (locked)
                d->m_hasLock = true;
            running = running || ( /*it != allProcesses.constEnd()*/count > 1 && !locked);
        }
        break;
    }

    return running;
}
