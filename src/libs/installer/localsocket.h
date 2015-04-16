/**************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
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
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
**
** $QT_END_LICENSE$
**
**************************************************************************/

#ifndef LOCALSOCKET_H
#define LOCALSOCKET_H

#include <QLocalSocket>

#if defined(Q_OS_WIN) && QT_VERSION < QT_VERSION_CHECK(5,5,0)

// This is a crude hack to work around QLocalSocket::waitForReadyRead returning instantly
// if there are still bytes left in the buffer. This has been fixed in Qt 5.5.0 ...
class LocalSocket : public QLocalSocket
{
public:
    LocalSocket(QObject *parent = 0) : QLocalSocket(parent), inReadyRead(false)
    {
    }

    qint64 bytesAvailable() const {
        if (inReadyRead)
            return 0;
        return QLocalSocket::bytesAvailable();
    }

    bool waitForReadyRead(int msecs = 30000) {
        inReadyRead = true;
        bool result = QLocalSocket::waitForReadyRead(msecs);
        inReadyRead = false;
        return result;
    }

private:
    bool inReadyRead;
};

#else

typedef QLocalSocket LocalSocket;

#endif

#endif // LOCALSOCKET_H
