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
#include "getrepositorymetainfojob.h"

#include "constants.h"
#include "errors.h"
#include "lib7z_facade.h"
#include "messageboxhandler.h"
#include "packagemanagercore_p.h"
#include "qinstallerglobal.h"

#include "kdupdaterfiledownloader.h"
#include "kdupdaterfiledownloaderfactory.h"

#include <QtCore/QFile>
#include <QtCore/QTimer>
#include <QtCore/QUrl>

#include <QtGui/QMessageBox>

#include <QtNetwork/QAuthenticator>

#include <QtXml/QDomDocument>
#include <QtXml/QDomElement>

using namespace KDUpdater;
using namespace QInstaller;


// -- GetRepositoryMetaInfoJob::ZipRunnable

class GetRepositoryMetaInfoJob::ZipRunnable : public QObject, public QRunnable
{
    Q_OBJECT

public:
    ZipRunnable(const QString &archive, const QString &targetDir, QPointer<FileDownloader> downloader)
        : QObject()
        , QRunnable()
        , m_archive(archive)
        , m_targetDir(targetDir)
        , m_downloader(downloader)
    {}

    ~ZipRunnable()
    {
        if (m_downloader)
            m_downloader->deleteLater();
    }

    void run()
    {
        QFile archive(m_archive);
        if (archive.open(QIODevice::ReadOnly)) {
            try {
                Lib7z::extractArchive(&archive, m_targetDir);
                if (!archive.remove()) {
                    qWarning("Could not delete file %s: %s", qPrintable(m_archive),
                        qPrintable(archive.errorString()));
                }
                emit finished(true, QString());
            } catch (const Lib7z::SevenZipException& e) {
                emit finished(false, tr("Error while extracting %1. Error: %2").arg(m_archive, e.message()));
            } catch (...) {
                emit finished(false, tr("Unknown exception caught while extracting %1.").arg(m_archive));
            }
        } else {
            emit finished(false, tr("Could not open %1 for reading. Error: %2").arg(m_archive,
                archive.errorString()));
        }
    }

Q_SIGNALS:
    void finished(bool success, const QString &errorString);

private:
    const QString m_archive;
    const QString m_targetDir;
    QPointer<FileDownloader> m_downloader;
};


// -- GetRepositoryMetaInfoJob

GetRepositoryMetaInfoJob::GetRepositoryMetaInfoJob(PackageManagerCorePrivate *corePrivate, QObject *parent)
    : KDJob(parent),
    m_canceled(false),
    m_silentRetries(3),
    m_retriesLeft(m_silentRetries),
    m_downloader(0),
    m_waitForDone(false),
    m_corePrivate(corePrivate)
{
    setCapabilities(Cancelable);
}

GetRepositoryMetaInfoJob::~GetRepositoryMetaInfoJob()
{
    if (m_downloader)
        m_downloader->deleteLater();
}

Repository GetRepositoryMetaInfoJob::repository() const
{
    return m_repository;
}

