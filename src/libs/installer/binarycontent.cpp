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

#include "binaryformat.h"
#include "binaryformatenginehandler.h"
#include "errors.h"
#include "fileio.h"
#include "fileutils.h"
#include "kdupdaterupdateoperationfactory.h"
#include "utils.h"

#include <QFile>
#include <QResource>

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
    \internal
    Registers the resource found at \a segment within \a file into the Qt resource system.
*/
static QByteArray addResourceFromBinary(QFile* file, const Range<qint64> &segment)
{
    if (segment.length() <= 0)
        return 0;

    if (!file->seek(segment.start())) {
        throw Error(QCoreApplication::translate("BinaryContent",
            "Could not seek to in-binary resource. (offset: %1, length: %2)")
            .arg(QString::number(segment.start()), QString::number(segment.length())));
    }

    QByteArray ba = QInstaller::retrieveData(file, segment.length());
    if (!QResource::registerResource((const uchar*) ba.constData(), QLatin1String(":/metadata"))) {
        throw Error(QCoreApplication::translate("BinaryContent",
            "Could not register in-binary resource."));
    }
    return ba;
}


// -- Private

class BinaryContent::Private : public QSharedData
{
public:
    Private();
    explicit Private(const QString &path);
    Private(const Private &other);
    ~Private();

    qint64 m_magicMarker;
    qint64 m_dataBlockStart;

    QSharedPointer<QFile> m_appBinary;
    QSharedPointer<QFile> m_binaryDataFile;

    QList<Operation *> m_performedOperations;
    QList<QPair<QString, QString> > m_performedOperationsData;

    QVector<QByteArray> m_resourceMappings;
    QVector<Range<qint64> > m_metadataResourceSegments;

    BinaryFormatEngineHandler m_binaryFormatEngineHandler;
};


BinaryContent::Private::Private()
    : m_magicMarker(Q_INT64_C(0))
    , m_dataBlockStart(Q_INT64_C(0))
    , m_appBinary(0)
    , m_binaryDataFile(0)
{}

BinaryContent::Private::Private(const QString &path)
    : m_magicMarker(Q_INT64_C(0))
    , m_dataBlockStart(Q_INT64_C(0))
    , m_appBinary(new QFile(path))
    , m_binaryDataFile(0)
{}

BinaryContent::Private::Private(const Private &other)
    : QSharedData(other)
    , m_magicMarker(other.m_magicMarker)
    , m_dataBlockStart(other.m_dataBlockStart)
    , m_appBinary(other.m_appBinary)
    , m_binaryDataFile(other.m_binaryDataFile)
    , m_performedOperations(other.m_performedOperations)
    , m_performedOperationsData(other.m_performedOperationsData)
    , m_resourceMappings(other.m_resourceMappings)
    , m_metadataResourceSegments(other.m_metadataResourceSegments)
    , m_binaryFormatEngineHandler(other.m_binaryFormatEngineHandler)
{}

BinaryContent::Private::~Private()
{
    foreach (const QByteArray &rccData, m_resourceMappings)
        QResource::unregisterResource((const uchar*) rccData.constData(), QLatin1String(":/metadata"));
    m_resourceMappings.clear();
}


// -- BinaryContent

BinaryContent::BinaryContent()
    : d(new Private)
{}

BinaryContent::~BinaryContent()
{}

BinaryContent::BinaryContent(const QString &path)
    : d(new Private(path))
{}

BinaryContent::BinaryContent(const BinaryContent &rhs)
    : d(rhs.d)
{}

BinaryContent &BinaryContent::operator=(const BinaryContent &rhs)
{
    if (this != &rhs)
        d = rhs.d;
    return *this;
}

