
/* Copyright (C) 2026 The Qt Company Ltd.
 *
 * SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0
*/
#include "downloadfiletask.h"

#include "downloadfiletask_p.h"
#include "globals.h"
#include "loggingutils.h"
#include "productkeycheck.h"

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QHostAddress>
#include <QHostInfo>
#include <QNetworkProxyFactory>
#include <QSslError>
#include <QTemporaryFile>
#include <fileutils.h>

namespace QInstaller {

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::DownloadFileTask
    \internal
*/

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::Downloader
    \internal
*/

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::AuthenticationRequiredException
    \internal
*/

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::Data
    \internal
*/

AuthenticationRequiredException::AuthenticationRequiredException(Type type, const QString &message)
    : TaskException(message)
    , m_type(type)
{
}

Downloader::Downloader()
    : m_finished(0)
    , m_downloadDeadlineTimerInterval(30000)
    , m_downloadPaused(false)
    , m_downloadResumed(false)

{
}

Downloader::~Downloader()
{
    for (const auto &pair : m_downloads) {
        pair.first->disconnect();
        pair.first->abort();
        pair.first->deleteLater();
    }
}

void Downloader::download(QFutureInterface<FileTaskResult> &fi, const QList<FileTaskItem> &items,
    QNetworkProxyFactory *networkProxyFactory, const bool progressValueInBytes)
{
    m_items = items;
    m_futureInterface = &fi;
    m_progressValueInBytes = progressValueInBytes;

    fi.reportStarted();
    fi.setExpectedResultCount(items.count());

    m_nam.setProxyFactory(networkProxyFactory);
    connect(&m_nam, &QNetworkAccessManager::authenticationRequired, this,
        &Downloader::onAuthenticationRequired);
    connect(&m_nam, &QNetworkAccessManager::proxyAuthenticationRequired, this,
            &Downloader::onProxyAuthenticationRequired);

    auto netInfo = QNetworkInformation::instance();
    if (netInfo) {
        connect(netInfo, &QNetworkInformation::reachabilityChanged,
                this, &Downloader::onReachabilityChanged);
    }
    connect(&m_timer, &QTimer::timeout, this, &Downloader::onTimeout);
    QTimer::singleShot(0, this, &Downloader::doDownload);
}

void Downloader::doDownload()
{
    m_timer.start(1000); // Use a timer to check for canceled downloads.
    runDownloadDeadlineTimer();

    foreach (const FileTaskItem &item, m_items) {
        if (!startDownload(item))
            break;
    }

    if (m_items.isEmpty() || m_futureInterface->isCanceled()) {
        m_futureInterface->reportFinished();
        emit finished();    // emit finished, so the event loop can shutdown
    }
}


// -- private slots

void Downloader::onReadyRead()
{
    QNetworkReply *const reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply)
        return;

    Data &data = *m_downloads[reply];
    if (!data.file) {
        std::unique_ptr<QFile> file = Q_NULLPTR;
        const QString target = data.taskItem.target();
        if (target.isEmpty()) {
            std::unique_ptr<QTemporaryFile> tmp(new QTemporaryFile);
            tmp->setAutoRemove(false);
            file = std::move(tmp);
        } else {
            std::unique_ptr<QFile> tmp(new QFile(target));
            file = std::move(tmp);
        }

        if (file->exists() && (!QFileInfo(file->fileName()).isFile())) {
            m_futureInterface->reportException(TaskException(tr("Target file \"%1\" already exists "
                "but is not a file.").arg(file->fileName())));
            return;
        }

        if (!file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            //: %2 is a sentence describing the error
            m_futureInterface->reportException(
                        TaskException(tr("Cannot open file \"%1\" for writing: %2").arg(
                                          QDir::toNativeSeparators(file->fileName()),
                                          file->errorString())));
            return;
        }
        data.file = std::move(file);
    }

    if (!data.file->isOpen()) {
        //: %2 is a sentence describing the error.
        m_futureInterface->reportException(
                    TaskException(tr("File \"%1\" not open for writing: %2").arg(
                                      QDir::toNativeSeparators(data.file->fileName()),
                                      data.file->errorString())));
        return;
    }

    // If we sent a Range request but the server responded with 200 (not 206),
    // it doesn't support Range and is sending the full file from the beginning.
    // Could be also proxy interference, but we have no way to detect that.
    if (data.rangeRequestSent) {
        data.rangeRequestSent = false;
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (statusCode == 200) {
            qCDebug(QInstaller::lcServer) << "Download was resumed. Server does not support Range requests for"
                << QUrl(data.taskItem.source()).fileName() << "- restarting download the file from beginning.";
            data.file->seek(0);
            data.file->resize(0);
            data.observer->resetCheckSumData();
            data.observer->setBytesTransfered(0);
            data.observer->setBytesToTransfer(0);
        }
    }

