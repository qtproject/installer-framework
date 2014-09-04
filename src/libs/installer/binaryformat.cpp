/**************************************************************************
**
** Copyright (C) 2012-2014 Digia Plc and/or its subsidiary(-ies).
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

#include <QFileInfo>
#include <QFlags>
#include <QUuid>

namespace QInstaller {

/*!
    \class Resource
    \brief The Resource class provides an interface for reading from an underlying device.

    Resource is an interface for reading inside a device, but is not supposed to write to the
    the device it wraps. The resource class is created by either passing a path to an already
    existing binary (e.g. a zipped archive, a Qt Resource file etc.) or by passing an name and
    segment inside an already existing QIODevice, passed as device.

    The resource name can be set at any time using setName(). The segment passed inside the
    constructor represents the offset and size of the resource inside the device.
*/

/*!
    Creates a resource providing the data in \a path.

    \sa open()
 */
Resource::Resource(const QString &path)
    : m_device(0)
    , m_name(QFileInfo(path).fileName().toUtf8())
    , m_deviceOpened(false)
{
    m_inputFile.setFileName(path);
    m_segment = Range<qint64>::fromStartAndLength(0, m_inputFile.size());
}

/*!
    Creates a resource identified by \a name providing the data in \a path.

    \sa open()
*/
Resource::Resource(const QByteArray &name, const QString &path)
    : m_device(0)
    , m_name(name)
    , m_deviceOpened(false)
{
    m_inputFile.setFileName(path);
    m_segment = Range<qint64>::fromStartAndLength(0, m_inputFile.size());
}

/*!
    Creates a resource providing the data in a \a device.

    \sa open()
*/
Resource::Resource(const QSharedPointer<QIODevice> &device)
    : m_device(device)
    , m_segment(Range<qint64>::fromStartAndLength(0, device->size()))
    , m_name(QUuid::createUuid().toByteArray())
    , m_deviceOpened(false)
{
}

/*!
    Creates a resource providing a data \a segment within a \a device.

    \sa open()
*/
Resource::Resource(const QSharedPointer<QIODevice> &device, const Range<qint64> &segment)
    : m_device(device)
    , m_segment(segment)
    , m_name(QUuid::createUuid().toByteArray())
    , m_deviceOpened(false)
{
}

/*!
    Creates a resource identified by \a  name providing a data \a segment within a \a device.

    \sa open()
 */
Resource::Resource(const QByteArray &name, const QSharedPointer<QIODevice> &device,
        const Range<qint64> &segment)
    : m_device(device)
    , m_segment(segment)
    , m_name(name)
    , m_deviceOpened(false)
{
}

Resource::~Resource()
{
    if (isOpen())
        close();
}

/*!
    \reimp
 */
bool Resource::seek(qint64 pos)
{
    if (m_inputFile.isOpen())
        return m_inputFile.seek(pos) && QIODevice::seek(pos);
    return QIODevice::seek(pos);
}

QByteArray Resource::name() const
{
    return m_name;
}

void Resource::setName(const QByteArray &name)
{
    m_name = name;
}

/*!
    A Resource will always be opened in QIODevice::ReadOnly mode. The function will return true
    if successful.
*/
bool Resource::open()
{
    if (isOpen())
        return false;

    if (m_device.isNull()) {
        if (!m_inputFile.open(QIODevice::ReadOnly)) {
            setErrorString(m_inputFile.errorString());
            return false;
        }
        return open(QIODevice::ReadOnly);
    }

    if (m_device->isOpen()) {
        if (!QFlags<QIODevice::OpenModeFlag>(m_device->openMode()).testFlag(QIODevice::ReadOnly)) {
            setErrorString(tr("Could not open the underlying device. Already opened write only."));
            return false;
        }
        return open(QIODevice::ReadOnly);
    }

    m_deviceOpened = m_device->open(QIODevice::ReadOnly);
    if (!m_deviceOpened) {
        setErrorString(m_device->errorString());
        return false;
    }
    return open(QIODevice::ReadOnly);
}
/*!
    \reimp
 */
void Resource::close()
{
    m_inputFile.close();
    if (!m_device.isNull() && m_deviceOpened) {
        m_device->close();
        m_deviceOpened = false;
    }
    QIODevice::close();
}

/*!
    \reimp
 */
qint64 Resource::size() const
{
    return m_segment.length();
}

/*!
    \reimp
 */
qint64 Resource::readData(char* data, qint64 maxSize)
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
qint64 Resource::writeData(const char* data, qint64 maxSize)
{
    Q_UNUSED(data);
    Q_UNUSED(maxSize);
    // should never be called, as we're read only
    return -1;
}

void Resource::copyData(Resource *resource, QFileDevice *out)
{
    qint64 left = resource->size();
    char data[4096];
    while (left > 0) {
        const qint64 len = qMin<qint64>(left, 4096);
        const qint64 bytesRead = resource->read(data, len);
        if (bytesRead != len) {
            throw QInstaller::Error(tr("Read failed after %1 bytes: %2")
                .arg(QString::number(resource->size() - left), resource->errorString()));
        }
        const qint64 bytesWritten = out->write(data, len);
        if (bytesWritten != len) {
            throw QInstaller::Error(tr("Write failed after %1 bytes: %2")
                .arg(QString::number(resource->size() - left), out->errorString()));
        }
        left -= len;
    }
}


/*!
    \class ResourceCollection
    \brief A Resource Collection is an abstraction that groups together a number of resources.

    The resources are supposed to be sequential, so the collection keeps them ordered once a new
    resource is added.

    The resources collection can be written to and read from a QFileDevice. The resource
    collection name can be set at any time using setName().
*/

ResourceCollection::ResourceCollection(const QByteArray &name)
    : m_name(name)
{
}

