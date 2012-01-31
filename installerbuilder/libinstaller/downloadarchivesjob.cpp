/**************************************************************************
**
** This file is part of Qt SDK**
**
** Copyright (c) 2012 Nokia Corporation and/or its subsidiary(-ies).*
**
** Contact:  Nokia Corporation qt-info@nokia.com**
**
** No Commercial Usage
**
** This file contains pre-release code and may not be distributed.
** You may use this file in accordance with the terms and conditions
** contained in the Technology Preview License Agreement accompanying
** this package.
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
** rights. These rights are described in the Nokia Qt LGPL Exception version
** 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** If you are unsure which license is appropriate for your use, please contact
** (qt-info@nokia.com).
**
**************************************************************************/
#include "downloadarchivesjob.h"

#include "common/binaryformatenginehandler.h"
#include "component.h"
#include "cryptosignatureverifier.h"
#include "messageboxhandler.h"
#include "packagemanagercore.h"

#include <kdupdaterfiledownloader.h>
#include <kdupdaterfiledownloaderfactory.h>

#include <QtCore/QFile>
#include <QtCore/QTimerEvent>

using namespace QInstaller;
using namespace KDUpdater;


/*!
    Creates a new DownloadArchivesJob with \a parent.
*/
DownloadArchivesJob::DownloadArchivesJob(const QByteArray &publicKey, PackageManagerCore *core)
    : KDJob(core),
      m_core(core),
      m_downloader(0),
      m_archivesDownloaded(0),
      m_archivesToDownloadCount(0),
      m_canceled(false),
      m_publicKey(publicKey),
      m_lastFileProgress(0),
      m_progressChangedTimerId(0)
{
    setCapabilities(Cancelable);
}

/*!
    Destroys the DownloadArchivesJob.
    All temporary files get deleted.
*/
DownloadArchivesJob::~DownloadArchivesJob()
{
    foreach (const QString &fileName, m_temporaryFiles) {
        QFile file(fileName);
        if (!file.remove())
            qWarning("Could not delete file %s: %s", qPrintable(fileName), qPrintable(file.errorString()));
    }

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
    if (m_downloader != 0)
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

        connect(m_downloader, SIGNAL(downloadCompleted()), this, SLOT(finishedHashDownload()),
            Qt::QueuedConnection);
        m_downloader->download();
    } else {
        QMetaObject::invokeMethod(this, "fetchNextArchive", Qt::QueuedConnection);
    }
}

void DownloadArchivesJob::finishedHashDownload()
{
    Q_ASSERT(m_downloader != 0);

    const QString tempFile = m_downloader->downloadedFileName();
    QFile sha1HashFile(tempFile);
    if (sha1HashFile.open(QFile::ReadOnly))
        m_currentHash = sha1HashFile.readAll();
    else
        finishWithError(tr("Downloading hash signature failed."));

    m_temporaryFiles.insert(tempFile);

    fetchNextArchive();
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

    if (m_downloader != 0)
        m_downloader->deleteLater();

    m_downloader = setupDownloader();
    if (!m_downloader) {
        m_archivesToDownload.removeFirst();
        QMetaObject::invokeMethod(this, "fetchNextArchive", Qt::QueuedConnection);
        return;
    }

    emit progressChanged(double(m_archivesDownloaded) / m_archivesToDownloadCount);
    connect(m_downloader, SIGNAL(downloadProgress(double)), this, SLOT(emitDownloadProgress(double)));
    connect(m_downloader, SIGNAL(downloadCompleted()), this, SLOT(registerFile()), Qt::QueuedConnection);

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
    Registers the just downloaded file in the intaller's file system.
*/
void DownloadArchivesJob::registerFile()
{
    Q_ASSERT(m_downloader != 0);

    ++m_archivesDownloaded;
    if (m_progressChangedTimerId) {
        killTimer(m_progressChangedTimerId);
        m_progressChangedTimerId = 0;
        emit progressChanged(double(m_archivesDownloaded) / m_archivesToDownloadCount);
    }

    const QString tempFile = m_downloader->downloadedFileName();
    if (m_core->testChecksum()) {
        QFile archiveFile(tempFile);
        if (archiveFile.open(QFile::ReadOnly)) {
            static QByteArray buffer(1024 * 1024, '\0');
            QCryptographicHash hash(QCryptographicHash::Sha1);
            while (true) {
                const qint64 numRead = archiveFile.read(buffer.data(), buffer.size());
                if (numRead <= 0)
                    break;
                hash.addData(buffer.constData(), numRead);
            }

            const QByteArray archiveHash = hash.result().toHex();
            if ((archiveHash != m_currentHash) && (!m_canceled)) {
                //TODO: Maybe we should try to download the file again automatically
                const QMessageBox::Button res =
                    MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(),
                    QLatin1String("DownloadError"), tr("Download Error"), tr("Hash verification while "
                    "downloading failed. This is a temporary error, please retry."),
                    QMessageBox::Retry | QMessageBox::Cancel, QMessageBox::Cancel);

                if (res == QMessageBox::Cancel) {
                    finishWithError(tr("Could not verify Hash"));
                    return;
                }

                fetchNextArchiveHash();
                return;
            }
        } else {
            finishWithError(tr("Could not open %1").arg(tempFile));
        }
    }

    m_temporaryFiles.insert(tempFile);
    const QPair<QString, QString> pair = m_archivesToDownload.takeFirst();
    QInstallerCreator::BinaryFormatEngineHandler::instance()->registerArchive(pair.first, tempFile);

    fetchNextArchiveHash();
}

