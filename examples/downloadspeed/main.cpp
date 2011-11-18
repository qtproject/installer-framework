/****************************************************************************
** Copyright (C) 2001-2010 Klaralvdalens Datakonsult AB.  All rights reserved.
**
** This file is part of the KD Tools library.
**
** Licensees holding valid commercial KD Tools licenses may use this file in
** accordance with the KD Tools Commercial License Agreement provided with
** the Software.
**
**
** This file may be distributed and/or modified under the terms of the
** GNU Lesser General Public License version 2 and version 3 as published by the
** Free Software Foundation and appearing in the file LICENSE.LGPL included.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
** Contact info@kdab.com if any conditions of this licensing are not
** clear to you.
**
**********************************************************************/

#include <kdupdaterfiledownloader.h>
#include <kdupdaterfiledownloaderfactory.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>
#include <QtCore/QObject>
#include <QtCore/QUrl>

static QString format(double data)
{
    if (data < 1024.0)
        return QString::fromLatin1("%L1 B").arg(data);
    data /= 1024.0;
    if (data < 1024.0)
        return QString::fromLatin1("%L1 KB").arg(data, 0, 'f', 2);
    data /= 1024.0;
    if (data < 1024.0)
        return QString::fromLatin1("%L1 MB").arg(data, 0, 'f', 2);
    data /= 1024.0;
    return QString::fromLatin1("%L1 GB").arg(data, 0, 'f', 2);
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
        qDebug() << "Download speed: " << format(speed) + "/sec";
    }

    void downloadProgress(double progress)
    {
        qDebug() << "Progress: " << progress;
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

        qDebug() << days + hours + minutes + seconds + tr(" remaining.");
    }

    void downloadStatus(const QString &status)
    {
        qDebug() << status;
    }

    void downloadProgress(qint64 bytesReceived, qint64 bytesToReceive)
    {
        qDebug() << "Bytes received: " << bytesReceived << ", Bytes to receive: " << bytesToReceive;
    }

private:
    volatile bool m_downloadFinished;
};


// http://get.qt.nokia.com/qt/source/qt-mac-opensource-4.7.4.dmg
// ftp://ftp.trolltech.com/qt/source/qt-mac-opensource-4.7.4.dmg

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    if (a.arguments().count() < 2)
        return EXIT_FAILURE;

    const QUrl url(a.arguments().value(1));
    qDebug() << url.toString();
    KDUpdater::FileDownloader *loader = KDUpdater::FileDownloaderFactory::instance().create(url.scheme());
    if (loader) {
        loader->setUrl(url);

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
