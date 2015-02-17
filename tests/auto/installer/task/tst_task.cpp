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

#include <copyfiletask.h>
#include <downloadfiletask.h>
#include <fileio.h>

#include <QFutureWatcher>
#include <QSignalSpy>
#include <QTest>
#include <QTemporaryFile>

using namespace QInstaller;

static const qint64 scLargeSize = 4194304LL;

class tst_Task : public QObject
{
    Q_OBJECT

private slots:
    void copyFile()
    {
        QTemporaryFile file;
        file.setAutoRemove(false);

        if (file.open()) {
            const QString filename = file.fileName();
            QInstaller::blockingWrite(&file, QByteArray(scLargeSize, '1'));
            file.close();

            CopyFileTask fileTask(filename);
            QFutureWatcher<FileTaskResult> watcher;

            QSignalSpy started(&watcher, SIGNAL(started()));
            QSignalSpy finished(&watcher, SIGNAL(finished()));
            QSignalSpy progress(&watcher, SIGNAL(progressValueChanged(int)));

            watcher.setFuture(QtConcurrent::run(&CopyFileTask::doTask, &fileTask));

            watcher.waitForFinished();
            QTest::qWait(10); // Spin the event loop to deliver queued signals.

            QCOMPARE(started.count(), 1);
            QCOMPARE(finished.count(), 1);

            FileTaskResult result = watcher.result();
            QCOMPARE(watcher.future().resultCount(), 1);

            QVERIFY(QFile(result.target()).exists());
            QCOMPARE(file.size(), QFile(result.target()).size());
            QCOMPARE(result.checkSum().toHex(), QByteArray("85304f87b8d90554a63c6f6d1e9cc974fbef8d32"));
        }
    }

    void downloadFile()
    {
        QTemporaryFile file;
        file.setAutoRemove(false);

        if (file.open()) {
            const QString filename = file.fileName();
            QInstaller::blockingWrite(&file, QByteArray(scLargeSize, '1'));
            file.close();

            DownloadFileTask fileTask(QLatin1String("file:///") + filename);
            QFutureWatcher<FileTaskResult> watcher;

            QSignalSpy started(&watcher, SIGNAL(started()));
            QSignalSpy finished(&watcher, SIGNAL(finished()));
            QSignalSpy progress(&watcher, SIGNAL(progressValueChanged(int)));

            watcher.setFuture(QtConcurrent::run(&DownloadFileTask::doTask, &fileTask));

            watcher.waitForFinished();
            QTest::qWait(10); // Spin the event loop to deliver queued signals.

            QCOMPARE(started.count(), 1);
            QCOMPARE(finished.count(), 1);

            FileTaskResult result = watcher.result();
            QCOMPARE(watcher.future().resultCount(), 1);

            QVERIFY(QFile(result.target()).exists());
            QCOMPARE(file.size(), QFile(result.target()).size());
            QCOMPARE(result.checkSum().toHex(), QByteArray("85304f87b8d90554a63c6f6d1e9cc974fbef8d32"));
        }
    }
};

QTEST_MAIN(tst_Task)

#include "tst_task.moc"
