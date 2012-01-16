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
#include "getrepositoriesmetainfojob.h"

#include "getrepositorymetainfojob.h"
#include "qinstallerglobal.h"

#include <QtCore/QDebug>

using namespace KDUpdater;
using namespace QInstaller;


// -- GetRepositoriesMetaInfoJob

GetRepositoriesMetaInfoJob::GetRepositoriesMetaInfoJob(const QByteArray &publicKey, QObject *parent)
    : KDJob(parent),
      m_publicKey(publicKey),
      m_canceled(false),
      m_silentRetries(3),
      m_haveIgnoredError(false)
{
    setCapabilities(Cancelable);
}

QSet<Repository> GetRepositoriesMetaInfoJob::repositories() const
{
    return m_repositories;
}

void GetRepositoriesMetaInfoJob::setRepositories(const QSet<Repository> &repos)
{
    m_repositories = repos;
    foreach (const Repository &repo, repos) {
        if (repo.isEnabled())
            m_tmpRepositories += repo;
    }
}

QStringList GetRepositoriesMetaInfoJob::temporaryDirectories() const
{
    return m_repositoryByTemporaryDirectory.keys();
}

QStringList GetRepositoriesMetaInfoJob::releaseTemporaryDirectories() const
{
    m_tempDirDeleter.releaseAll();
    return m_repositoryByTemporaryDirectory.keys();
}

Repository GetRepositoriesMetaInfoJob::repositoryForTemporaryDirectory(const QString &tmpDir) const
{
    return m_repositoryByTemporaryDirectory.value(tmpDir);
}

int GetRepositoriesMetaInfoJob::numberOfRetrievedRepositories() const
{
    return m_repositoryByTemporaryDirectory.size();
}

int GetRepositoriesMetaInfoJob::silentRetries() const
{
    return m_silentRetries;
}

void GetRepositoriesMetaInfoJob::setSilentRetries(int retries)
{
    m_silentRetries = retries;
}

void GetRepositoriesMetaInfoJob::reset()
{
    m_canceled = false;
    m_silentRetries = 3;
    m_errorString.clear();
    m_haveIgnoredError = false;

    m_repositories.clear();
    m_tmpRepositories.clear();
    m_tempDirDeleter.releaseAndDeleteAll();
    m_repositoryByTemporaryDirectory.clear();

    setError(KDJob::NoError);
    setErrorString(QString());
    setCapabilities(Cancelable);
}

bool GetRepositoriesMetaInfoJob::isCanceled() const
{
    return m_canceled;
}

void GetRepositoriesMetaInfoJob::doStart()
{
    fetchNextRepo();
}

void GetRepositoriesMetaInfoJob::doCancel()
{
    m_canceled = true;
    if (m_job)
        m_job->cancel();
}

void GetRepositoriesMetaInfoJob::fetchNextRepo()
{
    if (m_job) {
        m_job->deleteLater();
        m_job = 0;
    }

    if (m_canceled) {
        emitFinishedWithError(KDJob::Canceled, m_errorString);
        return;
    }

    if (m_tmpRepositories.isEmpty()) {
        if (m_haveIgnoredError)
            emitFinishedWithError(QInstaller::UserIgnoreError, m_errorString);
        else
            emitFinished();
        return;
    }

    m_job = new GetRepositoryMetaInfoJob(m_publicKey, this);
    m_job->setSilentRetries(silentRetries());
    m_job->setRepository(m_tmpRepositories.takeLast());
    connect(m_job, SIGNAL(finished(KDJob*)), this, SLOT(jobFinished(KDJob*)));
    connect(m_job, SIGNAL(infoMessage(KDJob*, QString)), this, SIGNAL(infoMessage(KDJob*, QString)));
    m_job->start();
}

void GetRepositoriesMetaInfoJob::jobFinished(KDJob *j)
{
    const GetRepositoryMetaInfoJob *const job = qobject_cast<const GetRepositoryMetaInfoJob *>(j);
    Q_ASSERT(job);

    if (job->error() != KDJob::NoError && !job->temporaryDirectory().isEmpty()) {
        try {
            removeDirectory(job->temporaryDirectory());
        } catch (...) {
        }
    }

    if (job->error() == KDJob::Canceled
        || (job->error() >= KDJob::UserDefinedError && job->error() < QInstaller::UserIgnoreError)) {
            emit infoMessage(j, job->errorString());
            qDebug() << job->errorString();
            emitFinishedWithError(job->error(), job->errorString());
            return;
    }

    if (job->error() == QInstaller::UserIgnoreError) {
        m_haveIgnoredError = true;
        m_errorString = job->errorString();
    } else {
        const QString &tmpdir = job->releaseTemporaryDirectory();
        job->m_tempDirDeleter.passAndRelease(m_tempDirDeleter, tmpdir);
        m_repositoryByTemporaryDirectory.insert(tmpdir, job->repository());
    }
    QMetaObject::invokeMethod(this, "fetchNextRepo", Qt::QueuedConnection);
}
