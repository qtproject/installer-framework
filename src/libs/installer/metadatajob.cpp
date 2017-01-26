/**************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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

#include <QTemporaryDir>

namespace QInstaller {

static QUrl resolveUrl(const FileTaskResult &result, const QString &url)
{
    QUrl u(url);
    if (u.isRelative())
        return QUrl(result.taskItem().source()).resolved(u);
    return u;
}

MetadataJob::MetadataJob(QObject *parent)
    : KDJob(parent)
    , m_core(0)
{
    setCapabilities(Cancelable);
    connect(&m_xmlTask, SIGNAL(finished()), this, SLOT(xmlTaskFinished()));
    connect(&m_metadataTask, SIGNAL(finished()), this, SLOT(metadataTaskFinished()));
    connect(&m_metadataTask, SIGNAL(progressValueChanged(int)), this, SLOT(progressChanged(int)));
}

MetadataJob::~MetadataJob()
{
    reset();
}

Repository MetadataJob::repositoryForDirectory(const QString &directory) const
{
    return m_metadata.value(directory).repository;
}


// -- private slots

void MetadataJob::doStart()
{
    reset();
    if (!m_core) {
        emitFinishedWithError(KDJob::Canceled, tr("Missing package manager core engine."));
        return; // We can't do anything here without core, so avoid tons of !m_core checks.
    }

    emit infoMessage(this, tr("Preparing meta information download..."));
    const bool onlineInstaller = m_core->isInstaller() && !m_core->isOfflineOnly();
    if (onlineInstaller || (m_core->isUpdater() || m_core->isPackageManager())) {
        QList<FileTaskItem> items;
        const ProductKeyCheck *const productKeyCheck = ProductKeyCheck::instance();
        foreach (const Repository &repo, m_core->settings().repositories()) {
            if (repo.isEnabled() && productKeyCheck->isValidRepository(repo)) {
                QAuthenticator authenticator;
                authenticator.setUser(repo.username());
                authenticator.setPassword(repo.password());

                QString url = repo.url().toString() + QLatin1String("/Updates.xml?");
                if (!m_core->value(QLatin1String("UrlQueryString")).isEmpty())
                    url += m_core->value(QLatin1String("UrlQueryString")) + QLatin1Char('&');

                // also append a random string to avoid proxy caches
                FileTaskItem item(url.append(QString::number(qrand() * qrand())));
                item.insert(TaskRole::UserRole, QVariant::fromValue(repo));
                item.insert(TaskRole::Authenticator, QVariant::fromValue(authenticator));
                items.append(item);
            }
        }
        DownloadFileTask *const xmlTask = new DownloadFileTask(items);
        xmlTask->setProxyFactory(m_core->proxyFactory());
        m_xmlTask.setFuture(QtConcurrent::run(&DownloadFileTask::doTask, xmlTask));
    } else {
        emitFinished();
    }
}

void MetadataJob::doCancel()
{
    reset();
    emitFinishedWithError(KDJob::Canceled, tr("Meta data download canceled."));
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
            qDebug() << e.message();

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
            qDebug() << e.message();
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
                            if (m_core->isUpdater() || m_core->isPackageManager())
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

    if (error() != KDJob::NoError)
        return;

    if (status == XmlDownloadSuccess) {
        setProcessedAmount(0);
        DownloadFileTask *const metadataTask = new DownloadFileTask(m_packages);
        metadataTask->setProxyFactory(m_core->proxyFactory());
        m_metadataTask.setFuture(QtConcurrent::run(&DownloadFileTask::doTask, metadataTask));
        emit infoMessage(this, tr("Retrieving meta information from remote repository..."));
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
        reset();
        emitFinishedWithError(QInstaller::ExtractionError, e.message());
    } catch (const QUnhandledException &e) {
        reset();
        emitFinishedWithError(QInstaller::DownloadError, QLatin1String(e.what()));
    } catch (...) {
        reset();
        emitFinishedWithError(QInstaller::DownloadError, tr("Unknown exception during extracting."));
    }

    if (error() != KDJob::NoError)
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

void MetadataJob::metadataTaskFinished()
{
    try {
        m_metadataTask.waitForFinished();
        QFuture<FileTaskResult> future = m_metadataTask.future();
        if (future.resultCount() > 0) {
            emit infoMessage(this, tr("Extracting meta information..."));
            foreach (const FileTaskResult &result, future.results()) {
                const FileTaskItem item = result.value(TaskRole::TaskItem).value<FileTaskItem>();
                UnzipArchiveTask *task = new UnzipArchiveTask(result.target(),
                    item.value(TaskRole::UserRole).toString());

                QFutureWatcher<void> *watcher = new QFutureWatcher<void>();
                m_unzipTasks.insert(watcher, qobject_cast<QObject*> (task));
                connect(watcher, SIGNAL(finished()), this, SLOT(unzipTaskFinished()));
                watcher->setFuture(QtConcurrent::run(&UnzipArchiveTask::doTask, task));
            }
        } else {
            emitFinished();
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

void MetadataJob::reset()
{
    m_packages.clear();
    m_metadata.clear();

    setError(KDJob::NoError);
    setErrorString(QString());
    setCapabilities(Cancelable);

    try {
        m_xmlTask.cancel();
        m_metadataTask.cancel();
        foreach (QFutureWatcher<void> *const watcher, m_unzipTasks.keys()) {
            watcher->cancel();
            watcher->deleteLater();
        }
        foreach (QObject *const object, m_unzipTasks)
            object->deleteLater();
        m_unzipTasks.clear();
    } catch (...) {}
    m_tempDirDeleter.releaseAndDeleteAll();
}

MetadataJob::Status MetadataJob::parseUpdatesXml(const QList<FileTaskResult> &results)
{
    foreach (const FileTaskResult &result, results) {
        if (error() != KDJob::NoError)
            return XmlDownloadFailure;

        Metadata metadata;
        QTemporaryDir tmp(QDir::tempPath() + QLatin1String("/remoterepo-XXXXXX"));
        if (!tmp.isValid()) {
            qDebug() << "Could not create unique temporary directory.";
            return XmlDownloadFailure;
        }

        tmp.setAutoRemove(false);
        metadata.directory = tmp.path();
        m_tempDirDeleter.add(metadata.directory);

        QFile file(result.target());
        if (!file.rename(metadata.directory + QLatin1String("/Updates.xml"))) {
            qDebug() << "Could not rename target to Updates.xml. Error:" << file.errorString();
            return XmlDownloadFailure;
        }

        if (!file.open(QIODevice::ReadOnly)) {
            qDebug() << "Could not open Updates.xml for reading. Error:" << file.errorString();
            return XmlDownloadFailure;
        }

        QString error;
        QDomDocument doc;
        if (!doc.setContent(&file, &error)) {
            qDebug() << QString::fromLatin1("Could not fetch a valid version of Updates.xml from "
                "repository: %1. Error: %2").arg(metadata.repository.displayname(), error);
            return XmlDownloadFailure;
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
                for (int j = 0; j < c2.count(); ++j) {
                    if (c2.at(j).toElement().tagName() == scName)
                        packageName = c2.at(j).toElement().text();
                    else if (c2.at(j).toElement().tagName() == scRemoteVersion)
                        packageVersion = (online ? c2.at(j).toElement().text() : QString());
                    else if ((c2.at(j).toElement().tagName() == QLatin1String("SHA1")) && testCheckSum)
                        packageHash = c2.at(j).toElement().text();
                }

                const QString repoUrl = metadata.repository.url().toString();
                FileTaskItem item(QString::fromLatin1("%1/%2/%3meta.7z").arg(repoUrl, packageName,
                    packageVersion), metadata.directory + QString::fromLatin1("/%1-%2-meta.7z")
                    .arg(packageName, packageVersion));

                QAuthenticator authenticator;
                authenticator.setUser(metadata.repository.username());
                authenticator.setPassword(metadata.repository.password());

                item.insert(TaskRole::UserRole, metadata.directory);
                item.insert(TaskRole::Checksum, packageHash.toLatin1());
                item.insert(TaskRole::Authenticator, QVariant::fromValue(authenticator));
                m_packages.append(item);
            }
        }
        m_metadata.insert(metadata.directory, metadata);

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
                        qDebug() << "Replace repository:" << oldRepository.displayname() << "with:"
                            << newRepository.displayname();
                    }
                } else {
                    qDebug() << "Invalid additional repositories action set in Updates.xml fetched "
                        "from:" << metadata.repository.displayname() << "Line:" << el.lineNumber();
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
                    return XmlDownloadRetry;
                }
            } else if (s.updateDefaultRepositories(repositoryUpdates) == Settings::UpdatesApplied) {
                if (m_core->isUpdater() || m_core->isPackageManager())
                    m_core->writeMaintenanceConfigFiles();
                QFile::remove(result.target());
                return XmlDownloadRetry;
            }
        }
    }
    return XmlDownloadSuccess;
}

}   // namespace QInstaller
