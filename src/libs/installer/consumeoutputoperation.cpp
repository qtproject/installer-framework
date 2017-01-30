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

#include "consumeoutputoperation.h"
#include "packagemanagercore.h"
#include "utils.h"

#include <QFile>
#include <QDir>
#include <QProcess>
#include <QDebug>

using namespace QInstaller;

ConsumeOutputOperation::ConsumeOutputOperation(PackageManagerCore *core)
    : UpdateOperation(core)
{
    setName(QLatin1String("ConsumeOutput"));
}

void ConsumeOutputOperation::backup()
{
}

bool ConsumeOutputOperation::performOperation()
{
    // Arguments:
    // 1. key where the output will be saved
    // 2. executable path
    // 3. argument for the executable
    // 4. more arguments possible ...

    if (!checkArgumentCount(2, INT_MAX, tr("<to be saved installer key name> "
                                           "<executable> [argument1] [argument2] [...]")))
        return false;

    PackageManagerCore *const core = packageManager();
    if (!core) {
        setError(UserDefinedError);
        setErrorString(tr("Needed installer object in %1 operation is empty.").arg(name()));
        return false;
    }

    const QString installerKeyName = arguments().at(0);
    if (installerKeyName.isEmpty()) {
        setError(UserDefinedError);
        setErrorString(tr("Cannot save the output of \"%1\" to an empty installer key value.").arg(
            QDir::toNativeSeparators(arguments().at(1))));
        return false;
    }

    QString executablePath = arguments().at(1);
    QFileInfo executable(executablePath);
#ifdef Q_OS_WIN
    if (!executable.exists() && executable.suffix().isEmpty())
        executable = QFileInfo(executablePath + QLatin1String(".exe"));
#endif

    if (!executable.exists() || !executable.isExecutable()) {
        setError(UserDefinedError);
        setErrorString(tr("File \"%1\" does not exist or is not an executable binary.").arg(
            QDir::toNativeSeparators(executable.absoluteFilePath())));
        return false;
    }

    QByteArray executableOutput;


    const QStringList processArguments = arguments().mid(2);
    // in some cases it is not runable, because another process is blocking it(filewatcher ...)
    int waitCount = 0;
    while (executableOutput.isEmpty() && waitCount < 3) {
        QProcess process;
        process.start(executable.absoluteFilePath(), processArguments, QIODevice::ReadOnly);
        if (process.waitForFinished(10000)) {
            if (process.exitStatus() == QProcess::CrashExit) {
                qWarning() << executable.absoluteFilePath() << processArguments
                           << "crashed with exit code" << process.exitCode()
                           << "standard output: " << process.readAllStandardOutput()
                           << "error output: " << process.readAllStandardError();
                setError(UserDefinedError);
                setErrorString(tr("Running \"%1\" resulted in a crash.").arg(
                    QDir::toNativeSeparators(executable.absoluteFilePath())));
                return false;
            }
            executableOutput.append(process.readAllStandardOutput());
        }
        if (executableOutput.isEmpty()) {
            ++waitCount;
            static const int waitTimeInMilliSeconds = 500;
            uiDetachedWait(waitTimeInMilliSeconds);
        }
        if (process.state() > QProcess::NotRunning ) {
            qWarning() << executable.absoluteFilePath() << "process is still running, need to kill it.";
            process.kill();
        }

    }
    if (executableOutput.isEmpty()) {
        qWarning() << "Cannot get any query output from executable" << executable.absoluteFilePath();
    }
    core->setValue(installerKeyName, QString::fromLocal8Bit(executableOutput));
    return true;
}

bool ConsumeOutputOperation::undoOperation()
{
    return true;
}

bool ConsumeOutputOperation::testOperation()
{
    return true;
}
