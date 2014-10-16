/**************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
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
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
**
** $QT_END_LICENSE$
**
**************************************************************************/
#include "downloadfiletask.h"

#include "downloadfiletask_p.h"
#include "observer.h"

#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QNetworkProxyFactory>
#include <QSslError>
#include <QTemporaryFile>
#include <QTimer>

namespace QInstaller {

Downloader::Downloader()
    : m_finished(0)
{
    connect(&m_nam, SIGNAL(finished(QNetworkReply*)), SLOT(onFinished(QNetworkReply*)));
}

Downloader::~Downloader()
{
    m_nam.disconnect();
    foreach (QNetworkReply *const reply, m_downloads.keys()) {
        reply->disconnect();
        reply->abort();
        reply->deleteLater();
    }

    foreach (const Data &data, m_downloads.values()) {
        data.file->close();
        delete data.file;
        delete data.observer;
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
    connect(&m_nam, SIGNAL(authenticationRequired(QNetworkReply*, QAuthenticator*)), this,
        SLOT(onAuthenticationRequired(QNetworkReply*, QAuthenticator*)));
    QTimer::singleShot(0, this, SLOT(doDownload()));
}

void Downloader::doDownload()
{
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

    const Data &data = m_downloads[reply];
    if (!data.file->isOpen()) {
        m_futureInterface->reportException(FileTaskException(QString::fromLatin1("Target '%1' not "
            "open for write. Error: %2.").arg(data.file->fileName(), data.file->errorString())));
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
                m_futureInterface->reportException(FileTaskException(QString::fromLatin1("Writing "
                    "to target '%1' failed. Error: %2.").arg(data.file->fileName(),
                    data.file->errorString())));
                return;
            }
            written += toWrite;
        }

        data.observer->addSample(read);
        data.observer->addBytesTransfered(read);
        data.observer->addCheckSumData(buffer.data(), read);

        int progress = m_finished * 100;
        foreach (const Data &data, m_downloads.values())
            progress += data.observer->progressValue();
        if (!reply->attribute(QNetworkRequest::RedirectionTargetAttribute).isValid()) {
            m_futureInterface->setProgressValueAndText(progress / m_items.count(),
                data.observer->progressText());
        }
    }
}

void Downloader::onFinished(QNetworkReply *reply)
{
    const Data &data = m_downloads[reply];
    const QString filename = data.file->fileName();
    if (!m_futureInterface->isCanceled()) {
        if (reply->attribute(QNetworkRequest::RedirectionTargetAttribute).isValid()) {
            const QUrl url = reply->url()
                .resolved(reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl());
            const QList<QUrl> redirects = m_redirects.values(reply);
            if (!redirects.contains(url)) {
                data.file->close();
                data.file->remove();
                delete data.file;
                delete data.observer;

                FileTaskItem taskItem = data.taskItem;
                taskItem.insert(TaskRole::SourceFile, url.toString());
                QNetworkReply *const redirectReply = startDownload(taskItem);

                foreach (const QUrl &redirect, redirects)
                    m_redirects.insertMulti(redirectReply, redirect);
                m_redirects.insertMulti(redirectReply, url);

                m_downloads.remove(reply);
                m_redirects.remove(reply);
                reply->deleteLater();
                return;
            } else {
                m_futureInterface->reportException(FileTaskException(QString::fromLatin1("Redirect"
                    " loop detected '%1'.").arg(url.toString())));
                return;
            }
        }
    }

    const QByteArray ba = reply->readAll();
    if (!ba.isEmpty()) {
        data.observer->addSample(ba.size());
        data.observer->addBytesTransfered(ba.size());
        data.observer->addCheckSumData(ba.data(), ba.size());
    }

    const QByteArray expectedCheckSum = data.taskItem.value(TaskRole::Checksum).toByteArray();
    if (!expectedCheckSum.isEmpty()) {
        if (expectedCheckSum != data.observer->checkSum().toHex()) {
            m_futureInterface->reportException(FileTaskException(QString::fromLatin1("Checksum"
                " mismatch detected '%1'.").arg(reply->url().toString())));
        }
    }
    m_futureInterface->reportResult(FileTaskResult(filename, data.observer->checkSum(), data.taskItem));

    data.file->close();
    delete data.file;
    delete data.observer;

    m_downloads.remove(reply);
    m_redirects.remove(reply);
    reply->deleteLater();

    m_finished++;
    if (m_downloads.isEmpty() || m_futureInterface->isCanceled()) {
        m_futureInterface->reportFinished();
        emit finished();    // emit finished, so the event loop can shutdown
    }
}

