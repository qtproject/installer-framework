/**************************************************************************
**
** Copyright (C) 2022 The Qt Company Ltd.
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
#include "remoteclient.h"
#include "adminauthorization.h"
#include "utils.h"
#include "errors.h"
#include "loggingutils.h"

#include <QRunnable>
#include <QThread>

namespace QInstaller {

constexpr double scBackupProgressPart = 0.1;
constexpr double scPerformProgressPart = (1 - scBackupProgressPart);

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

    void run() override
    {
        Q_ASSERT(m_op != 0);

        QStringList directories;

        int removedCounter = 0;
        foreach (const QString &file, m_files) {
            removedCounter++;

            const QFileInfo fi(file);
            if (LoggingHandler::instance().verboseLevel() == LoggingHandler::Detailed)
                emit currentFileChanged(QDir::toNativeSeparators(file));
            emit progressChanged(double(removedCounter) / m_files.count());
            if (fi.isFile() || fi.isSymLink()) {
                m_op->deleteFileNowOrLater(fi.absoluteFilePath());
            } else if (fi.isDir()) {
                directories.append(file);
            }
        }

        std::sort(directories.begin(), directories.end(), [](const QString &lhs, const QString &rhs) {
            // Doesn't match the original creation order, nor will the sorted list be a logical
            // directory tree. Only requirement is that subdirectories get removed first.
            const int lhsParts = QDir::fromNativeSeparators(lhs)
                .split(QLatin1Char('/'), Qt::SkipEmptyParts).size();
            const int rhsParts = QDir::fromNativeSeparators(rhs)
                .split(QLatin1Char('/'), Qt::SkipEmptyParts).size();

            if (lhsParts == rhsParts)
                return lhs < rhs;

            return lhsParts > rhsParts;
        });

        for (auto &directory : qAsConst(directories)) {
            removeSystemGeneratedFiles(directory);
            QDir(directory).rmdir(directory); // directory may not exist
        }
    }

signals:
    void currentFileChanged(const QString &filename);
    void progressChanged(double);

private:
    QStringList m_files;
    ExtractArchiveOperation *m_op;
};

class ExtractArchiveOperation::Callback : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(Callback)

public:
    Callback()
        : m_lastProgressPercentage(0)
    {}

    QStringList extractedFiles() const
    {
        return m_extractedFiles;
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
        const double currentProgress = double(completed) / total
            * scPerformProgressPart + scBackupProgressPart;

        const int currentProgressPercentage = qRound(currentProgress * 100);
        if (currentProgressPercentage > m_lastProgressPercentage) {
            // Emit only full percentage changes
            m_lastProgressPercentage = currentProgressPercentage;
            emit progressChanged(currentProgress);
        }
    }

private:
    QStringList m_extractedFiles;
    int m_lastProgressPercentage;
};

class ExtractArchiveOperation::Worker : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(Worker)

public:
    Worker(const QString &archivePath, const QString &targetDir, quint64 totalEntries, Callback *callback)
        : m_archivePath(archivePath)
        , m_targetDir(targetDir)
        , m_totalEntries(totalEntries)
        , m_callback(callback)
    {}

Q_SIGNALS:
    void finished(bool success, const QString &errorString);

public Q_SLOTS:
    void run()
    {
        m_archive.reset(ArchiveFactory::instance().create(m_archivePath));
        if (!m_archive) {
            emit finished(false, tr("Could not create handler object for archive \"%1\": \"%2\".")
                .arg(m_archivePath, QLatin1String(Q_FUNC_INFO)));
            return;
        }

        connect(m_archive.get(), &AbstractArchive::currentEntryChanged, m_callback, &Callback::onCurrentEntryChanged);
        connect(m_archive.get(), &AbstractArchive::completedChanged, m_callback, &Callback::onCompletedChanged);

        if (!m_archive->open(QIODevice::ReadOnly)) {
            emit finished(false, tr("Cannot open archive \"%1\" for reading: %2").arg(m_archivePath,
                m_archive->errorString()));
            return;
        }

        if (!m_archive->extract(m_targetDir, m_totalEntries)) {
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
            m_archive->cancel();
            break;
        case PackageManagerCore::Failure:
            m_archive->cancel();
            break;
        default: // ignore all other status values
            break;
        }
    }

private:
    QString m_archivePath;
    QString m_targetDir;
    quint64 m_totalEntries;
    QScopedPointer<AbstractArchive> m_archive;
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
