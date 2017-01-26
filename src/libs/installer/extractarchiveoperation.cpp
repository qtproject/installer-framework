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

#include "extractarchiveoperation.h"
#include "extractarchiveoperation_p.h"

#include <QtCore/QEventLoop>
#include <QtCore/QThread>
#include <QtCore/QThreadPool>

using namespace QInstaller;


ExtractArchiveOperation::ExtractArchiveOperation()
{
    setName(QLatin1String("Extract"));
}

void ExtractArchiveOperation::backup()
{
    // we need to backup on the fly...
}

bool ExtractArchiveOperation::performOperation()
{
    const QStringList args = arguments();
    if (args.count() != 2) {
        setError(InvalidArguments);
        setErrorString(tr("Invalid arguments in %0: %1 arguments given, %2 expected%3.")
            .arg(name()).arg(arguments().count()).arg(tr("exactly 2"), QLatin1String("")));
        return false;
    }

    const QString archivePath = args.first();
    const QString targetDir = args.at(1);

    Receiver receiver;
    Callback callback;

    connect(&callback, SIGNAL(currentFileChanged(QString)), this, SLOT(fileFinished(QString)));
    connect(&callback, SIGNAL(progressChanged(double)), this, SIGNAL(progressChanged(double)));

    if (PackageManagerCore *core = this->value(QLatin1String("installer")).value<PackageManagerCore*>()) {
        connect(core, SIGNAL(statusChanged(QInstaller::PackageManagerCore::Status)), &callback,
            SLOT(statusChanged(QInstaller::PackageManagerCore::Status)));
    }

    //Runnable is derived from QRunable which will be deleted by the ThreadPool -> no parent is needed
    Runnable *runnable = new Runnable(archivePath, targetDir, &callback);
    connect(runnable, SIGNAL(finished(bool,QString)), &receiver, SLOT(runnableFinished(bool,QString)),
        Qt::QueuedConnection);

    QEventLoop loop;
    connect(&receiver, SIGNAL(finished()), &loop, SLOT(quit()));
    if (QThreadPool::globalInstance()->tryStart(runnable)) {
        loop.exec();
    } else {
        // in case there is no availabe thread we should call it directly this is more a hack
        runnable->run();
        receiver.runnableFinished(true, QString());
    }

    typedef QPair<QString, QString> StringPair;
    QVector<StringPair> backupFiles = callback.backupFiles;

    //TODO use backups for rollback, too? doesn't work for uninstallation though

    //delete all backups we can delete right now, remember the rest
    foreach (const StringPair &i, backupFiles)
        deleteFileNowOrLater(i.second);

    if (!receiver.success) {
        setError(UserDefinedError);
        setErrorString(receiver.errorString);
        return false;
    }
    return true;
}

bool ExtractArchiveOperation::undoOperation()
{
    Q_ASSERT(arguments().count() == 2);
    //const QString archivePath = arguments().first();
    //const QString targetDir = arguments().last();

    const QStringList files = value(QLatin1String("files")).toStringList();

    WorkerThread *const thread = new WorkerThread(this, files);
    connect(thread, SIGNAL(currentFileChanged(QString)), this, SIGNAL(outputTextChanged(QString)));
    connect(thread, SIGNAL(progressChanged(double)), this, SIGNAL(progressChanged(double)));

    QEventLoop loop;
    connect(thread, SIGNAL(finished()), &loop, SLOT(quit()), Qt::QueuedConnection);
    thread->start();
    loop.exec();
    thread->deleteLater();
    return true;
}

bool ExtractArchiveOperation::testOperation()
{
    return true;
}

Operation *ExtractArchiveOperation::clone() const
{
    return new ExtractArchiveOperation();
}

/*!
    This slot is direct connected to the caller so please don't call it from another thread in the same time.
*/
void ExtractArchiveOperation::fileFinished(const QString &filename)
{
    QStringList files = value(QLatin1String("files")).toStringList();
    files.prepend(filename);
    setValue(QLatin1String("files"), files);
    emit outputTextChanged(filename);
}
