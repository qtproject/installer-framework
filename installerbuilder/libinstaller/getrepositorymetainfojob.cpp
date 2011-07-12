/**************************************************************************
**
** This file is part of Qt SDK**
**
** Copyright (c) 2011 Nokia Corporation and/or its subsidiary(-ies).*
**
** Contact:  Nokia Corporation qt-info@nokia.com**
**
** No Commercial Usage
**
** This file contains pre-release code and may not be distributed.
** You may use this file in accordance with the terms and conditions
** contained in the Technology Preview License Agreement accompanying
** this package.
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
** rights. These rights are described in the Nokia Qt LGPL Exception version
** 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** If you are unsure which license is appropriate for your use, please contact
** (qt-info@nokia.com).
**
**************************************************************************/
#include "getrepositorymetainfojob.h"

#include "constants.h"
#include "common/errors.h"
#include "common/utils.h"
#include "cryptosignatureverifier.h"
#include "lib7z_facade.h"
#include "messageboxhandler.h"
#include "qinstallerglobal.h"

#include <KDUpdater/FileDownloader>
#include <KDUpdater/FileDownloaderFactory>

#include <QtCore/QFile>
#include <QtCore/QTimer>
#include <QtCore/QUrl>

#include <QtGui/QMessageBox>

#include <QtXml/QDomDocument>
#include <QtXml/QDomElement>

using namespace KDUpdater;
using namespace QInstaller;


// -- GetRepositoryMetaInfoJob

GetRepositoryMetaInfoJob::GetRepositoryMetaInfoJob(const QByteArray &publicKey, QObject *parent)
    : KDJob(parent),
    m_canceled(false),
    m_silentRetries(3),
    m_retriesLeft(m_silentRetries),
    m_publicKey(publicKey),
    m_downloader()
{
    setCapabilities(Cancelable);
}

GetRepositoryMetaInfoJob::~GetRepositoryMetaInfoJob()
{
    delete m_downloader;
}

Repository GetRepositoryMetaInfoJob::repository() const
{
    return m_repository;
}

void GetRepositoryMetaInfoJob::setRepository(const Repository &r)
{
    m_repository = r;
    verbose() << "Setting repository with URL : " << r.url().toString() << std::endl;
}

int GetRepositoryMetaInfoJob::silentRetries() const
{
    return m_silentRetries;
}

void GetRepositoryMetaInfoJob::setSilentRetries(int retries)
{
    m_silentRetries = retries;
}

void GetRepositoryMetaInfoJob::doStart()
{
    m_retriesLeft = m_silentRetries;
    startUpdatesXmlDownload();
}

void GetRepositoryMetaInfoJob::doCancel()
{
    m_canceled = true;
    if (m_downloader)
        m_downloader->cancelDownload();
}

QString GetRepositoryMetaInfoJob::temporaryDirectory() const
{
    return m_temporaryDirectory;
}

QString GetRepositoryMetaInfoJob::releaseTemporaryDirectory() const
{
    m_tempDirDeleter.releaseAll();
    return m_temporaryDirectory;
}

// updates.xml download

void GetRepositoryMetaInfoJob::startUpdatesXmlDownload()
{
    if (m_downloader) {
        m_downloader->deleteLater();
        m_downloader = 0;
    }

    const QUrl url = m_repository.url();
    if (url.isEmpty()) {
        emitFinishedWithError(QInstaller::InvalidUrl, tr("Empty repository URL."));
        return;
    }

    if (!url.isValid()) {
        emitFinishedWithError(QInstaller::InvalidUrl, tr("Invalid repository URL: %1.").arg(url.toString()));
        return;
    }

    m_downloader = FileDownloaderFactory::instance().create(url.scheme(), 0, QUrl(), this);
    if (!m_downloader) {
        emitFinishedWithError(QInstaller::InvalidUrl, tr("URL scheme not supported: %1 (%2).")
            .arg(url.scheme(), url.toString()));
        return;
    }

    m_downloader->setUrl(QUrl(url.toString() + QLatin1String("/Updates.xml")));
    m_downloader->setAutoRemoveDownloadedFile(false);
    connect(m_downloader, SIGNAL(downloadCompleted()), this, SLOT(updatesXmlDownloadFinished()));
    connect(m_downloader, SIGNAL(downloadCanceled()), this, SLOT(updatesXmlDownloadCanceled()));
    connect(m_downloader, SIGNAL(downloadAborted(QString)), this,
        SLOT(updatesXmlDownloadError(QString)), Qt::QueuedConnection);
    m_downloader->download();
}

