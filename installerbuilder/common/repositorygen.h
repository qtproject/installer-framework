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
#ifndef QINSTALLER_REPOSITORYGEN_H
#define QINSTALLER_REPOSITORYGEN_H

#include <QString>
#include <QStringList>
#include <QVector>

namespace QInstaller {
    struct PackageInfo {
        QString name;
        QString version;
        QString directory;
        QStringList dependencies;
    };


    QMap<QString, QString> buildPathToVersionMap( const QVector<PackageInfo>& info );
    void compressMetaDirectories( const QString &configDir, const QString &repoDir, const QString &baseDir,
                                  const QMap<QString, QString> &versionMapping );
    void compressMetaDirectories( const QString& configDir, const QString& repoDir );
    void compressDirectory( const QStringList &paths, const QString &archivePath );
    void copyComponentData( const QString &packageDir, const QString &configDir, const QString &repoDir,
                            const QVector<PackageInfo> &infos );
    void generateMetaDataDirectory( const QString &outDir, const QString &dataDir,
                                    const QVector<PackageInfo> &packages, const QString &appName,
                                    const QString& appVersion, const QString &redirectUpdateUrl = QString() );
    QVector<PackageInfo> createListOfPackages( const QStringList &components, const QString &packagesDirectory,
                                               bool addDependencies = true );
}

#endif // QINSTALLER_REPOSITORYGEN_H
