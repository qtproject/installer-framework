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

#ifndef DOWNLOADARCHIVESJOB_H
#define DOWNLOADARCHIVESJOB_H

#include "job.h"

#include <QtCore/QPair>

QT_BEGIN_NAMESPACE
class QTimerEvent;
QT_END_NAMESPACE

namespace KDUpdater {
    class FileDownloader;
}

namespace QInstaller {

class MessageBoxHandler;
class PackageManagerCore;

class DownloadArchivesJob : public Job
{
    Q_OBJECT

public:
    explicit DownloadArchivesJob(PackageManagerCore *core);
    ~DownloadArchivesJob();

    int numberOfDownloads() const { return m_archivesDownloaded; }
    void setArchivesToDownload(const QList<QPair<QString, QString> > &archives);

Q_SIGNALS:
    void progressChanged(double progress);
    void outputTextChanged(const QString &progress);
    void downloadStatusChanged(const QString &status);

protected:
    void doStart();
    void doCancel();
    void timerEvent(QTimerEvent *event);

protected Q_SLOTS:
    void registerFile();
    void downloadCanceled();
    void downloadFailed(const QString &error);
    void finishWithError(const QString &error);
    void fetchNextArchive();
    void fetchNextArchiveHash();
    void finishedHashDownload();
    void emitDownloadProgress(double progress);

private:
    KDUpdater::FileDownloader *setupDownloader(const QString &suffix = QString(), const QString &queryString = QString());

private:
    PackageManagerCore *m_core;
    KDUpdater::FileDownloader *m_downloader;

    int m_archivesDownloaded;
    int m_archivesToDownloadCount;
    QList<QPair<QString, QString> > m_archivesToDownload;

    bool m_canceled;
    QByteArray m_currentHash;
    double m_lastFileProgress;
    int m_progressChangedTimerId;
};

} // namespace QInstaller

#endif  // DOWNLOADARCHIVESJOB_H
