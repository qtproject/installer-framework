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

#include <filedownloader.h>
#include <filedownloaderfactory.h>

#include <fileutils.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>
#include <QtCore/QObject>
#include <QtCore/QUrl>

#include <QtNetwork/QNetworkProxy>

// -- Receiver

class Receiver : public QObject
{
    Q_OBJECT

public:
    Receiver() : QObject(), m_downloadFinished(false) {}
    ~Receiver() {}

    inline bool downloaded() { return m_downloadFinished; }

public slots:
    void downloadStarted()
    {
        m_downloadFinished = false;
    }

    void downloadFinished()
    {
        m_downloadFinished = true;
    }

    void downloadAborted(const QString &error)
    {
        m_downloadFinished = true;
        qDebug() << "Error:" << error;
    }

    void downloadSpeed(qint64 speed)
    {
        qDebug() << "Download speed:" <<
            QInstaller::humanReadableSize(speed) + QLatin1String("/sec");
    }

    void downloadProgress(double progress)
    {
        qDebug() << "Progress:" << progress;
    }

    void estimatedDownloadTime(int time)
    {
        if (time <= 0) {
            qDebug() << "Unknown time remaining.";
            return;
        }

        const int d = time / 86400;
        const int h = (time / 3600) - (d * 24);
        const int m = (time / 60) - (d * 1440) - (h * 60);
        const int s = time % 60;

        QString days;
        if (d > 0)
            days = QString::number(d) + QLatin1String(d < 2 ? " day" : " days") + QLatin1String(", ");

        QString hours;
        if (h > 0)
            hours = QString::number(h) + QLatin1String(h < 2 ? " hour" : " hours") + QLatin1String(", ");

        QString minutes;
        if (m > 0)
            minutes = QString::number(m) + QLatin1String(m < 2 ? " minute" : " minutes");

        QString seconds;
        if (s >= 0 && minutes.isEmpty())
            seconds = QString::number(s) + QLatin1String(s < 2 ? " second" : " seconds");

        qDebug() << days + hours + minutes + seconds + tr("remaining.");
    }

    void downloadStatus(const QString &status)
    {
        qDebug() << status;
    }

    void downloadProgress(qint64 bytesReceived, qint64 bytesToReceive)
    {
        qDebug() << "Bytes received:" << bytesReceived << ", Bytes to receive:" << bytesToReceive;
    }

private:
    bool m_downloadFinished;
};


// http://get.qt.nokia.com/qt/source/qt-mac-opensource-4.7.4.dmg
// ftp://ftp.trolltech.com/qt/source/qt-mac-opensource-4.7.4.dmg

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

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    if (a.arguments().count() < 2)
        return EXIT_FAILURE;

    const QUrl url(a.arguments().value(1));
    KDUpdater::FileDownloaderFactory::setFollowRedirects(true);
    qDebug() << url.toString();
    KDUpdater::FileDownloader *loader = KDUpdater::FileDownloaderFactory::instance().create(url.scheme(), 0);
    if (loader) {
        loader->setUrl(url);
        loader->setProxyFactory(new ProxyFactory());

        Receiver r;
        r.connect(loader, &KDUpdater::FileDownloader::downloadStarted, &r, &Receiver::downloadStarted);
        r.connect(loader, &KDUpdater::FileDownloader::downloadCanceled, &r, &Receiver::downloadFinished);
        r.connect(loader, &KDUpdater::FileDownloader::downloadCompleted, &r, &Receiver::downloadFinished);
        r.connect(loader, &KDUpdater::FileDownloader::downloadAborted, &r, &Receiver::downloadAborted);

        r.connect(loader, &KDUpdater::FileDownloader::downloadSpeed, &r, &Receiver::downloadSpeed);
        r.connect(loader, &KDUpdater::FileDownloader::downloadStatus, &r, &Receiver::downloadStatus);
        r.connect(loader, SIGNAL(downloadProgress(double)), &r, SLOT(downloadProgress(double)));
        r.connect(loader, &KDUpdater::FileDownloader::estimatedDownloadTime, &r, &Receiver::estimatedDownloadTime);
        r.connect(loader, SIGNAL(downloadProgress(qint64,qint64)), &r, SLOT(downloadProgress(qint64,qint64)));

        loader->download();
        while (!r.downloaded())
            QCoreApplication::processEvents();

        delete loader;
    }

    return EXIT_SUCCESS;
}

#include "main.moc"