void GetRepositoryMetaInfoJob::updatesXmlDownloadCanceled()
{
    emitFinishedWithError(KDJob::Canceled, m_downloader->errorString());
}

void GetRepositoryMetaInfoJob::updatesXmlDownloadFinished()
{
    emit infoMessage(this, tr("Retrieving component meta information..."));

    const QString fn = m_downloader->downloadedFileName();
    Q_ASSERT(!fn.isEmpty());
    Q_ASSERT(QFile::exists(fn));

    try {
        m_temporaryDirectory = createTemporaryDirectory(QLatin1String("remoterepo"));
        m_tempDirDeleter.add(m_temporaryDirectory);
    } catch (const QInstaller::Error& e) {
        emitFinishedWithError(QInstaller::ExtractionError, e.message());
        return;
    }

    QFile updatesFile(fn);
    if (!updatesFile.rename(m_temporaryDirectory + QLatin1String("/Updates.xml"))) {
        emitFinishedWithError(QInstaller::DownloadError,
            tr("Could not move Updates.xml to target location: %1.").arg(updatesFile.errorString()));
        return;
    }

    if (!updatesFile.open(QIODevice::ReadOnly)) {
        emitFinishedWithError(QInstaller::DownloadError, tr("Could not open Updates.xml for reading: %1.")
            .arg(updatesFile.errorString()));
        return;
    }

    QString err;
    QDomDocument doc;
    if (!doc.setContent(&updatesFile, &err)) {
        const QString msg =  tr("Could not fetch a valid version of Updates.xml from repository: %1. "
            "Error: %2.").arg(m_repository.url().toString(), err);
        verbose() << msg << std::endl;

        const QMessageBox::StandardButton b =
            MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(),
            QLatin1String("updatesXmlDownloadError"), tr("Download Error"), msg, QMessageBox::Cancel);

        if (b == QMessageBox::Cancel || b == QMessageBox::NoButton) {
            emitFinishedWithError(KDJob::Canceled, msg);
            return;
        }
    }

    emit infoMessage(this, tr("Parsing component meta information..."));

    const QDomElement root = doc.documentElement();
    const QDomNodeList children = root.childNodes();
    for (int i = 0; i < children.count(); ++i) {
        const QDomElement el = children.at(i).toElement();
        if (el.isNull())
            continue;
        if (el.tagName() == QLatin1String("PackageUpdate")) {
            const QDomNodeList c2 = el.childNodes();
            for (int j = 0; j < c2.count(); ++j)
                if (c2.at(j).toElement().tagName() == scName)
                    m_packageNames << c2.at(j).toElement().text();
                else if (c2.at(j).toElement().tagName() == scVersion)
                    m_packageVersions << c2.at(j).toElement().text();
                else if (c2.at(j).toElement().tagName() == QLatin1String("SHA1"))
                    m_packageHash << c2.at(j).toElement().text();
        }
    }

    setTotalAmount(m_packageNames.count() + 1);
    setProcessedAmount(1);
    if (m_packageNames.isEmpty()) {
        emitFinished();
        return;
    }

    emit infoMessage(this, tr("Finished updating component meta information..."));

    fetchNextMetaInfo();
}

void GetRepositoryMetaInfoJob::updatesXmlDownloadError(const QString &err)
{
    if (m_retriesLeft <= 0) {
        const QString msg = tr("Could not fetch Updates.xml from repository: %1. Error: %2.")
            .arg(m_repository.url().toString(), err);
        verbose() << msg << std::endl;

        QMessageBox::StandardButtons buttons = QMessageBox::Retry | QMessageBox::Cancel;
        const QMessageBox::StandardButton b =
            MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(),
            QLatin1String("updatesXmlDownloadError"), tr("Download Error"), msg, buttons);

        if (b == QMessageBox::Cancel || b == QMessageBox::NoButton) {
            emitFinishedWithError(KDJob::Canceled, msg);
            return;
        }
    }

    m_retriesLeft--;
    QTimer::singleShot(1500, this, SLOT(startUpdatesXmlDownload()));
}

// meta data download

