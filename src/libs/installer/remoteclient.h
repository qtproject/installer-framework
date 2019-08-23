/**************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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

#ifndef REMOTECLIENT_H
#define REMOTECLIENT_H

#include "installer_global.h"
#include "protocol.h"

#include <QScopedPointer>

namespace QInstaller {

class RemoteClientPrivate;

class INSTALLER_EXPORT RemoteClient
{
    Q_DISABLE_COPY(RemoteClient)
    Q_DECLARE_PRIVATE(RemoteClient)

public:
    static RemoteClient &instance();
    void init(const QString &socketName, const QString &key, Protocol::Mode mode,
              Protocol::StartAs startAs);
    void setAuthorizationFallbackDisabled(bool disabled);

    void shutdown();
    void destroy();

    QString socketName() const;
    QString authorizationKey() const;
    QString socketPathName(const QString &socketName) const;

    bool isActive() const;
    void setActive(bool active);

private:
    RemoteClient();
    ~RemoteClient();

private:
    static RemoteClient *s_instance;
    QScopedPointer<RemoteClientPrivate> d_ptr;
};

} // namespace QInstaller

#endif // REMOTECLIENT_H
