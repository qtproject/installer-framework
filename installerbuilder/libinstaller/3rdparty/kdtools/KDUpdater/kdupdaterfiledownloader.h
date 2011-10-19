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

#include "kdupdater.h"
#include <KDToolsCore/pimpl_ptr.h>

#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtCore/QCryptographicHash>

namespace KDUpdater
{
    KDTOOLS_UPDATER_EXPORT QByteArray calculateHash( QIODevice* device, QCryptographicHash::Algorithm algo );
    KDTOOLS_UPDATER_EXPORT QByteArray calculateHash( const QString& path, QCryptographicHash::Algorithm algo );
    
    class HashVerificationJob;

    class KDTOOLS_UPDATER_EXPORT FileDownloader : public QObject
    {
        Q_OBJECT
        Q_PROPERTY( bool autoRemoveDownloadedFile READ isAutoRemoveDownloadedFile WRITE setAutoRemoveDownloadedFile )
        Q_PROPERTY( QUrl url READ url WRITE setUrl )
        Q_PROPERTY( QString scheme READ scheme )

    public:
        explicit FileDownloader(const QString& scheme, QObject* parent=0);
        ~FileDownloader();

        void setUrl(const QUrl& url);
        QUrl url() const;

        void setSha1Sum( const QByteArray& sha1 );
        QByteArray sha1Sum() const;

        QString errorString() const;
        QString scheme() const;

        virtual bool canDownload() const = 0;
        virtual bool isDownloaded() const = 0;
        virtual QString downloadedFileName() const = 0;
        virtual void setDownloadedFileName(const QString &name) = 0;
        virtual FileDownloader* clone( QObject* parent=0 ) const = 0;

        void download();

        void setAutoRemoveDownloadedFile(bool val);
        bool isAutoRemoveDownloadedFile() const;

        void setFollowRedirects( bool val );
        bool followRedirects() const;

    public Q_SLOTS:
        virtual void cancelDownload();
        void sha1SumVerified( KDUpdater::HashVerificationJob* job );

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

#ifndef Q_MOC_RUN
    private:
#endif
        void downloadCompleted();
        void downloadAborted(const QString& errorMessage);

    protected:
        void setDownloadCanceled();
        void setDownloadCompleted( const QString& filepath );
        void setDownloadAborted( const QString& error );

        void runDownloadSpeedTimer();
        void stopDownloadSpeedTimer();

        void addSample(qint64 sample);
        int downloadSpeedTimerId() const;
        void setProgress(qint64 bytesReceived, qint64 bytesToReceive);

        void emitDownloadSpeed();
        void emitDownloadStatus();
        void emitDownloadProgress();
        void emitEstimatedDownloadTime();

    private Q_SLOTS:
        virtual void doDownload() = 0;

    private:
        struct FileDownloaderData;
        kdtools::pimpl_ptr<FileDownloaderData> d;
    };
}

#endif
