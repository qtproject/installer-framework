/**************************************************************************
**
** Copyright (C) 2012-2013 Digia Plc and/or its subsidiary(-ies).
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

#include "binaryformat.h"

#include "errors.h"
#include "fileio.h"
#include "fileutils.h"
#include "lib7z_facade.h"
#include "utils.h"

#include "kdupdaterupdateoperationfactory.h"

#include <QDebug>
#include <QResource>
#include <QTemporaryFile>

using namespace QInstaller;
using namespace QInstallerCreator;

/*!
    Search through 1MB, if smaller through the whole file. Note: QFile::map() does
    not change QFile::pos(). Fallback to read the file content in case we can't map it.

    Note: Failing to map the file can happen for example while having a remote connection
          established to the admin server process and we do not support map over the socket.
*/
qint64 QInstaller::findMagicCookie(QFile *in, quint64 magicCookie)
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
        data = QByteArray((const char*)mapped, maxSearch);
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
    Creates an archive providing the data in \a path.
    \a path can be a path to a file or to a directory. If it's a file, it's considered to be
    pre-zipped and gets delivered as it is. If it's a directory, it gets zipped by Archive.
 */
Archive::Archive(const QString &path)
    : m_device(0)
    , m_isTempFile(false)
    , m_path(path)
    , m_name(QFileInfo(path).fileName().toUtf8())
{
}

Archive::Archive(const QByteArray &identifier, const QByteArray &data)
    : m_device(0)
    , m_isTempFile(true)
    , m_path(generateTemporaryFileName())
    , m_name(identifier)
{
    QFile file(m_path);
    file.open(QIODevice::WriteOnly);
    file.write(data);
}

/*!
    Creates an archive identified by \a identifier providing a data \a segment within a \a device.
 */
Archive::Archive(const QByteArray &identifier, const QSharedPointer<QFile> &device, const Range<qint64> &segment)
    : m_device(device)
    , m_segment(segment)
    , m_isTempFile(false)
    , m_name(identifier)
{
}

Archive::~Archive()
{
    if (isOpen())
        close();
    if (m_isTempFile)
        QFile::remove(m_path);
}

/*!
    \reimp
 */
bool Archive::seek(qint64 pos)
{
    if (m_inputFile.isOpen())
        return m_inputFile.seek(pos) && QIODevice::seek(pos);
    return QIODevice::seek(pos);
}

QByteArray Archive::name() const
{
    return m_name;
}

void Archive::setName(const QByteArray &name)
{
    m_name = name;
}

/*!
    \reimpl
 */
void Archive::close()
{
    m_inputFile.close();
    if (QFileInfo(m_path).isDir())
        m_inputFile.remove();
    QIODevice::close();
}

/*!
    \reimp
 */
bool Archive::open(OpenMode mode)
{
    if (isOpen())
        return false;

    const bool writeOnly = (mode & QIODevice::WriteOnly) != QIODevice::NotOpen;
    const bool append = (mode & QIODevice::Append) != QIODevice::NotOpen;

    // no write support
    if (writeOnly || append)
        return false;

    if (m_device != 0)
        return QIODevice::open(mode);

    const QFileInfo fi(m_path);
    if (fi.isFile()) {
        m_inputFile.setFileName(m_path);
        if (!m_inputFile.open(mode)) {
            setErrorString(tr("Could not open archive file %1 for reading.").arg(m_path));
            return false;
        }
        setOpenMode(mode);
        return true;
    }

    if (fi.isDir()) {
        if (m_inputFile.fileName().isEmpty() || !m_inputFile.exists()) {
            if (!createZippedFile())
                return false;
        }
        Q_ASSERT(!m_inputFile.fileName().isEmpty());
        if (!m_inputFile.open(mode))
            return false;
        setOpenMode(mode);
        return true;
    }

    setErrorString(tr("Could not create archive from %1: Not a file.").arg(m_path));
    return false;
}

bool Archive::createZippedFile()
{
    QTemporaryFile file;
    file.setAutoRemove(false);
    if (!file.open())
        return false;

    m_inputFile.setFileName(file.fileName());
    file.close();
    m_inputFile.open(QIODevice::ReadWrite);
    try {
        Lib7z::createArchive(&m_inputFile, QStringList() << m_path);
    } catch(Lib7z::SevenZipException &e) {
        m_inputFile.close();
        setErrorString(e.message());
        return false;
    }

    if (!Lib7z::isSupportedArchive(&m_inputFile)) {
        m_inputFile.close();
        setErrorString(tr("Error while packing directory at %1").arg(m_path));
        return false;
    }
    m_inputFile.close();
    return true;
}