void GetRepositoryMetaInfoJob::setRepository(const Repository &r)
{
    m_repository = r;
    qDebug() << "Setting repository with URL:" << r.url().toString();
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

void GetRepositoryMetaInfoJob::finished(int error, const QString &errorString)
{
    m_waitForDone = true;
    m_threadPool.waitForDone();
    (error > KDJob::NoError) ? emitFinishedWithError(error, errorString) : emitFinished();
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

// Updates.xml download

void GetRepositoryMetaInfoJob::startUpdatesXmlDownload()
{
    if (m_downloader) {
        m_downloader->deleteLater();
        m_downloader = 0;
    }

    const QUrl url = m_repository.url();
    if (url.isEmpty()) {
        finished(QInstaller::InvalidUrl, tr("Empty repository URL."));
        return;
    }

    if (!url.isValid()) {
        finished(QInstaller::InvalidUrl, tr("Invalid repository URL: %1").arg(url.toString()));
        return;
    }

    m_downloader = FileDownloaderFactory::instance().create(url.scheme(), this);
    if (!m_downloader) {
        finished(QInstaller::InvalidUrl, tr("URL scheme not supported: %1 (%2)").arg(url.scheme(),
            url.toString()));
        return;
    }

    // append a random string to avoid proxy caches
    m_downloader->setUrl(QUrl(url.toString() + QString::fromLatin1("/Updates.xml?")
        .append(QString::number(qrand() * qrand()))));

    QAuthenticator auth;
    auth.setUser(m_repository.username());
    auth.setPassword(m_repository.password());
    m_downloader->setAuthenticator(auth);

    m_downloader->setAutoRemoveDownloadedFile(false);
    connect(m_downloader, SIGNAL(downloadCompleted()), this, SLOT(updatesXmlDownloadFinished()));
    connect(m_downloader, SIGNAL(downloadCanceled()), this, SLOT(updatesXmlDownloadCanceled()));
    connect(m_downloader, SIGNAL(downloadAborted(QString)), this,
        SLOT(updatesXmlDownloadError(QString)), Qt::QueuedConnection);
    m_downloader->download();
}

void GetRepositoryMetaInfoJob::updatesXmlDownloadCanceled()
{
    finished(KDJob::Canceled, m_downloader->errorString());
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
        finished(QInstaller::ExtractionError, e.message());
        return;
    }

    QFile updatesFile(fn);
    if (!updatesFile.rename(m_temporaryDirectory + QLatin1String("/Updates.xml"))) {
        finished(QInstaller::DownloadError, tr("Could not move Updates.xml to target location. Error: %1")
            .arg(updatesFile.errorString()));
        return;
    }

    if (!updatesFile.open(QIODevice::ReadOnly)) {
        finished(QInstaller::DownloadError, tr("Could not open Updates.xml for reading. Error: %1")
            .arg(updatesFile.errorString()));
        return;
    }

    QString err;
    QDomDocument doc;
    if (!doc.setContent(&updatesFile, &err)) {
        const QString msg =  tr("Could not fetch a valid version of Updates.xml from repository: %1. "
            "Error: %2").arg(m_repository.url().toString(), err);

        const QMessageBox::StandardButton b =
            MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(),
            QLatin1String("updatesXmlDownloadError"), tr("Download Error"), msg, QMessageBox::Cancel);

        if (b == QMessageBox::Cancel || b == QMessageBox::NoButton) {
            finished(KDJob::Canceled, msg);
            return;
        }
    }

    emit infoMessage(this, tr("Parsing component meta information..."));

    const QDomElement root = doc.documentElement();
    // search for additional repositories that we might need to check
    const QDomNode repositoryUpdate  = root.firstChildElement(QLatin1String("RepositoryUpdate"));
    if (!repositoryUpdate.isNull()) {
        QHash<QString, QPair<Repository, Repository> > repositoryUpdates;
        const QDomNodeList children = repositoryUpdate.toElement().childNodes();
        for (int i = 0; i < children.count(); ++i) {
            const QDomElement el = children.at(i).toElement();
            if (!el.isNull() && el.tagName() == QLatin1String("Repository")) {
                const QString action = el.attribute(QLatin1String("action"));
                if (action == QLatin1String("add")) {
                    // add a new repository to the defaults list
                    Repository repository(el.attribute(QLatin1String("url")), true);
                    repository.setUsername(el.attribute(QLatin1String("username")));
                    repository.setPassword(el.attribute(QLatin1String("password")));
                    repositoryUpdates.insertMulti(action, qMakePair(repository, Repository()));

                    qDebug() << "Repository to add:" << repository.url().toString();
                } else if (action == QLatin1String("remove")) {
                    // remove possible default repositories using the given server url
                    Repository repository(el.attribute(QLatin1String("url")), true);
                    repositoryUpdates.insertMulti(action, qMakePair(repository, Repository()));

                    qDebug() << "Repository to remove:" << repository.url().toString();
                } else if (action == QLatin1String("replace")) {
                    // replace possible default repositories using the given server url
                    Repository oldRepository(el.attribute(QLatin1String("oldUrl")), true);
                    Repository newRepository(el.attribute(QLatin1String("newUrl")), true);
                    newRepository.setUsername(el.attribute(QLatin1String("username")));
                    newRepository.setPassword(el.attribute(QLatin1String("password")));

                    // store the new repository and the one old it replaces
                    repositoryUpdates.insertMulti(action, qMakePair(newRepository, oldRepository));
                    qDebug() << "Replace repository:" << oldRepository.url().toString() << "with:"
                        << newRepository.url().toString();
                } else {
                    qDebug() << "Invalid additional repositories action set in Updates.xml fetched from:"
                        << m_repository.url().toString() << "Line:" << el.lineNumber();
                }
            }
        }

        if (!repositoryUpdates.isEmpty()) {
            if (m_corePrivate->m_settings.updateDefaultRepositories(repositoryUpdates)
                == Settings::UpdatesApplied) {
                    if (m_corePrivate->isUpdater() || m_corePrivate->isPackageManager())
                        m_corePrivate->writeMaintenanceConfigFiles();
                    finished(QInstaller::RepositoryUpdatesReceived, tr("Repository updates received."));
                    return;
            }
        }
    }

    const QDomNodeList children = root.childNodes();
    for (int i = 0; i < children.count(); ++i) {
        const QDomElement el = children.at(i).toElement();
        if (el.isNull())
            continue;
        if (el.tagName() == QLatin1String("PackageUpdate")) {
            const QDomNodeList c2 = el.childNodes();
            for (int j = 0; j < c2.count(); ++j) {
                if (c2.at(j).toElement().tagName() == scName)
                    m_packageNames << c2.at(j).toElement().text();
                else if (c2.at(j).toElement().tagName() == scRemoteVersion)
                    m_packageVersions << c2.at(j).toElement().text();
                else if (c2.at(j).toElement().tagName() == QLatin1String("SHA1"))
                    m_packageHash << c2.at(j).toElement().text();
            }
        }
    }

    setTotalAmount(m_packageNames.count() + 1);
    setProcessedAmount(1);
    emit infoMessage(this, tr("Finished updating component meta information..."));

    if (m_packageNames.isEmpty())
        finished(KDJob::NoError);
    else
        fetchNextMetaInfo();
}

