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

#include "binarycontent.h"

#include "binarylayout.h"
#include "errors.h"
#include "fileio.h"
#include "fileutils.h"

namespace QInstaller {

/*!
    \class QInstaller::BinaryContent
    \inmodule QtInstallerFramework
    \brief The BinaryContent class handles binary information embedded into executables.

    The following types of binary information can be embedded into executable files: Qt resources,
    performed operations, and resource collections.

    The magic marker is a \c quint64 that identifies the kind of the binary: \c installer or
    \c uninstaller (maintenance tool).

    The magic cookie is a \c quint64 describing whether the binary is the file holding just data
    or whether it includes the executable as well.
*/

/*!
    Searches for the given magic cookie \a magicCookie starting from the end of the file \a in.
    Returns the position of the magic cookie inside the binary. Throws Error on failure.

    \note Searches through up to 1MB of data, if smaller, through the whole file.
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
        // Fallback to read the file content in case we can't map it.

        // Note: Failing to map the file can happen for example while having a remote connection
        // established to the privileged server process and we do not support map over the socket.
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
        // map does not change QFile::pos()
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
    Tries to read the binary layout of the file \a file. It starts searching from the end of the
    file \a file for the given \a magicCookie using findMagicCookie(). If the cookie was found, it
    fills a BinaryLayout structure and returns it. Throws Error on failure.
*/
BinaryLayout BinaryContent::binaryLayout(QFile *file, quint64 magicCookie)
{
    BinaryLayout layout;
    layout.endOfBinaryContent = BinaryContent::findMagicCookie(file, magicCookie) + sizeof(qint64);

    const qint64 posOfMetaDataCount = layout.endOfBinaryContent - (4 * sizeof(qint64));
    if (!file->seek(posOfMetaDataCount)) {
        throw QInstaller::Error(QCoreApplication::translate("BinaryLayout",
            "Cannot seek to %1 to read the embedded meta data count.").arg(posOfMetaDataCount));
    }

    // read the meta resources count
    const qint64 metaResourcesCount = QInstaller::retrieveInt64(file);

    const qint64 posOfResourceCollectionsSegment = layout.endOfBinaryContent
        - ((metaResourcesCount * (2 * sizeof(qint64))) // minus the size of the meta data segments
        + (8 * sizeof(qint64))); // meta count, offset/length collection index, marker, cookie...
    if (!file->seek(posOfResourceCollectionsSegment)) {
        throw Error(QCoreApplication::translate("BinaryLayout",
            "Cannot seek to %1 to read the resource collection segment.")
            .arg(posOfResourceCollectionsSegment));
    }

    // read the resource collection index offset and length
    layout.resourceCollectionsSegment = QInstaller::retrieveInt64Range(file);

    // read the meta data resource segments
    for (int i = 0; i < metaResourcesCount; ++i)
        layout.metaResourceSegments.append(QInstaller::retrieveInt64Range(file));

    if (metaResourcesCount != layout.metaResourceSegments.count()) {
        throw Error(QCoreApplication::translate("BinaryLayout",
            "Unexpected mismatch of meta resources. Read %1, expected: %2.")
            .arg(layout.metaResourceSegments.count()).arg(metaResourcesCount));
    }

    // read the operations offset and length
    layout.operationsSegment = QInstaller::retrieveInt64Range(file);

    // resources count
    Q_UNUSED(QInstaller::retrieveInt64(file)) // read it, but deliberately not used

    // read the binary content size
    layout.binaryContentSize = QInstaller::retrieveInt64(file);
    layout.endOfExectuable = layout.endOfBinaryContent - layout.binaryContentSize;

    layout.magicMarker = QInstaller::retrieveInt64(file);
    layout.magicCookie = QInstaller::retrieveInt64(file);

    // adjust the offsets to match the actual binary
    for (int i = 0; i < layout.metaResourceSegments.count(); ++i)
        layout.metaResourceSegments[i].move(layout.endOfExectuable);
    layout.metaResourcesSegment = Range<qint64>::fromStartAndEnd(layout.metaResourceSegments
        .first().start(), layout.metaResourceSegments.last().end());

    layout.operationsSegment.move(layout.endOfExectuable);
    layout.resourceCollectionsSegment.move(layout.endOfExectuable);

    return layout;
}

/*!
    Reads the binary content of the given file \a file. It starts by reading the binary layout of
    the file using binaryLayout() using \a magicCookie. Throws Error on failure.

    If \a operations is not 0, it is set to the performed operations from a previous run of for
    example the maintenance tool.

    If \a manager is not 0, it is first cleared and then set to the resource collections embedded
    into the binary.

    If \a magicMarker is not 0, it is set to the magic marker found in the binary.
*/
void BinaryContent::readBinaryContent(QFile *file, QList<OperationBlob> *operations,
    ResourceCollectionManager *manager, qint64 *magicMarker, quint64 magicCookie)
{
    const BinaryLayout layout = BinaryContent::binaryLayout(file, magicCookie);

    if (manager)
        manager->clear();

    if (manager) {    // append the meta resources
        ResourceCollection metaResources("QResources");
        foreach (const Range<qint64> &segment, layout.metaResourceSegments) {
            metaResources.appendResource(QSharedPointer<Resource>(new Resource(file->fileName(),
                segment)));
        }
        manager->insertCollection(metaResources);
    }

    if (operations) {
        const qint64 posOfOperationsBlock = layout.operationsSegment.start();
        if (!file->seek(posOfOperationsBlock)) {
            throw Error(QCoreApplication::translate("BinaryContent",
                "Cannot seek to %1 to read the operation data.").arg(posOfOperationsBlock));
        }
        // read the operations count
        qint64 operationsCount = QInstaller::retrieveInt64(file);
        // read the operations
        for (int i = 0; i < operationsCount; ++i) {
            const QString name = QInstaller::retrieveString(file);
            const QString xml = QInstaller::retrieveString(file);
            operations->append(OperationBlob(name, xml));
        }
        // operations count
        Q_UNUSED(QInstaller::retrieveInt64(file)) // read it, but deliberately not used
    }

    if (manager) {    // read the collection index and data
        const qint64 posOfResourceCollectionBlock = layout.resourceCollectionsSegment.start();
        if (!file->seek(posOfResourceCollectionBlock)) {
            throw Error(QCoreApplication::translate("BinaryContent", "Cannot seek to %1 to "
                "read the resource collection block.").arg(posOfResourceCollectionBlock));
        }
        manager->read(file, layout.endOfExectuable);
    }

    if (magicMarker)
        *magicMarker = layout.magicMarker;
}

/*!
    Writes the binary content to the given file \a out. Throws Error on failure.

    The binary content is written in the following order:

    \list
        \li Meta resources \a manager
        \li Operations \a operations
        \li Resource collections \a manager
        \li Magic marker \a magicMarker
        \li Magic cookie \a magicCookie
    \endlist

    For more information see the BinaryLayout documentation.
*/
void BinaryContent::writeBinaryContent(QFile *out, const QList<OperationBlob> &operations,
    const ResourceCollectionManager &manager, qint64 magicMarker, quint64 magicCookie)
{
    const qint64 endOfBinary = out->pos();
    ResourceCollectionManager localManager = manager;

    // resources
    qint64 pos = out->pos();
    QVector<Range<qint64> > metaResourceSegments;
    const ResourceCollection collection = localManager.collectionByName("QResources");
    foreach (const QSharedPointer<Resource> &resource, collection.resources()) {
        const bool isOpen = resource->isOpen();
        if ((!isOpen) && (!resource->open())) {
            throw Error(QCoreApplication::translate("BinaryContent",
                "Cannot open meta resource %1.").arg(resource->errorString()));
        }

        resource->seek(0);
        resource->copyData(out);
        metaResourceSegments.append(Range<qint64>::fromStartAndEnd(pos, out->pos()));
        pos = out->pos();

        if (!isOpen) // If we reach that point, either the resource was opened already...
            resource->close();           // or we did open it and have to close it again.
    }
    localManager.removeCollection("QResources");

    // operations
    QInstaller::appendInt64(out, operations.count());
    foreach (const OperationBlob &operation, operations) {
        QInstaller::appendString(out, operation.name);
        QInstaller::appendString(out, operation.xml);
    }
    QInstaller::appendInt64(out, operations.count());
    const Range<qint64> operationsSegment = Range<qint64>::fromStartAndEnd(pos, out->pos());

    // resource collections data and index
    const Range<qint64> resourceCollectionsSegment = localManager.write(out, -endOfBinary);
    QInstaller::appendInt64Range(out, resourceCollectionsSegment.moved(-endOfBinary));

    // meta resource segments
    foreach (const Range<qint64> &segment, metaResourceSegments)
        QInstaller::appendInt64Range(out, segment.moved(-endOfBinary));

    // operations segment
    QInstaller::appendInt64Range(out, operationsSegment.moved(-endOfBinary));

    // resources count
    QInstaller::appendInt64(out, metaResourceSegments.count());

    const qint64 binaryContentSize = (out->pos() + (3 * sizeof(qint64))) - endOfBinary;
    QInstaller::appendInt64(out, binaryContentSize);
    QInstaller::appendInt64(out, magicMarker);
    QInstaller::appendInt64(out, magicCookie);
}

} // namespace QInstaller