/*!
    \reimp
 */
qint64 Archive::size() const
{
    // if we got a device, we just pass the length of the segment
    if (m_device != 0)
        return m_segment.length();

    const QFileInfo fi(m_path);
    // if we got a regular file, we pass the size of the file
    if (fi.isFile())
        return fi.size();

    if (fi.isDir()) {
        if (m_inputFile.fileName().isEmpty() || !m_inputFile.exists()) {
            if (!const_cast< Archive* >(this)->createZippedFile()) {
                throw Error(QCoreApplication::translate("Archive",
                    "Cannot create zipped file for path %1: %2").arg(m_path, errorString()));
            }
        }
        Q_ASSERT(!m_inputFile.fileName().isEmpty());
        return m_inputFile.size();
    }
    return 0;
}

/*!
    \reimp
 */
qint64 Archive::readData(char* data, qint64 maxSize)
{
    if (m_device == 0)
        return m_inputFile.read(data, maxSize);

    const qint64 p = m_device->pos();
    m_device->seek(m_segment.start() + pos());
    const qint64 amountRead = m_device->read(data, qMin<quint64>(maxSize, m_segment.length() - pos()));
    m_device->seek(p);
    return amountRead;
}

/*!
    \reimp
 */
qint64 Archive::writeData(const char* data, qint64 maxSize)
{
    Q_UNUSED(data);
    Q_UNUSED(maxSize);
    // should never be called, as we're read only
    return -1;
}

QByteArray Component::name() const
{
    return m_name;
}

void Component::setName(const QByteArray &ba)
{
    m_name = ba;
}

Range<qint64> Component::binarySegment() const
{
    return m_binarySegment;
}

void Component::setBinarySegment(const Range<qint64> &r)
{
    m_binarySegment = r;
}

Component Component::readFromIndexEntry(const QSharedPointer<QFile> &in, qint64 offset)
{
    Component c;
    c.m_name = QInstaller::retrieveByteArray(in.data());
    c.m_binarySegment = QInstaller::retrieveInt64Range(in.data()).moved(offset);

    c.readData(in, offset);

    return c;
}

void Component::writeIndexEntry(QFileDevice *out, qint64 positionOffset) const
{
    QInstaller::appendByteArray(out, m_name);
    m_binarySegment.moved(positionOffset);
    QInstaller::appendInt64(out, binarySegment().start());
    QInstaller::appendInt64(out, binarySegment().length());
}

void Component::writeData(QFileDevice *out, qint64 offset) const
{
    const qint64 dataBegin = out->pos() + offset;

    QInstaller::appendInt64(out, m_archives.count());

    qint64 start = out->pos() + offset;

    // Why 16 + 16? This is 24, not 32???
    const int foo = 3 * sizeof(qint64);
    // add 16 + 16 + number of name characters for each archive (the size of the table)
    foreach (const QSharedPointer<Archive> &archive, m_archives)
        start += foo + archive->name().count();

    QList<qint64> starts;
    foreach (const QSharedPointer<Archive> &archive, m_archives) {
        QInstaller::appendByteArray(out, archive->name());
        starts.push_back(start);
        QInstaller::appendInt64Range(out, Range<qint64>::fromStartAndLength(start, archive->size()));
        start += archive->size();
    }

    foreach (const QSharedPointer<Archive> &archive, m_archives) {
        if (!archive->open(QIODevice::ReadOnly)) {
            throw Error(tr("Could not open archive %1: %2").arg(QLatin1String(archive->name()),
                archive->errorString()));
        }

        const qint64 expectedStart = starts.takeFirst();
        const qint64 actualStart = out->pos() + offset;
        Q_UNUSED(expectedStart);
        Q_UNUSED(actualStart);
        Q_ASSERT(expectedStart == actualStart);
        QInstaller::blockingCopy(archive.data(), out, archive->size());
    }

    m_binarySegment = Range<qint64>::fromStartAndEnd(dataBegin, out->pos() + offset);
}

void Component::readData(const QSharedPointer<QFile> &in, qint64 offset)
{
    const qint64 pos = in->pos();

    in->seek(m_binarySegment.start());
    const qint64 count = QInstaller::retrieveInt64(in.data());

    QVector<QByteArray> names;
    QVector<Range<qint64> > ranges;
    for (int i = 0; i < count; ++i) {
        names.push_back(QInstaller::retrieveByteArray(in.data()));
        ranges.push_back(QInstaller::retrieveInt64Range(in.data()).moved(offset));
    }

    for (int i = 0; i < ranges.count(); ++i)
        m_archives.append(QSharedPointer<Archive>(new Archive(names.at(i), in, ranges.at(i))));

    in->seek(pos);
}

