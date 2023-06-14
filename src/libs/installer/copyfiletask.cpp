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
#include "copyfiletask.h"
#include "observer.h"

#include <QDir>
#include <QFileInfo>
#include <QTemporaryFile>

namespace QInstaller {

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::CopyFileTask
    \internal
*/

CopyFileTask::CopyFileTask(const FileTaskItem &item)
    : AbstractFileTask(item)
{
}

CopyFileTask::CopyFileTask(const QString &source)
    : AbstractFileTask(source)
{
}

CopyFileTask::CopyFileTask(const QString &source, const QString &target)
    : AbstractFileTask(source, target)
{
}

void CopyFileTask::doTask(QFutureInterface<FileTaskResult> &fi)
{
    fi.reportStarted();
    fi.setExpectedResultCount(1);

    if (taskItems().isEmpty()) {
        fi.reportException(TaskException(tr("Invalid task item count.")));
        fi.reportFinished(); return;    // error
    }

    const FileTaskItem item = taskItems().first();
    FileTaskObserver observer(QCryptographicHash::Sha1);

    QFile source(item.source());
    if (!source.open(QIODevice::ReadOnly)) {
        fi.reportException(TaskException(tr("Cannot open file \"%1\" for reading: %2")
            .arg(QDir::toNativeSeparators(source.fileName()), source.errorString())));
        fi.reportFinished(); return;    // error
    }
    observer.setBytesToTransfer(source.size());

    QScopedPointer<QFile> file;
    const QString target = item.target();
    if (target.isEmpty()) {
        QTemporaryFile *tmp = new QTemporaryFile;
        tmp->setAutoRemove(false);
        file.reset(tmp);
    } else {
        file.reset(new QFile(target));
    }
    if (!file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        fi.reportException(TaskException(tr("Cannot open file \"%1\" for writing: %2")
            .arg(QDir::toNativeSeparators(file->fileName()), file->errorString())));
        fi.reportFinished(); return;    // error
    }

    QByteArray buffer(32768, Qt::Uninitialized);
    while (!source.atEnd() && source.error() == QFile::NoError) {
        if (fi.isCanceled())
            break;
        if (fi.isPaused())
            fi.waitForResume();

        const qint64 read = source.read(buffer.data(), buffer.size());
        qint64 written = 0;
        while (written < read) {
            const qint64 toWrite = file->write(buffer.constData() + written, read - written);
            if (toWrite < 0) {
                fi.reportException(TaskException(tr("Writing to file \"%1\" failed: %2")
                    .arg(QDir::toNativeSeparators(file->fileName()), file->errorString())));
            }
            written += toWrite;
        }

        observer.addSample(read);
        observer.timerEvent(nullptr);
        observer.addBytesTransfered(read);
        observer.addCheckSumData(buffer.left(read));

        fi.setProgressValueAndText(observer.progressValue(), observer.progressText());
    }

    fi.reportResult(FileTaskResult(file->fileName(), observer.checkSum(), item, false), 0);
    fi.reportFinished();
}

}   // namespace QInstaller
