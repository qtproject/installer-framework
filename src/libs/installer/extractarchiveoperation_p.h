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
#ifndef EXTRACTARCHIVEOPERATION_P_H
#define EXTRACTARCHIVEOPERATION_P_H

#include "extractarchiveoperation.h"

#include "fileutils.h"
#include "archivefactory.h"
#include "packagemanagercore.h"

#include <QRunnable>
#include <QThread>

namespace QInstaller {

class WorkerThread : public QThread
{
    Q_OBJECT
    Q_DISABLE_COPY(WorkerThread)

public:
    WorkerThread(ExtractArchiveOperation *op, const QStringList &files)
        : m_files(files)
        , m_op(op)
    {
        setObjectName(QLatin1String("ExtractArchive"));
    }

    void run()
    {
        Q_ASSERT(m_op != 0);

        int removedCounter = 0;
        foreach (const QString &file, m_files) {
            removedCounter++;

            const QFileInfo fi(file);
            emit currentFileChanged(QDir::toNativeSeparators(file));
            emit progressChanged(double(removedCounter) / m_files.count());
            if (fi.isFile() || fi.isSymLink()) {
                m_op->deleteFileNowOrLater(fi.absoluteFilePath());
            } else if (fi.isDir()) {
                removeSystemGeneratedFiles(file);
                fi.dir().rmdir(file); // directory may not exist
            }
        }
    }

signals:
    void currentFileChanged(const QString &filename);
    void progressChanged(double);

private:
    QStringList m_files;
    ExtractArchiveOperation *m_op;
};

typedef QPair<QString, QString> Backup;
typedef QVector<Backup> BackupFiles;

class ExtractArchiveOperation::Callback : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(Callback)

public:
    Callback() = default;

    BackupFiles backupFiles() const
    {
        return m_backupFiles;
    }

    QStringList extractedFiles() const
    {
        return m_extractedFiles;
    }

    static QString generateBackupName(const QString &fn)
    {
        const QString bfn = fn + QLatin1String(".tmpUpdate");
        QString res = bfn;
        int i = 0;
        while (QFile::exists(res))
            res = bfn + QString::fromLatin1(".%1").arg(i++);
        return res;
    }

    bool prepareForFile(const QString &filename)
    {
        if (!QFile::exists(filename))
            return true;
        const QString backup = generateBackupName(filename);
        QFile f(filename);
        const bool renamed = f.rename(backup);
        if (f.exists() && !renamed) {
            qCritical("Cannot rename %s to %s: %s", qPrintable(filename), qPrintable(backup),
                qPrintable(f.errorString()));
            return false;
        }
        m_backupFiles.append(qMakePair(filename, backup));
        return true;
    }

Q_SIGNALS:
    void progressChanged(double progress);

public Q_SLOTS:
    void onCurrentEntryChanged(const QString &filename)
    {
        m_extractedFiles.prepend(QDir::toNativeSeparators(filename));
    }

    void onCompletedChanged(quint64 completed, quint64 total)
    {
        emit progressChanged(double(completed) / total);
    }

private:
    BackupFiles m_backupFiles;
    QStringList m_extractedFiles;
};

class ExtractArchiveOperation::Worker : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(Worker)

public:
    Worker(const QString &archivePath, const QString &targetDir, Callback *callback)
        : m_archivePath(archivePath)
        , m_targetDir(targetDir)
        , m_canceled(false)
        , m_callback(callback)
    {}

Q_SIGNALS:
    void finished(bool success, const QString &errorString);

public Q_SLOTS:
    void run()
    {
        m_canceled = false;
        m_archive.reset(ArchiveFactory::instance().create(m_archivePath));
        if (!m_archive) {
            emit finished(false, tr("Could not create handler object for archive \"%1\": \"%2\".")
                .arg(m_archivePath, QLatin1String(Q_FUNC_INFO)));
            return;
        }

        connect(m_archive.get(), &AbstractArchive::currentEntryChanged, m_callback, &Callback::onCurrentEntryChanged);
        connect(m_archive.get(), &AbstractArchive::completedChanged, m_callback, &Callback::onCompletedChanged);

        if (!(m_archive->open(QIODevice::ReadOnly) && m_archive->isSupported())) {
            emit finished(false, tr("Cannot open archive \"%1\" for reading: %2").arg(m_archivePath,
                m_archive->errorString()));
            return;
        }
        const QVector<ArchiveEntry> entries = m_archive->list();
        if (entries.isEmpty()) {
            emit finished(false, tr("Error while reading contents of archive \"%1\": %2").arg(m_archivePath,
                m_archive->errorString()));
            return;
        }
        for (auto &entry : entries) {
            QString completeFilePath = m_targetDir + QDir::separator() + entry.path;
            if (!entry.isDirectory && !m_callback->prepareForFile(completeFilePath)) {
                emit finished(false, tr("Cannot prepare for file \"%1\"").arg(completeFilePath));
                return;
            }
        }
        if (m_canceled) {
            // For large archives the reading takes some time, and the user might have
            // canceled before we start the actual extracting.
            emit finished(false, tr("Extract for archive \"%1\" canceled.").arg(m_archivePath));
        } else if (!m_archive->extract(m_targetDir, entries.size())) {
            emit finished(false, tr("Error while extracting archive \"%1\": %2").arg(m_archivePath,
                m_archive->errorString()));
        } else {
            emit finished(true, QString());
        }
    }

    void onStatusChanged(PackageManagerCore::Status status)
    {
        if (!m_archive)
            return;

        switch (status) {
        case PackageManagerCore::Canceled:
            m_canceled = true;
            m_archive->cancel();
            break;
        case PackageManagerCore::Failure:
            m_canceled = true;
            m_archive->cancel();
            break;
        default: // ignore all other status values
            break;
        }
    }

private:
    QString m_archivePath;
    QString m_targetDir;
    QScopedPointer<AbstractArchive> m_archive;
    bool m_canceled;
    Callback *m_callback;
};

class ExtractArchiveOperation::Receiver : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(Receiver)

public:
    Receiver() = default;

    bool success() const {
        return m_success;
    }

    QString errorString() const {
        return m_errorString;
    }

public slots:
    void workerFinished(bool ok, const QString &msg)
    {
        m_success = ok;
        m_errorString = msg;
        emit finished();
    }

signals:
    void finished();

private:
    bool m_success = false;
    QString m_errorString;
};

}

#endif // EXTRACTARCHIVEOPERATION_P_H
