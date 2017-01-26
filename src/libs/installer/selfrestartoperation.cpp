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

#include "selfrestartoperation.h"
#include "packagemanagercore.h"

#include <kdselfrestarter.h>

using namespace QInstaller;

SelfRestartOperation::SelfRestartOperation()
{
    setName(QLatin1String("SelfRestart"));
}

void SelfRestartOperation::backup()
{
    setValue(QLatin1String("PreviousSelfRestart"), KDSelfRestarter::restartOnQuit());
}

bool SelfRestartOperation::performOperation()
{
    PackageManagerCore *const core = value(QLatin1String("installer")).value<PackageManagerCore*>();
    if (!core) {
        setError(UserDefinedError);
        setErrorString(tr("Installer object needed in '%1' operation is empty.").arg(name()));
        return false;
    }

    if (!core->isUpdater() && !core->isPackageManager()) {
        setError(UserDefinedError);
        setErrorString(tr("Self Restart: Only valid within updater or packagemanager mode."));
        return false;
    }

    if (!arguments().isEmpty()) {
        setError(InvalidArguments);
        setErrorString(tr("Self Restart: Invalid arguments"));
        return false;
    }
    KDSelfRestarter::setRestartOnQuit(true);
    return KDSelfRestarter::restartOnQuit();
}

bool SelfRestartOperation::undoOperation()
{
    KDSelfRestarter::setRestartOnQuit(value(QLatin1String("PreviousSelfRestart")).toBool());
    return true;
}

bool SelfRestartOperation::testOperation()
{
    return true;
}

Operation *SelfRestartOperation::clone() const
{
    return new SelfRestartOperation();
}
