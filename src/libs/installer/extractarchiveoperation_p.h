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
#ifndef EXTRACTARCHIVEOPERATION_P_H
#define EXTRACTARCHIVEOPERATION_P_H

#include "extractarchiveoperation.h"

#include "fileutils.h"
#include "lib7z_extract.h"
#include "lib7z_facade.h"
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

class ExtractArchiveOperation::Callback : public QObject, public Lib7z::ExtractCallback
{
    Q_OBJECT
    Q_DISABLE_COPY(Callback)

public:
    Callback() = default;

    BackupFiles backupFiles() const {
        return m_backupFiles;
    }

public slots:
    void statusChanged(QInstaller::PackageManagerCore::Status status)
    {
        switch(status) {
            case PackageManagerCore::Canceled:
                m_state = E_ABORT;
                break;
            case PackageManagerCore::Failure:
                m_state = E_FAIL;
                break;
            default:    // ignore all other status values
                break;
        }
    }

signals:
    void currentFileChanged(const QString &filename);
    void progressChanged(double progress);

private:
    void setCurrentFile(const QString &filename) Q_DECL_OVERRIDE
    {
        emit currentFileChanged(QDir::toNativeSeparators(filename));
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

    bool prepareForFile(const QString &filename) Q_DECL_OVERRIDE
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

    HRESULT setCompleted(quint64 completed, quint64 total) Q_DECL_OVERRIDE
    {
        emit progressChanged(double(completed) / total);
        return m_state;
    }

private:
    HRESULT m_state = S_OK;
    BackupFiles m_backupFiles;
};

class ExtractArchiveOperation::Runnable : public QObject, public QRunnable
{
    Q_OBJECT
    Q_DISABLE_COPY(Runnable)

public:
    Runnable(const QString &archivePath, const QString &targetDir,
            ExtractArchiveOperation::Callback *callback)
        : m_archivePath(archivePath)
        , m_targetDir(targetDir)
        , m_callback(callback)
    {}

    void run()
    {
        QFile archive(m_archivePath);
        if (!archive.open(QIODevice::ReadOnly)) {
            emit finished(false, tr("Cannot open archive \"%1\" for reading: %2").arg(m_archivePath,
                archive.errorString()));
            return;
        }

        try {
            Lib7z::extractArchive(&archive, m_targetDir, m_callback);
            emit finished(true, QString());
        } catch (const Lib7z::SevenZipException& e) {
            emit finished(false, tr("Error while extracting archive \"%1\": %2").arg(m_archivePath,
                e.message()));
        } catch (...) {
            emit finished(false, tr("Unknown exception caught while extracting \"%1\".")
                .arg(m_archivePath));
        }
    }

signals:
    void finished(bool success, const QString &errorString);

private:
    QString m_archivePath;
    QString m_targetDir;
    ExtractArchiveOperation::Callback *m_callback;
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
    void runnableFinished(bool ok, const QString &msg)
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
