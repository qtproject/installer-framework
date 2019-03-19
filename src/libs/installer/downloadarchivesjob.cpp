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
#include "downloadarchivesjob.h"

#include "binaryformatenginehandler.h"
#include "component.h"
#include "messageboxhandler.h"
#include "packagemanagercore.h"
#include "utils.h"

#include "filedownloader.h"
#include "filedownloaderfactory.h"

#include <QtCore/QFile>
#include <QtCore/QTimerEvent>

using namespace QInstaller;
using namespace KDUpdater;


/*!
    Creates a new DownloadArchivesJob with \a parent.
*/
DownloadArchivesJob::DownloadArchivesJob(PackageManagerCore *core)
    : Job(core)
    , m_core(core)
    , m_downloader(nullptr)
    , m_archivesDownloaded(0)
    , m_archivesToDownloadCount(0)
    , m_canceled(false)
    , m_lastFileProgress(0)
    , m_progressChangedTimerId(0)
{
    setCapabilities(Cancelable);
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
    Sets the archives to download. The first value of each pair contains the file name to register
    the file in the installer's internal file system, the second one the source url.
*/
void DownloadArchivesJob::setArchivesToDownload(const QList<QPair<QString, QString> > &archives)
{
    m_archivesToDownload = archives;
    m_archivesToDownloadCount = archives.count();
}

/*!
    \reimp
*/
void DownloadArchivesJob::doStart()
{
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
    if (m_core->testChecksum()) {
        if (m_canceled) {
            finishWithError(tr("Canceled"));
            return;
        }

        if (m_archivesToDownload.isEmpty()) {
            emitFinished();
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
    Emits the global download progress during a single download in a lazy way (uses a timer to reduce to
    much processChanged).
*/
void DownloadArchivesJob::emitDownloadProgress(double progress)
{
    m_lastFileProgress = progress;
    if (!m_progressChangedTimerId)
        m_progressChangedTimerId = startTimer(5);
}

/*!
    This is used to reduce the progressChanged signals.
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
    Registers the just downloaded file in the installer's file system.
*/
void DownloadArchivesJob::registerFile()
{
    Q_ASSERT(m_downloader != nullptr);

    if (m_canceled)
        return;

    if (m_core->testChecksum() && m_currentHash != m_downloader->sha1Sum().toHex()) {
        //TODO: Maybe we should try to download the file again automatically
        const QMessageBox::Button res =
            MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(),
            QLatin1String("DownloadError"), tr("Download Error"), tr("Hash verification while "
            "downloading failed. This is a temporary error, please retry."),
            QMessageBox::Retry | QMessageBox::Cancel, QMessageBox::Cancel);

        if (res == QMessageBox::Cancel) {
            finishWithError(tr("Cannot verify Hash"));
            return;
        }
    } else {
        ++m_archivesDownloaded;
        if (m_progressChangedTimerId) {
            killTimer(m_progressChangedTimerId);
            m_progressChangedTimerId = 0;
            emit progressChanged(double(m_archivesDownloaded) / m_archivesToDownloadCount);
        }

        const QPair<QString, QString> pair = m_archivesToDownload.takeFirst();
        BinaryFormatEngineHandler::instance()->registerResource(pair.first,
            m_downloader->downloadedFileName());
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
        .arg(m_archivesToDownload.first().second, error), QMessageBox::Retry | QMessageBox::Cancel);

    if (b == QMessageBox::Retry)
        QMetaObject::invokeMethod(this, "fetchNextArchiveHash", Qt::QueuedConnection);
    else
        downloadCanceled();
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
    const QFileInfo fi = QFileInfo(m_archivesToDownload.first().first);
    const Component *const component = m_core->componentByName(PackageManagerCore::checkableName(QFileInfo(fi.path()).fileName()));
    if (component) {
        QString fullQueryString;
        if (!queryString.isEmpty())
            fullQueryString = QLatin1String("?") + queryString;
        const QUrl url(m_archivesToDownload.first().second + suffix + fullQueryString);
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
            connect(downloader, &FileDownloader::downloadStatus, this, &DownloadArchivesJob::downloadStatusChanged);

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