/*!
    Reads binary content stored in the passed application binary. Maps the embedded resources into
    memory and instantiates performed operations if available.
*/
BinaryContent BinaryContent::readAndRegisterFromBinary(const QString &path)
{
    BinaryContent c = BinaryContent::readFromBinary(path);
    c.registerEmbeddedQResources();
    c.registerPerformedOperations();
    return c;
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

BinaryContent BinaryContent::readFromBinary(const QString &path)
{
    BinaryContent c(path);

    // Try to read the binary layout of the calling application. We need to figure out
    // if we are in installer or an unistaller (maintenance, package manager, updater) binary.
    QInstaller::openForRead(c.d->m_appBinary.data());
    quint64 cookiePos = findMagicCookie(c.d->m_appBinary.data(), BinaryContent::MagicCookie);
    if (!c.d->m_appBinary->seek(cookiePos - sizeof(qint64))) {   // seek to read the marker
        throw Error(QCoreApplication::translate("BinaryContent",
            "Could not seek to %1 to read the magic marker.").arg(cookiePos - sizeof(qint64)));
    }
    const qint64 magicMarker = QInstaller::retrieveInt64(c.d->m_appBinary.data());

    if (magicMarker != MagicInstallerMarker) {
        // We are not an installer, so we need to read the data from the .dat file.

        QFileInfo fi(path);
        QString bundlePath; // On OSX it's not inside the bundle, deserves TODO.
        if (QInstaller::isInBundle(fi.absoluteFilePath(), &bundlePath))
            fi.setFile(bundlePath);

        c.d->m_binaryDataFile.reset(new QFile(fi.absolutePath() + QLatin1Char('/') + fi.baseName()
            + QLatin1String(".dat")));
        QInstaller::openForRead(c.d->m_binaryDataFile.data());
        cookiePos = findMagicCookie(c.d->m_binaryDataFile.data(), BinaryContent::MagicCookieDat);
        readBinaryData(c, c.d->m_binaryDataFile, readBinaryLayout(c.d->m_binaryDataFile.data(),
            cookiePos));
    } else {
        // We are an installer, all data is appended to our binary itself.
        readBinaryData(c, c.d->m_appBinary, readBinaryLayout(c.d->m_appBinary.data(), cookiePos));
    }
    return c;
}

/* static */
BinaryLayout BinaryContent::readBinaryLayout(QFile *const file, qint64 cookiePos)
{
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

/* static */
void BinaryContent::readBinaryData(BinaryContent &content, const QSharedPointer<QFile> &file,
    const BinaryLayout &layout)
{
    content.d->m_magicMarker = layout.magicMarker;
    content.d->m_metadataResourceSegments = layout.metadataResourceSegments;

    const qint64 dataBlockStart = layout.endOfData - layout.dataBlockSize;
    const qint64 operationsStart = layout.operationsStart + dataBlockStart;
    if (!file->seek(operationsStart))
        throw Error(QCoreApplication::translate("BinaryContent", "Could not seek to operation list."));

    const qint64 operationsCount = QInstaller::retrieveInt64(file.data());
    qDebug() << "Number of operations:" << operationsCount;

    for (int i = 0; i < operationsCount; ++i) {
        const QString name = QInstaller::retrieveString(file.data());
        const QString data = QInstaller::retrieveString(file.data());
        content.d->m_performedOperationsData.append(qMakePair(name, data));
    }

    // seek to the position of the resource collections segment info
    const qint64 resourceOffsetAndLengtSize = 2 * sizeof(qint64);
    const qint64 resourceSectionSize = resourceOffsetAndLengtSize * layout.resourceCount;
    qint64 offset = layout.endOfData - layout.indexSize - resourceSectionSize
        - resourceOffsetAndLengtSize;
    if (!file->seek(offset)) {
        throw Error(QCoreApplication::translate("BinaryContent",
            "Could not seek to read the resource collection segment info."));
    }

    offset = QInstaller::retrieveInt64(file.data()) + dataBlockStart;
    if (!file->seek(offset)) {
        throw Error(QCoreApplication::translate("BinaryContent", "Could not seek to start "
            "position of resource collection block."));
    }

    ResourceCollectionManager collectionManager;
    collectionManager.read(file, dataBlockStart);
    content.d->m_binaryFormatEngineHandler.registerResources(collectionManager.collections());

    if (!QInstaller::isVerbose())
        return;

    const QList<ResourceCollection> collections = collectionManager.collections();
    qDebug() << "Number of resource collections loaded:" << collections.count();
    foreach (const ResourceCollection &collection, collections) {
        const QList<QSharedPointer<Resource> > resources = collection.resources();
        qDebug() << collection.name().data() << "loaded...";
        QStringList resourcesWithSize;
        foreach (const QSharedPointer<Resource> &resource, resources) {
            resourcesWithSize.append(QString::fromLatin1("%1 - %2")
                .arg(QString::fromUtf8(resource->name()), humanReadableSize(resource->size())));
        }
        if (!resourcesWithSize.isEmpty()) {
            qDebug() << " - " << resources.count() << "resources: "
                << qPrintable(resourcesWithSize.join(QLatin1String("; ")));
        }
    }
}

/*!
    Registers already performed operations.
*/
int BinaryContent::registerPerformedOperations()
{
    if (d->m_performedOperations.count() > 0)
        return d->m_performedOperations.count();

    for (int i = 0; i < d->m_performedOperationsData.count(); ++i) {
        const QPair<QString, QString> opPair = d->m_performedOperationsData.at(i);
        QScopedPointer<Operation> op(KDUpdater::UpdateOperationFactory::instance().create(opPair.first));
        if (op.isNull()) {
            qWarning() << QString::fromLatin1("Failed to load unknown operation %1").arg(opPair.first);
            continue;
        }

        if (!op->fromXml(opPair.second)) {
            qWarning() << "Failed to load XML for operation:" << opPair.first;
            continue;
        }
        d->m_performedOperations.append(op.take());
    }
    return d->m_performedOperations.count();
}

/*!
    Returns the operations performed during installation. Returns an empty list if no operations
    are instantiated, performed or the binary is the installer application.
*/
OperationList BinaryContent::performedOperations() const
{
    return d->m_performedOperations;
}

/*!
    Returns the magic marker found in the binary. Returns 0 if no marker has been found.
*/
qint64 BinaryContent::magicMarker() const
{
    return d->m_magicMarker;
}

/*!
    Registers the Qt resources embedded in this binary.
*/
int BinaryContent::registerEmbeddedQResources()
{
    if (d->m_resourceMappings.count() > 0)
        return d->m_resourceMappings.count();

    const bool hasBinaryDataFile = !d->m_binaryDataFile.isNull();
    QFile *const data = hasBinaryDataFile ? d->m_binaryDataFile.data() : d->m_appBinary.data();
    if (data != 0 && !data->isOpen() && !data->open(QIODevice::ReadOnly)) {
        throw Error(QCoreApplication::translate("BinaryContent", "Could not open binary %1: %2")
            .arg(data->fileName(), data->errorString()));
    }

    foreach (const Range<qint64> &i, d->m_metadataResourceSegments)
        d->m_resourceMappings.append(addResourceFromBinary(data, i));

    d->m_appBinary.clear();
    if (hasBinaryDataFile)
        d->m_binaryDataFile.clear();

    return d->m_resourceMappings.count();
}

/*!
    Registers the passed file as default resource content. If the embedded resources are already
    mapped into memory, it will replace the first with the new content.
*/
void BinaryContent::registerAsDefaultQResource(const QString &path)
{
    QFile resource(path);
    bool success = resource.open(QIODevice::ReadOnly);
    if (success && (d->m_resourceMappings.count() > 0)) {
        success = QResource::unregisterResource((const uchar*) d->m_resourceMappings.first()
            .constData(), QLatin1String(":/metadata"));
        if (success)
            d->m_resourceMappings.remove(0);
    }

    if (success) {
        d->m_resourceMappings.prepend(addResourceFromBinary(&resource,
            Range<qint64>::fromStartAndEnd(0, resource.size())));
    } else {
        qWarning() << QString::fromLatin1("Could not register '%1' as default resource.").arg(path);
    }
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
