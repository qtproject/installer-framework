/**************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
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
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
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
