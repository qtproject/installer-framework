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
