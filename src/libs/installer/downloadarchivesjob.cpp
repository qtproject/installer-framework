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
#include "downloadarchivesjob.h"

#include "binaryformatenginehandler.h"
#include "component.h"
#include "messageboxhandler.h"
#include "packagemanagercore.h"
#include "utils.h"
#include "fileutils.h"

#include "filedownloader.h"
#include "filedownloaderfactory.h"

#include <QtCore/QFile>
#include <QtCore/QTimerEvent>

using namespace QInstaller;
using namespace KDUpdater;


static constexpr uint scMaxRetries = 5;

/*!
    Creates a new DownloadArchivesJob with parent \a core.
*/
DownloadArchivesJob::DownloadArchivesJob(PackageManagerCore *core, const QString &objectName)
    : Job(core)
    , m_core(core)
    , m_downloader(nullptr)
    , m_archivesDownloaded(0)
    , m_archivesToDownloadCount(0)
    , m_canceled(false)
    , m_lastFileProgress(0)
    , m_progressChangedTimerId(0)
    , m_totalSizeToDownload(0)
    , m_totalSizeDownloaded(0)
    , m_retryCount(scMaxRetries)
{
    setCapabilities(Cancelable);
    setObjectName(objectName);
}

/*!
    Destroys the DownloadArchivesJob.
*/
DownloadArchivesJob::~DownloadArchivesJob()
{
    if (m_downloader)
        m_downloader->deleteLater();
}

/*!
    Sets the \a archives to download. The first value of each pair contains the file name to register
    the file in the installer's internal file system, the second one the source url.
*/
void DownloadArchivesJob::setArchivesToDownload(const QList<PackageManagerCore::DownloadItem> &archives)
{
    m_archivesToDownload = archives;
    m_archivesToDownloadCount = archives.count();
}

/*!
    Sets the expected total size of archives to download to \a total.
*/
void DownloadArchivesJob::setExpectedTotalSize(quint64 total)
{
    m_totalSizeToDownload = total;
}

/*!
    \reimp
*/
void DownloadArchivesJob::doStart()
{
    m_totalDownloadSpeedTimer.start();
    m_archivesDownloaded = 0;
    fetchNextArchiveHash();
}

/*!
    \reimp
*/
void DownloadArchivesJob::doCancel()
{
    m_canceled = true;
    if (m_downloader != nullptr)
        m_downloader->cancelDownload();
}

void DownloadArchivesJob::fetchNextArchiveHash()
{
    if (m_archivesToDownload.isEmpty()) {
        emitFinished();
        return;
    }

    if (m_archivesToDownload.first().checkSha1CheckSum) {
        if (m_canceled) {
            finishWithError(tr("Canceled"));
            return;
        }

        if (m_downloader)
            m_downloader->deleteLater();

        m_downloader = setupDownloader(QLatin1String(".sha1"));
        if (!m_downloader) {
            m_archivesToDownload.removeFirst();
            QMetaObject::invokeMethod(this, "fetchNextArchiveHash", Qt::QueuedConnection);
            return;
        }

        connect(m_downloader, &FileDownloader::downloadCompleted,
                this, &DownloadArchivesJob::finishedHashDownload, Qt::QueuedConnection);
        m_downloader->download();
    } else {
        QMetaObject::invokeMethod(this, "fetchNextArchive", Qt::QueuedConnection);
    }
}

void DownloadArchivesJob::finishedHashDownload()
{
    Q_ASSERT(m_downloader != nullptr);

    QFile sha1HashFile(m_downloader->downloadedFileName());
    if (sha1HashFile.open(QFile::ReadOnly)) {
        emit hashDownloadReady(m_downloader->downloadedFileName());
        m_currentHash = sha1HashFile.readAll();
        fetchNextArchive();
    } else {
        finishWithError(tr("Downloading hash signature failed."));
    }
}

/*!
    Fetches the next archive and registers it in the installer.
*/
void DownloadArchivesJob::fetchNextArchive()
{
    if (m_canceled) {
        finishWithError(tr("Canceled"));
        return;
    }

    if (m_archivesToDownload.isEmpty()) {
        emitFinished();
        return;
    }

    if (m_downloader != nullptr)
        m_downloader->deleteLater();

    m_downloader = setupDownloader(QString(), m_core->value(scUrlQueryString));
    if (!m_downloader) {
        m_archivesToDownload.removeFirst();
        QMetaObject::invokeMethod(this, "fetchNextArchiveHash", Qt::QueuedConnection);
        return;
    }

    emit progressChanged(double(m_archivesDownloaded) / m_archivesToDownloadCount);
    connect(m_downloader, SIGNAL(downloadProgress(double)), this, SLOT(emitDownloadProgress(double)));
    connect(m_downloader, &FileDownloader::downloadCompleted,
            this, &DownloadArchivesJob::registerFile, Qt::QueuedConnection);

    m_downloader->download();
}

