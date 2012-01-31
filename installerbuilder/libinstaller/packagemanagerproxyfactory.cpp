/**************************************************************************
**
** This file is part of Installer Framework
**
** Copyright (c) 2012 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (info@qt.nokia.com)
**
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
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** Other Usage
**
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
** If you have questions regarding the use of this file, please contact
** Nokia at info@qt.nokia.com.
**
**************************************************************************/
#include "packagemanagerproxyfactory.h"

#include "packagemanagercore.h"
#include "settings.h"

namespace QInstaller {

PackageManagerProxyFactory::PackageManagerProxyFactory(const PackageManagerCore *const core)
    : m_core(core)
{
}

PackageManagerProxyFactory *PackageManagerProxyFactory::clone() const
{
    return new PackageManagerProxyFactory(m_core);
}

QList<QNetworkProxy> PackageManagerProxyFactory::queryProxy(const QNetworkProxyQuery &query)
{
    const Settings &settings = m_core->settings();
    QList<QNetworkProxy> list;

    if (settings.proxyType() == Settings::SystemProxy) {
#if defined(Q_OS_UNIX) && !defined(Q_OS_MAC)
        QUrl proxyUrl = QUrl::fromUserInput(QString::fromUtf8(qgetenv("http_proxy")));
        if (proxyUrl.isValid()) {
            return list << QNetworkProxy(QNetworkProxy::HttpProxy, proxyUrl.host(), proxyUrl.port(),
                proxyUrl.userName(), proxyUrl.password());
        }
#endif
        return QNetworkProxyFactory::systemProxyForQuery(query);
    }

    if ((settings.proxyType() == Settings::NoProxy))
        return list << QNetworkProxy(QNetworkProxy::NoProxy);

    if (query.queryType() == QNetworkProxyQuery::UrlRequest) {
        if (query.url().scheme() == QLatin1String("ftp"))
            return list << settings.ftpProxy();

        if (query.url().scheme() == QLatin1String("http"))
            return list << settings.httpProxy();
    }
    return list << QNetworkProxy(QNetworkProxy::DefaultProxy);
}

}   // QInstaller