QString Component::dataDirectory() const
{
    return m_dataDirectory;
}

void Component::setDataDirectory(const QString &path)
{
    m_dataDirectory = path;
}

bool Component::operator<(const Component& other) const
{
    if (m_name != other.name())
        return m_name < other.m_name;
    return m_binarySegment < other.m_binarySegment;
}

bool Component::operator==(const Component& other) const
{
    return m_name == other.m_name && m_binarySegment == other.m_binarySegment;
}

/*!
    Destroys this component.
 */
Component::~Component()
{
}

/*!
    Appends \a archive to this component. The component takes ownership of \a archive.
 */
void Component::appendArchive(const QSharedPointer<Archive>& archive)
{
    Q_ASSERT(archive);
    archive->setParent(0);
    m_archives.push_back(archive);
}

/*!
    Returns the archives associated with this component.
 */
QVector<QSharedPointer<Archive> > Component::archives() const
{
    return m_archives;
}

QSharedPointer<Archive> Component::archiveByName(const QByteArray &name) const
{
    foreach (const QSharedPointer<Archive>& i, m_archives) {
        if (i->name() == name)
            return i;
    }
    return QSharedPointer<Archive>();
}


// -- ComponentIndex

ComponentIndex::ComponentIndex()
{
}

ComponentIndex ComponentIndex::read(const QSharedPointer<QFile> &dev, qint64 offset)
{
    ComponentIndex result;
    const qint64 size = QInstaller::retrieveInt64(dev.data());
    for (int i = 0; i < size; ++i)
        result.insertComponent(Component::readFromIndexEntry(dev, offset));
    QInstaller::retrieveInt64(dev.data());
    return result;
}

void ComponentIndex::writeIndex(QFileDevice *out, qint64 offset) const
{
    // Q: why do we write the size twice?
    // A: for us to be able to read it beginning from the end of the file as well
    QInstaller::appendInt64(out, componentCount());
    foreach (const Component& i, components())
        i.writeIndexEntry(out, offset);
    QInstaller::appendInt64(out, componentCount());
}

void ComponentIndex::writeComponentData(QFileDevice *out, qint64 offset) const
{
    QInstaller::appendInt64(out, componentCount());

    foreach (const Component &component, m_components)
        component.writeData(out, offset);
}

Component ComponentIndex::componentByName(const QByteArray &id) const
{
    return m_components.value(id);
}

void ComponentIndex::insertComponent(const Component& c)
{
    m_components.insert(c.name(), c);
}

void ComponentIndex::removeComponent(const QByteArray &name)
{
    m_components.remove(name);
}

QVector<Component> ComponentIndex::components() const
{
    return m_components.values().toVector();
}

int ComponentIndex::componentCount() const
{
    return m_components.size();
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
    if (!QResource::registerResource((const uchar*)ba.constData(), QLatin1String(":/metadata"))) {
            throw Error(QCoreApplication::translate("BinaryContent",
                "Could not register in-binary resource."));
    }
    return ba;
}


// -- BinaryContentPrivate

BinaryContentPrivate::BinaryContentPrivate()
    : m_magicMarker(Q_INT64_C(0))
    , m_dataBlockStart(Q_INT64_C(0))
    , m_appBinary(0)
    , m_binaryDataFile(0)
    , m_binaryFormatEngineHandler(m_componentIndex)
{
}

BinaryContentPrivate::BinaryContentPrivate(const QString &path)
    : m_magicMarker(Q_INT64_C(0))
    , m_dataBlockStart(Q_INT64_C(0))
    , m_appBinary(new QFile(path))
    , m_binaryDataFile(0)
    , m_binaryFormatEngineHandler(m_componentIndex)
{
}

BinaryContentPrivate::BinaryContentPrivate(const BinaryContentPrivate &other)
    : QSharedData(other)
    , m_magicMarker(other.m_magicMarker)
    , m_dataBlockStart(other.m_dataBlockStart)
    , m_appBinary(other.m_appBinary)
    , m_binaryDataFile(other.m_binaryDataFile)
    , m_performedOperations(other.m_performedOperations)
    , m_performedOperationsData(other.m_performedOperationsData)
    , m_resourceMappings(other.m_resourceMappings)
    , m_metadataResourceSegments(other.m_metadataResourceSegments)
    , m_componentIndex(other.m_componentIndex)
    , m_binaryFormatEngineHandler(other.m_binaryFormatEngineHandler)
{
}

