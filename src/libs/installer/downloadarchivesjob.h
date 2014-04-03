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

#ifndef DOWNLOADARCHIVESJOB_H
#define DOWNLOADARCHIVESJOB_H

#include <kdjob.h>

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

class DownloadArchivesJob : public KDJob
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
