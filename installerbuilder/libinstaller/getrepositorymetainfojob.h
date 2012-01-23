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
#ifndef GETREPOSITORYMETAINFOJOB_H
#define GETREPOSITORYMETAINFOJOB_H

#include <kdjob.h>

#include <QtCore/QPointer>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QThreadPool>

#include <common/fileutils.h>
#include <common/repository.h>

#include "installer_global.h"

namespace KDUpdater {
    class FileDownloader;
}

namespace QInstaller {

class GetRepositoriesMetaInfoJob;
class PackageManagerCorePrivate;

class INSTALLER_EXPORT GetRepositoryMetaInfoJob : public KDJob
{
    Q_OBJECT
    class ZipRunnable;
    friend class QInstaller::GetRepositoriesMetaInfoJob;

public:
    explicit GetRepositoryMetaInfoJob(PackageManagerCorePrivate *corePrivate, QObject *parent = 0);
    ~GetRepositoryMetaInfoJob();

    Repository repository() const;
    void setRepository(const Repository &r);

    int silentRetries() const;
    void setSilentRetries(int retries);

    QString temporaryDirectory() const;
    QString releaseTemporaryDirectory() const;

private:
    /* reimp */ void doStart();
    /* reimp */ void doCancel();
    void finished(int error, const QString &errorString = QString());

private Q_SLOTS:
    void startUpdatesXmlDownload();
    void updatesXmlDownloadCanceled();
    void updatesXmlDownloadFinished();
    void updatesXmlDownloadError(const QString &error);

    void fetchNextMetaInfo();
    void metaDownloadCanceled();
    void metaDownloadFinished();
    void metaDownloadError(const QString &error);

    void unzipFinished(bool status, const QString &error);

private:
    bool m_canceled;
    int m_silentRetries;
    int m_retriesLeft;
    const QByteArray m_publicKey;
    Repository m_repository;
    QStringList m_packageNames;
    QStringList m_packageVersions;
    QStringList m_packageHash;
    QPointer<KDUpdater::FileDownloader> m_downloader;
    QString m_currentPackageName;
    QString m_currentPackageVersion;
    QString m_temporaryDirectory;
    mutable TempDirDeleter m_tempDirDeleter;

    bool m_waitForDone;
    QThreadPool m_threadPool;
    PackageManagerCorePrivate *m_corePrivate;
};

}   // namespace QInstaller

#endif // GETREPOSITORYMETAINFOJOB_H
