/**************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
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

#include <QTemporaryDir>
#include <QtMath>

const QStringList metaElements = {QLatin1String("Script"), QLatin1String("Licenses"), QLatin1String("UserInterfaces"), QLatin1String("Translations")};

namespace QInstaller {

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
    , m_addCompressedPackages(false)
    , m_downloadableChunkSize(1000)
    , m_taskNumber(0)
{
    setCapabilities(Cancelable);
    connect(&m_xmlTask, &QFutureWatcherBase::finished, this, &MetadataJob::xmlTaskFinished);
    connect(&m_metadataTask, &QFutureWatcherBase::finished, this, &MetadataJob::metadataTaskFinished);
    connect(&m_metadataTask, &QFutureWatcherBase::progressValueChanged, this, &MetadataJob::progressChanged);
}

MetadataJob::~MetadataJob()
{
    resetCompressedFetch();
    reset();
}

/*
 * Parse the metadata of currently selected repositories. We cannot
 * return all metadata as that contains metadata also from categorized archived
 * repositories which might not be currently selected.
 */

QList<Metadata> MetadataJob::metadata() const
{
    QList<Metadata> metadata = m_metaFromDefaultRepositories.values();
    foreach (RepositoryCategory repositoryCategory, m_core->settings().repositoryCategories()) {
        if (m_core->isUpdater() || (repositoryCategory.isEnabled() && m_fetchedArchive.contains(repositoryCategory.displayname()))) {
            QList<ArchiveMetadata> archiveMetaList = m_fetchedArchive.values(repositoryCategory.displayname());
            foreach (ArchiveMetadata archiveMeta, archiveMetaList) {
                metadata.append(archiveMeta.metaData);
            }
        }
    }
    return metadata;
}

Repository MetadataJob::repositoryForDirectory(const QString &directory) const
{
    if (m_metaFromDefaultRepositories.contains(directory))
        return m_metaFromDefaultRepositories.value(directory).repository;
    else
        return m_metaFromArchive.value(directory).repository;
}

// -- private slots

