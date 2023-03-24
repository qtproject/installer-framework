/**************************************************************************
**
** Copyright (C) 2023 The Qt Company Ltd.
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

#include "elevatedexecuteoperation.h"

#include "environment.h"
#include "qprocesswrapper.h"
#include "globals.h"
#include "packagemanagercore.h"

#include <QtCore/QDebug>
#include <QtCore/QProcessEnvironment>
#include <QtCore/QRegularExpression>
#include <QtCore/QThread>

using namespace QInstaller;

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::ElevatedExecuteOperation
    \internal
*/

class ElevatedExecuteOperation::Private
{
public:
    explicit Private(ElevatedExecuteOperation *qq)
        : q(qq)
        , process(nullptr)
        , showStandardError(false)
    {
    }

private:
    ElevatedExecuteOperation *const q;

public:
    void readProcessOutput();
    int run(QStringList &arguments, const OperationType type);

private:
    bool needsRerunWithReplacedVariables(QStringList &arguments, const OperationType type);
    void setErrorMessage(const QString &message);

private:
    QProcessWrapper *process;
    bool showStandardError;
    QString m_customErrorMessage;
};

ElevatedExecuteOperation::ElevatedExecuteOperation(PackageManagerCore *core)
    : UpdateOperation(core)
    , d(new Private(this))
{
    // this operation has to "overwrite" the Execute operation from KDUpdater
    setName(QLatin1String("Execute"));
    setRequiresUnreplacedVariables(true);
}

ElevatedExecuteOperation::~ElevatedExecuteOperation()
{
    delete d;
}

bool ElevatedExecuteOperation::performOperation()
{
    // This operation receives only one argument. It is the complete
    // command line of the external program to execute.
    if (!checkArgumentCount(1, INT_MAX))
        return false;

    QStringList args;
    foreach (const QString &argument, arguments()) {
        if (argument!=QLatin1String("UNDOEXECUTE"))
            args.append(argument);
        else
            break; //we don't need the UNDOEXECUTE args here
    }

    if (requiresUnreplacedVariables()) {
        PackageManagerCore *const core = packageManager();
        args = core->replaceVariables(args);
    }
    return d->run(args, Operation::Perform) ? false : true;
}

int ElevatedExecuteOperation::Private::run(QStringList &arguments, const OperationType type)
{
    QStringList args = arguments;
    QString workingDirectory;
    QStringList filteredWorkingDirectoryArgs = args.filter(QLatin1String("workingdirectory="),
        Qt::CaseInsensitive);
    if (!filteredWorkingDirectoryArgs.isEmpty()) {
        QString workingDirectoryArgument = filteredWorkingDirectoryArgs.at(0);
        workingDirectory = workingDirectoryArgument;
        workingDirectory.replace(QLatin1String("workingdirectory="), QString(), Qt::CaseInsensitive);
        args.removeAll(workingDirectoryArgument);
    }

    QStringList filteredCustomErrorMessage = args.filter(QLatin1String("errormessage="),
        Qt::CaseInsensitive);
    if (!filteredCustomErrorMessage.isEmpty()) {
        QString customErrorMessageArgument = filteredCustomErrorMessage.at(0);
        m_customErrorMessage = customErrorMessageArgument;
        m_customErrorMessage.replace(QLatin1String("errormessage="), QString(), Qt::CaseInsensitive);
        args.removeAll(customErrorMessageArgument);
    }

    if (args.last().endsWith(QLatin1String("showStandardError"))) {
        showStandardError = true;
        args.pop_back();
    }

    QList< int > allowedExitCodes;

    static const QRegularExpression re(QLatin1String("^\\{((-?\\d+,)*-?\\d+)\\}$"));
    const QRegularExpressionMatch match = re.match(args.first());
    if (match.hasMatch()) {
        const QStringList numbers = match.captured(1).split(QLatin1Char(','));
        for(QStringList::const_iterator it = numbers.constBegin(); it != numbers.constEnd(); ++it)
            allowedExitCodes.push_back(it->toInt());
        args.pop_front();
    } else {
        allowedExitCodes.push_back(0);
    }

    const QString callstr = args.join(QLatin1String(" "));

    // unix style: when there's an ampersand after the command, it's started detached
    if (args.count() >= 2 && args.last() == QLatin1String("&")) {
        int returnValue = NoError;
        args.pop_back();
        const bool success = QProcessWrapper::startDetached(args.front(), args.mid(1));
        if (!success) {
            q->setError(UserDefinedError);
            setErrorMessage(tr("Cannot start detached: \"%1\"").arg(callstr));

            returnValue = Error;
        }
        return returnValue;
    }

    process = new QProcessWrapper();
    if (!workingDirectory.isEmpty()) {
        process->setWorkingDirectory(workingDirectory);
        qCDebug(QInstaller::lcInstallerInstallLog) << "ElevatedExecuteOperation setWorkingDirectory:"
            << workingDirectory;
    }

    QProcessEnvironment penv = QProcessEnvironment::systemEnvironment();
    // there is no way to serialize a QProcessEnvironment properly other than per mangled QStringList:
    // (i.e. no other way to list all keys)
    process->setEnvironment(KDUpdater::Environment::instance().applyTo(penv).toStringList());

    if (showStandardError)
        process->setProcessChannelMode(QProcessWrapper::MergedChannels);

    connect(q, &ElevatedExecuteOperation::cancelProcess, process, &QProcessWrapper::cancel);

    //we still like the none blocking possibility to perform this operation without threads
    QEventLoop loop;
    if (QThread::currentThread() == qApp->thread()) {
        QObject::connect(process, &QProcessWrapper::finished, &loop, &QEventLoop::quit);
    }
    //readProcessOutput should only called from this current Thread -> Qt::DirectConnection
    QObject::connect(process, SIGNAL(readyRead()), q, SLOT(readProcessOutput()), Qt::DirectConnection);
    process->start(args.front(), args.mid(1));
    qCDebug(QInstaller::lcInstallerInstallLog) << args.front() << "started, arguments:"
        << QStringList(args.mid(1)).join(QLatin1String(" "));

    bool success = false;
    //we still like the none blocking possibility to perform this operation without threads
    if (QThread::currentThread() == qApp->thread()) {
        success = process->waitForStarted();
    } else {
        success = process->waitForFinished(-1);
    }

    int returnValue = NoError;
    if (!success) {
        q->setError(UserDefinedError);
        setErrorMessage(tr("Cannot start: \"%1\": %2").arg(callstr,
            process->errorString()));
        if (!needsRerunWithReplacedVariables(arguments, type)) {
            returnValue = Error;
        } else {
            returnValue = NeedsRerun;
        }
    }

    if (QThread::currentThread() == qApp->thread()) {
        if (process->state() != QProcessWrapper::NotRunning) {
            loop.exec();
        }
        readProcessOutput();
    }

    q->setValue(QLatin1String("ExitCode"), process->exitCode());

    if (success) {
        const QByteArray standardErrorOutput = process->readAllStandardError();
        // in error case it would be useful to see something in verbose output
        if (!standardErrorOutput.isEmpty())
            qCWarning(QInstaller::lcInstallerInstallLog).noquote() << standardErrorOutput;

        if (process->exitStatus() == QProcessWrapper::CrashExit) {
            q->setError(UserDefinedError);
            setErrorMessage(tr("Program crashed: \"%1\"").arg(callstr));
            returnValue = Error;
        } else if (!allowedExitCodes.contains(process->exitCode()) && returnValue != NeedsRerun) {
            if (!needsRerunWithReplacedVariables(arguments, type)) {
                q->setError(UserDefinedError);
                setErrorMessage(tr("Execution failed (Unexpected exit code: %1): \"%2\"")
                    .arg(QString::number(process->exitCode()), callstr));
                returnValue = Error;
            } else {
                returnValue = NeedsRerun;
            }
        }
    }
    Q_ASSERT(process);
    Q_ASSERT(process->state() == QProcessWrapper::NotRunning);
    delete process;
    process = nullptr;

    return returnValue;
}

