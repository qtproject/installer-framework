/****************************************************************************
**
** Copyright (C) 2013 Klaralvdalens Datakonsult AB (KDAB)
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
****************************************************************************/

#ifndef FILEDOWNLOADERFACTORY_H
#define FILEDOWNLOADERFACTORY_H

#include "genericfactory.h"
#include "updater.h"

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

class KDTOOLS_EXPORT FileDownloaderFactory : public GenericFactory<FileDownloader, QString,
                                                                     QObject*>
{
    Q_DISABLE_COPY(FileDownloaderFactory)
    struct FileDownloaderFactoryData {
        FileDownloaderFactoryData() : m_factory(0) {}
        ~FileDownloaderFactoryData() { delete m_factory; }

        bool m_followRedirects;
        bool m_ignoreSslErrors;
        QStringList m_supportedSchemes;
        FileDownloaderProxyFactory *m_factory;
    };

public:
    static FileDownloaderFactory &instance();
    ~FileDownloaderFactory();

    template<typename T>
    void registerFileDownloader(const QString &scheme)
    {
        registerProduct<T>(scheme);
        d->m_supportedSchemes.append(scheme);
    }
    FileDownloader *create(const QString &scheme, QObject *parent = 0) const;

    static bool followRedirects();
    static void setFollowRedirects(bool val);

    static void setProxyFactory(FileDownloaderProxyFactory *factory);

    static bool ignoreSslErrors();
    static void setIgnoreSslErrors(bool ignore);

    static QStringList supportedSchemes();
    static bool isSupportedScheme(const QString &scheme);

private:
    FileDownloaderFactory();

private:
    FileDownloaderFactoryData *d;
};

} // namespace KDUpdater

#endif // FILEDOWNLOADERFACTORY_H
