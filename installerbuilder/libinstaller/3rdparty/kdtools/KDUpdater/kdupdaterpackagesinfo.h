/****************************************************************************
** Copyright (C) 2001-2010 Klaralvdalens Datakonsult AB.  All rights reserved.
**
** This file is part of the KD Tools library.
**
** Licensees holding valid commercial KD Tools licenses may use this file in
** accordance with the KD Tools Commercial License Agreement provided with
** the Software.
**
**
** This file may be distributed and/or modified under the terms of the
** GNU Lesser General Public License version 2 and version 3 as published by the
** Free Software Foundation and appearing in the file LICENSE.LGPL included.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
** Contact info@kdab.com if any conditions of this licensing are not
** clear to you.
**
**********************************************************************/

#ifndef KD_UPDATER_PACKAGES_INFO_H
#define KD_UPDATER_PACKAGES_INFO_H

#include "kdupdater.h"
#include <QObject>
#include <QDate>
#include <QString>
#include <QStringList>
#include <QVariant>

namespace KDUpdater
{
    class Application;
    class UpdateInstaller;

    struct KDTOOLS_UPDATER_EXPORT PackageInfo
    {
        QString name;
        QString pixmap;
        QString title;
        QString description;
        QString version;
        QStringList dependencies;
        QStringList translations;
        QDate lastUpdateDate;
        QDate installDate;
        bool forcedInstallation;
        bool virtualComp;
        quint64 uncompressedSize;
    };

    class KDTOOLS_UPDATER_EXPORT PackagesInfo : public QObject
    {
        Q_OBJECT

    public:
        ~PackagesInfo();
        
        enum Error
        {
            NoError=0,
            NotYetReadError,
            CouldNotReadPackageFileError,
            InvalidXmlError,
            InvalidContentError
        };

        Application* application() const;

        bool isValid() const;
        QString errorString() const;
        Error error() const;
        
        void setFileName(const QString& fileName);
        QString fileName() const;

        void setApplicationName(const QString& name);
        QString applicationName() const;

        void setApplicationVersion(const QString& version);
        QString applicationVersion() const;

        int packageInfoCount() const;
        PackageInfo packageInfo(int index) const;
        int findPackageInfo(const QString& pkgName) const;
        QVector<KDUpdater::PackageInfo> packageInfos() const;
        void writeToDisk();

        int compatLevel() const;
        void setCompatLevel(int level);

        bool installPackage( const QString& pkgName, const QString& version, const QString& title = QString(), const QString& description = QString()
                           , const QStringList& dependencies = QStringList(), bool forcedInstallation = false, bool virtualComp = false, quint64 uncompressedSize = 0 );
        bool updatePackage(const QString &pkgName, const QString &version, const QDate &date );
        bool removePackage( const QString& pkgName );

    public Q_SLOTS:
        void refresh();

    Q_SIGNALS:
        void reset();

    protected:
        explicit PackagesInfo( Application * application=0 );        

    private:
        friend class Application;
        friend class UpdateInstaller;
        struct PackagesInfoData;
        PackagesInfoData* d;
    };

}

#endif
