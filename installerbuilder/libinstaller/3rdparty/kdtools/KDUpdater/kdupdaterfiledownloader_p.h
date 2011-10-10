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
#include <KDToolsCore/pimpl_ptr.h>

#include <QtCore/QCryptographicHash>
#include <QtNetwork/QNetworkReply>

QT_BEGIN_NAMESPACE
class QIODevice;
QT_END_NAMESPACE

// these classes are not a part of the public API

namespace KDUpdater
{

    //TODO make it a KDJob once merged
    class HashVerificationJob : public QObject
    {
        Q_OBJECT
    public:
        enum Error {
            NoError=0,
            ReadError=128,
            SumsDifferError
        };
        
        explicit HashVerificationJob( QObject* parent=0 );
        ~HashVerificationJob();
        
        void setDevice( QIODevice* dev );
        void setSha1Sum( const QByteArray& data );
 
        bool hasError() const;
        int error() const;
        
        void start();
 
    Q_SIGNALS:
        void finished( KDUpdater::HashVerificationJob* );
 
    private:
        void emitFinished();
        /* reimp */ void timerEvent( QTimerEvent* te );

    private:
        class Private;
        kdtools::pimpl_ptr<Private> d;
    };

    class LocalFileDownloader : public FileDownloader
    {
        Q_OBJECT

    public:
        explicit LocalFileDownloader(QObject* parent=0);
        ~LocalFileDownloader();

        bool canDownload() const;
        bool isDownloaded() const;
        QString downloadedFileName() const;
        /* reimp */ void setDownloadedFileName(const QString &name);
        /* reimp */ LocalFileDownloader* clone( QObject* parent=0 ) const;

    public Q_SLOTS:
        void cancelDownload();

    protected:
        void timerEvent(QTimerEvent* te);
        /* reimp */ void onError();
        /* reimp */ void onSuccess();

    private Q_SLOTS:
        /* reimp */ void doDownload();

    private:
        struct LocalFileDownloaderData;
        LocalFileDownloaderData* d;
    };

    class ResourceFileDownloader : public FileDownloader
    {
        Q_OBJECT

    public:
        explicit ResourceFileDownloader(QObject* parent=0);
        ~ResourceFileDownloader();

        bool canDownload() const;
        bool isDownloaded() const;
        QString downloadedFileName() const;
        /* reimp */ void setDownloadedFileName(const QString &name);
        /* reimp */ ResourceFileDownloader* clone( QObject* parent=0 ) const;

    public Q_SLOTS:
        void cancelDownload();

    protected:
        void timerEvent(QTimerEvent* te);
        /* reimp */ void onError();
        /* reimp */ void onSuccess();

    private Q_SLOTS:
        /* reimp */ void doDownload();

    private:
        struct ResourceFileDownloaderData;
        ResourceFileDownloaderData* d;
    };

    class FtpDownloader : public FileDownloader
    {
        Q_OBJECT

    public:
        explicit FtpDownloader(QObject* parent=0);
        ~FtpDownloader();

        bool canDownload() const;
        bool isDownloaded() const;
        QString downloadedFileName() const;
        /* reimp */ void setDownloadedFileName(const QString &name);
        /* reimp */ FtpDownloader* clone( QObject* parent=0 ) const;

    public Q_SLOTS:
        void cancelDownload();

    protected:
        /* reimp */ void onError();
        /* reimp */ void onSuccess();
        void timerEvent(QTimerEvent *event);

    private Q_SLOTS:
        /* reimp */ void doDownload();
        void ftpDone(bool error);
        void ftpCmdStarted(int id);
        void ftpCmdFinished(int id, bool error);
        void ftpStateChanged(int state);
        void ftpDataTransferProgress(qint64 done, qint64 total);
        void ftpReadyRead();

    private:
        struct FtpDownloaderData;
        FtpDownloaderData* d;
    };

    class HttpDownloader : public FileDownloader
    {
        Q_OBJECT

    public:
        explicit HttpDownloader(QObject* parent=0);
        ~HttpDownloader();

        bool canDownload() const;
        bool isDownloaded() const;
        QString downloadedFileName() const;
        /* reimp */ void setDownloadedFileName(const QString &name);
        /* reimp */ HttpDownloader* clone( QObject* parent=0 ) const;

    public Q_SLOTS:
        void cancelDownload();

    protected:
        /* reimp */ void onError();
        /* reimp */ void onSuccess();
        void timerEvent(QTimerEvent *event);

    private Q_SLOTS:
        /* reimp */ void doDownload();
        void httpReadyRead();
        void httpReadProgress( qint64 done, qint64 total );
        void httpError( QNetworkReply::NetworkError );
        void httpDone( bool error );
        void httpReqFinished();

    private:
        struct HttpDownloaderData;
        HttpDownloaderData* d;
    };

    class SignatureVerificationResult;
    class SignatureVerifier;

    class SignatureVerificationDownloader : public FileDownloader
    {
        Q_OBJECT
    public:
        explicit SignatureVerificationDownloader( FileDownloader* downloader, QObject* parent=0 );
        ~SignatureVerificationDownloader();

        QUrl signatureUrl() const;
        void setSignatureUrl( const QUrl& url );

        const SignatureVerifier* signatureVerifier() const;
        void setSignatureVerifier( const SignatureVerifier* verifier );

        SignatureVerificationResult result() const;

        /* reimp */ bool canDownload() const;
        /* reimp */ bool isDownloaded() const;
        /* reimp */ QString downloadedFileName() const;
        /* reimp */ void setDownloadedFileName(const QString &name);
        /* reimp */ FileDownloader* clone( QObject* parent=0 ) const;

    public Q_SLOTS:
        /* reimp */ void cancelDownload();

    protected:
        /* reimp */ void onError();
        /* reimp */ void onSuccess();

    private Q_SLOTS:
        /* reimp */ void doDownload();
        void dataDownloadStarted();
        void dataDownloadCanceled();
        void dataDownloadCompleted();
        void dataDownloadAborted(const QString& errorMessage);
        void signatureDownloadCanceled();
        void signatureDownloadCompleted();
        void signatureDownloadAborted(const QString& errorMessage);

    private:
        class Private;
        kdtools::pimpl_ptr<Private> d;
    };
}

#endif
