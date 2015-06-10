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
#include "testrepository.h"

#include <kdupdaterfiledownloader.h>
#include <kdupdaterfiledownloaderfactory.h>

#include <QtCore/QFile>

using namespace QInstaller;

TestRepository::TestRepository(QObject *parent)
    : KDJob(parent)
    , m_downloader(0)
{
    setTimeout(10000);
    setAutoDelete(false);
    setCapabilities(Cancelable);
}

TestRepository::~TestRepository()
{
    if (m_downloader)
        m_downloader->deleteLater();
}

Repository TestRepository::repository() const
{
    return m_repository;
}

void TestRepository::setRepository(const Repository &repository)
{
    cancel();

    setError(NoError);
    setErrorString(QString());
    m_repository = repository;
}

void TestRepository::doStart()
{
    if (m_downloader)
        m_downloader->deleteLater();

    const QUrl url = m_repository.url();
    if (url.isEmpty()) {
        emitFinishedWithError(InvalidUrl, tr("Empty repository URL."));
        return;
    }

    m_downloader = KDUpdater::FileDownloaderFactory::instance().create(url.scheme(), this);
    if (!m_downloader) {
        emitFinishedWithError(InvalidUrl, tr("URL scheme not supported: %1 (%2).")
            .arg(url.scheme(), url.toString()));
        return;
    }

    QAuthenticator auth;
    auth.setUser(m_repository.username());
    auth.setPassword(m_repository.password());
    m_downloader->setAuthenticator(auth);

    connect(m_downloader, &KDUpdater::FileDownloader::downloadCompleted,
            this, &TestRepository::downloadCompleted);
    connect(m_downloader, &KDUpdater::FileDownloader::downloadAborted,
            this, &TestRepository::downloadAborted, Qt::QueuedConnection);
    connect(m_downloader, &KDUpdater::FileDownloader::authenticatorChanged,
            this, &TestRepository::onAuthenticatorChanged);

    m_downloader->setAutoRemoveDownloadedFile(true);
    m_downloader->setUrl(QUrl(url.toString() + QString::fromLatin1("/Updates.xml")));

    m_downloader->download();
}

void TestRepository::doCancel()
{
    if (m_downloader) {
        QString errorString = m_downloader->errorString();
        if (errorString.isEmpty())
            errorString = tr("Got a timeout while testing: '%1'").arg(m_repository.displayname());
        // at the moment the download sends downloadCompleted() if we cancel it, so just
        disconnect(m_downloader, 0, this, 0);
        m_downloader->cancelDownload();
        emitFinishedWithError(KDJob::Canceled, errorString);
    }
}

void TestRepository::downloadCompleted()
{
    QString errorMsg;
    int error = DownloadError;

    if (m_downloader->isDownloaded()) {
        QFile file(m_downloader->downloadedFileName());
        if (file.exists() && file.open(QIODevice::ReadOnly)) {
            QDomDocument doc;
            QString errorMsg;
            if (!doc.setContent(&file, &errorMsg)) {
                error = InvalidUpdatesXml;
                errorMsg = tr("Could not parse Updates.xml! Error: %1.").arg(errorMsg);
            } else {
                error = NoError;
            }
        } else {
            errorMsg = tr("Updates.xml could not be opened for reading!");
        }
    } else {
        errorMsg = tr("Updates.xml could not be found on server!");
    }

    if (error > NoError)
        emitFinishedWithError(error, errorMsg);
    else
        emitFinished();

    m_downloader->deleteLater();
    m_downloader = 0;
}

void TestRepository::downloadAborted(const QString &reason)
{
    emitFinishedWithError(DownloadError, reason);
}

void TestRepository::onAuthenticatorChanged(const QAuthenticator &authenticator)
{
    m_repository.setUsername(authenticator.user());
    m_repository.setPassword(authenticator.password());
}