void Downloader::onError(QNetworkReply::NetworkError error)
{
    QNetworkReply *const reply = qobject_cast<QNetworkReply *>(sender());
    if (reply) {
        const Data &data = m_downloads[reply];
        m_futureInterface->reportException(FileTaskException(QString::fromLatin1("Network error "
            "while downloading target '%1'. Error: %2.").arg(data.file->fileName(),
            reply->errorString())));
    } else {
        m_futureInterface->reportException(FileTaskException(QString::fromLatin1("Unknown network "
            "error while downloading. Error: %1.").arg(error)));
    }
}

void Downloader::onSslErrors(const QList<QSslError> &sslErrors)
{
#ifdef QT_NO_SSL
    Q_UNUSED(sslErrors);
#else
    foreach (const QSslError &error, sslErrors)
        qDebug() << QString::fromLatin1("SSL error: %s").arg(error.errorString());
#endif
}

void Downloader::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    Q_UNUSED(bytesReceived)
    QNetworkReply *const reply = qobject_cast<QNetworkReply *>(sender());
    if (reply) {
        const Data &data = m_downloads[reply];
        data.observer->setBytesToTransfer(bytesTotal);
    }
}

void Downloader::onAuthenticationRequired(QNetworkReply *reply, QAuthenticator *authenticator)
{
    if (!authenticator || !reply || !m_downloads.contains(reply))
        return;

    FileTaskItem *item = &m_downloads[reply].taskItem;
    const QAuthenticator auth = item->value(TaskRole::Authenticator).value<QAuthenticator>();
    if (!auth.user().isEmpty()) {
        authenticator->setUser(auth.user());
        authenticator->setPassword(auth.password());
        item->insert(TaskRole::Authenticator, QVariant()); // clear so we fail on next call
    } else {
        m_futureInterface->reportException(FileTaskException(QString::fromLatin1("Could not "
            "authenticate using the provided credentials. Source: '%1'.").arg(reply->url()
            .toString())));
    }
}


// -- private

bool Downloader::testCanceled()
{
    // TODO: figure out how to implement pause and resume
    if (m_futureInterface->isPaused()) {
        m_futureInterface->togglePaused();  // Note: this will trigger cancel
        m_futureInterface->reportException(FileTaskException(QLatin1String("Pause and resume not "
            "supported by network transfers.")));
    }
    return m_futureInterface->isCanceled();
}

QNetworkReply *Downloader::startDownload(const FileTaskItem &item)
{
    QUrl const source = item.source();
    if (!source.isValid()) {
        m_futureInterface->reportException(FileTaskException(QString::fromLatin1("Invalid source "
            "'%1'. Error: %2.").arg(source.toString(), source.errorString())));
        return 0;
    }

    QScopedPointer<QFile> file;
    const QString target = item.target();
    if (target.isEmpty()) {
        QTemporaryFile *tmp = new QTemporaryFile;
        tmp->setAutoRemove(false);
        file.reset(tmp);
    } else {
        file.reset(new QFile(target));
    }

    if (file->exists() && (!QFileInfo(file->fileName()).isFile())) {
        m_futureInterface->reportException(FileTaskException(QString::fromLatin1("Target file "
            "'%1' already exists but is not a file.").arg(file->fileName())));
        return 0;
    }

    if (!file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        m_futureInterface->reportException(FileTaskException(QString::fromLatin1("Could not open "
            "target '%1' for write. Error: %2.").arg(file->fileName(), file->errorString())));
        return 0;
    }

    QNetworkReply *reply = m_nam.get(QNetworkRequest(source));
    m_downloads.insert(reply, Data(file.take(), new FileTaskObserver(QCryptographicHash::Sha1), item));

    connect(reply, SIGNAL(readyRead()), this, SLOT(onReadyRead()));
    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this,
        SLOT(onError(QNetworkReply::NetworkError)));
#ifndef QT_NO_SSL
    connect(reply, SIGNAL(sslErrors(QList<QSslError>)), SLOT(onSslErrors(QList<QSslError>)));
#endif
    connect(reply, SIGNAL(downloadProgress(qint64, qint64)), this, SLOT(onDownloadProgress(qint64,
        qint64)));
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
    connect(&downloader, SIGNAL(finished()), &el, SLOT(quit()));

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
