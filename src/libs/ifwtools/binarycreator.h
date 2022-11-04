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

#ifndef BINARYCREATOR_H
#define BINARYCREATOR_H

#include "ifwtools_global.h"

#include "repositorygen.h"
#include "fileutils.h"
#include "binaryformat.h"

#include <abstractarchive.h>

#include <QtCore/QString>
#include <QtCore/QFile>

namespace QInstallerTools {

struct Input
{
    QString outputPath;
    QString installerExePath;
    QInstallerTools::PackageInfoVector packages;
    QInstaller::ResourceCollectionManager manager;
};

typedef QInstaller::AbstractArchive::CompressionLevel Compression;

struct IFWTOOLS_EXPORT BinaryCreatorArgs
{
    QString target;
    QString configFile;
    QString templateBinary;
    QStringList packagesDirectories;
    QStringList repositoryDirectories;
    QString archiveSuffix = QLatin1String("7z");
    Compression compression = Compression::Normal;
    bool onlineOnly = false;
    bool offlineOnly = false;
    QStringList resources;
    QStringList filteredPackages;
    FilterType ftype = QInstallerTools::Exclude;
    bool compileResource = false;
    QString signingIdentity;
    bool createMaintenanceTool = false;
};

class BundleBackup
{
public:
    explicit BundleBackup(const QString &bundle = QString())
        : bundle(bundle)
    {
        if (!bundle.isEmpty() && QFileInfo::exists(bundle)) {
            backup = QInstaller::generateTemporaryFileName(bundle);
            QFile::rename(bundle, backup);
        }
    }

    ~BundleBackup()
    {
        if (!backup.isEmpty()) {
            QInstaller::removeDirectory(bundle);
            QFile::rename(backup, bundle);
        }
    }

    void release() const
    {
        if (!backup.isEmpty())
            QInstaller::removeDirectory(backup);
        backup.clear();
    }

private:
    const QString bundle;
    mutable QString backup;
};

class WorkingDirectoryChange
{
public:
    explicit WorkingDirectoryChange(const QString &path)
        : oldPath(QDir::currentPath())
    {
        QDir::setCurrent(path);
    }

    virtual ~WorkingDirectoryChange()
    {
        QDir::setCurrent(oldPath);
    }

private:
    const QString oldPath;
};

void copyConfigData(const QString &configFile, const QString &targetDir);
void copyHighDPIImage(const QFileInfo &childFileInfo, const QString &childName, const QString &targetFile);

int IFWTOOLS_EXPORT createBinary(BinaryCreatorArgs args, QString &argumentError);
void IFWTOOLS_EXPORT createMTDatFile(QFile &datFile);

} // namespace QInstallerTools

#endif // BINARYCREATOR_H
