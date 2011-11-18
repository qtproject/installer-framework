/**************************************************************************
**
** This file is part of Qt SDK**
**
** Copyright (c) 2010 Nokia Corporation and/or its subsidiary(-ies).*
**
** Contact:  Nokia Corporation qt-info@nokia.com**
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
#ifndef GETREPOSITORIESMETAINFOJOB_H
#define GETREPOSITORIESMETAINFOJOB_H

#include <KDToolsCore/kdjob.h>

#include <QtCore/QList>
#include <QtCore/QPointer>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <common/fileutils.h>
#include <common/repository.h>

#include "installer_global.h"

namespace KDUpdater {
    class FileDownloader;
}

namespace QInstaller {

class GetRepositoryMetaInfoJob;

class INSTALLER_EXPORT GetRepositoriesMetaInfoJob : public KDJob
{
    Q_OBJECT

public:
    explicit GetRepositoriesMetaInfoJob(const QByteArray &publicKey, QObject *parent = 0);

    QSet<Repository> repositories() const;
    void setRepositories(const QSet<Repository> &repositories);

    QStringList temporaryDirectories() const;
    QStringList releaseTemporaryDirectories() const;
    Repository repositoryForTemporaryDirectory(const QString &tmpDir) const;

    int numberOfRetrievedRepositories() const;

    int silentRetries() const;
    void setSilentRetries(int retries);

    void reset();
    bool isCanceled() const;

private Q_SLOTS:
    /* reimp */ void doStart();
    /* reimp */ void doCancel();

    void fetchNextRepo();
    void jobFinished(KDJob*);

private:
    QPointer<GetRepositoryMetaInfoJob> m_job;
    const QByteArray m_publicKey;
    bool m_canceled;
    int m_silentRetries;
    QSet<Repository> m_repositories;
    QList<Repository> m_tmpRepositories;
    QHash<QString, Repository> m_repositoryByTemporaryDirectory;
    bool m_haveIgnoredError;
    QString m_errorString;
    mutable TempDirDeleter m_tempDirDeleter;
};

}   // namespace QInstaller

#endif // GETREPOSITORIESMETAINFOJOB_H
