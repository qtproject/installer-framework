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
