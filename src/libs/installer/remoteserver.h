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

#ifndef REMOTESERVER_H
#define REMOTESERVER_H

#include "installer_global.h"
#include "protocol.h"

#include <QObject>

namespace QInstaller {

class RemoteServerPrivate;

class INSTALLER_EXPORT RemoteServer : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(RemoteServer)
    Q_DECLARE_PRIVATE(RemoteServer)

public:
    explicit RemoteServer(QObject *parent = 0);
    ~RemoteServer();

    void start();
    void init(const QString &socketName, const QString &authorizationKey, Protocol::Mode mode);

    QString socketName() const;
    QString authorizationKey() const;
    QString socketPathName(const QString &socketName) const;

private slots:
    void restartWatchdog();

private:
    QScopedPointer<RemoteServerPrivate> d_ptr;
};

} // namespace QInstaller

#endif // REMOTESERVER_H
