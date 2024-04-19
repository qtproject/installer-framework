/**************************************************************************
**
** Copyright (C) 2024 The Qt Company Ltd.
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
**************************************************************************/
#include "metadatajob.h"

#include "metadatajob_p.h"
#include "packagemanagercore.h"
#include "packagemanagerproxyfactory.h"
#include "productkeycheck.h"
#include "proxycredentialsdialog.h"
#include "serverauthenticationdialog.h"
#include "settings.h"
#include "testrepository.h"
#include "globals.h"

#include <QTemporaryDir>
#include <QtConcurrent>
#include <QtMath>
#include <QRandomGenerator>
#include <QApplication>

namespace QInstaller {

/*!
    \enum QInstaller::DownloadType

    \value All
    \value CompressedPackage
*/

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::MetadataJob
    \internal
*/

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::UnzipArchiveTask
    \internal
*/

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::UnzipArchiveException
    \internal
*/

static QUrl resolveUrl(const FileTaskResult &result, const QString &url)
{
    QUrl u(url);
    if (u.isRelative())
        return QUrl(result.taskItem().source()).resolved(u);
    return u;
}

MetadataJob::MetadataJob(QObject *parent)
    : Job(parent)
    , m_core(nullptr)
    , m_downloadType(DownloadType::All)
    , m_downloadableChunkSize(1000)
    , m_taskNumber(0)
    , m_defaultRepositoriesFetched(false)
{
    QByteArray downloadableChunkSize = qgetenv("IFW_METADATA_SIZE");
    if (!downloadableChunkSize.isEmpty()) {
        int chunkSize = QString::fromLocal8Bit(downloadableChunkSize).toInt();
        if (chunkSize > 0)
            m_downloadableChunkSize = chunkSize;
    }

    setCapabilities(Cancelable);
    connect(&m_xmlTask, &QFutureWatcherBase::finished, this, &MetadataJob::xmlTaskFinished);
    connect(&m_metadataTask, &QFutureWatcherBase::finished, this, &MetadataJob::metadataTaskFinished);
    connect(&m_metadataTask, &QFutureWatcherBase::progressValueChanged, this, &MetadataJob::progressChanged);
    connect(&m_updateCacheTask, &QFutureWatcherBase::finished, this, &MetadataJob::updateCacheTaskFinished);
}

MetadataJob::~MetadataJob()
{
    resetCompressedFetch();
    reset();

    if (!m_core)
        return;

    if (m_metaFromCache.isValid() && !m_core->settings().persistentLocalCache())
        m_metaFromCache.clear();
}

/*
 * Parse the metadata of currently selected repositories. We cannot
 * return all metadata as that contains metadata also from categorized archived
 * repositories which might not be currently selected.
 */

QList<Metadata *> MetadataJob::metadata() const
{
    const QSet<RepositoryCategory> categories = m_core->settings().repositoryCategories();
    QHash<RepositoryCategory, QSet<Repository>> repositoryHash;
    // Create hash of categorized repositories to avoid constructing
    // excess temp objects when filtering below.
    for (const RepositoryCategory &category : categories)
        repositoryHash.insert(category, category.repositories());

    QList<Metadata *> metadata = m_metaFromCache.items();
    // Filter cache items not associated with current repositories and categories
    QtConcurrent::blockingFilter(metadata, [&](const Metadata *item) {
        if (!item->isActive())
            return false;

        // No need to check if the repository is enabled here. Changing the network
        // settings resets the cache and we don't fetch the disabled repositories,
        // so the cached items stay inactive.

        if (item->isAvailableFromDefaultRepository())
            return true;

        QHash<RepositoryCategory, QSet<Repository>>::const_iterator it;
        for (it = repositoryHash.constBegin(); it != repositoryHash.constEnd(); ++it) {
            if (!it.key().isEnabled())
                continue; // Let's try the next one

            if (it->contains(item->repository()))
                return true;
        }
        return false;
    });

    return metadata;
}

/*
    Returns a repository object from the cache item matching \a directory. If the
    \a directory does not belong to the cache, an empty repository is returned.
*/
Repository MetadataJob::repositoryForCacheDirectory(const QString &directory) const
{
    QDir dir(directory);
    if (!QDir::fromNativeSeparators(dir.path())
            .startsWith(QDir::fromNativeSeparators(m_metaFromCache.path()))) {
        return Repository();
    }
    const QString dirName = dir.dirName();
    Metadata *cachedMeta = m_metaFromCache.itemByChecksum(dirName.toUtf8());
    if (cachedMeta)
        return cachedMeta->repository();

    return Repository();
}

bool MetadataJob::resetCache(bool init)
{
    // Need the path from current settings
    if (!m_core) {
        qCWarning(lcInstallerInstallLog) << "Cannot reset metadata cache: "
            "missing package manager core engine.";
        return false;
    }

    if (m_metaFromCache.isValid() && !m_core->settings().persistentLocalCache())
        m_metaFromCache.clear();

    m_metaFromCache.setPath(m_core->settings().localCachePath());

    if (!init)
        return true;

    const bool success = m_metaFromCache.initialize();
    if (success) {
        qCDebug(QInstaller::lcInstallerInstallLog) << "Using metadata cache from"
            << m_metaFromCache.path();
        qCDebug(QInstaller::lcInstallerInstallLog) << "Found"
            << m_metaFromCache.items().count() << "cached items.";
    }
    return success;
}

bool MetadataJob::clearCache()
{
    if (m_metaFromCache.clear())
        return true;

    setError(JobError::CacheError);
    setErrorString(m_metaFromCache.errorString());
    return false;
}

bool MetadataJob::isValidCache() const
{
    return m_metaFromCache.isValid();
}

// -- private slots

void MetadataJob::doStart()
{
    setError(Job::NoError);
    setErrorString(QString());
    m_metadataResult.clear();
    setProgressTotalAmount(100);

    if (!m_core) {
        emitFinishedWithError(Job::Canceled, tr("Missing package manager core engine."));
        return; // We can't do anything here without core, so avoid tons of !m_core checks.
    }
    if (!m_metaFromCache.isValid() && !resetCache(true)) {
        emitFinishedWithError(JobError::CacheError, m_metaFromCache.errorString());
        return;
    }

    const ProductKeyCheck *const productKeyCheck = ProductKeyCheck::instance();
    if (m_downloadType != DownloadType::CompressedPackage) {
        emit infoMessage(this, tr("Fetching latest update information..."));
        const bool onlineInstaller = m_core->isInstaller() && !m_core->isOfflineOnly();
        const QSet<Repository> repositories = getRepositories();

        if (onlineInstaller || m_core->isMaintainer()
                || (m_core->settings().allowRepositoriesForOfflineInstaller() && !repositories.isEmpty())) {
            static const QString updateFilePath(QLatin1Char('/') + scUpdatesXML + QLatin1Char('?'));
            static const QString randomQueryString = QString::number(QRandomGenerator::global()->generate());

            quint64 cachedCount = 0;
            setProgressTotalAmount(0); // Show only busy indicator during this loop as we have no progress to measure
            foreach (const Repository &repo, repositories) {
                // For not blocking the UI
                qApp->processEvents();

                if (repo.isEnabled() &&
                        productKeyCheck->isValidRepository(repo)) {
                    QAuthenticator authenticator;
                    authenticator.setUser(repo.username());
                    authenticator.setPassword(repo.password());

                    if (repo.isCompressed())
                        continue;

                    QString url;
                    url = repo.url().toString() + updateFilePath;
                    if (!m_core->value(scUrlQueryString).isEmpty())
                        url += m_core->value(scUrlQueryString) + QLatin1Char('&');
                    // also append a random string to avoid proxy caches
                    url.append(randomQueryString);

                    // Check if we can skip downloading already cached repositories
                    const Status foundStatus = findCachedUpdatesFile(repo, url);
                    if (foundStatus == XmlDownloadSuccess) {
                        // Found existing Updates.xml
                        ++cachedCount;
                        continue;
                    } else if (foundStatus == XmlDownloadRetry) {
                         // Repositories changed, restart with the new repositories
                        QMetaObject::invokeMethod(this, "doStart", Qt::QueuedConnection);
                        return;
                    }

                    QTemporaryDir tmp(QDir::tempPath() + QLatin1String("/remoterepo-XXXXXX"));
                    if (!tmp.isValid()) {
                        qCWarning(QInstaller::lcInstallerInstallLog) << "Cannot create unique temporary directory.";
                        continue;
                    }
                    tmp.setAutoRemove(false);
                    m_tempDirDeleter.add(tmp.path());
                    FileTaskItem item(url, tmp.path() + QLatin1String("/Updates.xml"));
                    item.insert(TaskRole::UserRole, QVariant::fromValue(repo));
                    item.insert(TaskRole::Authenticator, QVariant::fromValue(authenticator));
                    m_updatesXmlItems.append(item);
                }
            }
            setProgressTotalAmount(100);
            const quint64 totalCount = repositories.count();
            if (cachedCount > 0) {
                qCDebug(lcInstallerInstallLog).nospace() << "Loaded from cache "
                    << cachedCount << "/" << totalCount << ". Downloading remaining "
                    << m_updatesXmlItems.count() << "/" << totalCount <<".";
            } else {
                qCDebug(lcInstallerInstallLog).nospace() <<"Downloading " <<  m_updatesXmlItems.count()
                                                         << " items to cache.";
            }
            if (m_updatesXmlItems.count() > 0) {
                double taskCount = m_updatesXmlItems.length()/static_cast<double>(m_downloadableChunkSize);
                m_totalTaskCount = qCeil(taskCount);
                m_taskNumber = 0;
                startXMLTask();
            } else {
                emitFinished();
            }
        } else {
            emitFinished();
        }
    } else {
        resetCompressedFetch();

        bool repositoriesFound = false;
        foreach (const Repository &repo, m_core->settings().temporaryRepositories()) {
            if (repo.isCompressed() && repo.isEnabled() &&
                    productKeyCheck->isValidRepository(repo)) {
                repositoriesFound = true;
                startUnzipRepositoryTask(repo);

                //Set the repository disabled so we don't handle it many times
                Repository replacement = repo;
                replacement.setEnabled(false);
                Settings &s = m_core->settings();
                QSet<Repository> temporaries = s.temporaryRepositories();
                if (temporaries.contains(repo)) {
                    temporaries.remove(repo);
                    temporaries.insert(replacement);
                    s.setTemporaryRepositories(temporaries, true);
                }
            }
        }
        if (!repositoriesFound) {
            emitFinished();
        }
        else {
            setProgressTotalAmount(0);
            emit infoMessage(this, tr("Unpacking compressed repositories. This may take a while..."));
        }
    }
}

bool MetadataJob::startXMLTask()
{
    int chunkSize = qMin(m_updatesXmlItems.length(), m_downloadableChunkSize);
    QList<FileTaskItem> tempPackages = m_updatesXmlItems.mid(0, chunkSize);
    m_updatesXmlItems = m_updatesXmlItems.mid(chunkSize, m_updatesXmlItems.length());
    if (tempPackages.length() > 0) {
        DownloadFileTask *const xmlTask = new DownloadFileTask(tempPackages);
        xmlTask->setProxyFactory(m_core->proxyFactory());
        connect(&m_xmlTask, &QFutureWatcher<FileTaskResult>::progressValueChanged, this,
                &MetadataJob::progressChanged);
        m_xmlTask.setFuture(QtConcurrent::run(&DownloadFileTask::doTask, xmlTask));

        setInfoMessage(tr("Retrieving information from remote repositories..."));
        return true;
    }
    return false;
}

void MetadataJob::doCancel()
{
    reset();
    resetCache();
    emitFinishedWithError(Job::Canceled, tr("Metadata download canceled."));
}

void MetadataJob::startUnzipRepositoryTask(const Repository &repo)
{
    QTemporaryDir tempRepoDir(QDir::tempPath() + QLatin1String("/compressedRepo-XXXXXX"));
    if (!tempRepoDir.isValid()) {
        qCWarning(QInstaller::lcInstallerInstallLog) << "Cannot create unique temporary directory.";
        return;
    }
    tempRepoDir.setAutoRemove(false);
    m_tempDirDeleter.add(tempRepoDir.path());
    QString url = repo.url().toLocalFile();
    UnzipArchiveTask *task = new UnzipArchiveTask(url, tempRepoDir.path());
    QFutureWatcher<void> *watcher = new QFutureWatcher<void>();
    m_unzipRepositoryTasks.insert(watcher, qobject_cast<QObject*> (task));
    connect(watcher, &QFutureWatcherBase::finished, this,
        &MetadataJob::unzipRepositoryTaskFinished);
    connect(watcher, &QFutureWatcherBase::progressValueChanged, this,
        &MetadataJob::progressChanged);
    watcher->setFuture(QtConcurrent::run(&UnzipArchiveTask::doTask, task));
}

void MetadataJob::startUpdateCacheTask()
{
    const int toRegisterCount = m_fetchedMetadata.count();
    if (toRegisterCount > 0)
        emit infoMessage(this, tr("Updating local cache with %n new items...",
                                  nullptr, toRegisterCount));

    UpdateCacheTask *task = new UpdateCacheTask(m_metaFromCache, m_fetchedMetadata);
    m_updateCacheTask.setFuture(QtConcurrent::run(&UpdateCacheTask::doTask, task));
}

/*
    Resets the repository information from all cache items, which
    makes them inactive until associated with new repositories.
*/
void MetadataJob::resetCacheRepositories()
{
    for (auto *metaToReset : m_metaFromCache.items())
        metaToReset->setRepository(Repository());
}

void MetadataJob::unzipRepositoryTaskFinished()
{
    QFutureWatcher<void> *watcher = static_cast<QFutureWatcher<void> *>(sender());
    int error = Job::NoError;
    QString errorString;
    try {
        watcher->waitForFinished();    // trigger possible exceptions

        QHashIterator<QFutureWatcher<void> *, QObject*> i(m_unzipRepositoryTasks);
        while (i.hasNext()) {

            i.next();
            if (i.key() == watcher) {
                UnzipArchiveTask *task = qobject_cast<UnzipArchiveTask*> (i.value());
                QString url = task->target();

                QUrl targetUrl = targetUrl.fromLocalFile(url);
                Repository repo(targetUrl, false, true);
                url = repo.url().toString() + QLatin1String("/Updates.xml");
                TestRepository testJob(m_core);
                testJob.setRepository(repo);
                testJob.start();
                testJob.waitForFinished();
                error = testJob.error();
                errorString = testJob.errorString();
                if (error == Job::NoError) {
                    QTemporaryDir tmp(QDir::tempPath() + QLatin1String("/remoterepo-XXXXXX"));
                    if (!tmp.isValid()) {
                        qCWarning(QInstaller::lcInstallerInstallLog) << "Cannot create unique temporary directory.";
                        continue;
                    }
                    tmp.setAutoRemove(false);
                    m_tempDirDeleter.add(tmp.path());
                    FileTaskItem item(url, tmp.path() + QLatin1String("/Updates.xml"));

                    item.insert(TaskRole::UserRole, QVariant::fromValue(repo));
                    m_updatesXmlItems.append(item);
                } else {
                    //Repository is not valid, remove it
                    Settings &s = m_core->settings();
                    QSet<Repository> temporaries = s.temporaryRepositories();
                    foreach (Repository repository, temporaries) {
                        if (repository.url().toLocalFile() == task->archive()) {
                            temporaries.remove(repository);
                        }
                    }
                    s.setTemporaryRepositories(temporaries, false);
                }
            }
        }
        delete m_unzipRepositoryTasks.value(watcher);
        m_unzipRepositoryTasks.remove(watcher);
        delete watcher;

        //One can specify many zipped repository items at once. As the repositories are
        //unzipped one by one, we collect here all items before parsing xml files from those.
       if (m_updatesXmlItems.count() > 0 && m_unzipRepositoryTasks.isEmpty()) {
            startXMLTask();
        } else {
            if (error != Job::NoError) {
                emitFinishedWithError(QInstaller::DownloadError, errorString);
            }
        }

    } catch (const UnzipArchiveException &e) {
        reset();
        emitFinishedWithError(QInstaller::ExtractionError, e.message());
    } catch (const QUnhandledException &e) {
        reset();
        emitFinishedWithError(QInstaller::DownloadError, QLatin1String(e.what()));
    } catch (...) {
        reset();
        emitFinishedWithError(QInstaller::DownloadError, tr("Unknown exception during extracting."));
    }
}

void MetadataJob::xmlTaskFinished()
{
    Status status = XmlDownloadFailure;
    try {
        m_xmlTask.waitForFinished();
        m_updatesXmlResult.append(m_xmlTask.future().results());
        if (!startXMLTask()) {
            status = parseUpdatesXml(m_updatesXmlResult);
            m_updatesXmlResult.clear();
        } else {
            return;
        }
    } catch (const AuthenticationRequiredException &e) {
        if (e.type() == AuthenticationRequiredException::Type::Proxy) {
            qCWarning(QInstaller::lcInstallerInstallLog) << e.message();
            QString username;
            QString password;
            const QNetworkProxy proxy = e.proxy();
            if (m_core->isCommandLineInstance()) {
                qCDebug(QInstaller::lcInstallerInstallLog).noquote() << QString::fromLatin1("The proxy %1:%2 requires a username and password").arg(proxy.hostName(), proxy.port());
                askForCredentials(&username, &password, QLatin1String("Username: "), QLatin1String("Password: "));
            } else {
                ProxyCredentialsDialog proxyCredentials(proxy);
                if (proxyCredentials.exec() == QDialog::Accepted) {
                    username = proxyCredentials.userName();
                    password = proxyCredentials.password();
                }
            }
            if (!username.isEmpty()) {
                qCDebug(QInstaller::lcInstallerInstallLog) << "Retrying with new credentials ...";
                PackageManagerProxyFactory *factory = m_core->proxyFactory();

                factory->setProxyCredentials(proxy, username, password);
                m_core->setProxyFactory(factory);
                status = XmlDownloadRetry;
            } else {
                reset();
                emitFinishedWithError(QInstaller::DownloadError, tr("Missing proxy credentials."));
            }
        } else if (e.type() == AuthenticationRequiredException::Type::Server) {
            qCWarning(QInstaller::lcInstallerInstallLog) << e.message();
            QString username;
            QString password;
            if (m_core->isCommandLineInstance()) {
                qCDebug(QInstaller::lcInstallerInstallLog) << "Server Requires Authentication";
                qCDebug(QInstaller::lcInstallerInstallLog) << "You need to supply a username and password to access this site.";
                askForCredentials(&username, &password, QLatin1String("Username: "), QLatin1String("Password: "));
            } else {
                ServerAuthenticationDialog dlg(e.message(), e.taskItem());
                if (dlg.exec() == QDialog::Accepted) {
                    username = dlg.user();
                    password = dlg.password();
                }
            }
            if (!username.isEmpty()) {
                Repository original = e.taskItem().value(TaskRole::UserRole)
                    .value<Repository>();
                Repository replacement = original;
                replacement.setUsername(username);
                replacement.setPassword(password);

                Settings &s = m_core->settings();
                QSet<Repository> temporaries = s.temporaryRepositories();
                if (temporaries.contains(original)) {
                    temporaries.remove(original);
                    temporaries.insert(replacement);
                    s.addTemporaryRepositories(temporaries, true);
                } else {
                    QMultiHash<QString, QPair<Repository, Repository> > update;
                    update.insert(QLatin1String("replace"), qMakePair(original, replacement));

                    if (s.updateRepositoryCategories(update) == Settings::UpdatesApplied)
                        qCDebug(QInstaller::lcDeveloperBuild) << "Repository categories updated.";

                    if (s.updateDefaultRepositories(update) == Settings::UpdatesApplied
                        || s.updateUserRepositories(update) == Settings::UpdatesApplied) {
                            if (m_core->isMaintainer()) {
                                bool gainedAdminRights = false;
                                if (!m_core->directoryWritable(m_core->value(scTargetDir))) {
                                    m_core->gainAdminRights();
                                    gainedAdminRights = true;
                                }
                                m_core->writeMaintenanceConfigFiles();
                                if (gainedAdminRights)
                                    m_core->dropAdminRights();
                            }
                    }
                }
                status = XmlDownloadRetry;
            } else {
                reset();
                emitFinishedWithError(QInstaller::DownloadError, tr("Authentication failed."));
            }
        }
    } catch (const TaskException &e) {
        reset();
        emitFinishedWithError(QInstaller::DownloadError, e.message());
    } catch (const QUnhandledException &e) {
        reset();
        emitFinishedWithError(QInstaller::DownloadError, QLatin1String(e.what()));
    } catch (...) {
        reset();
        emitFinishedWithError(QInstaller::DownloadError, tr("Unknown exception during download."));
    }

    if (error() != Job::NoError)
        return;

    if (status == XmlDownloadSuccess) {
        if (!fetchMetaDataPackages()) {
            // No new metadata packages to fetch, still need to update the cache
            // for refreshed repositories.
            startUpdateCacheTask();
        }
    } else if (status == XmlDownloadRetry) {
        reset();
        QMetaObject::invokeMethod(this, "doStart", Qt::QueuedConnection);
    } else {
        reset();
        emitFinishedWithError(QInstaller::DownloadError, tr("Failure to fetch repositories."));
    }
}

void MetadataJob::unzipTaskFinished()
{
    QFutureWatcher<void> *watcher = static_cast<QFutureWatcher<void> *>(sender());
    try {
        watcher->waitForFinished();    // trigger possible exceptions
    } catch (const UnzipArchiveException &e) {
        emitFinishedWithError(QInstaller::ExtractionError, e.message());
    } catch (const QUnhandledException &e) {
        emitFinishedWithError(QInstaller::DownloadError, QLatin1String(e.what()));
    } catch (...) {
        emitFinishedWithError(QInstaller::DownloadError, tr("Unknown exception during extracting."));
    }

    if (error() != Job::NoError)
        return;

    delete m_unzipTasks.value(watcher);
    m_unzipTasks.remove(watcher);
    delete watcher;

    if (m_unzipTasks.isEmpty())
        startUpdateCacheTask();
}

void MetadataJob::progressChanged(int progress)
{
    setProcessedAmount(progress);
}

void MetadataJob::setProgressTotalAmount(int maximum)
{
    setTotalAmount(maximum);
}

void MetadataJob::metadataTaskFinished()
{
    try {
        m_metadataTask.waitForFinished();
        m_metadataResult.append(m_metadataTask.future().results());
        if (!fetchMetaDataPackages()) {
            if (m_metadataResult.count() > 0) {
                emit infoMessage(this, tr("Extracting meta information..."));
                foreach (const FileTaskResult &result, m_metadataResult) {
                    const FileTaskItem item = result.value(TaskRole::TaskItem).value<FileTaskItem>();
                    if (result.value(TaskRole::ChecksumMismatch).toBool()) {
                        QString mismatchMessage = tr("Checksum mismatch detected for \"%1\".")
                                .arg(item.value(TaskRole::SourceFile).toString());
                        if (m_core->settings().allowUnstableComponents()) {
                            m_shaMissmatchPackages.append(item.value(TaskRole::Name).toString());
                            qCWarning(QInstaller::lcInstallerInstallLog) << mismatchMessage;
                        } else {
                            throw QInstaller::TaskException(mismatchMessage);
                        }
                        QFileInfo fi(result.target());
                        QString targetPath = fi.absolutePath();
                        if (m_fetchedMetadata.contains(targetPath)) {
                            delete m_fetchedMetadata.value(targetPath);
                            m_fetchedMetadata.remove(targetPath);
                        }
                        continue;
                    }
                    UnzipArchiveTask *task = new UnzipArchiveTask(result.target(),
                        item.value(TaskRole::UserRole).toString());
                    task->setRemoveArchive(true);
                    task->setStoreChecksums(true);

                    QFutureWatcher<void> *watcher = new QFutureWatcher<void>();
                    m_unzipTasks.insert(watcher, qobject_cast<QObject*> (task));
                    connect(watcher, &QFutureWatcherBase::finished, this, &MetadataJob::unzipTaskFinished);
                    watcher->setFuture(QtConcurrent::run(&UnzipArchiveTask::doTask, task));
                }
                if (m_unzipTasks.isEmpty())
                    startUpdateCacheTask();

            } else {
                startUpdateCacheTask();
            }
        }
    } catch (const TaskException &e) {
        reset();
        emitFinishedWithError(QInstaller::DownloadError, e.message());
    } catch (const QUnhandledException &e) {
        reset();
        emitFinishedWithError(QInstaller::DownloadError, QLatin1String(e.what()));
    } catch (...) {
        reset();
        emitFinishedWithError(QInstaller::DownloadError, tr("Unknown exception during download."));
    }
}

void MetadataJob::updateCacheTaskFinished()
{
    try {
        m_updateCacheTask.waitForFinished();
    } catch (const CacheTaskException &e) {
        emitFinishedWithError(QInstaller::CacheError, e.message());
    } catch (const QUnhandledException &e) {
        emitFinishedWithError(QInstaller::CacheError, QLatin1String(e.what()));
    } catch (...) {
        emitFinishedWithError(QInstaller::CacheError, tr("Unknown exception during updating cache."));
    }

    if (error() != Job::NoError)
        return;

    setProcessedAmount(100);
    emitFinished();
}


// -- private

bool MetadataJob::fetchMetaDataPackages()
{
    //Download files in chunks. QtConcurrent will choke if too many task is given to it
    int chunkSize = qMin(m_packages.length(), m_downloadableChunkSize);
    QList<FileTaskItem> tempPackages = m_packages.mid(0, chunkSize);
    m_packages = m_packages.mid(chunkSize, m_packages.length());
    if (tempPackages.length() > 0) {
        setProcessedAmount(0);
        DownloadFileTask *const metadataTask = new DownloadFileTask(tempPackages);
        metadataTask->setProxyFactory(m_core->proxyFactory());
        m_metadataTask.setFuture(QtConcurrent::run(&DownloadFileTask::doTask, metadataTask));
        setInfoMessage(tr("Retrieving meta information from remote repository..."));
        return true;
    }
    return false;
}

void MetadataJob::reset()
{
    m_packages.clear();
    m_updatesXmlItems.clear();
    m_defaultRepositoriesFetched = false;
    m_fetchedCategorizedRepositories.clear();

    qDeleteAll(m_fetchedMetadata);
    m_fetchedMetadata.clear();

    setError(Job::NoError);
    setErrorString(QString());
    setCapabilities(Cancelable);

    try {
        m_xmlTask.cancel();
        m_xmlTask.waitForFinished();
        m_metadataTask.cancel();
        m_metadataTask.waitForFinished();
    } catch (...) {}
    m_tempDirDeleter.releaseAndDeleteAll();
    m_metadataResult.clear();
    m_updatesXmlResult.clear();
    m_taskNumber = 0;
}

void MetadataJob::resetCompressedFetch()
{
    setError(Job::NoError);
    setErrorString(QString());

    try {
        foreach (QFutureWatcher<void> *const watcher, m_unzipTasks.keys()) {
            watcher->cancel();
            watcher->deleteLater();
        }
        foreach (QObject *const object, m_unzipTasks)
            object->deleteLater();
        m_unzipTasks.clear();

        foreach (QFutureWatcher<void> *const watcher, m_unzipRepositoryTasks.keys()) {
            watcher->cancel();
            watcher->deleteLater();
        }
        foreach (QObject *const object, m_unzipRepositoryTasks)
            object->deleteLater();
        m_unzipRepositoryTasks.clear();
    } catch (...) {}
}

MetadataJob::Status MetadataJob::parseUpdatesXml(const QList<FileTaskResult> &results)
{
    foreach (const FileTaskResult &result, results) {
        if (error() != Job::NoError)
            return XmlDownloadFailure;

        //If repository is not found, target might be empty. Do not continue parsing the
        //repository and do not prevent further repositories usage.
        if (result.target().isEmpty()) {
            continue;
        }

        QFileInfo fileInfo(result.target());
        std::unique_ptr<Metadata> metadata(new Metadata(fileInfo.absolutePath()));
        QFile file(result.target());
        if (!file.open(QIODevice::ReadOnly)) {
            qCWarning(QInstaller::lcInstallerInstallLog) << "Cannot open Updates.xml for reading:"
                << file.errorString();
            return XmlDownloadFailure;
        }
        const FileTaskItem item = result.value(TaskRole::TaskItem).value<FileTaskItem>();
        const Repository repository = item.value(TaskRole::UserRole).value<Repository>();

        QCryptographicHash hash(QCryptographicHash::Sha1);
        hash.addData(&file);
        const QByteArray updatesChecksum = hash.result().toHex();

        if (!repository.xmlChecksum().isEmpty() && updatesChecksum != repository.xmlChecksum()) {
            qCWarning(lcDeveloperBuild).noquote().nospace() << "The checksum for Updates.xml "
                "file downloaded from repository:\n" << repository.url().toString() << "\ndoes not "
                "match the expected value:\n\tActual SHA1: " << updatesChecksum << "\n\tExpected SHA1: "
                << repository.xmlChecksum() << Qt::endl;
        }

        bool refreshed;
        // Check if we have cached the metadata for this repository already
        Status status = refreshCacheItem(result, updatesChecksum, &refreshed);
        if (status != XmlDownloadSuccess)
            return status;

        if (refreshed) // Found existing metadata
            continue;

        metadata->setChecksum(updatesChecksum);

        file.seek(0);

        QString error;
        QDomDocument doc;
        if (!doc.setContent(&file, &error)) {
            qCWarning(QInstaller::lcInstallerInstallLog).nospace() << "Cannot fetch a valid version of Updates.xml from repository "
                               << metadata->repository().displayname() << ": " << error;
            //If there are other repositories, try to use those
            continue;
        }
        file.close();

        metadata->setRepository(repository);
        const bool online = !(metadata->repository().url().scheme()).isEmpty();

        bool testCheckSum = true;
        const QDomElement root = doc.documentElement();
        const QDomNode checksum = root.firstChildElement(QLatin1String("Checksum"));
        if (!checksum.isNull())
            testCheckSum = (checksum.toElement().text().toLower() == scTrue);

        // If we have top level sha1 and MetadataName elements, we have compressed
        // all metadata inside one repository to a single 7z file. Fetch that
        // instead of component specific meta 7z files.
        const QDomNode sha1 = root.firstChildElement(scSHA1);
        QDomElement metadataNameElement = root.firstChildElement(scMetadataName);
        QDomNodeList children = root.childNodes();
        if (!sha1.isNull() && !metadataNameElement.isNull()) {
            const QString repoUrl = metadata->repository().url().toString();
            const QString metadataName = metadataNameElement.toElement().text();
            addFileTaskItem(QString::fromLatin1("%1/%2").arg(repoUrl, metadataName),
                metadata->path() + QString::fromLatin1("/%1").arg(metadataName),
                metadata.get(), sha1.toElement().text(), QString());
        } else {
            bool metaFound = false;
            for (int i = 0; i < children.count(); ++i) {
                const QDomElement el = children.at(i).toElement();
                if (!el.isNull() && el.tagName() == QLatin1String("PackageUpdate")) {
                    const QDomNodeList c2 = el.childNodes();
                    QString packageName, packageVersion, packageHash;
                    metaFound = parsePackageUpdate(c2, packageName, packageVersion, packageHash,
                                                   online, testCheckSum);

                    // If meta element (script, licenses, etc.) is not found, no need to fetch metadata.
                    if (metaFound) {
                        const QString repoUrl = metadata->repository().url().toString();
                        addFileTaskItem(QString::fromLatin1("%1/%2/%3meta.7z").arg(repoUrl, packageName, packageVersion),
                            metadata->path() + QString::fromLatin1("/%1-%2-meta.7z").arg(packageName, packageVersion),
                            metadata.get(), packageHash, packageName);
                    } else {
                        QString fileName = metadata->path() + QLatin1Char('/') + packageName;
                        QDir directory(fileName);
                        if (!directory.exists()) {
                            directory.mkdir(fileName);
                        }
                    }
                }
            }
        }

        // Remember the fetched metadata
        Metadata *const metadataPtr = metadata.get();
        const QString categoryName = metadata->repository().categoryname();
        if (categoryName.isEmpty())
            metadata->setAvailableFromDefaultRepository(true);
        else
            m_fetchedCategorizedRepositories.insert(metadataPtr->repository()); // For faster lookups

        const QString metadataPath = metadata->path();
        m_fetchedMetadata.insert(metadataPath, metadata.release());

        // search for additional repositories that we might need to check
        status = parseRepositoryUpdates(root, result, metadataPtr);
        if (status == XmlDownloadRetry) {
            // The repository update may have removed or replaced current repositories,
            // clear repository information from cached items and refresh on next fetch run.
            resetCacheRepositories();
            return status;
        }
    }
    double taskCount = m_packages.length()/static_cast<double>(m_downloadableChunkSize);
    m_totalTaskCount = qCeil(taskCount);
    m_taskNumber = 0;

    return XmlDownloadSuccess;
}

MetadataJob::Status MetadataJob::refreshCacheItem(const FileTaskResult &result,
    const QByteArray &checksum, bool *refreshed)
{
    Q_ASSERT(refreshed);
    *refreshed = false;

    Metadata *cachedMetadata = m_metaFromCache.itemByChecksum(checksum);
    if (!cachedMetadata)
        return XmlDownloadSuccess;

    const FileTaskItem item = result.value(TaskRole::TaskItem).value<FileTaskItem>();
    const Repository repository = item.value(TaskRole::UserRole).value<Repository>();

    if (cachedMetadata->isValid() && !repository.isCompressed()) {
        // Refresh repository information to cache. Same repository may appear in multiple
        // categories and the metadata may be available from default repositories simultaneously.
        cachedMetadata->setRepository(repository);
        if (!repository.categoryname().isEmpty())
            m_fetchedCategorizedRepositories.insert(repository); // For faster lookups
        else
            cachedMetadata->setAvailableFromDefaultRepository(true);

        // Refresh also persistent information, the url of the repository may have changed
        // from the last fetch.
        cachedMetadata->setPersistentRepositoryPath(repository.url());

        // search for additional repositories that we might need to check
        if (cachedMetadata->containsRepositoryUpdates()) {
            QDomDocument doc = cachedMetadata->updatesDocument();
            const Status status = parseRepositoryUpdates(doc.documentElement(), result, cachedMetadata);
            if (status == XmlDownloadRetry) {
                // The repository update may have removed or replaced current repositories,
                // clear repository information from cached items and refresh on next fetch run.
                resetCacheRepositories();
                return status;
            }
        }
        *refreshed = true;
        return XmlDownloadSuccess;
    }
    // Missing or corrupted files, or compressed repository which takes priority
    // over remote repository. We will re-download and uncompress
    // the metadata. Remove broken item from the cache.
    if (!m_metaFromCache.removeItem(checksum)) {
        qCWarning(lcInstallerInstallLog) << m_metaFromCache.errorString();
        return XmlDownloadFailure;
    }
    return XmlDownloadSuccess;
}

MetadataJob::Status MetadataJob::findCachedUpdatesFile(const Repository &repository, const QString &fileUrl)
{
    if (repository.xmlChecksum().isEmpty())
        return XmlDownloadFailure;

    Metadata *metadata = m_metaFromCache.itemByChecksum(repository.xmlChecksum());
    if (!metadata)
        return XmlDownloadFailure;

    const QString targetPath = metadata->path() + QLatin1Char('/') + scUpdatesXML;

    FileTaskItem cachedMetaTaskItem(fileUrl, targetPath);
    cachedMetaTaskItem.insert(TaskRole::UserRole, QVariant::fromValue(repository));
    const FileTaskResult cachedMetaTaskResult(targetPath, repository.xmlChecksum(), cachedMetaTaskItem, false);

    bool isCached = false;
    const Status status = refreshCacheItem(cachedMetaTaskResult, repository.xmlChecksum(), &isCached);
    if (isCached)
        return XmlDownloadSuccess;
    else if (status == XmlDownloadRetry)
        return XmlDownloadRetry;
    else
        return XmlDownloadFailure;
}

MetadataJob::Status MetadataJob::parseRepositoryUpdates(const QDomElement &root,
    const FileTaskResult &result, Metadata *metadata)
{
    MetadataJob::Status status = XmlDownloadSuccess;
    const QDomNode repositoryUpdate = root.firstChildElement(QLatin1String("RepositoryUpdate"));
    if (!repositoryUpdate.isNull()) {
        const QMultiHash<QString, QPair<Repository, Repository> > repositoryUpdates
            = searchAdditionalRepositories(repositoryUpdate, result, *metadata);
        if (!repositoryUpdates.isEmpty())
            status = setAdditionalRepositories(repositoryUpdates, result, *metadata);
    }
    return status;
}

QSet<Repository> MetadataJob::getRepositories()
{
    QSet<Repository> repositories;

    //In the first run, get always the default repositories
    if (!m_defaultRepositoriesFetched) {
        repositories = m_core->settings().repositories();
        m_defaultRepositoriesFetched = true;
    }

    // Fetch repositories under archive which are selected in UI.
    // If repository is already fetched, do not fetch it again.
    for (const RepositoryCategory &repositoryCategory : m_core->settings().repositoryCategories()) {
        if (!repositoryCategory.isEnabled())
            continue;

        for (const Repository &repository : repositoryCategory.repositories()) {
            if (!m_fetchedCategorizedRepositories.contains(repository))
                repositories.insert(repository);
        }
    }
    return repositories;
}

void MetadataJob::addFileTaskItem(const QString &source, const QString &target, Metadata *metadata,
                                  const QString &sha1, const QString &packageName)
{
    FileTaskItem item(source, target);
    QAuthenticator authenticator;
    authenticator.setUser(metadata->repository().username());
    authenticator.setPassword(metadata->repository().password());

    item.insert(TaskRole::UserRole, metadata->path());
    item.insert(TaskRole::Checksum, sha1.toLatin1());
    item.insert(TaskRole::Authenticator, QVariant::fromValue(authenticator));
    item.insert(TaskRole::Name, packageName);
    m_packages.append(item);
}

bool MetadataJob::parsePackageUpdate(const QDomNodeList &c2, QString &packageName,
                                    QString &packageVersion, QString &packageHash,
                                    bool online, bool testCheckSum)
{
    bool metaFound = false;
    for (int j = 0; j < c2.count(); ++j) {
        const QDomElement element = c2.at(j).toElement();
        if (element.tagName() == scName)
            packageName = element.text();
        else if (element.tagName() == scVersion)
            packageVersion = (online ? element.text() : QString());
        else if ((element.tagName() == QLatin1String("SHA1")) && testCheckSum)
            packageHash = element.text();
        else {
            foreach (QString meta, scMetaElements) {
                if (element.tagName() == meta) {
                    metaFound = true;
                    break;
                }
            }
        }
    }
    return metaFound;
}

QMultiHash<QString, QPair<Repository, Repository> > MetadataJob::searchAdditionalRepositories
    (const QDomNode &repositoryUpdate, const FileTaskResult &result, const Metadata &metadata)
{
    QMultiHash<QString, QPair<Repository, Repository> > repositoryUpdates;
    const QDomNodeList children = repositoryUpdate.toElement().childNodes();
    for (int i = 0; i < children.count(); ++i) {
        const QDomElement el = children.at(i).toElement();
        if (!el.isNull() && el.tagName() == QLatin1String("Repository")) {
            const QString action = el.attribute(QLatin1String("action"));
            if (action == QLatin1String("add")) {
                // add a new repository to the defaults list
                Repository repository(resolveUrl(result, el.attribute(QLatin1String("url"))), true);
                repository.setUsername(el.attribute(QLatin1String("username")));
                repository.setPassword(el.attribute(QLatin1String("password")));
                repository.setDisplayName(el.attribute(QLatin1String("displayname")));
                if (ProductKeyCheck::instance()->isValidRepository(repository)) {
                    repositoryUpdates.insert(action, qMakePair(repository, Repository()));
                    qDebug() << "Repository to add:" << repository.displayname();
                }
            } else if (action == QLatin1String("remove")) {
                // remove possible default repositories using the given server url
                Repository repository(resolveUrl(result, el.attribute(QLatin1String("url"))), true);
                repository.setDisplayName(el.attribute(QLatin1String("displayname")));
                repositoryUpdates.insert(action, qMakePair(repository, Repository()));

                qDebug() << "Repository to remove:" << repository.displayname();
            } else if (action == QLatin1String("replace")) {
                // replace possible default repositories using the given server url
                Repository oldRepository(resolveUrl(result, el.attribute(QLatin1String("oldUrl"))), true);
                Repository newRepository(resolveUrl(result, el.attribute(QLatin1String("newUrl"))), true);
                newRepository.setUsername(el.attribute(QLatin1String("username")));
                newRepository.setPassword(el.attribute(QLatin1String("password")));
                newRepository.setDisplayName(el.attribute(QLatin1String("displayname")));

                if (ProductKeyCheck::instance()->isValidRepository(newRepository)) {
                    // store the new repository and the one old it replaces
                    repositoryUpdates.insert(action, qMakePair(newRepository, oldRepository));
                    qDebug() << "Replace repository" << oldRepository.displayname() << "with"
                        << newRepository.displayname();
                }
            } else {
                qDebug() << "Invalid additional repositories action set in Updates.xml fetched "
                    "from" << metadata.repository().displayname() << "line:" << el.lineNumber();
            }
        }
    }
    return repositoryUpdates;
}

MetadataJob::Status MetadataJob::setAdditionalRepositories(QMultiHash<QString, QPair<Repository, Repository> > repositoryUpdates,
                                            const FileTaskResult &result, const Metadata& metadata)
{
    MetadataJob::Status status = XmlDownloadSuccess;
    Settings &s = m_core->settings();
    const QSet<Repository> temporaries = s.temporaryRepositories();
    // in case the temp repository introduced something new, we only want that temporary
    if (temporaries.contains(metadata.repository())) {
        QSet<Repository> tmpRepositories;
        typedef QPair<Repository, Repository> RepositoryPair;

        QList<RepositoryPair> values = repositoryUpdates.values(QLatin1String("add"));
        foreach (const RepositoryPair &value, values)
            tmpRepositories.insert(value.first);

        values = repositoryUpdates.values(QLatin1String("replace"));
        foreach (const RepositoryPair &value, values)
            tmpRepositories.insert(value.first);

        tmpRepositories = tmpRepositories.subtract(temporaries);
        if (tmpRepositories.count() > 0) {
            s.addTemporaryRepositories(tmpRepositories, true);
            QFile::remove(result.target());
            m_defaultRepositoriesFetched = false;
            qDeleteAll(m_fetchedMetadata);
            m_fetchedMetadata.clear();
            status = XmlDownloadRetry;
        }
    } else if (s.updateDefaultRepositories(repositoryUpdates) == Settings::UpdatesApplied) {
        if (m_core->isMaintainer()) {
            bool gainedAdminRights = false;
            if (!m_core->directoryWritable(m_core->value(scTargetDir))) {
                m_core->gainAdminRights();
                gainedAdminRights = true;
            }
            m_core->writeMaintenanceConfigFiles();
            if (gainedAdminRights)
                m_core->dropAdminRights();
        }
        m_defaultRepositoriesFetched = false;
        qDeleteAll(m_fetchedMetadata);
        m_fetchedMetadata.clear();
        QFile::remove(result.target());
        status = XmlDownloadRetry;
    }
    return status;
}

void MetadataJob::setInfoMessage(const QString &message)
{
    m_taskNumber++;
    QString metaInformation = message;
    if (m_totalTaskCount > 1)
        metaInformation = QLatin1String(" %1 %2/%3 ").arg(message).arg(m_taskNumber).arg(m_totalTaskCount);
    emit infoMessage(this, metaInformation);

}
}   // namespace QInstaller
