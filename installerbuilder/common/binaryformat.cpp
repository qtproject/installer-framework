/**************************************************************************
**
** This file is part of Qt SDK**
**
** Copyright (c) 2011 Nokia Corporation and/or its subsidiary(-ies).*
**
** Contact:  Nokia Corporation qt-info@nokia.com**
**
** No Commercial Usage
**
** This file contains pre-release code and may not be distributed.
** You may use this file in accordance with the terms and conditions
** contained in the Technology Preview License Agreement accompanying
** this package.
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception version
** 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** If you are unsure which license is appropriate for your use, please contact
** (qt-info@nokia.com).
**
**************************************************************************/
#include "binaryformat.h"

#include "errors.h"
#include "fileutils.h"
#include "lib7z_facade.h"
#include "utils.h"
#include "zipjob.h"

#include <KDUpdater/UpdateOperation>
#include <KDUpdater/UpdateOperationFactory>

#include <QtCore/QResource>
#include <QtCore/QTemporaryFile>

#include <errno.h>

using namespace QInstaller;
using namespace QInstallerCreator;

/*
TRANSLATOR QInstallerCreator::Archive
*/

/*
TRANSLATOR QInstallerCreator::Component
*/

static inline QByteArray &theBuffer(int size)
{
    static QByteArray b;
    if (size > b.size())
        b.resize(size);
    return b;
}

void QInstaller::appendFileData(QIODevice *out, QIODevice *in)
{
    Q_ASSERT(!in->isSequential());
    const qint64 size = in->size();
    blockingCopy(in, out, size);
}


void QInstaller::retrieveFileData(QIODevice *out, QIODevice *in)
{
    qint64 size = QInstaller::retrieveInt64(in);
    appendData(in, out, size);
/*    QByteArray &b = theBuffer(size);
    blockingRead(in, b.data(), size);
    blockingWrite(out, b.constData(), size);*/
}

void QInstaller::appendInt64(QIODevice *out, qint64 n)
{
    blockingWrite(out, reinterpret_cast<const char*>(&n), sizeof(n));
}

void QInstaller::appendInt64Range(QIODevice *out, const Range<qint64> &r)
{
    appendInt64(out, r.start());
    appendInt64(out, r.length());
}

qint64 QInstaller::retrieveInt64(QIODevice *in)
{
    qint64 n = 0;
    blockingRead(in, reinterpret_cast<char*>(&n), sizeof(n));
    return n;
}

Range<qint64> QInstaller::retrieveInt64Range(QIODevice *in)
{
    const quint64 start = retrieveInt64(in);
    const quint64 length = retrieveInt64(in);
    return Range<qint64>::fromStartAndLength(start, length);
}



#if 0
// Faster or not?
static void appendFileData(QIODevice *out, const QString &fileName)
{
    QFile file(fileName);
    openForRead(file);
    qint64 size = file.size();
    QInstaller::appendInt64(out, size);
    if (size == 0)
        return;
    uchar *data = file.map(0, size);
    if (!data)
        throw Error(QInstaller::tr("Cannot map file %1").arg(file.fileName()));
    blockingWrite(out, (const char *)data, size);
    if (!file.unmap(data))
        throw Error(QInstaller::tr("Cannot unmap file %1").arg(file.fileName()));
}
#endif

void QInstaller::appendData(QIODevice *out, QIODevice *in, qint64 size)
{
    while(size > 0) {
        const qint64 nextSize = qMin(size, 16384LL);
        QByteArray &b = theBuffer(nextSize);
        blockingRead(in, b.data(), nextSize);
        blockingWrite(out, b.constData(), nextSize);
        size -= nextSize;
    }
}

void QInstaller::appendString(QIODevice *out, const QString &str)
{
    appendByteArray(out, str.toUtf8());
}

void QInstaller::appendByteArray(QIODevice *out, const QByteArray &ba)
{
    appendInt64(out, ba.size());
    blockingWrite(out, ba.constData(), ba.size());
}

void QInstaller::appendStringList(QIODevice *out, const QStringList &list)
{
    appendInt64(out, list.size());
    foreach (const QString &s, list)
        appendString(out, s);
}

