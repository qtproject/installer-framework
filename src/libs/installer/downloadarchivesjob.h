/**************************************************************************
**
** This file is part of Installer Framework
**
** Copyright (c) 2010-2012 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (qt-info@nokia.com)
**
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** Other Usage
**
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
** If you have questions regarding the use of this file, please contact
** Nokia at qt-info@nokia.com.
**
**************************************************************************/

#ifndef DOWNLOADARCHIVESJOB_H
#define DOWNLOADARCHIVESJOB_H

#include <kdjob.h>

#include <QtCore/QPair>
#include <QtCore/QSet>

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
    explicit DownloadArchivesJob(PackageManagerCore *core = 0);
    ~DownloadArchivesJob();

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
    KDUpdater::FileDownloader *setupDownloader(const QString &prefix = QString());

private:
    PackageManagerCore *m_core;
    KDUpdater::FileDownloader *m_downloader;

    int m_archivesDownloaded;
    int m_archivesToDownloadCount;
    QList<QPair<QString, QString> > m_archivesToDownload;

    bool m_canceled;
    QSet<QString> m_temporaryFiles;
    QByteArray m_currentHash;
    double m_lastFileProgress;
    int m_progressChangedTimerId;
};

} // namespace QInstaller

#endif  // DOWNLOADARCHIVESJOB_H