void GetRepositoryMetaInfoJob::fetchNextMetaInfo()
{
    emit infoMessage(this, tr("Retrieving component information from remote repository..."));

    if (m_canceled) {
        metaDownloadCanceled();
        return;
    }

    if (m_packageNames.isEmpty() && m_currentPackageName.isEmpty()) {
        emitFinished();
        return;
    }

    QString next = m_currentPackageName;
    QString nextVersion = m_currentPackageVersion;
    if (next.isEmpty()) {
        m_retriesLeft = m_silentRetries;
        next = m_packageNames.takeLast();
        nextVersion = m_packageVersions.takeLast();
    }

    verbose() << "fetching metadata of " << next << " in version " << nextVersion << std::endl;

    bool online = true;
    if (m_repository.url().scheme().isEmpty())
        online = false;

    const QString repoUrl = m_repository.url().toString();
    const QUrl url = QString::fromLatin1("%1/%2/%3meta.7z").arg(repoUrl, next,
        online ? nextVersion : QLatin1String(""));

    // must be deleteLater: this code is executed from slots listening to m_downloader, avoid
    // reentrancy issues
    m_downloader->deleteLater();
    if (!m_publicKey.isEmpty()) {
        const CryptoSignatureVerifier verifier(m_publicKey);
        const QUrl sigUrl = QString::fromLatin1("%1/%2/meta.7z.sig").arg(repoUrl, next);
        m_downloader = FileDownloaderFactory::instance().create(url.scheme(), &verifier, sigUrl, this);
    } else {
        m_downloader = FileDownloaderFactory::instance().create(url.scheme(), this);
    }

    if (!m_downloader) {
        qWarning() << "Scheme not supported:" << next;
        m_currentPackageName.clear();
        m_currentPackageVersion.clear();
        QMetaObject::invokeMethod(this, "fetchNextMetaInfo", Qt::QueuedConnection);
        return;
    }

    m_currentPackageName = next;
    m_currentPackageVersion = nextVersion;
    m_downloader->setUrl(url);
    m_downloader->setAutoRemoveDownloadedFile(true);

    connect(m_downloader, SIGNAL(downloadCanceled()), this, SLOT(metaDownloadCanceled()));
    connect(m_downloader, SIGNAL(downloadCompleted()), this, SLOT(metaDownloadFinished()));
    connect(m_downloader, SIGNAL(downloadAborted(QString)), this, SLOT(metaDownloadError(QString)),
        Qt::QueuedConnection);

    m_downloader->download();
}

void GetRepositoryMetaInfoJob::metaDownloadCanceled()
{
    emitFinishedWithError(KDJob::Canceled, m_downloader->errorString());
}

void GetRepositoryMetaInfoJob::metaDownloadFinished()
{
    const QString fn = m_downloader->downloadedFileName();
    Q_ASSERT(!fn.isEmpty());

    QFile arch(fn);
    if (!arch.open(QIODevice::ReadOnly)) {
        emitFinishedWithError(QInstaller::ExtractionError, tr("Could not open meta info archive: %1. "
            "Error: %2.").arg(fn, arch.errorString()));
        return;
    }

    if (!m_packageHash.isEmpty()) {
        //verify file hash
        QByteArray expectedFileHash = m_packageHash.back().toLatin1();
        QByteArray archContent = arch.readAll();
        QByteArray realFileHash = QString::fromLatin1(QCryptographicHash::hash(archContent,
            QCryptographicHash::Sha1).toHex()).toLatin1();
        if (expectedFileHash != realFileHash) {
            metaDownloadError(tr("Bad hash."));
            return;
        }
        m_packageHash.removeLast();
        m_currentPackageName.clear();
    }

    try {
        // TODO: make this threaded
        Lib7z::extractArchive(&arch, m_temporaryDirectory);
        if (!arch.remove())
            qWarning("Could not delete file %s: %s", qPrintable(fn), qPrintable(arch.errorString()));
    } catch (const Lib7z::SevenZipException& e) {
        emitFinishedWithError(QInstaller::ExtractionError,
            tr("Could not open meta info archive: %1. Error: %2.").arg(fn, e.message()));
        return;
    }

    fetchNextMetaInfo();
}

void GetRepositoryMetaInfoJob::metaDownloadError(const QString &err)
{
    if (err == QObject::tr("Bad signature"))
        emit infoMessage(this, tr("The RSA signature of one component could not be verified."));

    if (err == QObject::tr("Bad hash"))
        emit infoMessage(this, tr("The hash of one component does not match the expected one."));

    if (m_retriesLeft <= 0) {
        const QString msg = tr("Could not download meta information for component: %1. Error: %2.")
            .arg(m_currentPackageName, err);
        verbose() << msg << std::endl;

        QMessageBox::StandardButtons buttons = QMessageBox::Retry | QMessageBox::Cancel;
        const QMessageBox::StandardButton b =
            MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(),
            QLatin1String("updatesXmlDownloadError"), tr("Download Error"), msg, buttons);

        if (b == QMessageBox::Cancel || b == QMessageBox::NoButton) {
            emitFinishedWithError(KDJob::Canceled, msg);
            return;
        }
    }

    m_retriesLeft--;
    QTimer::singleShot(1500, this, SLOT(fetchNextMetaInfo()));
}
