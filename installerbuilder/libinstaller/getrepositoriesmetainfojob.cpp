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

#include "getrepositoriesmetainfojob.h"

#include "getrepositorymetainfojob.h"
#include "packagemanagercore_p.h"
#include "qinstallerglobal.h"

#include <QtCore/QDebug>

using namespace KDUpdater;
using namespace QInstaller;


// -- GetRepositoriesMetaInfoJob

GetRepositoriesMetaInfoJob::GetRepositoriesMetaInfoJob(PackageManagerCorePrivate *corePrivate)
    : KDJob(corePrivate),
      m_canceled(false),
      m_silentRetries(3),
      m_haveIgnoredError(false),
      m_corePrivate(corePrivate)
{
    setCapabilities(Cancelable);
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

// -- private Q_SLOTS

void GetRepositoriesMetaInfoJob::doStart()
{
    if ((m_corePrivate->isInstaller() && !m_corePrivate->isOfflineOnly())
        || (m_corePrivate->isUpdater() || m_corePrivate->isPackageManager())) {
            foreach (const Repository &repo, m_corePrivate->m_settings.repositories()) {
                if (repo.isEnabled())
                    m_repositories += repo;
            }
    }

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

    if (m_repositories.isEmpty()) {
        if (m_haveIgnoredError)
            emitFinishedWithError(QInstaller::UserIgnoreError, m_errorString);
        else
            emitFinished();
        return;
    }

    m_job = new GetRepositoryMetaInfoJob(m_corePrivate, this);
    connect(m_job, SIGNAL(finished(KDJob*)), this, SLOT(jobFinished(KDJob*)));
    connect(m_job, SIGNAL(infoMessage(KDJob*, QString)), this, SIGNAL(infoMessage(KDJob*, QString)));

    m_job->setSilentRetries(silentRetries());
    m_job->setRepository(m_repositories.takeLast());
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

    if (job->error() == QInstaller::RepositoryUpdatesReceived) {
        reset();
        QMetaObject::invokeMethod(this, "doStart", Qt::QueuedConnection);
    } else {
        QMetaObject::invokeMethod(this, "fetchNextRepo", Qt::QueuedConnection);
    }
}
