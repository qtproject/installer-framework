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

#ifndef KD_UPDATER_FILE_DOWNLOADER_FACTORY_H
#define KD_UPDATER_FILE_DOWNLOADER_FACTORY_H

#include "kdupdater.h"
#include <kdgenericfactory.h>

#include <QtCore/QStringList>
#include <QtCore/QUrl>

#include <QtNetwork/QNetworkProxyFactory>

QT_BEGIN_NAMESPACE
class QObject;
QT_END_NAMESPACE

namespace KDUpdater {

class FileDownloader;

class KDTOOLS_EXPORT FileDownloaderProxyFactory : public QNetworkProxyFactory
{
    public:
        virtual ~FileDownloaderProxyFactory() {}
        virtual FileDownloaderProxyFactory *clone() const = 0;
};

class KDTOOLS_EXPORT FileDownloaderFactory : public KDGenericFactory<FileDownloader>
{
    Q_DISABLE_COPY(FileDownloaderFactory)

public:
    static FileDownloaderFactory &instance();
    ~FileDownloaderFactory();

    template<typename T>
    void registerFileDownloader(const QString &scheme)
    {
        registerProduct<T>(scheme);
    }
    FileDownloader *create(const QString &scheme, QObject *parent = 0) const;

    static bool followRedirects();
    static void setFollowRedirects(bool val);

    static void setProxyFactory(FileDownloaderProxyFactory *factory);

private:
    FileDownloaderFactory();

private:
    struct FileDownloaderFactoryData;
    FileDownloaderFactoryData *d;
};

} // namespace KDUpdater

#endif // KD_UPDATER_FILE_DOWNLOADER_FACTORY_H