/*!
    Emits the global download \a progress during a single download in a lazy way (uses a timer to reduce to
    much processChanged).
*/
void DownloadArchivesJob::emitDownloadProgress(double progress)
{
    m_lastFileProgress = progress;
    if (!m_progressChangedTimerId)
        m_progressChangedTimerId = startTimer(5);
}

/*!
    This is used to reduce the \c progressChanged signals for \a event.
*/
void DownloadArchivesJob::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == m_progressChangedTimerId) {
        killTimer(m_progressChangedTimerId);
        m_progressChangedTimerId = 0;
        emit progressChanged((double(m_archivesDownloaded) + m_lastFileProgress) / m_archivesToDownloadCount);
    }
}

/*!
    Builds a textual representation of the total download \a status and
    emits the \c {downloadStatusChanged()} signal.
*/
void DownloadArchivesJob::onDownloadStatusChanged(const QString &status)
{
    if (!m_downloader || m_canceled) {
        emit downloadStatusChanged(status);
        return;
    }

    QString extendedStatus;
    quint64 currentDownloaded = m_totalSizeDownloaded + m_downloader->getBytesReceived();
    if (m_totalSizeToDownload > 0) {
        QString bytesReceived = humanReadableSize(currentDownloaded);
        const QString bytesToReceive = humanReadableSize(m_totalSizeToDownload);

        // remove the unit from the bytesReceived value if bytesToReceive has the same
        const QString tmp = bytesToReceive.mid(bytesToReceive.indexOf(QLatin1Char(' ')));
        if (bytesReceived.endsWith(tmp))
            bytesReceived.chop(tmp.length());

        extendedStatus = tr("%1 of %2").arg(bytesReceived, bytesToReceive);
    } else if (currentDownloaded > 0) {
        extendedStatus = tr("%1 downloaded.").arg(humanReadableSize(currentDownloaded));
    }

    const quint64 totalDownloadSpeed = currentDownloaded
        / double(m_totalDownloadSpeedTimer.elapsed() / 1000);

    if (m_totalSizeToDownload > 0 && totalDownloadSpeed > 0) {
        const qint64 time = (m_totalSizeToDownload - currentDownloaded) / totalDownloadSpeed;

        int s = time % 60;
        const int d = time / 86400;
        const int h = (time / 3600) - (d * 24);
        const int m = (time / 60) - (d * 1440) - (h * 60);

        QString days;
        if (d > 0)
            days = tr("%n day(s), ", "", d);

        QString hours;
        if (h > 0)
            hours = tr("%n hour(s), ", "", h);

        QString minutes;
        if (m > 0)
            minutes = tr("%n minute(s)", "", m);

        QString seconds;
        if (s >= 0 && minutes.isEmpty()) {
            s = (s <= 0 ? 1 : s);
            seconds = tr("%n second(s)", "", s);
        }
        extendedStatus += tr(" - %1%2%3%4 remaining.").arg(days, hours, minutes, seconds);
    } else {
        extendedStatus += tr(" - unknown time remaining.");
    }

    emit downloadStatusChanged(tr("Archive: ") + status
        + QLatin1String("<br>") + tr("Total: ")+ extendedStatus);
}

/*!
    Registers the just downloaded file in the installer's file system.
*/
void DownloadArchivesJob::registerFile()
{
    Q_ASSERT(m_downloader != nullptr);

    if (m_canceled || m_archivesToDownload.isEmpty())
        return;

    if (m_archivesToDownload.first().checkSha1CheckSum && m_currentHash != m_downloader->sha1Sum().toHex()) {
        //TODO: Maybe we should try to download the file again automatically
        const QMessageBox::Button res =
            MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(),
            QLatin1String("DownloadError"), tr("Download Error"), tr("Hash verification while "
            "downloading failed. This is a temporary error, please retry.\n\n"
            "Expected: %1 \nDownloaded: %2").arg(QString::fromLatin1(m_currentHash), QString::fromLatin1(m_downloader->sha1Sum().toHex())),
            QMessageBox::Retry | QMessageBox::Cancel, QMessageBox::Retry);

        if (res == QMessageBox::Cancel) {
            finishWithError(tr("Cannot verify Hash\nExpected: %1 \nDownloaded: %2")
                .arg(QString::fromLatin1(m_currentHash), QString::fromLatin1(m_downloader->sha1Sum().toHex())));
            return;
        }
        // When using command line instance, only retry a number of times to avoid
        // infinite loop in case the automatic answer for the messagebox is "Retry"
        if (m_core->isCommandLineInstance() && (--m_retryCount == 0)) {
           finishWithError(tr("Retry count (%1) exceeded").arg(scMaxRetries));
           return;
        }
    } else {
        m_retryCount = scMaxRetries;

        ++m_archivesDownloaded;
        m_totalSizeDownloaded += QFile(m_downloader->downloadedFileName()).size();
        if (m_progressChangedTimerId) {
            killTimer(m_progressChangedTimerId);
            m_progressChangedTimerId = 0;
            emit progressChanged(double(m_archivesDownloaded) / m_archivesToDownloadCount);
        }

        const PackageManagerCore::DownloadItem item = m_archivesToDownload.takeFirst();
        BinaryFormatEngineHandler::instance()->registerResource(item.fileName,
            m_downloader->downloadedFileName());

        emit fileDownloadReady(m_downloader->downloadedFileName());
    }
    fetchNextArchiveHash();
}

