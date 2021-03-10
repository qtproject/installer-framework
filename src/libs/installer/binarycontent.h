/**************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
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

#ifndef BINARYCONTENT_H
#define BINARYCONTENT_H

#include "binaryformat.h"
#include "binarylayout.h"

QT_BEGIN_NAMESPACE
class QFile;
QT_END_NAMESPACE

namespace QInstaller {

class INSTALLER_EXPORT BinaryContent
{
public:
    // the marker to distinguish what kind of binary
    static const qint64 MagicInstallerMarker = 0x12023233UL;
    static const qint64 MagicUninstallerMarker = 0x12023234UL;

    static const qint64 MagicUpdaterMarker = 0x12023235UL;
    static const qint64 MagicPackageManagerMarker = 0x12023236UL;

    // additional distinguishers only used at runtime, not written to the binary itself
    enum MagicMarkerSupplement {
        Default = 0x0,
        OfflineGenerator = 0x1,
        PackageViewer = 0x2
    };

    // the cookie put at the end of the file
    static const quint64 MagicCookie = 0xc2630a1c99d668f8LL;  // binary
    static const quint64 MagicCookieDat = 0xc2630a1c99d668f9LL; // data

    static qint64 findMagicCookie(QFile *file, quint64 magicCookie);
    static BinaryLayout binaryLayout(QFile *file, quint64 magicCookie);

    static void readBinaryContent(QFile *file,
                                QList<OperationBlob> *operations,
                                ResourceCollectionManager *manager,
                                qint64 *magicMarker,
                                quint64 magicCookie);

    static void writeBinaryContent(QFile *out,
                                const QList<OperationBlob> &operations,
                                const ResourceCollectionManager &manager,
                                qint64 magicMarker,
                                quint64 magicCookie);
};

} // namespace QInstaller

#endif // BINARYCONTENT_H
