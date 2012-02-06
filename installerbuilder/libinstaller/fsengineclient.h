/**************************************************************************
**
** This file is part of Installer Framework
**
** Copyright (c) 2010-2012 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (qt-info@nokia.com)
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
** Nokia at qt-info@nokia.com.
**
**************************************************************************/

#ifndef FSENGINECLIENT_H
#define FSENGINECLIENT_H

#include "installer_global.h"

#include <QtCore/QAbstractFileEngineHandler>

#include <QtNetwork/QHostAddress>

QT_BEGIN_NAMESPACE
class QTcpSocket;
QT_END_NAMESPACE

class INSTALLER_EXPORT FSEngineClientHandler : public QAbstractFileEngineHandler
{
public:
    static FSEngineClientHandler& instance();

    QAbstractFileEngine* create(const QString &fileName) const;
    void init(quint16 port, const QHostAddress &a = QHostAddress::LocalHost);

    bool connect(QTcpSocket *socket);

    bool isActive() const;
    void setActive(bool active);

    void enableTestMode();
    bool isServerRunning() const;
    QString authorizationKey() const;

    void setStartServerCommand(const QString &command, bool startAsAdmin = false);
    void setStartServerCommand(const QString &command, const QStringList &arguments, bool startAsAdmin = false);

protected:
    FSEngineClientHandler();
    ~FSEngineClientHandler();

private:
    class Private;
    Private *d;
};

#endif