BinaryContentPrivate::~BinaryContentPrivate()
{
    foreach (const QByteArray &rccData, m_resourceMappings)
        QResource::unregisterResource((const uchar*)rccData.constData(), QLatin1String(":/metadata"));
    m_resourceMappings.clear();
}


// -- BinaryContent

BinaryContent::BinaryContent()
    : d(new BinaryContentPrivate)
{
}

BinaryContent::BinaryContent(const QString &path)
    : d(new BinaryContentPrivate(path))
{
}

BinaryContent::BinaryContent(const BinaryContent &rhs)
    : d(rhs.d)
{
}

/*!
    Reads binary content stored in the current application binary. Maps the embedded resources into memory
    and instantiates performed operations if available.
*/
BinaryContent BinaryContent::readAndRegisterFromApplicationFile()
{
    BinaryContent c = BinaryContent::readFromApplicationFile();
    c.registerEmbeddedQResources();
    c.registerPerformedOperations();
    return c;
}

/*!
    Reads binary content stored in the passed application binary. Maps the embedded resources into memory
    and instantiates performed operations if available.
*/
BinaryContent BinaryContent::readAndRegisterFromBinary(const QString &path)
{
    BinaryContent c = BinaryContent::readFromBinary(path);
    c.registerEmbeddedQResources();
    c.registerPerformedOperations();
    return c;
}

/*!
    Reads binary content stored in the current application binary.
*/
BinaryContent BinaryContent::readFromApplicationFile()
{
#ifdef Q_OS_MAC
    // On Mac, data is always in a separate file so that the binary can be signed
    QDir dataPath(QCoreApplication::applicationDirPath());
    dataPath.cdUp();
    dataPath.cd(QLatin1String("Resources"));
    return BinaryContent::readFromBinary(dataPath.filePath(QLatin1String("installer.dat")));
#else
    return BinaryContent::readFromBinary(QCoreApplication::applicationFilePath());
#endif
}

/*!
 * \class QInstaller::BinaryContent
 *
 * BinaryContent handles binary information embedded into executables.
 * Qt resources as well as component information can be stored.
 *
 * Explanation of the binary blob at the end of the installer or separate data file:
 *
 * \verbatim
 * Meta data segment 0
 * Meta data segment ...
 * Meta data segment n
 * ------------------------------------------------------
 * Component data segment 0
 * Component data segment ..
 * Component data segment n
 * ------------------------------------------------------
 * Component index segment
 * ------------------------------------------------------
 * quint64 offset of component index segment
 * quint64 length of component index segment
 * ------------------------------------------------------
 * qint64 offset of meta data segment 0
 * qint64 length of meta data segment 0
 * qint64 offset of meta data segment ..
 * qint64 length of meta data segment ..
 * qint64 offset of meta data segment n
 * qint64 length of meta data segment n
 * ------------------------------------------------------
 * operations start offest
 * operations end
 * quint64 embedded resource count
 * quint64 data block size
 * quint64 Magic marker
 * quint64 Magic cookie (0xc2 0x63 0x0a 0x1c 0x99 0xd6 0x68 0xf8)
 * <eof>
 *
 * All offsets are addresses relative to the end of the file.
 *
 * Meta data segments are stored as Qt resources, which must be "mounted"
 * via QResource::registerResource()
 *
 * Component index segment:
 * quint64 number of index entries
 * QString identifier of component 0
 * quint64 offset of component data segment 0
 * quint64 length of component data segment 0
 * QString identifier of component ..
 * quint64 offset of component data segment ..
 * quint64 length of component data segment ..
 * QString identifier of component n
 * quint64 offset of component data segment n
 * quint64 length of component data segment n
 * quint64 number of index entries
 *
 * Component data segment:
 * quint64 number of archives in this component
 * QString name of archive 0
 * quint64 offset of archive 0
 * quint64 length of archive 0
 * QString name of archive ..
 * quint64 offset of archive ..
 * quint64 length of archive ..
 * QString name of archive n
 * quint64 offset of archive n
 * quint64 length of archive n
 * Archive 0
 * Archive ..
 * Archive n
 * \endverbatim
 */