    QByteArray buffer(32768, Qt::Uninitialized);
    while (reply->bytesAvailable()) {

        const qint64 read = reply->read(buffer.data(), buffer.size());
        qint64 written = 0;
        while (written < read) {
            const qint64 toWrite = data.file->write(buffer.constData() + written, read - written);
            if (toWrite < 0) {
                //: %2 is a sentence describing the error.
                m_futureInterface->reportException(
                            TaskException(tr("Writing to file \"%1\" failed: %2").arg(
                                                  QDir::toNativeSeparators(data.file->fileName()),
                                              data.file->errorString())));
                return;
            }
            written += toWrite;
        }

        data.observer->addSample(read);
        data.observer->addBytesTransfered(read);
        if (data.file->fileName().endsWith(QLatin1String(".sha1")))
            data.observer->setExpectedSha1(buffer.left(read));
        else
            data.observer->addCheckSumData(buffer.left(read));

        if (!reply->attribute(QNetworkRequest::RedirectionTargetAttribute).isValid()) {
            if (m_progressValueInBytes) {
                quint64 progress = 0;
                for (const auto &pair : m_downloads)
                    progress += pair.second->observer->bytesTransfered();
                emit progressChanged(progress);
            } else {
                int progress = m_finished * 100;
                m_futureInterface->setProgressValueAndText(progress / m_items.count(),
                    data.observer->progressText());
            }
        }
    }
}

void Downloader::onFinished()
{
    QNetworkReply *const reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply || reply->error() != QNetworkReply::NoError ||  m_downloads.find(reply) == m_downloads.end())
        return;

    Data &data = *m_downloads[reply];

    const QString filename = data.file ? data.file->fileName() : QString();
    if (!m_futureInterface->isCanceled()) {
        if (reply->attribute(QNetworkRequest::RedirectionTargetAttribute).isValid()) {
            const QUrl url = reply->url()
                .resolved(reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl());
            const QList<QUrl> redirects = m_redirects.values(reply);
            if (!redirects.contains(url)) {
                if (data.file)
                    data.file->remove();

                FileTaskItem taskItem = data.taskItem;
                taskItem.insert(TaskRole::SourceFile, url.toString());
                QNetworkReply *const redirectReply = startDownload(taskItem);

                foreach (const QUrl &redirect, redirects)
                    m_redirects.insert(redirectReply, redirect);
                m_redirects.insert(redirectReply, url);

                m_downloads.erase(reply);
                m_redirects.remove(reply);
                reply->deleteLater();
                return;
            } else {
                if (data.file)
                    data.file->close();
                m_futureInterface->reportException(TaskException(tr("Redirect loop detected for \"%1\".")
                    .arg(url.toString())));
                return;
            }
        }
    }

    // Drain any remaining data from the reply buffer that readyRead may not
    // have delivered (the last chunk can arrive together with the finish signal)
    if (data.file && data.file->isOpen() && reply->bytesAvailable() > 0) {
        const QByteArray remaining = reply->readAll();
        if (!remaining.isEmpty()) {
            data.file->write(remaining);
            data.observer->addBytesTransfered(remaining.size());
            if (filename.endsWith(QLatin1String(".sha1")))
                data.observer->setExpectedSha1(remaining);
            else
                data.observer->addCheckSumData(remaining);
        }
    }

    if (data.file)
        data.file->close();

    // Expected checksum is read from file
    if (!data.observer->expectedSha1().isEmpty())
        data.taskItem.insert(TaskRole::Checksum, data.observer->expectedSha1());

    data.observer->setDownloadFinished(true);
    m_futureInterface->reportResult(FileTaskResult(filename, data.observer->checkSum(), data.taskItem));
    QFileInfo fi(filename);
    emit fileDownloaded(fi.fileName(), data.taskItem.value(TaskRole::ComponentName).toString());

    m_finished++;

    if (m_downloads.size() == m_finished || m_futureInterface->isCanceled()) {
        m_futureInterface->reportFinished();
        emit finished();    // emit finished, so the event loop can shutdown
    }
}

