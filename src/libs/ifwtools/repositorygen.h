/**************************************************************************
**
** Copyright (C) 2022 The Qt Company Ltd.
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

#ifndef REPOSITORYGEN_H
#define REPOSITORYGEN_H

#include "ifwtools_global.h"

#include <abstractarchive.h>

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QDomDocument>

namespace QInstallerTools {

struct IFWTOOLS_EXPORT PackageInfo
{
    QString name;
    QString version;
    QString directory;
    QStringList dependencies;
    QStringList copiedFiles;
    QString metaFile;
    QString metaNode;
    QString contentSha1;
    bool createContentSha1Node;
};
typedef QVector<PackageInfo> PackageInfoVector;
typedef QInstaller::AbstractArchive::CompressionLevel Compression;

enum IFWTOOLS_EXPORT FilterType {
    Include,
    Exclude
};

struct IFWTOOLS_EXPORT RepositoryInfo
{
    QStringList packages;
    QStringList repositoryPackages;
    QString repositoryDir;
};

void IFWTOOLS_EXPORT printRepositoryGenOptions();
QString IFWTOOLS_EXPORT makePathAbsolute(const QString &path);
void IFWTOOLS_EXPORT copyWithException(const QString &source, const QString &target, const QString &kind = QString());

PackageInfoVector IFWTOOLS_EXPORT createListOfPackages(const QStringList &packagesDirectories, QStringList *packagesToFilter,
    FilterType ftype, QStringList packagesUpdatedWithSha = QStringList());

PackageInfoVector IFWTOOLS_EXPORT createListOfRepositoryPackages(const QStringList &repositoryDirectories, QStringList *packagesToFilter,
    FilterType filterType);

QHash<QString, QString> IFWTOOLS_EXPORT buildPathToVersionMapping(const PackageInfoVector &info);

void IFWTOOLS_EXPORT createArchive(const QString &filename, const QStringList &data, Compression compression = Compression::Normal);

void IFWTOOLS_EXPORT compressMetaDirectories(const QString &repoDir, const QString &existingUnite7zUrl,
    const QHash<QString, QString> &versionMapping, bool createSplitMetadata, bool createUnifiedMetadata);

QStringList unifyMetadata(const QString &repoDir, const QString &existingRepoDir, QDomDocument doc);
void splitMetadata(const QStringList &entryList, const QString &repoDir, QDomDocument doc,
                   const QHash<QString, QString> &versionMapping);
void copyScriptFiles(const QDomNodeList &childNodes, const PackageInfo &info, bool &foundDownloadableArchives, const QString &targetDir);

void IFWTOOLS_EXPORT copyMetaData(const QString &outDir, const QString &dataDir, const PackageInfoVector &packages,
    const QString &appName, const QString& appVersion, const QStringList &uniteMetadatas);
void IFWTOOLS_EXPORT copyComponentData(const QStringList &packageDir, const QString &repoDir,
                                       PackageInfoVector *const infos, const QString &archiveSuffix,
                                       Compression compression = Compression::Normal);

void IFWTOOLS_EXPORT filterNewComponents(const QString &repositoryDir, QInstallerTools::PackageInfoVector &packages);

QString IFWTOOLS_EXPORT existingUniteMeta7z(const QString &repositoryDir);
PackageInfoVector IFWTOOLS_EXPORT collectPackages(RepositoryInfo info, QStringList *filteredPackages, FilterType filterType, bool updateNewComponents, QStringList packagesUpdatedWithSha);
void IFWTOOLS_EXPORT createRepository(RepositoryInfo info, PackageInfoVector *packages, const QString &tmpMetaDir,
                                      bool createComponentMetadata, bool createUnifiedMetadata, const QString &archiveSuffix,
                                      Compression compression = Compression::Normal);
} // namespace QInstallerTools

#endif // REPOSITORYGEN_H
