/**************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://qt.io/terms-conditions. For further
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
**
** $QT_END_LICENSE$
**
**************************************************************************/

#include "extractarchiveoperation_p.h"

#include <QEventLoop>
#include <QThreadPool>

namespace QInstaller {

ExtractArchiveOperation::ExtractArchiveOperation(PackageManagerCore *core)
    : UpdateOperation(core)
{
    setName(QLatin1String("Extract"));
}

void ExtractArchiveOperation::backup()
{
    // we need to backup on the fly...
}

bool ExtractArchiveOperation::performOperation()
{
    if (!checkArgumentCount(2))
        return false;

    const QStringList args = arguments();
    const QString archivePath = args.at(0);
    const QString targetDir = args.at(1);

    Receiver receiver;
    Callback callback;

    connect(&callback, &Callback::currentFileChanged, this, &ExtractArchiveOperation::fileFinished);
    connect(&callback, &Callback::progressChanged, this, &ExtractArchiveOperation::progressChanged);

    if (PackageManagerCore *core = packageManager()) {
        connect(core, &PackageManagerCore::statusChanged, &callback, &Callback::statusChanged);
    }

    Runnable *runnable = new Runnable(archivePath, targetDir, &callback);
    connect(runnable, &Runnable::finished, &receiver, &Receiver::runnableFinished,
        Qt::QueuedConnection);

    QEventLoop loop;
    connect(&receiver, &Receiver::finished, &loop, &QEventLoop::quit);
    if (QThreadPool::globalInstance()->tryStart(runnable)) {
        loop.exec();
    } else {
        // HACK: In case there is no availabe thread we should call it directly.
        runnable->run();
        receiver.runnableFinished(true, QString());
    }

    // TODO: Use backups for rollback, too? Doesn't work for uninstallation though.

    // delete all backups we can delete right now, remember the rest
    foreach (const Backup &i, callback.backupFiles())
        deleteFileNowOrLater(i.second);

    if (!receiver.success()) {
        setError(UserDefinedError);
        setErrorString(receiver.errorString());
        return false;
    }
    return true;
}

bool ExtractArchiveOperation::undoOperation()
{
    Q_ASSERT(arguments().count() == 2);
    const QStringList files = value(QLatin1String("files")).toStringList();

    WorkerThread *const thread = new WorkerThread(this, files);
    connect(thread, &WorkerThread::currentFileChanged, this,
        &ExtractArchiveOperation::outputTextChanged);
    connect(thread, &WorkerThread::progressChanged, this,
        &ExtractArchiveOperation::progressChanged);

    QEventLoop loop;
    connect(thread, &QThread::finished, &loop, &QEventLoop::quit, Qt::QueuedConnection);
    thread->start();
    loop.exec();
    thread->deleteLater();
    return true;
}

bool ExtractArchiveOperation::testOperation()
{
    return true;
}

/*!
    This slot is direct connected to the caller so please don't call it from another thread in the
    same time.
*/
void ExtractArchiveOperation::fileFinished(const QString &filename)
{
    QStringList files = value(QLatin1String("files")).toStringList();
    files.prepend(filename);
    setValue(QLatin1String("files"), files);
    emit outputTextChanged(QDir::toNativeSeparators(filename));
}

} // namespace QInstaller
