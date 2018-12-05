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

#include "extractarchiveoperation_p.h"

#include <QEventLoop>
#include <QThreadPool>
#include <QFileInfo>

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

    m_files.clear();

    QFileInfo fileInfo(archivePath);
    emit outputTextChanged(tr("Extracting \"%1\"").arg(fileInfo.fileName()));

    QEventLoop loop;
    connect(&receiver, &Receiver::finished, &loop, &QEventLoop::quit);
    if (QThreadPool::globalInstance()->tryStart(runnable)) {
        loop.exec();
    } else {
        // HACK: In case there is no availabe thread we should call it directly.
        runnable->run();
        receiver.runnableFinished(true, QString());
    }

    setValue(QLatin1String("files"), m_files);

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
    m_files.prepend(filename);
}

} // namespace QInstaller
