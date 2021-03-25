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

#include "adminauthorization.h"

#include "globals.h"

#include <QDebug>
#include <QApplication>
#include <QMessageBox>
#include <QProcess>

#include <unistd.h>

#include <iostream>

#define PKEXEC_COMMAND "/usr/bin/pkexec"

namespace QInstaller {

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::AdminAuthorization
    \internal
*/

static void printError(QWidget *parent, const QString &value)
{
    if (qobject_cast<QApplication*> (qApp) != 0) {
        QMessageBox::critical(parent, QObject::tr( "Error acquiring admin rights" ), value,
            QMessageBox::Ok, QMessageBox::Ok);
    } else {
        std::cout << value.toStdString() << std::endl;
    }
}

bool AdminAuthorization::execute(QWidget *parent, const QString &program, const QStringList &arguments)
{
    const QString fallback = program + QLatin1String(" ") + arguments.join(QLatin1String(" "));
    qCDebug(QInstaller::lcServer) << "Fallback:" << fallback;

    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(QLatin1String(PKEXEC_COMMAND), QStringList() << program << arguments, QIODevice::ReadOnly);

    if (!process.waitForStarted() || !process.waitForFinished(-1)) {
        printError(parent, process.errorString());
        if (process.state() > QProcess::NotRunning)
            process.kill();
        return false;
    }
    if (process.exitCode() != EXIT_SUCCESS) {
        printError(parent, QLatin1String(process.readAll()));
        return false;
    }
    return true;
}

// has no guarantee to work
bool AdminAuthorization::hasAdminRights()
{
    return getuid() == 0;
}

} // namespace QInstaller
