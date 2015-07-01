/**************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
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
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
**
** $QT_END_LICENSE$
**
**************************************************************************/

#ifndef DOWNLOADFILETASK_P_H
#define DOWNLOADFILETASK_P_H

#include "downloadfiletask.h"
#include <observer.h>

#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

#include <memory>
#include <unordered_map>

QT_BEGIN_NAMESPACE
class QSslError;
QT_END_NAMESPACE

namespace QInstaller {

struct Data
{
    Q_DISABLE_COPY(Data)

    Data()
        : file(Q_NULLPTR)
        , observer(Q_NULLPTR)
    {}

    Data(const FileTaskItem &fti)
        : taskItem(fti)
        , file(Q_NULLPTR)
        , observer(new FileTaskObserver(QCryptographicHash::Sha1))
    {}

    FileTaskItem taskItem;
    std::unique_ptr<QFile> file;
    std::unique_ptr<FileTaskObserver> observer;
};

class Downloader : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(Downloader)

public:
    Downloader();
    ~Downloader();

    void download(QFutureInterface<FileTaskResult> &fi, const QList<FileTaskItem> &items,
        QNetworkProxyFactory *networkProxyFactory);

signals:
    void finished();

private slots:
    void doDownload();
    void onReadyRead();
    void onFinished(QNetworkReply *reply);
    void onError(QNetworkReply::NetworkError error);
    void onSslErrors(const QList<QSslError> &sslErrors);
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onAuthenticationRequired(QNetworkReply *reply, QAuthenticator *authenticator);
    void onProxyAuthenticationRequired(const QNetworkProxy &proxy, QAuthenticator *authenticator);
    void onTimeout();

private:
    bool testCanceled();
    QNetworkReply *startDownload(const FileTaskItem &item);

private:
    QFutureInterface<FileTaskResult> *m_futureInterface;

    QTimer m_timer;
    int m_finished;
    QNetworkAccessManager m_nam;
    QList<FileTaskItem> m_items;
    QMultiHash<QNetworkReply*, QUrl> m_redirects;
    std::unordered_map<QNetworkReply*, std::unique_ptr<Data>> m_downloads;
};

}   // namespace QInstaller

#endif // DOWNLOADFILETASK_P_H
