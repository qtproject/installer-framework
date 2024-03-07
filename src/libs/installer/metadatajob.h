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

#ifndef METADATAJOB_H
#define METADATAJOB_H

#include "downloadfiletask.h"
#include "fileutils.h"
#include "job.h"
#include "metadata.h"
#include "metadatacache.h"
#include "repository.h"

#include <QFutureWatcher>

class QDomNodeList;
class QDomNode;

namespace QInstaller {

class PackageManagerCore;

enum DownloadType
{
    All,
    CompressedPackage
};

class INSTALLER_EXPORT MetadataJob : public Job
{
    Q_OBJECT
    Q_DISABLE_COPY(MetadataJob)

    enum Status {
        XmlDownloadRetry,
        XmlDownloadFailure,
        XmlDownloadSuccess
    };

public:
    explicit MetadataJob(QObject *parent = 0);
    ~MetadataJob();

    QList<Metadata *> metadata() const;
    Repository repositoryForCacheDirectory(const QString &directory) const;
    void setPackageManagerCore(PackageManagerCore *core) { m_core = core; }
    void addDownloadType(DownloadType downloadType) { m_downloadType = downloadType;}
    QStringList shaMismatchPackages() const { return m_shaMissmatchPackages; }

    bool resetCache(bool init = false);
    bool clearCache();
    bool isValidCache() const;

private slots:
    void doStart() override;
    void doCancel() override;

    void xmlTaskFinished();
    void unzipTaskFinished();
    void metadataTaskFinished();
    void updateCacheTaskFinished();
    void progressChanged(int progress);
    void setProgressTotalAmount(int maximum);
    void unzipRepositoryTaskFinished();
    bool startXMLTask();

private:
    bool fetchMetaDataPackages();
    void startUnzipRepositoryTask(const Repository &repo);
    void startUpdateCacheTask();
    void resetCacheRepositories();
    void reset();
    void resetCompressedFetch();
    Status parseUpdatesXml(const QList<FileTaskResult> &results);
    Status refreshCacheItem(const FileTaskResult &result, const QByteArray &checksum, bool *refreshed);
    Status findCachedUpdatesFile(const Repository &repository, const QString &fileUrl);
    Status parseRepositoryUpdates(const QDomElement &root, const FileTaskResult &result, Metadata *metadata);
    QSet<Repository> getRepositories();
    void addFileTaskItem(const QString &source, const QString &target, Metadata *metadata,
                         const QString &sha1, const QString &packageName);
    static bool parsePackageUpdate(const QDomNodeList &c2, QString &packageName, QString &packageVersion,
                            QString &packageHash, bool online, bool testCheckSum);
    QMultiHash<QString, QPair<Repository, Repository> > searchAdditionalRepositories(const QDomNode &repositoryUpdate,
                            const FileTaskResult &result, const Metadata &metadata);
    MetadataJob::Status setAdditionalRepositories(QMultiHash<QString, QPair<Repository, Repository> > repositoryUpdates,
                            const FileTaskResult &result, const Metadata& metadata);
    void setInfoMessage(const QString &message);

private:
    friend class Metadata;

private:
    PackageManagerCore *m_core;

    QList<FileTaskItem> m_packages;
    QList<FileTaskItem> m_updatesXmlItems;
    TempPathDeleter m_tempDirDeleter;
    QFutureWatcher<FileTaskResult> m_xmlTask;
    QFutureWatcher<FileTaskResult> m_metadataTask;
    QFutureWatcher<void> m_updateCacheTask;
    QHash<QFutureWatcher<void> *, QObject*> m_unzipTasks;
    QHash<QFutureWatcher<void> *, QObject*> m_unzipRepositoryTasks;
    DownloadType m_downloadType;
    QList<FileTaskResult> m_metadataResult;
    QList<FileTaskResult> m_updatesXmlResult;
    int m_downloadableChunkSize;
    int m_taskNumber;
    int m_totalTaskCount;
    QStringList m_shaMissmatchPackages;
    bool m_defaultRepositoriesFetched;

    QSet<Repository> m_fetchedCategorizedRepositories;
    QHash<QString, Metadata *> m_fetchedMetadata;
    MetadataCache m_metaFromCache;
};

}   // namespace QInstaller

#endif  // METADATAJOB_H
