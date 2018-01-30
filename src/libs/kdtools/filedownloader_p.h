/****************************************************************************
**
** Copyright (C) 2013 Klaralvdalens Datakonsult AB (KDAB)
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
****************************************************************************/

#ifndef FILEDOWNLOADER_P_H
#define FILEDOWNLOADER_P_H

#include "filedownloader.h"

#include <QtNetwork/QNetworkReply>
#include <QNetworkAccessManager>

// these classes are not a part of the public API

namespace KDUpdater {

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
    void onNetworkAccessibleChanged(QNetworkAccessManager::NetworkAccessibility accessible);
#ifndef QT_NO_SSL
    void onSslErrors(QNetworkReply* reply, const QList<QSslError> &errors);
#endif
private:
    void startDownload(const QUrl &url);
    void resumeDownload();

private:
    struct Private;
    Private *d;
};

} // namespace KDUpdater

#endif // FILEDOWNLOADER_P_H
