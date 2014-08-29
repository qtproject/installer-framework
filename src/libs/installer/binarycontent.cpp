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

#include "binarycontent.h"

#include "errors.h"
#include "fileio.h"
#include "fileutils.h"

namespace QInstaller {

/*!
    Search through 1MB, if smaller through the whole file. Note: QFile::map() does
    not change QFile::pos(). Fallback to read the file content in case we can't map it.

    Note: Failing to map the file can happen for example while having a remote connection
    established to the admin server process and we do not support map over the socket.
*/
qint64 BinaryContent::findMagicCookie(QFile *in, quint64 magicCookie)
{
    Q_ASSERT(in);
    Q_ASSERT(in->isOpen());
    Q_ASSERT(in->isReadable());

    const qint64 fileSize = in->size();
    const size_t markerSize = sizeof(qint64);
    const qint64 maxSearch = qMin((1024LL * 1024LL), fileSize);

    QByteArray data(maxSearch, Qt::Uninitialized);
    uchar *const mapped = in->map(fileSize - maxSearch, maxSearch);
    if (!mapped) {
        const int pos = in->pos();
        try {
            in->seek(fileSize - maxSearch);
            QInstaller::blockingRead(in, data.data(), maxSearch);
            in->seek(pos);
        } catch (const Error &error) {
            in->seek(pos);
            throw error;
        }
    } else {
        data = QByteArray((const char*) mapped, maxSearch);
        in->unmap(mapped);
    }

    qint64 searched = maxSearch - markerSize;
    while (searched >= 0) {
        if (memcmp(&magicCookie, (data.data() + searched), markerSize) == 0)
            return (fileSize - maxSearch) + searched;
        --searched;
    }
    throw Error(QCoreApplication::translate("QInstaller", "No marker found, stopped after %1.")
        .arg(humanReadableSize(maxSearch)));

    return -1; // never reached
}

/*!
* \class QInstaller::BinaryContent
*
* BinaryContent handles binary information embedded into executables.
* Qt resources as well as resource collections can be stored.
*
* Explanation of the binary blob at the end of the installer or separate data file:
*
* \verbatim
*
* Meta data entry [1 ... n]
* [Format]
*     Plain data (QResource)
* [Format]
* ----------------------------------------------------------
* Operation count (qint64)
* Operation entry [1 ... n]
* [Format]
*     Name (qint64, QString)
*     XML (qint64, QString)
* [Format]
* Operation count (qint64)
* ----------------------------------------------------------
* Component count
* Component data entry [1 ... n]
* [Format]
*     Archive count (qint64),
*     Name entry [1 ... n]
*     [Format]
*         Name (qint64, QByteArray),
*         Offset (qint64),
*         Length (qint64),
*     [Format]
*     Archive data entry [1 ... n]
*     [Format]
*         Plain data
*     [Format]
* [Format]
* ----------------------------------------------------------
* Component count (qint64)
* Component index entry [1 ... n]
* [Format]
*     Name (qint64, QByteArray)
*     Offset (qint64)
*     Length (qint64)
* [Format]
* Component count (qint64)
* ----------------------------------------------------------
* Component index block [Offset (qint64)]
* Component index block [Length (qint64)]
* ----------------------------------------------------------
* Resource segments [1 ... n]
* [Format]
*     Offset (qint64)
*     Length (qint64)
* [Format]
* ----------------------------------------------------------
* Operations information block [Offset (qint64)]
* Operations information block [Length (qint64)]
* ----------------------------------------------------------
* Meta data count (qint64)
* ----------------------------------------------------------
* Binary content size [Including Marker and Cookie (qint64)]
* ----------------------------------------------------------
* Magic marker (qint64)
* Magic cookie (qint64)

* \endverbatim
*/

/* static */
BinaryLayout BinaryContent::binaryLayout(QFile *file, quint64 magicCookie)
{
    const qint64 cookiePos = BinaryContent::findMagicCookie(file, magicCookie);
    const qint64 indexSize = 5 * sizeof(qint64);
    if (!file->seek(cookiePos - indexSize)) {
        throw Error(QCoreApplication::translate("BinaryContent",
            "Could not seek to binary layout section."));
    }

    BinaryLayout layout;
    layout.operationsStart = QInstaller::retrieveInt64(file);
    layout.operationsEnd = QInstaller::retrieveInt64(file);
    layout.resourceCount = QInstaller::retrieveInt64(file);
    layout.dataBlockSize = QInstaller::retrieveInt64(file);
    layout.magicMarker = QInstaller::retrieveInt64(file);
    layout.magicCookie = QInstaller::retrieveInt64(file);
    layout.indexSize = indexSize + sizeof(qint64);
    layout.endOfData = file->pos();

    qDebug() << "Operations start:" << layout.operationsStart;
    qDebug() << "Operations end:" << layout.operationsEnd;
    qDebug() << "Resource count:" << layout.resourceCount;
    qDebug() << "Data block size:" << layout.dataBlockSize;
    qDebug() << "Magic marker:" << layout.magicMarker;
    qDebug() << "Magic cookie:" << layout.magicCookie;
    qDebug() << "Index size:" << layout.indexSize;
    qDebug() << "End of data:" << layout.endOfData;

    const qint64 resourceOffsetAndLengtSize = 2 * sizeof(qint64);
    const qint64 dataBlockStart = layout.endOfData - layout.dataBlockSize;
    for (int i = 0; i < layout.resourceCount; ++i) {
        const qint64 offset = layout.endOfData - layout.indexSize
            - (resourceOffsetAndLengtSize * (i + 1));
        if (!file->seek(offset)) {
            throw Error(QCoreApplication::translate("BinaryContent",
                "Could not seek to metadata index."));
        }
        const qint64 metadataResourceOffset = QInstaller::retrieveInt64(file);
        const qint64 metadataResourceLength = QInstaller::retrieveInt64(file);
        layout.metadataResourceSegments.append(Range<qint64>::fromStartAndLength(metadataResourceOffset
            + dataBlockStart, metadataResourceLength));
    }

    return layout;
}

void BinaryContent::readBinaryContent(const QSharedPointer<QFile> &in,
    ResourceCollection *metaResources, QList<OperationBlob> *operations,
    ResourceCollectionManager *manager, qint64 *magicMarker, quint64 magicCookie)
{
    const qint64 pos = BinaryContent::findMagicCookie(in.data(), magicCookie);
    const qint64 endOfBinaryContent = pos + sizeof(qint64);

    const qint64 posOfMetaDataCount = endOfBinaryContent - (4 * sizeof(qint64));
    if (!in->seek(posOfMetaDataCount)) {
        throw Error(QCoreApplication::translate("BinaryContent",
            "Could not seek to %1 to read the embedded meta data count.").arg(posOfMetaDataCount));
    }
    // read the meta resources count
    const qint64 metaResourcesCount = QInstaller::retrieveInt64(in.data());

    const qint64 posOfResourceCollectionsSegment = endOfBinaryContent
        - ((metaResourcesCount * (2 * sizeof(qint64))) // minus the size of the meta data segments
        + (8 * sizeof(qint64))); // meta count, offset/length component index, marker, cookie...
    if (!in->seek(posOfResourceCollectionsSegment)) {
        throw Error(QCoreApplication::translate("BinaryContent",
            "Could not seek to %1 to read the resource collection segment.")
            .arg(posOfResourceCollectionsSegment));
    }
    // read the resource collection index offset and length
    const Range<qint64> resourceCollectionsSegment = QInstaller::retrieveInt64Range(in.data());

    // read the meta data resource segments
    QVector<Range<qint64> > metaDataResourceSegments;
    for (int i = 0; i < metaResourcesCount; ++i)
        metaDataResourceSegments.append(QInstaller::retrieveInt64Range(in.data()));

    // read the operations offset and length
    const Range<qint64> operationsSegment = QInstaller::retrieveInt64Range(in.data());

    // resources count
    Q_UNUSED(QInstaller::retrieveInt64(in.data())) // read it, but deliberately not used

    // read the binary content size
    const qint64 binaryContentSize = QInstaller::retrieveInt64(in.data());
    const qint64 endOfBinary = endOfBinaryContent - binaryContentSize; // end of "compiled" binary

    // read the marker
    const qint64 marker = QInstaller::retrieveInt64(in.data());
    if (magicMarker)
        *magicMarker = marker;

    // the cookie
    Q_UNUSED(QInstaller::retrieveInt64(in.data())) // read it, but deliberately not used

    // append the calculated resource segments
    if (metaResources) {
        foreach (const Range<qint64> &segment, metaDataResourceSegments) {
            metaResources->appendResource(QSharedPointer<Resource>(new Resource(in,
                segment.moved(endOfBinary))));
        }
    }

    const qint64 posOfOperationsBlock = endOfBinary + operationsSegment.start();
    if (!in->seek(posOfOperationsBlock)) {
        throw Error(QCoreApplication::translate("BinaryContent",
            "Could not seek to %1 to read the operation data.").arg(posOfOperationsBlock));
    }
    // read the operations count
    qint64 operationsCount = QInstaller::retrieveInt64(in.data());
    // read the operations
    for (int i = 0; i < operationsCount; ++i) {
        const QString name = QInstaller::retrieveString(in.data());
        const QString xml = QInstaller::retrieveString(in.data());
        if (operations)
            operations->append(OperationBlob(name, xml));
    }
    // operations count
    Q_UNUSED(QInstaller::retrieveInt64(in.data())) // read it, but deliberately not used

    // read the resource collections count
    const qint64 collectionCount = QInstaller::retrieveInt64(in.data());

    const qint64 posOfResourceCollectionBlock = endOfBinary + resourceCollectionsSegment.start();
    if (!in->seek(posOfResourceCollectionBlock)) {
        throw Error(QCoreApplication::translate("BinaryContent",
            "Could not seek to %1 to read the resource collection block.")
            .arg(posOfResourceCollectionBlock));
    }
    if (manager) {    // read the component index and data
        manager->read(in, endOfBinary);
        if (manager->collectionCount() != collectionCount) {
            throw Error(QCoreApplication::translate("BinaryContent",
                "Unexpected mismatch of resource collections. Read %1, expected: %2.")
                .arg(manager->collectionCount()).arg(collectionCount));
        }
    }
}

void BinaryContent::writeBinaryContent(const QSharedPointer<QFile> &out,
    const ResourceCollection &metaResources, const QList<OperationBlob> &operations,
    const ResourceCollectionManager &manager, qint64 magicMarker, quint64 magicCookie)
{
    const qint64 endOfBinary = out->pos();

    // resources
    qint64 pos = out->pos();
    QVector<Range<qint64> > metaResourceSegments;
    foreach (const QSharedPointer<Resource> &resource, metaResources.resources()) {
        const bool isOpen = resource->isOpen();
        if ((!isOpen) && (!resource->open())) {
            throw Error(QCoreApplication::translate("BinaryContent",
                "Could not open meta resource. Error: %1").arg(resource->errorString()));
        }

        resource->seek(0);
        resource->copyData(out.data());
        metaResourceSegments.append(Range<qint64>::fromStartAndEnd(pos, out->pos())
            .moved(-endOfBinary));
        pos = out->pos();

        if (!isOpen) // If we reach that point, either the resource was opened already...
            resource->close();           // or we did open it and have to close it again.
    }

    // operations
    QInstaller::appendInt64(out.data(), operations.count());
    foreach (const OperationBlob &operation, operations) {
        QInstaller::appendString(out.data(), operation.name);
        QInstaller::appendString(out.data(), operation.xml);
    }
    QInstaller::appendInt64(out.data(), operations.count());
    const Range<qint64> operationsSegment = Range<qint64>::fromStartAndEnd(pos, out->pos())
        .moved(-endOfBinary);

    // resource collections data and index
    const Range<qint64> resourceCollectionsSegment = manager.write(out.data(), -endOfBinary)
        .moved(-endOfBinary);
    QInstaller::appendInt64Range(out.data(), resourceCollectionsSegment);

    // meta resource segments
    foreach (const Range<qint64> &segment, metaResourceSegments)
        QInstaller::appendInt64Range(out.data(), segment);

    // operations segment
    QInstaller::appendInt64Range(out.data(), operationsSegment);

    // resources count
    QInstaller::appendInt64(out.data(), metaResourceSegments.count());

    const qint64 binaryContentSize = (out->pos() + (3 * sizeof(qint64))) - endOfBinary;
    QInstaller::appendInt64(out.data(), binaryContentSize);
    QInstaller::appendInt64(out.data(), magicMarker);
    QInstaller::appendInt64(out.data(), magicCookie);
}

} // namespace QInstaller
