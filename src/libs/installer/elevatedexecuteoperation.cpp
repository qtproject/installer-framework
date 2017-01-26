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

#include "elevatedexecuteoperation.h"

#include "environment.h"
#include "qprocesswrapper.h"

#include <QtCore/QThread>
#include <QtCore/QProcessEnvironment>
#include <QtCore/QDebug>

using namespace QInstaller;

class ElevatedExecuteOperation::Private
{
public:
    explicit Private(ElevatedExecuteOperation *qq)
        : q(qq)
        , process(0)
        , showStandardError(false)
    {
    }

private:
    ElevatedExecuteOperation *const q;

public:
    void readProcessOutput();
    bool run(const QStringList &arguments);

    QProcessWrapper *process;
    bool showStandardError;
};

ElevatedExecuteOperation::ElevatedExecuteOperation()
    : d(new Private(this))
{
    // this operation has to "overwrite" the Execute operation from KDUpdater
    setName(QLatin1String("Execute"));
}

ElevatedExecuteOperation::~ElevatedExecuteOperation()
{
    delete d;
}

bool ElevatedExecuteOperation::performOperation()
{
    // This operation receives only one argument. It is the complete
    // command line of the external program to execute.
    if (arguments().isEmpty()) {
        setError(InvalidArguments);
        setErrorString(tr("Invalid arguments in %0: %1 arguments given, %2 expected%3.")
            .arg(name()).arg(arguments().count()).arg(tr("at least 1"), QLatin1String("")));
        return false;
    }
    QStringList args;
    foreach (const QString &argument, arguments()) {
        if (argument!=QLatin1String("UNDOEXECUTE"))
            args.append(argument);
        else
            break; //we don't need the UNDOEXECUTE args here
    }

    return d->run(args);
}

bool ElevatedExecuteOperation::Private::run(const QStringList &arguments)
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

    QString customErrorMessage;
    QStringList filteredCustomErrorMessage = args.filter(QLatin1String("errormessage="),
        Qt::CaseInsensitive);
    if (!filteredCustomErrorMessage.isEmpty()) {
        QString customErrorMessageArgument = filteredCustomErrorMessage.at(0);
        customErrorMessage = customErrorMessageArgument;
        customErrorMessage.replace(QLatin1String("errormessage="), QString(), Qt::CaseInsensitive);
        args.removeAll(customErrorMessageArgument);
    }

    if (args.last().endsWith(QLatin1String("showStandardError"))) {
        showStandardError = true;
        args.pop_back();
    }

    QList< int > allowedExitCodes;

    QRegExp re(QLatin1String("^\\{((-?\\d+,)*-?\\d+)\\}$"));
    if (re.exactMatch(args.first())) {
        const QStringList numbers = re.cap(1).split(QLatin1Char(','));
        for(QStringList::const_iterator it = numbers.constBegin(); it != numbers.constEnd(); ++it)
            allowedExitCodes.push_back(it->toInt());
        args.pop_front();
    } else {
        allowedExitCodes.push_back(0);
    }

    const QString callstr = args.join(QLatin1String(" "));

    // unix style: when there's an ampersand after the command, it's started detached
    if (args.count() >= 2 && args.last() == QLatin1String("&")) {
        args.pop_back();
        const bool success = QProcessWrapper::startDetached(args.front(), args.mid(1));
        if (!success) {
            q->setError(UserDefinedError);
            q->setErrorString(tr("Execution failed: Could not start detached: \"%1\"").arg(callstr));
        }
        return success;
    }

    process = new QProcessWrapper();
    if (!workingDirectory.isEmpty()) {
        process->setWorkingDirectory(workingDirectory);
        qDebug() << "ElevatedExecuteOperation setWorkingDirectory:" << workingDirectory;
    }

    QProcessEnvironment penv;
    // there is no way to serialize a QProcessEnvironment properly other than per mangled QStringList:
    // (i.e. no other way to list all keys)
    process->setEnvironment(KDUpdater::Environment::instance().applyTo(penv).toStringList());

    if (showStandardError)
        process->setProcessChannelMode(QProcessWrapper::MergedChannels);

    connect(q, SIGNAL(cancelProcess()), process, SLOT(cancel()));

    //we still like the none blocking possibility to perform this operation without threads
    QEventLoop loop;
    if (QThread::currentThread() == qApp->thread()) {
        QObject::connect(process, SIGNAL(finished(int, QProcess::ExitStatus)), &loop, SLOT(quit()));
    }
    //readProcessOutput should only called from this current Thread -> Qt::DirectConnection
    QObject::connect(process, SIGNAL(readyRead()), q, SLOT(readProcessOutput()), Qt::DirectConnection);
    process->start(args.front(), args.mid(1));
    qDebug() << args.front() << "started, arguments:" << QStringList(args.mid(1)).join(QLatin1String(" "));

    bool success = false;
    //we still like the none blocking possibility to perform this operation without threads
    if (QThread::currentThread() == qApp->thread()) {
        success = process->waitForStarted();
    } else {
        success = process->waitForFinished(-1);
    }

    bool returnValue = true;
    if (!success) {
        q->setError(UserDefinedError);
        //TODO: pass errorString() through the wrapper */
        q->setErrorString(tr("Execution failed: Could not start: \"%1\"(%2)").arg(callstr,
            process->errorString()));
        returnValue = false;
    }

    if (QThread::currentThread() == qApp->thread()) {
        if (process->state() != QProcessWrapper::NotRunning) {
            loop.exec();
        }
        readProcessOutput();
    }

    q->setValue(QLatin1String("ExitCode"), process->exitCode());

    if (process->exitStatus() == QProcessWrapper::CrashExit) {
        q->setError(UserDefinedError);
        q->setErrorString(tr("Execution failed(Crash): \"%1\"").arg(callstr));
        returnValue = false;
    }

    if (!allowedExitCodes.contains(process->exitCode())) {
        q->setError(UserDefinedError);
        if (customErrorMessage.isEmpty()) {
            q->setErrorString(tr("Execution failed(Unexpected exit code: %1): \"%2\"")
                .arg(QString::number(process->exitCode()), callstr));
        } else {
            q->setErrorString(customErrorMessage);
        }

        QByteArray standardErrorOutput = process->readAllStandardError();
        // in error case it would be useful to see something in verbose output
        if (!standardErrorOutput.isEmpty())
            qWarning() << standardErrorOutput;

        returnValue = false;
    }

    Q_ASSERT(process);
    process->deleteLater();
    process = 0;

    return returnValue;
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
        qDebug() << Q_FUNC_INFO << "can only be called from the same thread as the process is.";
    }
    const QByteArray output = process->readAll();
    if (!output.isEmpty()) {
        if (q->error() == UserDefinedError)
            qWarning() << output;
        else
            qDebug() << output;
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

    return d->run(args);
}

bool ElevatedExecuteOperation::testOperation()
{
    // TODO
    return true;
}

Operation *ElevatedExecuteOperation::clone() const
{
    return new ElevatedExecuteOperation;
}

void ElevatedExecuteOperation::backup()
{
}


#include "moc_elevatedexecuteoperation.cpp"