void Downloader::errorOccurred(QNetworkReply::NetworkError error)
{
    QNetworkReply *const reply = qobject_cast<QNetworkReply *>(sender());

    if (error == QNetworkReply::ProxyAuthenticationRequiredError)
        return; // already handled by onProxyAuthenticationRequired
    if (error == QNetworkReply::AuthenticationRequiredError)
        return; // already handled by onAuthenticationRequired
    if (error == QNetworkReply::RemoteHostClosedError || error == QNetworkReply::TemporaryNetworkFailureError || error == QNetworkReply::UnknownNetworkError)
        return; // wait for the reconnection

    if (reply) {
        const Data &data = *m_downloads[reply];
        //Do not throw error if Updates.xml not found. The repository might be removed
        //with RepositoryUpdate in Updates.xml later.
        //: %2 is a sentence describing the error
        if (data.taskItem.source().contains(QLatin1String("Updates.xml"), Qt::CaseInsensitive)) {
            qCWarning(QInstaller::lcServer) << QString::fromLatin1("Network error while downloading '%1': %2.").arg(
                   data.taskItem.source(), reply->errorString());
            disconnectNetworkReply(reply);
            m_downloads.erase(reply);
            m_redirects.remove(reply);
            reply->deleteLater();
            if (m_downloads.size() <= 0 || m_downloads.size() == m_finished) {
                m_futureInterface->reportFinished();
                emit finished();    // emit finished, so the event loop can shutdown
            }
        } else {
            if (isDownloadResumed()) {
                shutDown();
                return;
            }
            stopDownloadDeadlineTimer();
            if (data.taskItem.source().contains(QLatin1String("_meta"), Qt::CaseInsensitive)) {
                QString errorString = tr("Network error while downloading '%1': %2.").arg(data.taskItem.source(), reply->errorString());
                errorString.append(ProductKeyCheck::instance()->additionalMetaDownloadWarning());
                m_futureInterface->reportException(MetaDownloadException(errorString));
                emit finished();
            } else {
                m_futureInterface->reportException(
                    TaskException(tr("Network error while downloading '%1': %2.").arg(
                                        data.taskItem.source(), reply->errorString())));
                emit finished();
            }
        }
    } else {
        stopDownloadDeadlineTimer();
        //: %1 is a sentence describing the error
        m_futureInterface->reportException(
                    TaskException(tr("Unknown network error while downloading \"%1\".").arg(error)));
    }
    setDownloadResumed(false);
}

void Downloader::onSslErrors(const QList<QSslError> &sslErrors)
{
#ifdef QT_NO_SSL
    Q_UNUSED(sslErrors);
#else
    foreach (const QSslError &error, sslErrors)
        qCWarning(QInstaller::lcServer) << "SSL error:" << error.errorString();
#endif
}

void Downloader::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    QNetworkReply *const reply = qobject_cast<QNetworkReply *>(sender());
    if (reply) {
        const Data &data = *m_downloads[reply];
        if (isDownloadResumed()) {
            // On resume, bytesReceived/bytesTotal are relative to the new
            // QNetworkReply, not the overall download. Do NOT overwrite
            // bytesTransfered — onReadyRead's addBytesTransfered keeps the
            // correct running total across resumes.
            data.observer->setBytesToTransfer(bytesTotal + data.observer->bytesTransfered());
        } else {
            data.observer->setBytesToTransfer(bytesTotal);
            data.observer->setBytesTransfered(bytesReceived);
        }
    }
    runDownloadDeadlineTimer();
}

void Downloader::onAuthenticationRequired(QNetworkReply *reply, QAuthenticator *authenticator)
{
    if (!authenticator || !reply || m_downloads.find(reply) == m_downloads.cend())
        return;

    FileTaskItem *item = &m_downloads[reply]->taskItem;
    const QAuthenticator auth = item->value(TaskRole::Authenticator).value<QAuthenticator>();
    if (auth.user().isEmpty()) {
        AuthenticationRequiredException e(AuthenticationRequiredException::Type::Server,
            QCoreApplication::translate("AuthenticationRequiredException", "%1 at %2")
            .arg(authenticator->realm(), reply->url().host()));
        item->insert(TaskRole::Authenticator, QVariant::fromValue(QAuthenticator(*authenticator)));
        e.setFileTaskItem(*item);
        m_futureInterface->reportException(e);
    } else {
        authenticator->setUser(auth.user());
        authenticator->setPassword(auth.password());
        item->insert(TaskRole::Authenticator, QVariant()); // clear so we fail on next call
    }
}

void Downloader::onProxyAuthenticationRequired(const QNetworkProxy &proxy, QAuthenticator *)
{
    // Report to GUI thread.
    // (Job will ask for username/password, and restart the download ...)
    AuthenticationRequiredException e(AuthenticationRequiredException::Type::Proxy,
        QCoreApplication::translate("AuthenticationRequiredException",
        "Proxy requires authentication."));
    e.setProxy(proxy);
    m_futureInterface->reportException(e);
}