void GetRepositoryMetaInfoJob::updatesXmlDownloadError(const QString &err)
{
    if (m_retriesLeft <= 0) {
        const QString msg = tr("Could not fetch Updates.xml from repository: %1. Error: %2")
            .arg(m_repository.url().toString(), err);

        QMessageBox::StandardButtons buttons = QMessageBox::Retry | QMessageBox::Cancel;
        const QMessageBox::StandardButton b =
            MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(),
            QLatin1String("updatesXmlDownloadError"), tr("Download Error"), msg, buttons);

        if (b == QMessageBox::Cancel || b == QMessageBox::NoButton) {
            finished(KDJob::Canceled, msg);
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
        finished(KDJob::Canceled, m_downloader->errorString());
        return;
    }

    if (m_packageNames.isEmpty() && m_currentPackageName.isEmpty()) {
        finished(KDJob::NoError);
        return;
    }

    QString next = m_currentPackageName;
    QString nextVersion = m_currentPackageVersion;
    if (next.isEmpty()) {
        m_retriesLeft = m_silentRetries;
        next = m_packageNames.takeLast();
        nextVersion = m_packageVersions.takeLast();
    }

    qDebug() << "fetching metadata of" << next << "in version" << nextVersion;

    bool online = true;
    if (m_repository.url().scheme().isEmpty())
        online = false;

    const QString repoUrl = m_repository.url().toString();
    const QUrl url = QString::fromLatin1("%1/%2/%3meta.7z").arg(repoUrl, next,
        online ? nextVersion : QString());
        m_downloader = FileDownloaderFactory::instance().create(url.scheme(), this);

    if (!m_downloader) {
        m_currentPackageName.clear();
        m_currentPackageVersion.clear();
        qWarning() << "Scheme not supported: " << url.toString();
        QMetaObject::invokeMethod(this, "fetchNextMetaInfo", Qt::QueuedConnection);
        return;
    }

    m_currentPackageName = next;
    m_currentPackageVersion = nextVersion;
    m_downloader->setUrl(url);
    m_downloader->setAutoRemoveDownloadedFile(true);

    QAuthenticator auth;
    auth.setUser(m_repository.username());
    auth.setPassword(m_repository.password());
    m_downloader->setAuthenticator(auth);

    connect(m_downloader, SIGNAL(downloadCanceled()), this, SLOT(metaDownloadCanceled()));
    connect(m_downloader, SIGNAL(downloadCompleted()), this, SLOT(metaDownloadFinished()));
    connect(m_downloader, SIGNAL(downloadAborted(QString)), this, SLOT(metaDownloadError(QString)),
        Qt::QueuedConnection);

    m_downloader->download();
}

