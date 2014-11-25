/****************************************************************************
**
** Copyright (C) 2013 Klaralvdalens Datakonsult AB (KDAB)
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "kdupdaterfiledownloaderfactory.h"
#include "kdupdaterfiledownloader_p.h"

#include <QtNetwork/QSslSocket>

using namespace KDUpdater;

/*!
   \inmodule kdupdater
   \class KDUpdater::FileDownloaderFactory
   \brief The FileDownloaderFactory class acts as a factory for KDUpdater::FileDownloader.

   You can register one or more file downloaders with this factory and query them based on their
   scheme. The class follows the singleton design pattern. Only one instance of this class can
   be created and its reference can be fetched from the instance() method.
*/


FileDownloaderFactory& FileDownloaderFactory::instance()
{
    static KDUpdater::FileDownloaderFactory theFactory;
    return theFactory;
}

/*!
    Constructor
*/
FileDownloaderFactory::FileDownloaderFactory()
    : d (new FileDownloaderFactoryData)
{
    // Register the default file downloader set
    registerFileDownloader<LocalFileDownloader>( QLatin1String("file"));
    registerFileDownloader<HttpDownloader>(QLatin1String("ftp"));
    registerFileDownloader<HttpDownloader>(QLatin1String("http"));
    registerFileDownloader<ResourceFileDownloader >(QLatin1String("resource"));

#ifndef QT_NO_SSL
    if (QSslSocket::supportsSsl())
        registerFileDownloader<HttpDownloader>(QLatin1String("https"));
    else
        qWarning() << "Could not register file downloader for https protocol: QSslSocket::supportsSsl() returns false";
#endif

    d->m_followRedirects = false;
}

bool FileDownloaderFactory::followRedirects()
{
    return FileDownloaderFactory::instance().d->m_followRedirects;
}

void FileDownloaderFactory::setFollowRedirects(bool val)
{
    FileDownloaderFactory::instance().d->m_followRedirects = val;
}

void FileDownloaderFactory::setProxyFactory(FileDownloaderProxyFactory *factory)
{
    delete FileDownloaderFactory::instance().d->m_factory;
    FileDownloaderFactory::instance().d->m_factory = factory;
}

bool FileDownloaderFactory::ignoreSslErrors()
{
    return FileDownloaderFactory::instance().d->m_ignoreSslErrors;
}

void FileDownloaderFactory::setIgnoreSslErrors(bool ignore)
{
    FileDownloaderFactory::instance().d->m_ignoreSslErrors = ignore;
}

FileDownloaderFactory::~FileDownloaderFactory()
{
    delete d;
}

QStringList FileDownloaderFactory::supportedSchemes()
{
    return FileDownloaderFactory::instance().d->m_supportedSchemes;
}

bool FileDownloaderFactory::isSupportedScheme(const QString &scheme)
{
    return FileDownloaderFactory::instance().d->m_supportedSchemes.contains(scheme
        , Qt::CaseInsensitive);
}

/*!
     Returns a new instance of a KDUpdater::FileDownloader subclass. The subclass is instantiated
     based on the communication protocol string stored in \a scheme.

     \note Ownership of the created object remains with the programmer.
*/
FileDownloader *FileDownloaderFactory::create(const QString &scheme, QObject *parent) const
{
    FileDownloader *downloader = KDGenericFactory<FileDownloader>::create(scheme);
    if (downloader != 0) {
        downloader->setParent(parent);
        downloader->setScheme(scheme);
        downloader->setFollowRedirects(d->m_followRedirects);
        downloader->setIgnoreSslErrors(d->m_ignoreSslErrors);
        if (d->m_factory)
            downloader->setProxyFactory(d->m_factory->clone());
    }
    return downloader;
}

/*!
    \fn void KDUpdater::FileDownloaderFactory::registerFileDownloader(const QString &scheme)

    Registers a new file downloader with the factory based on \a scheme. If there is already
    a downloader with the same scheme, the downloader is replaced. When create() is called
    with that \a scheme, the file downloader is constructed using its default constructor.
*/