/*!
    \internal

    Canceling from the outside will not get noticed if we are waiting on a connection that
    does not create any events. QNam will drop after 45 seconds, though the user might have
    canceled the download before. In that case we block until the QNam timeout is reached,
    worst case resulting in deadlock while the application is shutting down at the same time.
*/
void Downloader::onTimeout()
{
    if (testCanceled()) {
        m_futureInterface->reportFinished();
        emit finished();
    }
}

void Downloader::onReachabilityChanged(QNetworkInformation::Reachability newReachability)
{
    if (newReachability == QNetworkInformation::Reachability::Online) {
        if (isDownloadPaused()) {
            setDownloadPaused(false);
            resumeDownload();
        }
    } else if (newReachability == QNetworkInformation::Reachability::Disconnected) {
        shutDown();
        setDownloadPaused(true);
        setDownloadResumed(false);
        stopDownloadDeadlineTimer();
        emit networkDisconnected();
    }
}

/*!
    Called when the download timer event \a event occurs.
*/
void Downloader::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == m_downloadDeadlineTimer.timerId()) {
        shutDown();
        resumeDownload();
    }
}

bool Downloader::testCanceled()
{
    return m_futureInterface->isCanceled();
}

void Downloader::shutDown()
{
    for (const auto &pair : m_downloads) {
        if (pair.second->observer->downloadFinished()) {
            continue;
        }
        if (pair.first) {
            disconnectNetworkReply(pair.first);
        }
    }
}

QNetworkReply *Downloader::startDownload(const FileTaskItem &item)
{
    QUrl const source = item.source();
    if (!source.isValid()) {
        //: %2 is a sentence describing the error
        m_futureInterface->reportException(TaskException(tr("Invalid source URL \"%1\": %2")
            .arg(source.toString(), source.errorString())));
        return 0;
    }

    if (QInstaller::lcServer().isDebugEnabled()
            && LoggingHandler::instance().verboseLevel() == LoggingHandler::Detailed
            && QFileInfo(source.toString()).suffix() != QLatin1String("sha1")) {
        const QString hostName = source.host();
        const QHostInfo info = QHostInfo::fromName(hostName);
        QStringList hostAddresses;
        const auto addresses = info.addresses();
        for (const QHostAddress &address : addresses)
            hostAddresses << address.toString();
        qCDebug(QInstaller::lcServer) << "Using host:" << hostName
                << "for" << source.fileName() << "\nIP:" << hostAddresses;
    }

    QNetworkRequest request(source);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);

    QNetworkReply *reply = m_nam.get(request);
    std::unique_ptr<Data> data(new Data(item));
    m_downloads[reply] = std::move(data);

    connectNetworkReply(reply);

    return reply;
}

void Downloader::connectNetworkReply(QNetworkReply *reply)
{

    connect(reply, &QNetworkReply::finished, this, &Downloader::onFinished);
    connect(reply, &QIODevice::readyRead, this, &Downloader::onReadyRead);
    connect(reply, SIGNAL(errorOccurred(QNetworkReply::NetworkError)), this,
        SLOT(errorOccurred(QNetworkReply::NetworkError)));
#ifndef QT_NO_SSL
    connect(reply, &QNetworkReply::sslErrors, this, &Downloader::onSslErrors);
#endif
    connect(reply, &QNetworkReply::downloadProgress, this, &Downloader::onDownloadProgress);
}

void Downloader::disconnectNetworkReply(QNetworkReply *reply)
{
    disconnect(reply, &QNetworkReply::finished, this, &Downloader::onFinished);
    disconnect(reply, &QIODevice::readyRead, this, &Downloader::onReadyRead);
    disconnect(reply, SIGNAL(errorOccurred(QNetworkReply::NetworkError)), this,
            SLOT(errorOccurred(QNetworkReply::NetworkError)));
#ifndef QT_NO_SSL
    disconnect(reply, &QNetworkReply::sslErrors, this, &Downloader::onSslErrors);
#endif
    disconnect(reply, &QNetworkReply::downloadProgress, this, &Downloader::onDownloadProgress);
}

