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

#ifndef FILEIO_H
#define FILEIO_H

#include "installer_global.h"

QT_BEGIN_NAMESPACE
class QByteArray;
class QFileDevice;
class QString;
QT_END_NAMESPACE

template <typename T>
class Range;

namespace QInstaller {

qint64 INSTALLER_EXPORT retrieveInt64(QFileDevice *in);
void INSTALLER_EXPORT appendInt64(QFileDevice *out, qint64 n);

Range<qint64> INSTALLER_EXPORT retrieveInt64Range(QFileDevice *in);
void INSTALLER_EXPORT appendInt64Range(QFileDevice *out, const Range<qint64> &r);

QString INSTALLER_EXPORT retrieveString(QFileDevice *in);
void INSTALLER_EXPORT appendString(QFileDevice *out, const QString &str);

QByteArray INSTALLER_EXPORT retrieveByteArray(QFileDevice *in);
void INSTALLER_EXPORT appendByteArray(QFileDevice *out, const QByteArray &ba);

QByteArray INSTALLER_EXPORT retrieveData(QFileDevice *in, qint64 size);
void INSTALLER_EXPORT appendData(QFileDevice *out, QFileDevice *in, qint64 size);

void INSTALLER_EXPORT openForRead(QFileDevice *dev);
void INSTALLER_EXPORT openForWrite(QFileDevice *dev);
void INSTALLER_EXPORT openForAppend(QFileDevice *dev);

qint64 INSTALLER_EXPORT blockingRead(QFileDevice *in, char *buffer, qint64 size);
qint64 INSTALLER_EXPORT blockingCopy(QFileDevice *in, QFileDevice *out, qint64 size);

qint64 INSTALLER_EXPORT blockingWrite(QFileDevice *out, const QByteArray &data);
qint64 INSTALLER_EXPORT blockingWrite(QFileDevice *out, const char *data, qint64 size);

} // namespace QInstaller

#endif // FILEIO_H
