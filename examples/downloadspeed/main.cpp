/**************************************************************************
**
** This file is part of Installer Framework
**
** Copyright (c) 2011-2012 Nokia Corporation and/or its subsidiary(-ies).
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

#include <kdupdaterfiledownloader.h>
#include <kdupdaterfiledownloaderfactory.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>
#include <QtCore/QObject>
#include <QtCore/QUrl>

#include <QtNetwork/QNetworkProxy>

static QString humanReadableSize(quint64 intSize)
{
    QString unit;
    double size;

    if (intSize < 1024 * 1024) {
        size = 1. + intSize / 1024.;
        unit = QObject::tr("kB");
    } else if (intSize < 1024 * 1024 * 1024) {
        size = 1. + intSize / 1024. / 1024.;
        unit = QObject::tr("MB");
    } else {
        size = 1. + intSize / 1024. / 1024. / 1024.;
        unit = QObject::tr("GB");
    }

    size = qRound(size * 10) / 10.0;
    return QString::fromLatin1("%L1 %2").arg(size, 0, 'g', 4).arg(unit);
}

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
        qDebug() << "Download speed:" << humanReadableSize(speed) + "/sec";
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
            days = QString::number(d) + (d < 2 ? " day" : " days") + QLatin1String(", ");

        QString hours;
        if (h > 0)
            hours = QString::number(h) + (h < 2 ? " hour" : " hours") + QLatin1String(", ");

        QString minutes;
        if (m > 0)
            minutes = QString::number(m) + (m < 2 ? " minute" : " minutes");

        QString seconds;
        if (s >= 0 && minutes.isEmpty())
            seconds = QString::number(s) + (s < 2 ? " second" : " seconds");

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
    volatile bool m_downloadFinished;
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
    qDebug() << url.toString();
    KDUpdater::FileDownloader *loader = KDUpdater::FileDownloaderFactory::instance().create(url.scheme(), 0);
    if (loader) {
        loader->setUrl(url);
        loader->setProxyFactory(new ProxyFactory());

        Receiver r;
        r.connect(loader, SIGNAL(downloadStarted()), &r, SLOT(downloadStarted()));
        r.connect(loader, SIGNAL(downloadCanceled()), &r, SLOT(downloadFinished()));
        r.connect(loader, SIGNAL(downloadCompleted()), &r, SLOT(downloadFinished()));
        r.connect(loader, SIGNAL(downloadAborted(QString)), &r, SLOT(downloadAborted(QString)));

        r.connect(loader, SIGNAL(downloadSpeed(qint64)), &r, SLOT(downloadSpeed(qint64)));
        r.connect(loader, SIGNAL(downloadStatus(QString)), &r, SLOT(downloadStatus(QString)));
        r.connect(loader, SIGNAL(downloadProgress(double)), &r, SLOT(downloadProgress(double)));
        r.connect(loader, SIGNAL(estimatedDownloadTime(int)), &r, SLOT(estimatedDownloadTime(int)));
        r.connect(loader, SIGNAL(downloadProgress(qint64, qint64)), &r, SLOT(downloadProgress(qint64, qint64)));

        loader->download();
        while (!r.downloaded())
            QCoreApplication::processEvents();

        delete loader;
    }

    return EXIT_SUCCESS;
}

#include "main.moc"
