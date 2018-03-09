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

#ifndef METADATAJOB_H
#define METADATAJOB_H

#include "downloadfiletask.h"
#include "fileutils.h"
#include "job.h"
#include "repository.h"

#include <QFutureWatcher>

namespace QInstaller {

class PackageManagerCore;

struct Metadata
{
    QString directory;
    Repository repository;
};

struct ArchiveMetadata
{
    QString archive;
    Metadata metaData;
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

    QList<Metadata> metadata() const;
    Repository repositoryForDirectory(const QString &directory) const;
    void setPackageManagerCore(PackageManagerCore *core) { m_core = core; }
    void addCompressedPackages(bool addCompressPackage) { m_addCompressedPackages = addCompressPackage;}
    QStringList shaMismatchPackages() const { return m_shaMissmatchPackages; }

private slots:
    void doStart();
    void doCancel();

    void xmlTaskFinished();
    void unzipTaskFinished();
    void metadataTaskFinished();
    void progressChanged(int progress);
    void setProgressTotalAmount(int maximum);
    void unzipRepositoryTaskFinished();
    void startXMLTask(const QList<FileTaskItem> items);

private:
    bool fetchMetaDataPackages();
    void startUnzipRepositoryTask(const Repository &repo);
    void reset();
    void resetCompressedFetch();
    Status parseUpdatesXml(const QList<FileTaskResult> &results);
    QSet<Repository> getRepositories();

private:
    PackageManagerCore *m_core;

    QList<FileTaskItem> m_packages;
    TempDirDeleter m_tempDirDeleter;
    QFutureWatcher<FileTaskResult> m_xmlTask;
    QFutureWatcher<FileTaskResult> m_metadataTask;
    QHash<QFutureWatcher<void> *, QObject*> m_unzipTasks;
    QHash<QFutureWatcher<void> *, QObject*> m_unzipRepositoryTasks;
    bool m_addCompressedPackages;
    QList<FileTaskItem> m_unzipRepositoryitems;
    QList<FileTaskResult> m_metadataResult;
    int m_downloadableChunkSize;
    int m_taskNumber;
    int m_totalTaskCount;
    QStringList m_shaMissmatchPackages;
    QHash<QString, ArchiveMetadata> m_fetchedArchive;
    QHash<QString, Metadata> m_metaFromDefaultRepositories;
    QHash<QString, Metadata> m_metaFromArchive; //for faster lookups.
};

}   // namespace QInstaller

#endif  // METADATAJOB_H
