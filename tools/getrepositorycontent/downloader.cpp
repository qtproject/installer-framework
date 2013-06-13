/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "downloader.h"

#include <fileutils.h>

#include <QtNetwork/QNetworkProxy>

class ProxyFactory : public KDUpdater::FileDownloaderProxyFactory
{
public:
    ProxyFactory() {}

    ProxyFactory *clone() const
    {
        return new ProxyFactory();
    }

    QList<QNetworkProxy> queryProxy(const QNetworkProxyQuery &query = QNetworkProxyQuery())
    {
        return QNetworkProxyFactory::systemProxyForQuery(query);
    }
};

Downloader::Downloader(const QUrl &source, const QString &target)
    : QObject()
    , m_source(source)
    , m_target(target)
    , m_fileDownloader(0)
{
    m_fileDownloader = KDUpdater::FileDownloaderFactory::instance().create(m_source.scheme(), this);
    if (!m_fileDownloader) {
        qWarning() << "No downloader registered for scheme: " << m_source.scheme();
        return;
    }
    m_fileDownloader->setDownloadedFileName(target);

    if (m_fileDownloader) {
        m_fileDownloader->setUrl(m_source);
        m_fileDownloader->setProxyFactory(new ProxyFactory());

        connect(m_fileDownloader, SIGNAL(downloadCanceled()), this, SLOT(downloadFinished()));
        connect(m_fileDownloader, SIGNAL(downloadCompleted()), this, SLOT(downloadFinished()));
        connect(m_fileDownloader, SIGNAL(downloadAborted(QString)), this, SLOT(downloadFinished(QString)));

        connect(m_fileDownloader, SIGNAL(downloadSpeed(qint64)), this, SLOT(downloadSpeed(qint64)));
        connect(m_fileDownloader, SIGNAL(downloadProgress(qint64, qint64)), this, SLOT(downloadProgress(qint64, qint64)));

        m_fileDownloader->setAutoRemoveDownloadedFile(false);
    }

}

void Downloader::downloadFinished(const QString &message)
{
    if (!message.isEmpty())
        qDebug() << "Error:" << message;
    progressBar.setStatus(100, 100);
    progressBar.update();
    printf("\n");
    emit finished();
}

void Downloader::downloadSpeed(qint64 speed)
{
    progressBar.setMessage(QString::fromLatin1("%1 /sec").arg(QInstaller::humanReadableSize(speed)));
}

void Downloader::downloadProgress(qint64 bytesReceived, qint64 bytesToReceive)
{
    if (bytesReceived == 0 || bytesToReceive == 0)
        return;
    progressBar.setStatus(bytesReceived, bytesToReceive);
    progressBar.update();
}

void Downloader::run()
{
    m_fileDownloader->download();
}
