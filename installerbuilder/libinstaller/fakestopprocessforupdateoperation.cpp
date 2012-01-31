/**************************************************************************
**
** This file is part of Qt SDK**
**
** Copyright (c) 2012 Nokia Corporation and/or its subsidiary(-ies).*
**
** Contact:  Nokia Corporation qt-info@nokia.com**
**
** No Commercial Usage
**
** This file contains pre-release code and may not be distributed.
** You may use this file in accordance with the terms and conditions
** contained in the Technology Preview License Agreement accompanying
** this package.
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception version
** 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** If you are unsure which license is appropriate for your use, please contact
** (qt-info@nokia.com).
**
**************************************************************************/

#include "fakestopprocessforupdateoperation.h"

#include <kdsysinfo.h>
#include <QtCore/QDir>

#include <algorithm>

using namespace KDUpdater;

/*!
    Copied from QInstaller with some adjustments
    Return true, if a process with \a name is running. On Windows, the comparision is case-insensitive.
*/
static bool isProcessRunning(const QString &name, const QList<ProcessInfo> &processes)
{
    for (QList<ProcessInfo>::const_iterator it = processes.constBegin(); it != processes.constEnd(); ++it) {
        if (it->name.isEmpty())
            continue;

#ifndef Q_WS_WIN
        if (it->name == name)
            return true;
        const QFileInfo fi(it->name);
        if (fi.fileName() == name || fi.baseName() == name)
            return true;
#else
        if (it->name.toLower() == name.toLower())
            return true;
        if (it->name.toLower() == QDir::toNativeSeparators(name.toLower()))
            return true;
        const QFileInfo fi(it->name);
        if (fi.fileName().toLower() == name.toLower() || fi.baseName().toLower() == name.toLower())
            return true;
#endif
    }
    return false;
}

static QStringList checkRunningProcessesFromList(const QStringList &processList)
{
    const QList<ProcessInfo> allProcesses = runningProcesses();
    QStringList stillRunningProcesses;
    foreach (const QString &process, processList) {
        if (!process.isEmpty() && isProcessRunning(process, allProcesses))
            stillRunningProcesses.append(process);
    }
    return stillRunningProcesses;
}

using namespace QInstaller;

FakeStopProcessForUpdateOperation::FakeStopProcessForUpdateOperation()
{
    setName(QLatin1String("FakeStopProcessForUpdate"));
}

void FakeStopProcessForUpdateOperation::backup()
{
}

bool FakeStopProcessForUpdateOperation::performOperation()
{
    return true;
}

bool FakeStopProcessForUpdateOperation::undoOperation()
{
    setError(KDUpdater::UpdateOperation::NoError);
    if (arguments().size() != 1) {
        setError(KDUpdater::UpdateOperation::InvalidArguments, QObject::tr("Number of arguments does not "
            "match : one is required"));
        return false;
    }

    QStringList processList = arguments()[0].split(QLatin1String(","), QString::SkipEmptyParts);
    qSort(processList);
    processList.erase(std::unique(processList.begin(), processList.end()), processList.end());
    if (!processList.isEmpty()) {
        const QStringList processes = checkRunningProcessesFromList(processList);
        if (!processes.isEmpty()) {
            setError(KDUpdater::UpdateOperation::UserDefinedError, tr("These processes should be stopped to "
                "continue:\n\n%1").arg(QDir::toNativeSeparators(processes.join(QLatin1String("\n")))));
        }
        return false;
    }
    return true;
}

bool FakeStopProcessForUpdateOperation::testOperation()
{
    return true;
}

Operation *FakeStopProcessForUpdateOperation::clone() const
{
    return new FakeStopProcessForUpdateOperation();
}
