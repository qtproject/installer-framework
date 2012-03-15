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

#ifndef KD_UPDATER_FILE_DOWNLOADER_P_H
#define KD_UPDATER_FILE_DOWNLOADER_P_H

#include "kdupdaterfiledownloader.h"

#include <QtCore/QCryptographicHash>
#include <QtNetwork/QNetworkReply>

// these classes are not a part of the public API

namespace KDUpdater {

//TODO make it a KDJob once merged
class HashVerificationJob : public QObject
{
    Q_OBJECT

public:
    enum Error {
        NoError = 0,
        ReadError = 128,
        SumsDifferError
    };

    explicit HashVerificationJob(QObject *parent = 0);
    ~HashVerificationJob();

    void setDevice(QIODevice *dev);
    void setSha1Sum(const QByteArray &data);

    bool hasError() const;
    int error() const;

    void start();

Q_SIGNALS:
    void finished(KDUpdater::HashVerificationJob *);

private:
    void emitFinished();
    void timerEvent(QTimerEvent *te);

private:
    class Private;
    Private *d;
};

class LocalFileDownloader : public FileDownloader
{
    Q_OBJECT

public:
    explicit LocalFileDownloader(QObject *parent = 0);
    ~LocalFileDownloader();

    bool canDownload() const;
    bool isDownloaded() const;
    QString downloadedFileName() const;
    void setDownloadedFileName(const QString &name);
    LocalFileDownloader *clone(QObject *parent = 0) const;

public Q_SLOTS:
    void cancelDownload();

protected:
    void timerEvent(QTimerEvent *te);
    void onError();
    void onSuccess();

private Q_SLOTS:
    void doDownload();

private:
    struct Private;
    Private *d;
};

class ResourceFileDownloader : public FileDownloader
{
    Q_OBJECT

public:
    explicit ResourceFileDownloader(QObject *parent = 0);
    ~ResourceFileDownloader();

    bool canDownload() const;
    bool isDownloaded() const;
    QString downloadedFileName() const;
    void setDownloadedFileName(const QString &name);
    ResourceFileDownloader *clone(QObject *parent = 0) const;

public Q_SLOTS:
    void cancelDownload();

protected:
    void timerEvent(QTimerEvent *te);
    void onError();
    void onSuccess();

private Q_SLOTS:
    void doDownload();

private:
    struct Private;
    Private *d;
};

class FtpDownloader : public FileDownloader
{
    Q_OBJECT

public:
    explicit FtpDownloader(QObject *parent = 0);
    ~FtpDownloader();

    bool canDownload() const;
    bool isDownloaded() const;
    QString downloadedFileName() const;
    void setDownloadedFileName(const QString &name);
    FtpDownloader *clone(QObject *parent = 0) const;

public Q_SLOTS:
    void cancelDownload();

protected:
    void onError();
    void onSuccess();
    void timerEvent(QTimerEvent *event);

private Q_SLOTS:
    void doDownload();

    void ftpDone(bool error);
    void ftpCmdStarted(int id);
    void ftpCmdFinished(int id, bool error);
    void ftpStateChanged(int state);
    void ftpDataTransferProgress(qint64 done, qint64 total);
    void ftpReadyRead();

private:
    struct Private;
    Private *d;
};

class HttpDownloader : public FileDownloader
{
    Q_OBJECT

public:
    explicit HttpDownloader(QObject *parent = 0);
    ~HttpDownloader();

    bool canDownload() const;
    bool isDownloaded() const;
    QString downloadedFileName() const;
    void setDownloadedFileName(const QString &name);
    HttpDownloader *clone(QObject *parent = 0) const;

public Q_SLOTS:
    void cancelDownload();

protected:
    void onError();
    void onSuccess();
    void timerEvent(QTimerEvent *event);

private Q_SLOTS:
    void doDownload();

    void httpReadyRead();
    void httpReadProgress(qint64 done, qint64 total);
    void httpError(QNetworkReply::NetworkError);
    void httpDone(bool error);
    void httpReqFinished();
    void onAuthenticationRequired(QNetworkReply *reply, QAuthenticator *authenticator);

private:
    void startDownload(const QUrl &url);

private:
    struct Private;
    Private *d;
};

} // namespace KDUpdater

#endif // KD_UPDATER_FILE_DOWNLOADER_P_H