QByteArray ResourceCollection::name() const
{
    return m_name;
}

void ResourceCollection::setName(const QByteArray &ba)
{
    m_name = ba;
}

void ResourceCollection::write(QFileDevice *out, qint64 offset) const
{
    const qint64 dataBegin = out->pos();

    QInstaller::appendInt64(out, m_resources.count());

    qint64 start = out->pos() + offset;

    // Why 16 + 16? This is 24, not 32???
    const int foo = 3 * sizeof(qint64);
    // add 16 + 16 + number of name characters for each resource (the size of the table)
    foreach (const QSharedPointer<Resource> &resource, m_resources)
        start += foo + resource->name().count();

    QList<qint64> starts;
    foreach (const QSharedPointer<Resource> &resource, m_resources) {
        QInstaller::appendByteArray(out, resource->name());
        starts.push_back(start);
        QInstaller::appendInt64Range(out, Range<qint64>::fromStartAndLength(start, resource->size()));
        start += resource->size();
    }

    foreach (const QSharedPointer<Resource> &resource, m_resources) {
        if (!resource->open()) {
            throw QInstaller::Error(tr("Could not open resource %1: %2")
                .arg(QString::fromUtf8(resource->name()), resource->errorString()));
        }

        const qint64 expectedStart = starts.takeFirst();
        const qint64 actualStart = out->pos() + offset;
        Q_UNUSED(expectedStart);
        Q_UNUSED(actualStart);
        Q_ASSERT(expectedStart == actualStart);
        resource->copyData(out);
    }

    m_segment = Range<qint64>::fromStartAndEnd(dataBegin, out->pos()).moved(offset);
}

void ResourceCollection::read(const QSharedPointer<QFile> &in, qint64 offset)
{
    const qint64 pos = in->pos();

    in->seek(m_segment.start());
    const qint64 count = QInstaller::retrieveInt64(in.data());

    QList<QByteArray> names;
    QList<Range<qint64> > ranges;
    for (int i = 0; i < count; ++i) {
        names.push_back(QInstaller::retrieveByteArray(in.data()));
        ranges.push_back(QInstaller::retrieveInt64Range(in.data()).moved(offset));
    }

    for (int i = 0; i < ranges.count(); ++i)
        m_resources.append(QSharedPointer<Resource>(new Resource(names.at(i), in, ranges.at(i))));

    in->seek(pos);
}

/*!
    Appends \a resource to this collection. The collection takes ownership of \a resource.
 */
void ResourceCollection::appendResource(const QSharedPointer<Resource>& resource)
{
    Q_ASSERT(resource);
    resource->setParent(0);
    m_resources.push_back(resource);
}

void ResourceCollection::appendResources(const QList<QSharedPointer<Resource> > &resources)
{
    foreach (const QSharedPointer<Resource> &resource, resources)
        appendResource(resource);
}

/*!
    Returns the resources associated with this component.
 */
QList<QSharedPointer<Resource> > ResourceCollection::resources() const
{
    return m_resources;
}

QSharedPointer<Resource> ResourceCollection::resourceByName(const QByteArray &name) const
{
    foreach (const QSharedPointer<Resource>& i, m_resources) {
        if (i->name() == name)
            return i;
    }
    return QSharedPointer<Resource>();
}


/*!
    \class ResourceCollectionManager
    \brief A Resource Collection Manager is an abstraction that groups together a number of
    resource collections.

    The resources collections can be written to and read from a QFileDevice.
*/

void  ResourceCollectionManager::read(const QSharedPointer<QFile> &dev, qint64 offset)
{
    const qint64 size = QInstaller::retrieveInt64(dev.data());
    for (int i = 0; i < size; ++i)
        insertCollection(readIndexEntry(dev, offset));
    QInstaller::retrieveInt64(dev.data());
}

Range<qint64> ResourceCollectionManager::write(QFileDevice *out, qint64 offset) const
{
    QInstaller::appendInt64(out, collectionCount());
    foreach (const ResourceCollection &collection, m_collections)
        collection.write(out, offset);

    const qint64 start = out->pos();

    // Q: why do we write the size twice?
    // A: for us to be able to read it beginning from the end of the file as well
    QInstaller::appendInt64(out, collectionCount());
    foreach (const ResourceCollection &collection, m_collections)
        writeIndexEntry(collection, out);
    QInstaller::appendInt64(out, collectionCount());

    return Range<qint64>::fromStartAndEnd(start, out->pos());
}

ResourceCollection ResourceCollectionManager::collectionByName(const QByteArray &name) const
{
    return m_collections.value(name);
}

void ResourceCollectionManager::insertCollection(const ResourceCollection& collection)
{
    m_collections.insert(collection.name(), collection);
}

void ResourceCollectionManager::removeCollection(const QByteArray &name)
{
    m_collections.remove(name);
}

QList<ResourceCollection> ResourceCollectionManager::collections() const
{
    return m_collections.values();
}

void ResourceCollectionManager::reset()
{
    m_collections.clear();
}

int ResourceCollectionManager::collectionCount() const
{
    return m_collections.size();
}

void ResourceCollectionManager::writeIndexEntry(const ResourceCollection &collection,
    QFileDevice *dev) const
{
    QInstaller::appendByteArray(dev, collection.name());
    QInstaller::appendInt64Range(dev, collection.segment());
}

ResourceCollection ResourceCollectionManager::readIndexEntry(const QSharedPointer<QFile> &in,
    qint64 offset)
{
    ResourceCollection c(QInstaller::retrieveByteArray(in.data()));
    c.setSegment(QInstaller::retrieveInt64Range(in.data()).moved(offset));
    c.read(in, offset);

    return c;
}

} // namespace QInstaller