void DownloadArchivesJob::downloadCanceled()
{
    emitFinishedWithError(Job::Canceled, m_downloader->errorString());
}

void DownloadArchivesJob::downloadFailed(const QString &error)
{
    if (m_canceled)
        return;

    const QMessageBox::StandardButton b =
        MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(),
        QLatin1String("archiveDownloadError"), tr("Download Error"), tr("Cannot download archive %1: %2")
        .arg(m_archivesToDownload.first().sourceUrl, error), QMessageBox::Retry | QMessageBox::Cancel,
        QMessageBox::Retry);

    if (b == QMessageBox::Retry) {
        // When using command line instance, only retry a number of times to avoid
        // infinite loop in case the automatic answer for the messagebox is "Retry"
         if (m_core->isCommandLineInstance() && (--m_retryCount == 0)) {
            finishWithError(tr("Retry count (%1) exceeded").arg(scMaxRetries));
            return;
         }

        QMetaObject::invokeMethod(this, "fetchNextArchiveHash", Qt::QueuedConnection);
    } else {
        downloadCanceled();
    }
}

void DownloadArchivesJob::finishWithError(const QString &error)
{
    const FileDownloader *const dl = qobject_cast<const FileDownloader*> (sender());
    const QString msg = tr("Cannot fetch archives: %1\nError while loading %2");
    if (dl != nullptr)
        emitFinishedWithError(QInstaller::DownloadError, msg.arg(error, dl->url().toString()));
    else
        emitFinishedWithError(QInstaller::DownloadError, msg.arg(error, m_downloader->url().toString()));
}

KDUpdater::FileDownloader *DownloadArchivesJob::setupDownloader(const QString &suffix, const QString &queryString)
{
    KDUpdater::FileDownloader *downloader = nullptr;
    const QFileInfo fi = QFileInfo(m_archivesToDownload.first().fileName);
    const Component *const component = m_core->componentByName(PackageManagerCore::checkableName(QFileInfo(fi.path()).fileName()));
    if (component) {
        QString fullQueryString;
        if (!queryString.isEmpty())
            fullQueryString = QLatin1String("?") + queryString;
        const QUrl url(m_archivesToDownload.first().sourceUrl + suffix + fullQueryString);
        const QString &scheme = url.scheme();
        downloader = FileDownloaderFactory::instance().create(scheme, this);

        if (downloader) {
            downloader->setUrl(url);
            downloader->setAutoRemoveDownloadedFile(false);

            QAuthenticator auth;
            auth.setUser(component->value(QLatin1String("username")));
            auth.setPassword(component->value(QLatin1String("password")));
            downloader->setAuthenticator(auth);

            connect(downloader, &FileDownloader::downloadCanceled, this, &DownloadArchivesJob::downloadCanceled);
            connect(downloader, &FileDownloader::downloadAborted, this, &DownloadArchivesJob::downloadFailed,
                Qt::QueuedConnection);
            connect(downloader, &FileDownloader::downloadStatus, this, &DownloadArchivesJob::onDownloadStatusChanged);

            if (FileDownloaderFactory::isSupportedScheme(scheme)) {
                downloader->setDownloadedFileName(component->localTempPath() + QLatin1Char('/')
                    + component->name() + QLatin1Char('/') + fi.fileName() + suffix);
            }

            emit outputTextChanged(tr("Downloading archive \"%1\" for component %2.")
                .arg(fi.fileName() + suffix, component->displayName()));
        } else {
            emit outputTextChanged(tr("Scheme %1 not supported (URL: %2).").arg(scheme, url.toString()));
        }
    } else {
        emit outputTextChanged(tr("Cannot find component for %1.").arg(QFileInfo(fi.path()).fileName()));
    }
    return downloader;
}