bool ElevatedExecuteOperation::Private::needsRerunWithReplacedVariables(QStringList &arguments, const OperationType type)
{
    if (type != Operation::Undo)
        return false;
    bool rerun = false;
    PackageManagerCore *const core = q->packageManager();
    for (int i = 0; i < arguments.count(); i++) {
        QString key = core->key(arguments.at(i));
        if (!key.isEmpty() && key.endsWith(QLatin1String("_OLD"))) {
            key.remove(key.length() - 4, 4);
            if (core->containsValue(key)) {
                key.prepend(QLatin1String("@"));
                key.append(QLatin1String("@"));
                QString value = core->replaceVariables(key);
                arguments.replace(i, value);
                rerun = true;
            }
        }
    }
    return rerun;
}

void ElevatedExecuteOperation::Private::setErrorMessage(const QString &message)
{
    if (m_customErrorMessage.isEmpty())
        q->setErrorString(message);
    else
        q->setErrorString(m_customErrorMessage);
}
/*!
 Cancels the ElevatedExecuteOperation. This methods tries to terminate the process
 gracefully by calling QProcessWrapper::terminate. After 10 seconds, the process gets killed.
 */
void ElevatedExecuteOperation::cancelOperation()
{
    emit cancelProcess();
}

void ElevatedExecuteOperation::Private::readProcessOutput()
{
    Q_ASSERT(process);
    Q_ASSERT(QThread::currentThread() == process->thread());
    if (QThread::currentThread() != process->thread()) {
        qCDebug(QInstaller::lcInstallerInstallLog) << Q_FUNC_INFO << "can only be called from the "
            "same thread as the process is.";
    }
    const QByteArray output = process->readAll();
    if (!output.isEmpty()) {
        if (q->error() == UserDefinedError)
            qCWarning(QInstaller::lcInstallerInstallLog)<< output;
        else
            qCDebug(QInstaller::lcInstallerInstallLog) << output;
        emit q->outputTextChanged(QString::fromLocal8Bit(output));
    }
}

bool ElevatedExecuteOperation::undoOperation()
{
    QStringList args;
    bool found = false;
    foreach (const QString &argument, arguments()) {
        if (found)
            args.append(argument);
        else
            found = argument == QLatin1String("UNDOEXECUTE");
    }
    if (args.isEmpty())
        return true;

    if (requiresUnreplacedVariables()) {
        PackageManagerCore *const core = packageManager();
        args = core->replaceVariables(args);
    }

    int returnValue = d->run(args, Operation::Undo);
    if (returnValue == NeedsRerun) {
        qCDebug(QInstaller::lcInstallerInstallLog).noquote() << QString::fromLatin1("Failed to run "
            "undo operation \"%1\" for component %2. Trying again with arguments %3").arg(name(),
            value(QLatin1String("component")).toString(), args.join(QLatin1String(", ")));
        setError(NoError);
        setErrorString(QString());
        returnValue = d->run(args, Operation::Undo);
    }
    return returnValue ? false : true;
}

bool ElevatedExecuteOperation::testOperation()
{
    // TODO
    return true;
}

void ElevatedExecuteOperation::backup()
{
}


#include "moc_elevatedexecuteoperation.cpp"
