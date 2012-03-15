/**************************************************************************
**
** This file is part of Installer Framework
**
** Copyright (c) 2010-2012 Nokia Corporation and/or its subsidiary(-ies).
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
#ifndef GETREPOSITORIESMETAINFOJOB_H
#define GETREPOSITORIESMETAINFOJOB_H

#include "fileutils.h"
#include "installer_global.h"
#include "repository.h"

#include "kdjob.h"

#include <QtCore/QList>
#include <QtCore/QPointer>
#include <QtCore/QString>
#include <QtCore/QStringList>

namespace KDUpdater {
    class FileDownloader;
}

namespace QInstaller {

class GetRepositoryMetaInfoJob;
class PackageManagerCorePrivate;

class INSTALLER_EXPORT GetRepositoriesMetaInfoJob : public KDJob
{
    Q_OBJECT

public:
    explicit GetRepositoriesMetaInfoJob(PackageManagerCorePrivate *corePrivate);

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
    bool m_canceled;
    int m_silentRetries;
    bool m_haveIgnoredError;
    PackageManagerCorePrivate *m_corePrivate;

    QString m_errorString;
    QList<Repository> m_repositories;
    mutable TempDirDeleter m_tempDirDeleter;
    QPointer<GetRepositoryMetaInfoJob> m_job;
    QHash<QString, Repository> m_repositoryByTemporaryDirectory;
};

}   // namespace QInstaller

#endif // GETREPOSITORIESMETAINFOJOB_H