void QInstaller::appendDictionary(QIODevice *out, const QHash<QString,QString> &dict)
{
    appendInt64(out, dict.size());
    foreach (const QString &key, dict.keys()) {
        appendString(out, key);
        appendString(out, dict.value(key));
    }
}

QString QInstaller::retrieveString(QIODevice *in)
{
    const QByteArray b = retrieveByteArray(in);
    return QString::fromUtf8(b);
}

QByteArray QInstaller::retrieveByteArray(QIODevice *in)
{
    QByteArray ba;
    const qint64 n = retrieveInt64(in);
    ba.resize(n);
    blockingRead(in, ba.data(), n);
    return ba;
}

QStringList QInstaller::retrieveStringList(QIODevice *in)
{
    QStringList list;
    for (qint64 i = retrieveInt64(in); --i >= 0;)
        list << retrieveString(in);
    return list;
}

QHash<QString,QString> QInstaller::retrieveDictionary(QIODevice *in)
{
    QHash<QString,QString> dict;
    for (qint64 i = retrieveInt64(in); --i >= 0;) {
        QString key = retrieveString(in);
        dict.insert(key, retrieveString(in));
    }
    return dict;
}

qint64 QInstaller::findMagicCookie(QFile *in)
{
    Q_ASSERT(in);
    Q_ASSERT(in->isOpen());
    Q_ASSERT(in->isReadable());
    const qint64 oldPos = in->pos();
    const qint64 MAX_SEARCH = 1024 * 1024;  // stop searching after one MB
    qint64 searched = 0;
    try {
        while (searched < MAX_SEARCH) {
            const qint64 pos = in->size() - searched - sizeof(qint64);
            if (pos < 0)
                throw Error(QObject::tr("Searched whole file, no marker found"));
            if (!in->seek(pos)) {
                throw Error(QObject::tr("Could not seek to %1 in file %2: %3.").arg(QString::number(pos),
                    in->fileName(), in->errorString()));
            }
            const quint64 num = static_cast<quint64>(retrieveInt64(in));
            if (num == MagicCookie) {
                in->seek(oldPos);
                return pos;
            }
            searched += 1;
        }
        throw Error(QObject::tr("No marker found, stopped after %1 bytes").arg(QString::number(MAX_SEARCH)));
    } catch (const Error& err) {
        in->seek(oldPos);
        throw err;
    } catch (...) {
        in->seek(oldPos);
        throw Error(QObject::tr("No marker found, unknown exception caught."));
    }
    return -1; // never reached
}

/*!
    Creates an archive providing the data in \a path.
    \a path can be a path to a file or to a directory. If it's a file, it's considered to be
    pre-zipped and gets delivered as it is. If it's a directory, it gets zipped by Archive.
 */
Archive::Archive(const QString &path)
    : m_device(0),
      m_isTempFile(false),
      m_path(path),
      m_name(QFileInfo(path).fileName().toUtf8())
{
}

Archive::Archive(const QByteArray &identifier, const QByteArray &data)
    : m_device(0),
      m_isTempFile(true),
      m_path(generateTemporaryFileName()),
      m_name(identifier)
{
    QFile file(m_path);
    file.open(QIODevice::WriteOnly);
    file.write(data);
}

/*!
    Creates an archive identified by \a identifier providing a data \a segment within a \a device.
 */
Archive::Archive(const QByteArray &identifier, QIODevice *device, const Range<qint64> &segment)
    : m_device(device),
      m_segment(segment),
      m_isTempFile(false),
      m_name(identifier)
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
    Copies the archives contents to the path \a name.
    If the archive is a zipped directory, \a name is threated as a directory. The archive gets extracted there.

    If the archive is a plain file and \a name an existing directory, it gets created
    with it's name. Otherwise it gets saved as \a name.
    Note that if a file with the \a name already exists, copy() return false (i.e. Archive will not overwrite it).
 */
