
/**************************************************************************
**
** Copyright (C) 2023 The Qt Company Ltd.
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
#include "downloadfiletask.h"

#include "downloadfiletask_p.h"
#include "globals.h"
#include "productkeycheck.h"

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QNetworkProxyFactory>
#include <QSslError>
#include <QTemporaryFile>

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
{
    connect(&m_timer, &QTimer::timeout, this, &Downloader::onTimeout);
    connect(&m_nam, &QNetworkAccessManager::finished, this, &Downloader::onFinished);
}

Downloader::~Downloader()
{
    m_nam.disconnect();
    for (const auto &pair : m_downloads) {
        pair.first->disconnect();
        pair.first->abort();
        pair.first->deleteLater();
    }
}

void Downloader::download(QFutureInterface<FileTaskResult> &fi, const QList<FileTaskItem> &items,
    QNetworkProxyFactory *networkProxyFactory)
{
    m_items = items;
    m_futureInterface = &fi;

    fi.reportStarted();
    fi.setExpectedResultCount(items.count());

    m_nam.setProxyFactory(networkProxyFactory);
    connect(&m_nam, &QNetworkAccessManager::authenticationRequired, this,
        &Downloader::onAuthenticationRequired);
    connect(&m_nam, &QNetworkAccessManager::proxyAuthenticationRequired, this,
            &Downloader::onProxyAuthenticationRequired);
    QTimer::singleShot(0, this, &Downloader::doDownload);
}

void Downloader::doDownload()
{
    m_timer.start(1000); // Use a timer to check for canceled downloads.

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
    if (testCanceled()) {
        m_futureInterface->reportFinished();
        emit finished(); return;    // error
    }

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

    QByteArray buffer(32768, Qt::Uninitialized);
    while (reply->bytesAvailable()) {
        if (testCanceled()) {
            m_futureInterface->reportFinished();
            emit finished(); return;    // error
        }

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
        data.observer->addCheckSumData(buffer.left(read));

        int progress = m_finished * 100;
        for (const auto &pair : m_downloads)
            progress += pair.second->observer->progressValue();
        if (!reply->attribute(QNetworkRequest::RedirectionTargetAttribute).isValid()) {
            m_futureInterface->setProgressValueAndText(progress / m_items.count(),
                data.observer->progressText());
        }
    }
}

void Downloader::onFinished(QNetworkReply *reply)
{
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
                m_futureInterface->reportException(TaskException(tr("Redirect loop detected for \"%1\".")
                    .arg(url.toString())));
                return;
            }
        }
    }

    const QByteArray ba = reply->readAll();
    if (!ba.isEmpty()) {
        data.observer->addSample(ba.size());
        data.observer->addBytesTransfered(ba.size());
        data.observer->addCheckSumData(ba);
    }

    const QByteArray expectedCheckSum = data.taskItem.value(TaskRole::Checksum).toByteArray();
    bool checksumMismatch = false;
    if (!expectedCheckSum.isEmpty()) {
        if (expectedCheckSum != data.observer->checkSum().toHex())
            checksumMismatch = true;
    }
    m_futureInterface->reportResult(FileTaskResult(filename, data.observer->checkSum(), data.taskItem,
                                                  checksumMismatch));

    m_downloads.erase(reply);
    m_redirects.remove(reply);
    reply->deleteLater();

    m_finished++;
    if (m_downloads.empty() || m_futureInterface->isCanceled()) {
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

    if (reply) {
        const Data &data = *m_downloads[reply];
        //Do not throw error if Updates.xml not found. The repository might be removed
        //with RepositoryUpdate in Updates.xml later.
        //: %2 is a sentence describing the error
        if (data.taskItem.source().contains(QLatin1String("Updates.xml"), Qt::CaseInsensitive)) {
            qCWarning(QInstaller::lcServer) << QString::fromLatin1("Network error while downloading '%1': %2.").arg(
                   data.taskItem.source(), reply->errorString());
        } else if (data.taskItem.source().contains(QLatin1String("_meta"), Qt::CaseInsensitive)) {
            QString errorString = tr("Network error while downloading '%1': %2.").arg(data.taskItem.source(), reply->errorString());
            errorString.append(ProductKeyCheck::instance()->additionalMetaDownloadWarning());
            m_futureInterface->reportException(TaskException(errorString));
        } else {
            m_futureInterface->reportException(
                TaskException(tr("Network error while downloading '%1': %2.").arg(
                                      data.taskItem.source(), reply->errorString())));
        }
    } else {
        //: %1 is a sentence describing the error
        m_futureInterface->reportException(
                    TaskException(tr("Unknown network error while downloading \"%1\".").arg(error)));
    }
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
    Q_UNUSED(bytesReceived)
    QNetworkReply *const reply = qobject_cast<QNetworkReply *>(sender());
    if (reply) {
        const Data &data = *m_downloads[reply];
        data.observer->setBytesToTransfer(bytesTotal);
    }
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
    // (MetadataJob will ask for username/password, and restart the download ...)
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
        // Inject exception, we can't use QFuturInterface::reportException() as the exception
        // store is "frozen" once cancel was called. On the other hand, client code could use
        // QFutureWatcherBase::isCanceled() or QFuture::isCanceled() to check for canceled futures.
        m_futureInterface->exceptionStore()
            .setException(TaskException(tr("Network transfers canceled.")));
        m_futureInterface->reportFinished();
        emit finished();
    }
}


// -- private

bool Downloader::testCanceled()
{
    // TODO: figure out how to implement pause and resume
    if (m_futureInterface->isPaused()) {
        m_futureInterface->togglePaused();  // Note: this will trigger cancel
        m_futureInterface->reportException(
                    TaskException(tr("Pause and resume not supported by network transfers.")));
    }
    return m_futureInterface->isCanceled();
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
    QNetworkRequest request(source);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);

    QNetworkReply *reply = m_nam.get(request);
    std::unique_ptr<Data> data(new Data(item));
    m_downloads[reply] = std::move(data);

    connect(reply, &QIODevice::readyRead, this, &Downloader::onReadyRead);
    connect(reply, SIGNAL(errorOccurred(QNetworkReply::NetworkError)), this,
        SLOT(errorOccurred(QNetworkReply::NetworkError)));
#ifndef QT_NO_SSL
    connect(reply, &QNetworkReply::sslErrors, this, &Downloader::onSslErrors);
#endif
    connect(reply, &QNetworkReply::downloadProgress, this, &Downloader::onDownloadProgress);
    return reply;
}


// -- DownloadFileTask

DownloadFileTask::DownloadFileTask(const QList<FileTaskItem> &items)
    : AbstractFileTask()
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

void DownloadFileTask::doTask(QFutureInterface<FileTaskResult> &fi)
{
    QEventLoop el;
    Downloader downloader;
    connect(&downloader, &Downloader::finished, &el, &QEventLoop::quit);

    QList<FileTaskItem> items = taskItems();
    if (!m_authenticator.isNull()) {
        for (int i = 0; i < items.count(); ++i) {
            if (items.at(i).value(TaskRole::Authenticator).isNull())
                items[i].insert(TaskRole::Authenticator, QVariant::fromValue(m_authenticator));
        }
    }
    downloader.download(fi, items, (m_proxyFactory.isNull() ? 0 : m_proxyFactory->clone()));
    el.exec();  // That's tricky here, we need to run our own event loop to keep QNAM working.
}

}   // namespace QInstaller
