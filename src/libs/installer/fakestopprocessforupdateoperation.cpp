/**************************************************************************
**
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
**************************************************************************/

#include "fakestopprocessforupdateoperation.h"

#include "messageboxhandler.h"
#include "packagemanagercore.h"

#include <QDir>

using namespace KDUpdater;
using namespace QInstaller;

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::FakeStopProcessForUpdateOperation
    \internal
*/

FakeStopProcessForUpdateOperation::FakeStopProcessForUpdateOperation(PackageManagerCore *core)
    : UpdateOperation(core)
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
    if (!checkArgumentCount(1))
        return false;

    PackageManagerCore *const core = packageManager();
    if (!core) {
        setError(KDUpdater::UpdateOperation::UserDefinedError, tr("Cannot get package manager "
            "core."));
        return false;
    }

    QStringList processes = arguments().at(0).split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (int i = processes.count() - 1; i >= 0; --i) {
        if (!core->isProcessRunning(processes.at(i)))
            processes.removeAt(i);
    }

    if (processes.isEmpty())
        return true;

    if (processes.count() == 1) {
        setError(UpdateOperation::UserDefinedError, tr("This process should be stopped before "
            "continuing: %1").arg(QDir::toNativeSeparators(processes.first())));
    } else {
        const QString sep = QString::fromWCharArray(L"\n   \u2022 ");   // Unicode bullet
        setError(UpdateOperation::UserDefinedError, tr("These processes should be stopped before "
            "continuing: %1").arg(sep + QDir::toNativeSeparators(processes.join(sep))));
    }
    return false;
}

bool FakeStopProcessForUpdateOperation::testOperation()
{
    return true;
}