bool Archive::copy(const QString &name)
{
    if (isZippedDirectory()) {
        const QFileInfo fi(name);
        if (fi.exists() && !fi.isDir())
            return false;
        errno = 0;
        if (!fi.exists() && !QDir().mkpath(fi.absoluteFilePath())) {
            setErrorString(tr("Could not create %1: %2").arg(name, QString::fromLocal8Bit(strerror(errno))));
            return false;
        }

        UnzipJob job;
        if (isOpen())
            close();
        open(QIODevice::ReadOnly);
        job.setInputDevice(this);
        job.setOutputPath(fi.absoluteFilePath());

        job.run();

        close();

        return true;
    } else {
        if (isOpen())
            close();

        open(QIODevice::ReadOnly);

        QFile target(QFileInfo(name).isDir() ? QString::fromLatin1("%1/%2").arg(name)
            .arg(QString::fromUtf8(m_name.data(), m_name.count())) : name);
        if (target.exists())
            return false;
        target.open(QIODevice::WriteOnly);
        blockingCopy(this, &target, size());

        close();
        return true;
    }
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

/*!
    Returns true, if this archive was created by zipping a directory.
 */
bool Archive::isZippedDirectory() const
{
    if (m_device == 0) {
        // easy, just check whether it's a dir
        return QFileInfo(m_path).isDir();
    } else {
        // more complex, check the zip header magic
        Archive* const arch = const_cast< Archive* >(this);

        const bool opened = !isOpen();
        if (opened)
            arch->open(QIODevice::ReadOnly);
        const qint64 p = pos();
        arch->seek(0);

        const QByteArray ba = arch->read(4);
        const bool result = ba == QByteArray("\x50\x4b\x03\04");

        arch->seek(p);
        if (opened)
            arch->close();
        return result;
    }
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

    // we

    const QFileInfo fi(m_path);
    if (fi.isFile()) {
        m_inputFile.setFileName(m_path);
        if (!m_inputFile.open(mode)) {
            setErrorString(tr("Could not open archive file %1 for reading.").arg(m_path));
            return false;
        }

/*        if (!Lib7z::isSupportedArchive(&m_inputFile)) {
            setErrorString(tr("Not in a supported archive format: %1").arg(m_path));
            m_inputFile.close();
            return false;
        }*/

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
        Lib7z::createArchive(&m_inputFile, m_path);
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
                throw Error(QObject::tr("Cannot create zipped file for path %1: %2").arg(m_path,
                    errorString()));
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

Component Component::readFromIndexEntry(QIODevice *in, qint64 offset)
{
    Component c;
    c.m_name = retrieveByteArray(in);
    c.m_binarySegment = retrieveInt64Range(in).moved(offset);

    c.readData(in, offset);

    return c;
}

void Component::writeIndexEntry(QIODevice *out, qint64 positionOffset) const
{
    appendByteArray(out, m_name);
    const Range<qint64> relative = m_binarySegment.moved(positionOffset);
    appendInt64(out, binarySegment().start());
    appendInt64(out, binarySegment().length());
}

void Component::writeData(QIODevice *out, qint64 offset) const
{
    const qint64 dataBegin = out->pos() + offset;

    appendInt64(out, m_archives.count());

    qint64 start = out->pos() + offset;

    // Why 16 + 16? This is 24, not 32???
    const int foo = 3 * sizeof(qint64);
    // add 16 + 16 + number of name characters for each archive (the size of the table)
    foreach (const QSharedPointer<Archive> &archive, m_archives)
        start += foo + archive->name().count();

    QList<qint64> starts;
    foreach (const QSharedPointer<Archive> &archive, m_archives) {
        appendByteArray(out, archive->name());
        starts.push_back(start);
        appendInt64Range(out, Range<qint64>::fromStartAndLength(start, archive->size()));
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
        blockingCopy(archive.data(), out, archive->size());
    }

    m_binarySegment = Range<qint64>::fromStartAndEnd(dataBegin, out->pos() + offset);
}

void Component::readData(QIODevice *in, qint64 offset)
{
    const qint64 pos = in->pos();

    in->seek(m_binarySegment.start());
    const qint64 count = retrieveInt64(in);

    QVector<QByteArray> names;
    QVector<Range<qint64> > ranges;
    for (int i = 0; i < count; ++i) {
        names.push_back(retrieveByteArray(in));
        ranges.push_back(retrieveInt64Range(in).moved(offset));
    }

    for (int i = 0; i < ranges.count(); ++i)
        m_archives.push_back(QSharedPointer<Archive>(new Archive(names.at(i), in, ranges.at(i))));

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

Component ComponentIndex::componentByName(const QByteArray &id) const
{
    return m_components.value(id);
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

void ComponentIndex::insertComponent(const Component& c)
{
    m_components.insert(c.name(), c);
}

int ComponentIndex::componentCount() const
{
    return m_components.size();
}

void ComponentIndex::removeComponent(const QByteArray &name)
{
    m_components.remove(name);
}

QVector<Component> ComponentIndex::components() const
{
    return m_components.values().toVector();
}

void ComponentIndex::writeComponentData(QIODevice *out, qint64 offset) const
{
    appendInt64(out, componentCount());

    foreach (const Component &component, m_components)
        component.writeData(out, offset);
}


ComponentIndex::ComponentIndex()
{
}

ComponentIndex ComponentIndex::read(QIODevice *dev, qint64 offset)
{
    ComponentIndex result;
    const qint64 size = retrieveInt64(dev);
    for (int i = 0; i < size; ++i)
        result.insertComponent(Component::readFromIndexEntry(dev, offset));
    retrieveInt64(dev);
    return result;
}

void ComponentIndex::writeIndex(QIODevice *out, qint64 offset) const
{
    // Q: why do we write the size twice?
    // A: for us to be able to read it beginning from the end of the file as well
    appendInt64(out, componentCount());
    foreach (const Component& i, components())
        i.writeIndexEntry(out, offset);
    appendInt64(out, componentCount());
}

/*!
    \internal
    Registers the resource found at \a segment within \a file into the Qt resource system.
 */
static const uchar* addResourceFromBinary(QFile* file, const Range<qint64> &segment)
{
    if (segment.length() <= 0)
        return 0;

    const uchar* const mapped = file->map(segment.start(), segment.length());
    if (!mapped) {
        throw Error(QObject::tr("Could not mmap in-binary resource. (offset=%1, length=%2")
            .arg(QString::number(segment.start()), QString::number(segment.length())));
    }

    if (!QResource::registerResource(mapped, QLatin1String(":/metadata")))
        throw Error(QObject::tr("Could not register in-binary resource."));

    return mapped;
}

BinaryContent::BinaryContent(const QString &path)
    : file(new QFile(path)),
       handler(components),
       magicmaker(0),
       dataBlockStart(0)
{
}

BinaryContent::~BinaryContent()
{
    foreach (const uchar *rccData, mappings)
        QResource::unregisterResource(rccData);
}

/*!
    \internal
    Registers the Qt resources embedded in this binary.
 */
int BinaryContent::registerEmbeddedQResources()
{
    if (!file->isOpen()) {
        if (!file->open(QIODevice::ReadOnly))
            throw Error(QObject::tr("Could not open binary %1: %2").arg(file->fileName(), file->errorString()));
    }

    foreach (const Range<qint64> &i, metadataResourceSegments)
        mappings.push_back(addResourceFromBinary(file.data(), i));
    return mappings.count();
}

/*!
    \internal
    \fn static BinaryContent BinaryContent::readFromApplicationFile()
    Reads BinaryContent stored in the current application binary.
 */
BinaryContent BinaryContent::readFromApplicationFile()
{
    return BinaryContent::readFromBinary(QCoreApplication::applicationFilePath());
}

/*!
 * \class QInstaller::BinaryContent
 *
 * BinaryContent handles binary information embedded into executables.
 * Qt resources as well as component information can be stored.
 *
 * Explanation of the binary blob at the end of the file:
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
 * quint64 offset of meta data segment 0
 * quint64 length of meta data segment 0
 * quint64 offset of meta data segment ..
 * quint64 length of meta data segment ..
 * quint64 offset of meta data segment n
 * quint64 length of meta data segment n
 * ------------------------------------------------------
 * quint64 number of meta data segments
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

    QFile* const file = c.file.data();

    if (!file->open(QIODevice::ReadOnly))
        throw Error(QObject::tr("Could not open binary %1: %2").arg(path, file->errorString()));

    const qint64 cookiepos = findMagicCookie(file);
    Q_ASSERT(cookiepos >= 0);
    const qint64 endOfData = cookiepos + sizeof(qint64);
    const qint64 indexSize = 6 * sizeof(qint64);
    if (!file->seek(endOfData - indexSize))
        throw Error(QObject::tr("Could not seek to binary layout section"));

    qint64 operationsStart = retrieveInt64(file);
    /*qint64 operationsEnd = */ retrieveInt64(file); // don't care
    const qint64 count = retrieveInt64(file);
    const qint64 dataBlockSize = retrieveInt64(file);
    const qint64 dataBlockStart = endOfData - dataBlockSize;

    operationsStart += dataBlockStart;
    //operationsEnd += dataBlockStart;
    c.magicmaker = retrieveInt64(file);

    const quint64 magicCookie = retrieveInt64(file);
    Q_UNUSED(magicCookie);
    Q_ASSERT(magicCookie == MagicCookie);

    const qint64 resourceSectionSize = 2 * sizeof(qint64) * count;
    for (int i = 0; i < count; ++i) {
        if (!file->seek(endOfData - indexSize - 2 * sizeof(qint64) * (i + 1)))
            throw Error(QObject::tr("Could not seek to metadata index"));

        const qint64 metadataResourceOffset = retrieveInt64(file) + dataBlockStart;
        const qint64 metadataResourceLength = retrieveInt64(file);
        c.metadataResourceSegments.push_back(Range<qint64>::fromStartAndLength(metadataResourceOffset,
            metadataResourceLength));
    }

    if (!file->seek(operationsStart))
        throw Error(QObject::tr("Could not seek to operation list"));

    QStack<KDUpdater::UpdateOperation*> performedOperations;
    const qint64 operationsCount = retrieveInt64(file);
    verbose() << "operationsCount=" << operationsCount << std::endl;

    for (int i = operationsCount; --i >= 0;) {
        const QString name = retrieveString(file);
        const QString xml = retrieveString(file);
        KDUpdater::UpdateOperation* const op = KDUpdater::UpdateOperationFactory::instance().create(name);
        QString debugXmlString(xml);
        debugXmlString.truncate(1000);
        verbose() << "Operation name=" << name << " xml=\n" << debugXmlString << std::endl;
        Q_ASSERT_X(op, "BinaryContent::readFromBinary", QString::fromLatin1("Invalid operation name='%1'")
            .arg(name).toLatin1());
        if (! op->fromXml(xml))
            qWarning() << "Failed to load XML for operation=" << name;
        performedOperations.push(op);
    }
    c.performedOperations = performedOperations;

    // seek to the position of the component index
    if (!file->seek(endOfData - indexSize - resourceSectionSize - 2 * sizeof(qint64)))
        throw Error(QObject::tr("Could not seek to component index information"));

    const qint64 compIndexStart = retrieveInt64(file) + dataBlockStart;
    if (!file->seek(compIndexStart))
        throw Error(QObject::tr("Could not seek to component index"));

    c.components = QInstallerCreator::ComponentIndex::read(file, dataBlockStart);
    c.handler.setComponentIndex(c.components);

    const QVector<QInstallerCreator::Component> components = c.components.components();
    verbose() << "components loaded:" << components.count() << std::endl;
    foreach (const QInstallerCreator::Component &component, components) {
        verbose() << "loaded " << component.name();
        const QVector<QSharedPointer<Archive> > archives = component.archives();
        verbose() << "  having " << archives.count() << " archives:" << std::endl;
        foreach (const QSharedPointer<Archive> &archive, archives)
            verbose() << "    " << archive->name() << " (" << archive->size() << " bytes)" << std::endl;
    }
    return c;
}