/*!
    Resumes network downloads.
*/
void Downloader::resumeDownload()
{
    std::unordered_map<QNetworkReply*, std::unique_ptr<Data>> tmpDownloads;

    for (auto it = m_downloads.begin(); it != m_downloads.end(); ) {

        if (it->second->observer->downloadFinished()) {
            ++it;
        } else {
            it->second->rangeRequestSent = false;
            it->second->observer->updateTotalBytesDownloadedBeforeResume();

            QNetworkRequest request(it->second->taskItem.source());
            request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
            request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
            const qint64 transferred = it->second->observer->bytesTransfered();
            const qint64 total = it->second->observer->bytesToTransfer();
            if (transferred > 0 && total > 0 && transferred < total) {
                request.setRawHeader(QByteArray("Range"), QString(QStringLiteral("bytes=%1-"))
                    .arg(transferred).toLatin1());
                it->second->rangeRequestSent = true;
            }
            QNetworkReply *reply = m_nam.get(request);

            tmpDownloads.try_emplace(reply, std::move(it->second));
            QNetworkReply *oldReply = it->first;
            oldReply->deleteLater();
            it = m_downloads.erase(it);
            connectNetworkReply(reply);
        }
    }

    // Reinsert modified elements back into the map
    for (auto& [new_key, data] : tmpDownloads)
        m_downloads[new_key] = std::move(data);

    setDownloadResumed(true);
    runDownloadDeadlineTimer();
}

/*!
    Restarts the download deadline timer.
*/
void Downloader::runDownloadDeadlineTimer()
{
    stopDownloadDeadlineTimer();
    m_downloadDeadlineTimer.start(m_downloadDeadlineTimerInterval, this);
}

/*!
    Stops the download deadline timer.
*/
void Downloader::stopDownloadDeadlineTimer()
{
    m_downloadDeadlineTimer.stop();
}

/*!
    Sets the download into a \a paused state.
*/
void Downloader::setDownloadPaused(bool paused)
{
    m_downloadPaused = paused;
}

/*!
    Returns the download paused state.
*/
bool Downloader::isDownloadPaused()
{
    return m_downloadPaused;
}

/*!
    Sets the download into a \a resumed state.
*/
void Downloader::setDownloadResumed(bool resumed)
{
    m_downloadResumed = resumed;
}

/*!
    Returns the download resumed state.
*/
bool Downloader::isDownloadResumed()
{
    return m_downloadResumed;
}

// -- DownloadFileTask

DownloadFileTask::DownloadFileTask(const QList<FileTaskItem> &items)
    : AbstractFileTask()
    , m_progressInBytes(false)
{
    setTaskItems(items);
}

void DownloadFileTask::setTaskItem(const FileTaskItem &item)
{
    AbstractFileTask::setTaskItem(item);
}

void DownloadFileTask::addTaskItem(const FileTaskItem &item)
{
    AbstractFileTask::addTaskItem(item);
}

void DownloadFileTask::setTaskItems(const QList<FileTaskItem> &items)
{
    AbstractFileTask::setTaskItems(items);
}

void DownloadFileTask::addTaskItems(const QList<FileTaskItem> &items)
{
    AbstractFileTask::addTaskItems(items);
}

void DownloadFileTask::setAuthenticator(const QAuthenticator &authenticator)
{
    m_authenticator = authenticator;
}

void DownloadFileTask::setProxyFactory(KDUpdater::FileDownloaderProxyFactory *factory)
{
    m_proxyFactory.reset(factory);
}

void DownloadFileTask::setProgressValueInBytes(bool progressInBytes)
{
    m_progressInBytes = progressInBytes;
}

bool DownloadFileTask::progressValueInBytes() const
{
    return m_progressInBytes;
}

void DownloadFileTask::doTask(QFutureInterface<FileTaskResult> &fi)
{
    QEventLoop el;
    Downloader downloader;
    connect(&downloader, &Downloader::finished, &el, &QEventLoop::quit);
    connect(&downloader, &Downloader::progressChanged, this, &DownloadFileTask::progressChanged);
    connect(&downloader, &Downloader::fileDownloaded, this, &DownloadFileTask::fileDownloaded);
    connect(&downloader, &Downloader::networkDisconnected, this, &DownloadFileTask::networkDisconnected);

    QList<FileTaskItem> items = taskItems();
    if (!m_authenticator.isNull()) {
        for (int i = 0; i < items.count(); ++i) {
            if (items.at(i).value(TaskRole::Authenticator).isNull())
                items[i].insert(TaskRole::Authenticator, QVariant::fromValue(m_authenticator));
        }
    }
    downloader.download(fi, items, (m_proxyFactory.isNull() ? 0 : m_proxyFactory->clone()), progressValueInBytes());
    el.exec();  // That's tricky here, we need to run our own event loop to keep QNAM working.
}

}   // namespace QInstaller
