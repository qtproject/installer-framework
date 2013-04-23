/**************************************************************************
**
** Copyright (C) 2012-2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
**************************************************************************/
#ifndef EXTRACTARCHIVEOPERATION_P_H
#define EXTRACTARCHIVEOPERATION_P_H

#include "extractarchiveoperation.h"

#include "fileutils.h"
#include "lib7z_facade.h"
#include "packagemanagercore.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QPair>
#include <QtCore/QThread>
#include <QtCore/QVector>

namespace QInstaller {

class WorkerThread : public QThread
{
    Q_OBJECT
public:
    WorkerThread(ExtractArchiveOperation *op, const QStringList &files, QObject *parent = 0)
        : QThread(parent)
        , m_files(files)
        , m_op(op)
    {
    }

    void run()
    {
        ExtractArchiveOperation *const op = m_op;//dynamic_cast< ExtractArchiveOperation* >(parent());
        Q_ASSERT(op != 0);

        foreach (const QString &file, m_files) {
            const QFileInfo fi(file);
            emit outputTextChanged(file);
            if (fi.isFile() || fi.isSymLink()) {
                op->deleteFileNowOrLater(fi.absoluteFilePath());
            } else if (fi.isDir()) {
                const QDir d = fi.dir();
                removeSystemGeneratedFiles(file);
                d.rmdir(file); // directory may not exist
            }
        }
    }

Q_SIGNALS:
    void outputTextChanged(const QString &filename);

private:
    QStringList m_files;
    ExtractArchiveOperation *m_op;
};


class ExtractArchiveOperation::Callback : public QObject, public Lib7z::ExtractCallback
{
    Q_OBJECT

public:
    HRESULT state;
    bool createBackups;
    QVector<QPair<QString, QString> > backupFiles;

    Callback() : state(S_OK), createBackups(true) {}

Q_SIGNALS:
    void progressChanged(const QString &filename);

public Q_SLOTS:
    void statusChanged(QInstaller::PackageManagerCore::Status status)
    {
        switch(status) {
            case PackageManagerCore::Canceled:
                state = E_ABORT;
                break;
            case PackageManagerCore::Failure:
                state = E_FAIL;
                break;
            case PackageManagerCore::Unfinished: // fall through
            case PackageManagerCore::Success:
            case PackageManagerCore::Running:
                //state = S_OK;
                break;
        }
    }

protected:
    void setCurrentFile(const QString &filename)
    {
        emit progressChanged(QDir::toNativeSeparators(filename));
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
        if (!createBackups)
            return true;
        if (!QFile::exists(filename))
            return true;
        const QString backup = generateBackupName(filename);
        QFile f(filename);
        const bool renamed = f.rename(backup);
        if (f.exists() && !renamed) {
            qCritical("Could not rename %s to %s: %s", qPrintable(filename), qPrintable(backup),
                qPrintable(f.errorString()));
            return false;
        }
        backupFiles.push_back(qMakePair(filename, backup));
        return true;
    }

    HRESULT setCompleted(quint64 /*completed*/, quint64 /*total*/)
    {
        return state;
    }
};

class ExtractArchiveOperation::Runnable : public QObject, public QRunnable
{
    Q_OBJECT

public:
    Runnable(const QString &archivePath_, const QString &targetDir_, ExtractArchiveOperation::Callback *callback_)
        : QObject()
        , QRunnable()
        , archivePath(archivePath_)
        , targetDir(targetDir_)
        , callback(callback_) {}

    void run()
    {
        QFile archive(archivePath);
        if (!archive.open(QIODevice::ReadOnly)) {

            emit finished(false, tr("Could not open %1 for reading: %2.").arg(archivePath, archive.errorString()));
            return;
        }

        try {
            Lib7z::extractArchive(&archive, targetDir, callback);
            emit finished(true, QString());
        } catch (const Lib7z::SevenZipException& e) {
            emit finished(false, tr("Error while extracting '%1': %2").arg(archivePath, e.message()));
        } catch (...) {
            emit finished(false, tr("Unknown exception caught while extracting %1.").arg(archivePath));
        }
    }

Q_SIGNALS:
    void finished(bool success, const QString &errorString);

private:
    const QString archivePath;
    const QString targetDir;
    ExtractArchiveOperation::Callback *const callback;
};


class ExtractArchiveOperation::Receiver : public QObject
{
    Q_OBJECT
public:
    explicit Receiver(QObject *parent = 0)
        : QObject(parent)
        , success(false) {}

public Q_SLOTS:
    void runnableFinished(bool ok, const QString &msg)
    {
        success = ok;
        errorString = msg;
        emit finished();
    }

Q_SIGNALS:
    void finished();

public:
    bool success;
    QString errorString;
};

}

#endif // EXTRACTARCHIVEOPERATION_P_H