void DownloadArchivesJob::downloadCanceled()
{
    emitFinishedWithError(KDJob::Canceled, m_downloader->errorString());
}

void DownloadArchivesJob::downloadFailed(const QString &error)
{
    if (m_canceled)
        return;

    const QMessageBox::StandardButton b =
        MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(),
        QLatin1String("archiveDownloadError"), tr("Download Error"), tr("Could not download archive: %1 : %2")
        .arg(m_archivesToDownload.first().second, error), QMessageBox::Retry | QMessageBox::Cancel);

    if (b == QMessageBox::Retry)
        QMetaObject::invokeMethod(this, "fetchNextArchiveHash", Qt::QueuedConnection);
    else
        downloadCanceled();
}

void DownloadArchivesJob::finishWithError(const QString &error)
{
    const FileDownloader *const dl = dynamic_cast<const FileDownloader*> (sender());
    const QString msg = tr("Could not fetch archives: %1\nError while loading %2");
    if (dl != 0)
        emitFinishedWithError(QInstaller::DownloadError, msg.arg(error, dl->url().toString()));
    else
        emitFinishedWithError(QInstaller::DownloadError, msg.arg(error, m_downloader->url().toString()));
}

KDUpdater::FileDownloader *DownloadArchivesJob::setupDownloader(const QString &prefix)
{
    KDUpdater::FileDownloader *downloader = 0;
    const QString targetUrl = m_archivesToDownload.first().second;
    const QString registerPath = m_archivesToDownload.first().first;

    const QFileInfo fi = QFileInfo(registerPath);
    const Component *const component = m_core->componentByName(QFileInfo(fi.path()).fileName());
    if (component) {
        const QUrl url(targetUrl + prefix);
        const QString &scheme = url.scheme();
        const CryptoSignatureVerifier verifier(m_publicKey);
        downloader = FileDownloaderFactory::instance().create(scheme, m_publicKey.isEmpty() ? 0 : &verifier,
            QUrl(targetUrl + QLatin1String(".sig")), this);

        if (downloader) {
            downloader->setUrl(url);
            downloader->setAutoRemoveDownloadedFile(false);

            QAuthenticator auth;
            auth.setUser(component->value(QLatin1String("username")));
            auth.setPassword(component->value(QLatin1String("password")));
            downloader->setAuthenticator(auth);

            connect(downloader, SIGNAL(downloadCanceled()), this, SLOT(downloadCanceled()));
            connect(downloader, SIGNAL(downloadAborted(QString)), this, SLOT(downloadFailed(QString)),
                Qt::QueuedConnection);
            connect(downloader, SIGNAL(downloadStatus(QString)), this, SIGNAL(downloadStatusChanged(QString)));

            if (scheme == QLatin1String("http") || scheme == QLatin1String("ftp") ||
                scheme == QLatin1String("file")) {
                    downloader->setDownloadedFileName(component->localTempPath() + QLatin1String("/")
                        + component->name() + QLatin1String("/") + fi.fileName() + prefix);
            }

            QString message = tr("Downloading archive hash for component: %1");
            if (prefix.isEmpty())
                message = tr("Downloading archive for component: %1");
            emit outputTextChanged(message.arg(component->displayName()));
        } else {
            emit outputTextChanged(tr("Scheme not supported: %1 (%2)").arg(scheme, url.toString()));
        }
    } else {
        emit outputTextChanged(tr("Could not find component for: %1.").arg(QFileInfo(fi.path()).fileName()));
    }
    return downloader;
}
