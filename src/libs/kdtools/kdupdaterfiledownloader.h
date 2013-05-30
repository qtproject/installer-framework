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

#ifndef KD_UPDATER_FILE_DOWNLOADER_H
#define KD_UPDATER_FILE_DOWNLOADER_H

#include "kdtoolsglobal.h"

#include <QtCore/QObject>
#include <QtCore/QUrl>

#include <QtNetwork/QAuthenticator>

namespace KDUpdater {

class FileDownloaderProxyFactory;

class KDTOOLS_EXPORT FileDownloader : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool autoRemoveDownloadedFile READ isAutoRemoveDownloadedFile WRITE setAutoRemoveDownloadedFile)
    Q_PROPERTY(QUrl url READ url WRITE setUrl)
    Q_PROPERTY(QString scheme READ scheme WRITE setScheme)

public:
    explicit FileDownloader(const QString &scheme, QObject *parent = 0);
    ~FileDownloader();

    QUrl url() const;
    void setUrl(const QUrl &url);

    QByteArray sha1Sum() const;

    QByteArray assumedSha1Sum() const;
    void setAssumedSha1Sum(const QByteArray &sha1);

    QString scheme() const;
    void setScheme(const QString &scheme);

    QString errorString() const;

    virtual bool canDownload() const = 0;
    virtual bool isDownloaded() const = 0;
    virtual QString downloadedFileName() const = 0;
    virtual void setDownloadedFileName(const QString &name) = 0;
    virtual FileDownloader *clone(QObject *parent=0) const = 0;

    void download();

    void setAutoRemoveDownloadedFile(bool val);
    bool isAutoRemoveDownloadedFile() const;

    void setFollowRedirects(bool val);
    bool followRedirects() const;

    FileDownloaderProxyFactory *proxyFactory() const;
    void setProxyFactory(FileDownloaderProxyFactory *factory);

    QAuthenticator authenticator() const;
    void setAuthenticator(const QAuthenticator &authenticator);

public Q_SLOTS:
    virtual void cancelDownload();

protected:
    virtual void onError() = 0;
    virtual void onSuccess() = 0;

Q_SIGNALS:
    void downloadStarted();
    void downloadCanceled();

    void downloadProgress(double progress);
    void estimatedDownloadTime(int seconds);
    void downloadSpeed(qint64 bytesPerSecond);
    void downloadStatus(const QString &status);
    void downloadProgress(qint64 bytesReceived, qint64 bytesToReceive);
    void authenticatorChanged(const QAuthenticator &authenticator);

#ifndef Q_MOC_RUN
private:
#endif
    void downloadCompleted();
    void downloadAborted(const QString &errorMessage);

protected:
    void setDownloadCanceled();
    void setDownloadCompleted();
    void setDownloadAborted(const QString &error);

    void runDownloadSpeedTimer();
    void stopDownloadSpeedTimer();

    void addSample(qint64 sample);
    int downloadSpeedTimerId() const;
    void setProgress(qint64 bytesReceived, qint64 bytesToReceive);

    void emitDownloadSpeed();
    void emitDownloadStatus();
    void emitDownloadProgress();
    void emitEstimatedDownloadTime();

    void addCheckSumData(const QByteArray &data);
    void addCheckSumData(const char *data, int length);
    void resetCheckSumData();

private Q_SLOTS:
    virtual void doDownload() = 0;

private:
    struct Private;
    Private *d;
};

} // namespace KDUpdater

#endif // KD_UPDATER_FILE_DOWNLOADER_H
