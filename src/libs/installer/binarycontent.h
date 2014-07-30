/**************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
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

#ifndef BINARYCONTENT_H
#define BINARYCONTENT_H

#include "binaryformat.h"
#include "range.h"

#include <QVector>

QT_BEGIN_NAMESPACE
class QFile;
QT_END_NAMESPACE

namespace QInstaller {

struct BinaryLayout
{
    QVector<Range<qint64> > metadataResourceSegments;
    qint64 operationsStart;
    qint64 operationsEnd;
    qint64 resourceCount;
    qint64 dataBlockSize;
    qint64 magicMarker;
    quint64 magicCookie;
    qint64 indexSize;
    qint64 endOfData;
};

class INSTALLER_EXPORT BinaryContent
{
public:
    static const qint64 MagicInstallerMarker = 0x12023233UL;
    static const qint64 MagicUninstallerMarker = 0x12023234UL;

    static const qint64 MagicUpdaterMarker = 0x12023235UL;
    static const qint64 MagicPackageManagerMarker = 0x12023236UL;

    // the cookie put at the end of the file
    static const quint64 MagicCookie = 0xc2630a1c99d668f8LL;  // binary
    static const quint64 MagicCookieDat = 0xc2630a1c99d668f9LL; // data

    BinaryContent();
    ~BinaryContent();

    BinaryContent(const BinaryContent &rhs);
    BinaryContent &operator=(const BinaryContent &rhs);

    static BinaryContent readFromBinary(const QString &path);
    static BinaryContent readAndRegisterFromBinary(const QString &path);
    static BinaryLayout readBinaryLayout(QFile *const file, qint64 cookiePos);
    static qint64 findMagicCookie(QFile *file, quint64 magicCookie);

    int registerPerformedOperations();
    OperationList performedOperations() const;

    qint64 magicMarker() const;
    int registerEmbeddedQResources();
    void registerAsDefaultQResource(const QString &path);

    static void readBinaryContent(const QSharedPointer<QFile> &in,
                                ResourceCollection *metaResources,
                                QList<OperationBlob> *operations,
                                ResourceCollectionManager *manager,
                                qint64 *magicMarker,
                                quint64 magicCookie);

    static void writeBinaryContent(const QSharedPointer<QFile> &out,
                                const ResourceCollection &metaResources,
                                const QList<OperationBlob> &operations,
                                const ResourceCollectionManager &manager,
                                qint64 magicMarker,
                                quint64 magicCookie);
private:
    explicit BinaryContent(const QString &path);
    static void readBinaryData(BinaryContent &content, const QSharedPointer<QFile> &file,
        const BinaryLayout &layout);

private:
    class Private;
    QSharedDataPointer<Private> d;
};

} // namespace QInstaller

#endif // BINARYCONTENT_H