BinaryContent BinaryContent::readFromBinary(const QString &path)
{
    BinaryContent c(path);

    // Try to read the binary layout of the calling application. We need to figure out
    // if we are in installer or an unistaller (maintenance, package manager, updater) binary.
    QInstaller::openForRead(c.d->m_appBinary.data());
    quint64 cookiePos = findMagicCookie(c.d->m_appBinary.data(), QInstaller::MagicCookie);
    if (!c.d->m_appBinary->seek(cookiePos - sizeof(qint64))) {   // seek to read the marker
        throw Error(QCoreApplication::translate("BinaryContent",
            "Could not seek to %1 to read the magic marker.").arg(cookiePos - sizeof(qint64)));
    }
    const qint64 magicMarker = QInstaller::retrieveInt64(c.d->m_appBinary.data());

    if (magicMarker != MagicInstallerMarker) {
        // We are not an installer, so we need to read the data from the .dat file.
        QFileInfo fi(path);
        if (QFileInfo(fi.absoluteFilePath() + QLatin1String("/../../..")).isBundle())
            fi.setFile(fi.absoluteFilePath()); // On OSX it's not inside the bundle, deserves TODO.

        c.d->m_binaryDataFile.reset(new QFile(fi.absolutePath() + QLatin1Char('/') + fi.baseName()
            + QLatin1String(".dat")));
        QInstaller::openForRead(c.d->m_binaryDataFile.data());
        cookiePos = findMagicCookie(c.d->m_binaryDataFile.data(), QInstaller::MagicCookieDat);
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

    // seek to the position of the component index
    const qint64 resourceOffsetAndLengtSize = 2 * sizeof(qint64);
    const qint64 resourceSectionSize = resourceOffsetAndLengtSize * layout.resourceCount;
    const qint64 offset = layout.endOfData - layout.indexSize - resourceSectionSize
        - resourceOffsetAndLengtSize;
    if (!file->seek(offset)) {
        throw Error(QCoreApplication::translate("BinaryContent",
            "Could not seek to component index information."));
    }
    const qint64 compIndexStart = QInstaller::retrieveInt64(file.data()) + dataBlockStart;
    if (!file->seek(compIndexStart))
        throw Error(QCoreApplication::translate("BinaryContent", "Could not seek to component index."));

    content.d->m_componentIndex = QInstallerCreator::ComponentIndex::read(file, dataBlockStart);
    content.d->m_binaryFormatEngineHandler.setComponentIndex(content.d->m_componentIndex);

    if (isVerbose()) {
        const QVector<QInstallerCreator::Component> components = content.d->m_componentIndex.components();
        qDebug() << "Number of components loaded:" << components.count();
        foreach (const QInstallerCreator::Component &component, components) {
            const QVector<QSharedPointer<Archive> > archives = component.archives();
            qDebug() << component.name().data() << "loaded...";
            QStringList archivesWithSize;
            foreach (const QSharedPointer<Archive> &archive, archives) {
                QString archiveWithSize(QLatin1String("%1 - %2"));
                archiveWithSize = archiveWithSize.arg(QString::fromLocal8Bit(archive->name()),
                    humanReadableSize(archive->size()));
                archivesWithSize.append(archiveWithSize);
            }
            if (!archivesWithSize.isEmpty()) {
                qDebug() << " - " << archives.count() << "archives: "
                    << qPrintable(archivesWithSize.join(QLatin1String("; ")));
            }
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

    for (int i = 0; i < d->m_performedOperationsData.count(); ++ i) {
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
    Returns the operations performed during installation. Returns an empty list if no operations are
    instantiated, performed or the binary is the installer application.
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
    Registers the passed file as default resource content. If the embedded resources are already mapped into
    memory, it will replace the first with the new content.
*/
void BinaryContent::registerAsDefaultQResource(const QString &path)
{
    QFile resource(path);
    bool success = resource.open(QIODevice::ReadOnly);
    if (success && (d->m_resourceMappings.count() > 0)) {
        success = QResource::unregisterResource((const uchar*)d->m_resourceMappings.first().constData(),
            QLatin1String(":/metadata"));
        if (success)
            d->m_resourceMappings.remove(0);
    }

    if (success) {
        d->m_resourceMappings.prepend(addResourceFromBinary(&resource, Range<qint64>::fromStartAndEnd(0,
            resource.size())));
    } else {
        qWarning() << QString::fromLatin1("Could not register '%1' as default resource.").arg(path);
    }
}

/*!
    Returns the binary component index as read from the file.
*/
QInstallerCreator::ComponentIndex BinaryContent::componentIndex() const
{
    return d->m_componentIndex;
}