void MetadataJob::doStart()
{
    setError(Job::NoError);
    setErrorString(QString());
    if (!m_core) {
        emitFinishedWithError(Job::Canceled, tr("Missing package manager core engine."));
        return; // We can't do anything here without core, so avoid tons of !m_core checks.
    }
    const ProductKeyCheck *const productKeyCheck = ProductKeyCheck::instance();
    if (!m_addCompressedPackages) {
        emit infoMessage(this, tr("Preparing meta information download..."));
        const bool onlineInstaller = m_core->isInstaller() && !m_core->isOfflineOnly();
        if (onlineInstaller || m_core->isMaintainer()) {
            QList<FileTaskItem> items;
            QSet<Repository> repositories = getRepositories();
            foreach (const Repository &repo, repositories) {
                if (repo.isEnabled() &&
                        productKeyCheck->isValidRepository(repo)) {
                    QAuthenticator authenticator;
                    authenticator.setUser(repo.username());
                    authenticator.setPassword(repo.password());

                    if (!repo.isCompressed()) {
                        QString url = repo.url().toString() + QLatin1String("/Updates.xml?");
                        if (!m_core->value(scUrlQueryString).isEmpty())
                            url += m_core->value(scUrlQueryString) + QLatin1Char('&');

                        // also append a random string to avoid proxy caches
                        FileTaskItem item(url.append(QString::number(qrand() * qrand())));
                        item.insert(TaskRole::UserRole, QVariant::fromValue(repo));
                        item.insert(TaskRole::Authenticator, QVariant::fromValue(authenticator));
                        items.append(item);
                    }
                    else {
                        qDebug() << "Trying to parse compressed repo as normal repository."\
                                  "Check repository syntax.";
                    }
                }
            }
            if (items.count() > 0) {
                startXMLTask(items);
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

void MetadataJob::startXMLTask(const QList<FileTaskItem> items)
{
    DownloadFileTask *const xmlTask = new DownloadFileTask(items);
    xmlTask->setProxyFactory(m_core->proxyFactory());
    m_xmlTask.setFuture(QtConcurrent::run(&DownloadFileTask::doTask, xmlTask));
}

void MetadataJob::doCancel()
{
    reset();
    emitFinishedWithError(Job::Canceled, tr("Meta data download canceled."));
}

void MetadataJob::startUnzipRepositoryTask(const Repository &repo)
{
    QTemporaryDir tempRepoDir(QDir::tempPath() + QLatin1String("/compressedRepo-XXXXXX"));
    if (!tempRepoDir.isValid()) {
        qDebug() << "Cannot create unique temporary directory.";
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
                    FileTaskItem item(url);
                    item.insert(TaskRole::UserRole, QVariant::fromValue(repo));
                    m_unzipRepositoryitems.append(item);
                } else {
                    //Repository is not valid, remove it
                    Repository repository;
                    repository.setUrl(QUrl(task->archive()));
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
        if (m_unzipRepositoryitems.count() > 0 && m_unzipRepositoryTasks.isEmpty()) {
            startXMLTask(m_unzipRepositoryitems);
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
        status = parseUpdatesXml(m_xmlTask.future().results());
    } catch (const AuthenticationRequiredException &e) {
        if (e.type() == AuthenticationRequiredException::Type::Proxy) {
            const QNetworkProxy proxy = e.proxy();
            ProxyCredentialsDialog proxyCredentials(proxy);
            qDebug().noquote() << e.message();

            if (proxyCredentials.exec() == QDialog::Accepted) {
                qDebug() << "Retrying with new credentials ...";
                PackageManagerProxyFactory *factory = m_core->proxyFactory();

                factory->setProxyCredentials(proxy, proxyCredentials.userName(),
                                             proxyCredentials.password());
                m_core->setProxyFactory(factory);
                status = XmlDownloadRetry;
            } else {
                reset();
                emitFinishedWithError(QInstaller::DownloadError, tr("Missing proxy credentials."));
            }
        } else if (e.type() == AuthenticationRequiredException::Type::Server) {
            qDebug().noquote() << e.message();
            ServerAuthenticationDialog dlg(e.message(), e.taskItem());
            if (dlg.exec() == QDialog::Accepted) {
                Repository original = e.taskItem().value(TaskRole::UserRole)
                    .value<Repository>();
                Repository replacement = original;
                replacement.setUsername(dlg.user());
                replacement.setPassword(dlg.password());

                Settings &s = m_core->settings();
                QSet<Repository> temporaries = s.temporaryRepositories();
                if (temporaries.contains(original)) {
                    temporaries.remove(original);
                    temporaries.insert(replacement);
                    s.addTemporaryRepositories(temporaries, true);
                } else {
                    QHash<QString, QPair<Repository, Repository> > update;
                    update.insert(QLatin1String("replace"), qMakePair(original, replacement));

                    if (s.updateDefaultRepositories(update) == Settings::UpdatesApplied
                        || s.updateUserRepositories(update) == Settings::UpdatesApplied) {
                            if (m_core->isMaintainer())
                                m_core->writeMaintenanceConfigFiles();
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
            emitFinished();
        }
    } else if (status == XmlDownloadRetry) {
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

    if (m_unzipTasks.isEmpty()) {
        setProcessedAmount(100);
        emitFinished();
    }
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
                            qWarning() << mismatchMessage;
                        } else {
                            throw QInstaller::TaskException(mismatchMessage);
                        }
                    }
                    UnzipArchiveTask *task = new UnzipArchiveTask(result.target(),
                        item.value(TaskRole::UserRole).toString());

                    QFutureWatcher<void> *watcher = new QFutureWatcher<void>();
                    m_unzipTasks.insert(watcher, qobject_cast<QObject*> (task));
                    connect(watcher, &QFutureWatcherBase::finished, this, &MetadataJob::unzipTaskFinished);
                    watcher->setFuture(QtConcurrent::run(&UnzipArchiveTask::doTask, task));
                }
            } else {
                emitFinished();
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


// -- private

bool MetadataJob::fetchMetaDataPackages()
{
    //Download files in chunks. QtConcurrent will choke if too many task is given to it
    int chunkSize = qMin(m_packages.length(), m_downloadableChunkSize);
    QList<FileTaskItem> tempPackages = m_packages.mid(0, chunkSize);
    m_packages = m_packages.mid(chunkSize, m_packages.length());
    if (tempPackages.length() > 0) {
        m_taskNumber++;
        setProcessedAmount(0);
        DownloadFileTask *const metadataTask = new DownloadFileTask(tempPackages);
        metadataTask->setProxyFactory(m_core->proxyFactory());
        m_metadataTask.setFuture(QtConcurrent::run(&DownloadFileTask::doTask, metadataTask));
        setProgressTotalAmount(100);
        QString metaInformation;
        if (m_totalTaskCount > 1)
            metaInformation = tr("Retrieving meta information from remote repository... %1/%2 ").arg(m_taskNumber).arg(m_totalTaskCount);
        else
            metaInformation = tr("Retrieving meta information from remote repository... ");
        emit infoMessage(this, metaInformation);
        return true;
    }
    return false;
}

void MetadataJob::reset()
{
    m_packages.clear();
    m_metaFromDefaultRepositories.clear();
    m_metaFromArchive.clear();
    m_fetchedArchive.clear();

    setError(Job::NoError);
    setErrorString(QString());
    setCapabilities(Cancelable);

    try {
        m_xmlTask.cancel();
        m_metadataTask.cancel();
    } catch (...) {}
    m_tempDirDeleter.releaseAndDeleteAll();
    m_metadataResult.clear();
    m_taskNumber = 0;
}

void MetadataJob::resetCompressedFetch()
{
    setError(Job::NoError);
    setErrorString(QString());
    m_unzipRepositoryitems.clear();

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
        Metadata metadata;
        QTemporaryDir tmp(QDir::tempPath() + QLatin1String("/remoterepo-XXXXXX"));
        if (!tmp.isValid()) {
            qDebug() << "Cannot create unique temporary directory.";
            return XmlDownloadFailure;
        }

        tmp.setAutoRemove(false);
        metadata.directory = tmp.path();
        m_tempDirDeleter.add(metadata.directory);

        QFile file(result.target());
        if (!file.rename(metadata.directory + QLatin1String("/Updates.xml"))) {
            qDebug() << "Cannot rename target to Updates.xml:" << file.errorString();
            return XmlDownloadFailure;
        }

        if (!file.open(QIODevice::ReadOnly)) {
            qDebug() << "Cannot open Updates.xml for reading:" << file.errorString();
            return XmlDownloadFailure;
        }

        QString error;
        QDomDocument doc;
        if (!doc.setContent(&file, &error)) {
            qDebug().nospace() << "Cannot fetch a valid version of Updates.xml from repository "
                               << metadata.repository.displayname() << ": " << error;
            //If there are other repositories, try to use those
            continue;
        }
        file.close();

        const FileTaskItem item = result.value(TaskRole::TaskItem).value<FileTaskItem>();
        metadata.repository = item.value(TaskRole::UserRole).value<Repository>();
        const bool online = !(metadata.repository.url().scheme()).isEmpty();

        bool testCheckSum = true;
        const QDomElement root = doc.documentElement();
        const QDomNode checksum = root.firstChildElement(QLatin1String("Checksum"));
        if (!checksum.isNull())
            testCheckSum = (checksum.toElement().text().toLower() == scTrue);

        QDomNodeList children = root.childNodes();
        for (int i = 0; i < children.count(); ++i) {
            const QDomElement el = children.at(i).toElement();
            if (!el.isNull() && el.tagName() == QLatin1String("PackageUpdate")) {
                const QDomNodeList c2 = el.childNodes();
                QString packageName, packageVersion, packageHash;
                bool metaFound = false;
                for (int j = 0; j < c2.count(); ++j) {
                    if (c2.at(j).toElement().tagName() == scName)
                        packageName = c2.at(j).toElement().text();
                    else if (c2.at(j).toElement().tagName() == scVersion)
                        packageVersion = (online ? c2.at(j).toElement().text() : QString());
                    else if ((c2.at(j).toElement().tagName() == QLatin1String("SHA1")) && testCheckSum)
                        packageHash = c2.at(j).toElement().text();
                    else {
                        foreach (QString meta, metaElements) {
                            if (c2.at(j).toElement().tagName() == meta) {
                                metaFound = true;
                                break;
                            }
                        }
                    }
                }

                const QString repoUrl = metadata.repository.url().toString();
                //If script element is not found, no need to fetch metadata
                if (metaFound) {
                    FileTaskItem item(QString::fromLatin1("%1/%2/%3meta.7z").arg(repoUrl, packageName,
                        packageVersion), metadata.directory + QString::fromLatin1("/%1-%2-meta.7z")
                        .arg(packageName, packageVersion));

                    QAuthenticator authenticator;
                    authenticator.setUser(metadata.repository.username());
                    authenticator.setPassword(metadata.repository.password());

                    item.insert(TaskRole::UserRole, metadata.directory);
                    item.insert(TaskRole::Checksum, packageHash.toLatin1());
                    item.insert(TaskRole::Authenticator, QVariant::fromValue(authenticator));
                    item.insert(TaskRole::Name, packageName);

                    m_packages.append(item);
                } else {
                    QString fileName = metadata.directory + QLatin1Char('/') + packageName;
                    QDir directory(fileName);
                    if (!directory.exists()) {
                        directory.mkdir(fileName);
                    }
                }
            }
        }
        if (metadata.repository.archivename().isEmpty()) {
            m_metaFromDefaultRepositories.insert(metadata.directory, metadata);
        } else {
            //Hash metadata to help checking if meta for repository is already fetched
            ArchiveMetadata archiveMetadata;
            archiveMetadata.metaData = metadata;
            m_fetchedArchive.insertMulti(metadata.repository.archivename(), archiveMetadata);
            // Hash for faster lookups
            m_metaFromArchive.insert(metadata.directory, metadata);
        }


        // search for additional repositories that we might need to check
        const QDomNode repositoryUpdate = root.firstChildElement(QLatin1String("RepositoryUpdate"));
        if (repositoryUpdate.isNull())
            continue;

        QHash<QString, QPair<Repository, Repository> > repositoryUpdates;
        children = repositoryUpdate.toElement().childNodes();
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
                        repositoryUpdates.insertMulti(action, qMakePair(repository, Repository()));
                        qDebug() << "Repository to add:" << repository.displayname();
                    }
                } else if (action == QLatin1String("remove")) {
                    // remove possible default repositories using the given server url
                    Repository repository(resolveUrl(result, el.attribute(QLatin1String("url"))), true);
                    repository.setDisplayName(el.attribute(QLatin1String("displayname")));
                    repositoryUpdates.insertMulti(action, qMakePair(repository, Repository()));

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
                        repositoryUpdates.insertMulti(action, qMakePair(newRepository, oldRepository));
                        qDebug() << "Replace repository" << oldRepository.displayname() << "with"
                            << newRepository.displayname();
                    }
                } else {
                    qDebug() << "Invalid additional repositories action set in Updates.xml fetched "
                        "from" << metadata.repository.displayname() << "line:" << el.lineNumber();
                }
            }
        }

        if (!repositoryUpdates.isEmpty()) {
            Settings &s = m_core->settings();
            const QSet<Repository> temporaries = s.temporaryRepositories();
            // in case the temp repository introduced something new, we only want that temporary
            if (temporaries.contains(metadata.repository)) {
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
                    m_metaFromDefaultRepositories.clear();
                    return XmlDownloadRetry;
                }
            } else if (s.updateDefaultRepositories(repositoryUpdates) == Settings::UpdatesApplied) {
                if (m_core->isMaintainer())
                    m_core->writeMaintenanceConfigFiles();
                m_metaFromDefaultRepositories.clear();
                QFile::remove(result.target());
                return XmlDownloadRetry;
            }
        }
    }
    double taskCount = m_packages.length()/static_cast<double>(m_downloadableChunkSize);
    m_totalTaskCount = qCeil(taskCount);
    m_taskNumber = 0;

    return XmlDownloadSuccess;
}

QSet<Repository> MetadataJob::getRepositories()
{
    QSet<Repository> repositories;

    //In the first run, m_metadata is empty. Get always the default repositories
    if (m_metaFromDefaultRepositories.isEmpty()) {
        repositories = m_core->settings().repositories();
    }

    // Fetch repositories under archive which are selected in UI.
    // If repository is already fetched, do not fetch it again.
    // In updater mode, fetch always all archive repositories to get updates
    foreach (RepositoryCategory repositoryCategory, m_core->settings().repositoryCategories()) {
        if (m_core->isUpdater() || (repositoryCategory.isEnabled() && !m_fetchedArchive.contains(repositoryCategory.displayname()))) {
            foreach (Repository repository, repositoryCategory.repositories()) {
                repositories.insert(repository);
            }
        }
    }
    return repositories;
}

}   // namespace QInstaller