void GetRepositoryMetaInfoJob::metaDownloadCanceled()
{
    finished(KDJob::Canceled, m_downloader->errorString());
}

void GetRepositoryMetaInfoJob::metaDownloadFinished()
{
    const QString fn = m_downloader->downloadedFileName();
    Q_ASSERT(!fn.isEmpty());

    QFile arch(fn);
    if (!arch.open(QIODevice::ReadOnly)) {
        finished(QInstaller::ExtractionError, tr("Could not open meta info archive: %1. Error: %2").arg(fn,
            arch.errorString()));
        return;
    }

    if (!m_packageHash.isEmpty()) {
        // verify file hash
        QByteArray expectedFileHash = m_packageHash.back().toLatin1();
        QByteArray archContent = arch.readAll();
        QByteArray realFileHash = QString::fromLatin1(QCryptographicHash::hash(archContent,
            QCryptographicHash::Sha1).toHex()).toLatin1();
        if (expectedFileHash != realFileHash) {
            emit infoMessage(this, tr("The hash of one component does not match the expected one."));
            metaDownloadError(tr("Bad hash."));
            return;
        }
        m_packageHash.removeLast();
    }
    arch.close();
    m_currentPackageName.clear();

    ZipRunnable *runnable = new ZipRunnable(fn, m_temporaryDirectory, m_downloader);
    connect(runnable, SIGNAL(finished(bool,QString)), this, SLOT(unzipFinished(bool,QString)));
    m_threadPool.start(runnable);

    if (!m_waitForDone)
        fetchNextMetaInfo();
}

void GetRepositoryMetaInfoJob::metaDownloadError(const QString &err)
{
    if (m_retriesLeft <= 0) {
        const QString msg = tr("Could not download meta information for component: %1. Error: %2")
            .arg(m_currentPackageName, err);

        QMessageBox::StandardButtons buttons = QMessageBox::Retry | QMessageBox::Cancel;
        const QMessageBox::StandardButton b =
            MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(),
            QLatin1String("updatesXmlDownloadError"), tr("Download Error"), msg, buttons);

        if (b == QMessageBox::Cancel || b == QMessageBox::NoButton) {
            finished(KDJob::Canceled, msg);
            return;
        }
    }

    m_retriesLeft--;
    QTimer::singleShot(1500, this, SLOT(fetchNextMetaInfo()));
}

void GetRepositoryMetaInfoJob::unzipFinished(bool ok, const QString &error)
{
    if (!ok)
        finished(QInstaller::ExtractionError, error);
}

#include "getrepositorymetainfojob.moc"
#include "moc_getrepositorymetainfojob.cpp"
