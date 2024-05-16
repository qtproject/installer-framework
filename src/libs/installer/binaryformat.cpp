/**************************************************************************
**
** Copyright (C) 2023 The Qt Company Ltd.
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

#include "binaryformat.h"

#include "errors.h"
#include "fileio.h"

#include <QFileInfo>
#include <QFlags>
#include <QUuid>

namespace QInstaller {

/*!
    \class QInstaller::OperationBlob
    \inmodule QtInstallerFramework
    \brief The OperationBlob class is a textual representation of an operation that can be
        instantiated and executed by the Qt Installer Framework.
*/

/*!
    \fn QInstaller::OperationBlob::OperationBlob(const QString &n, const QString &x)

    Constructs the operation blob with the given arguments, while \a n stands for the name part and
    \a x for the XML representation of the operation.
*/

/*!
    \variable QInstaller::OperationBlob::name
    \brief The name of the operation.
*/

/*!
    \variable QInstaller::OperationBlob::xml
    \brief The XML representation of the operation.
*/

/*!
    \class QInstaller::Resource
    \inmodule QtInstallerFramework
    \brief The Resource class is an interface for wrapping a file as read only device.

    Resource is an interface for reading inside a file, but is not supposed to write to the file it
    wraps. The \c Resource class is created by passing a path to an existing
    binary (such as a zipped archive or a Qt resource file).

    The resource name can be set at any time using setName() or during construction. The segment
    supplied during construction represents the offset and size of the resource inside the file.
*/

/*!
    \fn Range<qint64> QInstaller::Resource::segment() const

    Returns the range inside the file this resource represents.
*/

/*!
    \fn void QInstaller::Resource::setSegment(const Range<qint64> &segment)

    Sets the range to the \a segment of the file that this resource represents.
*/

/*!
    Creates a resource providing the data in \a path.
 */
Resource::Resource(const QString &path)
    : m_file(path)
    , m_name(QFileInfo(path).fileName().toUtf8())
    , m_segment(Range<qint64>::fromStartAndLength(0, m_file.size()))
{
}

/*!
    Creates a resource providing the data in \a path identified by \a name.
*/
Resource::Resource(const QString &path, const QByteArray &name)
    : m_file(path)
    , m_name(name)
    , m_segment(Range<qint64>::fromStartAndLength(0, m_file.size()))
{
}

/*!
    Creates a resource providing the data in \a path limited to \a segment.
*/
Resource::Resource(const QString &path, const Range<qint64> &segment)
    : m_file(path)
    , m_name(QFileInfo(path).fileName().toUtf8())
    , m_segment(segment)
{
}

/*!
    Destroys the resource. Calls close() if necessary before destroying the resource.
*/
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
    return QIODevice::seek(pos);
}

/*!
    Returns the name of the resource.
*/
QByteArray Resource::name() const
{
    return m_name;
}

/*!
    Sets the name of the resource to \a name.
*/
void Resource::setName(const QByteArray &name)
{
    m_name = name;
}

/*!
    Opens a resource in QIODevice::ReadOnly mode. The function returns \c true
    if successful. Optionally, \a permissions can be given.
*/
#if QT_VERSION < QT_VERSION_CHECK(6, 3, 0)
bool Resource::open()
#else
bool Resource::open(std::optional<QFile::Permissions> permissions)
#endif
{
    if (isOpen())
        return false;
#if QT_VERSION < QT_VERSION_CHECK(6, 3, 0)
    if (!m_file.open(QIODevice::ReadOnly | QIODevice::Unbuffered)) {
#else
    if (!m_file.open(QIODevice::ReadOnly | QIODevice::Unbuffered, permissions)) {
#endif
        setErrorString(m_file.errorString());
        return false;
    }

    if (!QIODevice::open(QIODevice::ReadOnly)) {
        setErrorString(tr("Cannot open resource %1 for reading.").arg(QString::fromUtf8(m_name)));
        return false;
    }
    return true;
}
/*!
    \reimp
 */
void Resource::close()
{
    m_file.close();
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
    // check if there is anything left to read
    maxSize = qMin<quint64>(maxSize, m_segment.length() - pos());
    if (maxSize <= 0)
        return 0;

    const qint64 p = m_file.pos();
    m_file.seek(m_segment.start() + pos());
    const qint64 amountRead = m_file.read(data, maxSize);
    m_file.seek(p);
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

/*!
    \fn void QInstaller::Resource::copyData(QFileDevice *out)

    Copies the resource data to a file called \a out. Throws Error on failure.
*/

/*!
    \overload

    Copies the resource data of \a resource to a file called \a out. Throws Error on failure.
*/
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
    \class QInstaller::ResourceCollection
    \inmodule QtInstallerFramework
    \brief The ResourceCollection class is an abstraction that groups together a number of resources.

    The resources are supposed to be sequential, so the collection keeps them ordered once a new
    resource is added. The name can be set at any time using setName().
*/

/*!
    The class constructor creates an empty resource collection. By default the collection gets a
    unique name assigned using QUuid.
*/
ResourceCollection::ResourceCollection()
    : m_name(QUuid::createUuid().toByteArray())
{
}

/*!
    The class constructor creates an empty resource collection with a name set to \a name.
*/
ResourceCollection::ResourceCollection(const QByteArray &name)
    : m_name(name)
{}

/*!
    Returns the name of the resource collection.
*/
QByteArray ResourceCollection::name() const
{
    return m_name;
}

/*!
    Sets the name of the resource collection to \a name.
*/
void ResourceCollection::setName(const QByteArray &name)
{
    m_name = name;
}

/*!
    Appends \a resource to this collection. The collection takes ownership of \a resource.
 */
void ResourceCollection::appendResource(const QSharedPointer<Resource>& resource)
{
    Q_ASSERT(resource);
    resource->setParent(nullptr);
    m_resources.append(resource);
}

/*!
    Appends a list of \a resources to this collection. The collection takes ownership of \a
    resources.
*/
void ResourceCollection::appendResources(const QList<QSharedPointer<Resource> > &resources)
{
    foreach (const QSharedPointer<Resource> &resource, resources)
        appendResource(resource);
}

/*!
    Returns the resources associated with this collection.
*/
QList<QSharedPointer<Resource> > ResourceCollection::resources() const
{
    return m_resources;
}

/*!
    Returns the resource associated with the name \a name.
*/
QSharedPointer<Resource> ResourceCollection::resourceByName(const QByteArray &name) const
{
    foreach (const QSharedPointer<Resource>& i, m_resources) {
        if (i->name() == name)
            return i;
    }
    return QSharedPointer<Resource>();
}


/*!
    \class QInstaller::ResourceCollectionManager
    \inmodule QtInstallerFramework
    \brief The ResourceCollectionManager class is an abstraction that groups together a number of
        resource collections.

    The resource collections it groups can be written to and read from a QFileDevice.
*/

/*!
    Reads the resource collection from the file \a dev. The \a offset argument is used to
    set the collection's resources segment information.
*/
void ResourceCollectionManager::read(QFileDevice *dev, qint64 offset)
{
    const qint64 size = QInstaller::retrieveInt64(dev);
    for (int i = 0; i < size; ++i) {
        ResourceCollection collection(QInstaller::retrieveByteArray(dev));
        const Range<qint64> segment = QInstaller::retrieveInt64Range(dev).moved(offset);

        const qint64 pos = dev->pos();

        dev->seek(segment.start());
        const qint64 count = QInstaller::retrieveInt64(dev);
        for (int i = 0; i < count; ++i) {
            QSharedPointer<Resource> resource(new Resource(dev->fileName()));
            resource->setName(QInstaller::retrieveByteArray(dev));
            resource->setSegment(QInstaller::retrieveInt64Range(dev).moved(offset));
            collection.appendResource(resource);
        }
        dev->seek(pos);

        insertCollection(collection);
    }
}

/*!
    Writes the resource collection to the file \a out. The \a offset argument is used to
    set the collection's segment information.
*/
Range<qint64> ResourceCollectionManager::write(QFileDevice *out, qint64 offset) const
{
    QHash < QByteArray, Range<qint64> > table;
    QInstaller::appendInt64(out, collectionCount());
    foreach (const ResourceCollection &collection, m_collections) {
        const qint64 dataBegin = out->pos();
        QInstaller::appendInt64(out, collection.resources().count());

        qint64 start = out->pos() + offset;
        foreach (const QSharedPointer<Resource> &resource, collection.resources()) {
            start += (sizeof(qint64))   // the number of bytes that get written and the
            + resource->name().size()   // resource name (see QInstaller::appendByteArray)
            + (2 * sizeof(qint64));     // the resource range (see QInstaller::appendInt64Range)
        }

        foreach (const QSharedPointer<Resource> &resource, collection.resources()) {
            QInstaller::appendByteArray(out, resource->name());
            QInstaller::appendInt64Range(out, Range<qint64>::fromStartAndLength(start,
                resource->size()));     // the actual range once the table has been written
            start += resource->size();  // adjust for next resource data
        }

        foreach (const QSharedPointer<Resource> &resource, collection.resources()) {
            if (!resource->open()) {
                throw QInstaller::Error(tr("Cannot open resource %1: %2")
                    .arg(QString::fromUtf8(resource->name()), resource->errorString()));
            }
            resource->copyData(out);
        }

        table.insert(collection.name(), Range<qint64>::fromStartAndEnd(dataBegin, out->pos())
            .moved(offset));
    }

    const qint64 start = out->pos();

    // Q: why do we write the size twice?
    // A: for us to be able to read it beginning from the end of the file as well
    QInstaller::appendInt64(out, collectionCount());
    foreach (const QByteArray &name, table.keys()) {
        QInstaller::appendByteArray(out, name);
        QInstaller::appendInt64Range(out, table.value(name));
    }
    QInstaller::appendInt64(out, collectionCount());

    return Range<qint64>::fromStartAndEnd(start, out->pos());
}

/*!
    Returns the collection associated with the name \a name.
*/
ResourceCollection ResourceCollectionManager::collectionByName(const QByteArray &name) const
{
    return m_collections.value(name);
}

/*!
    Inserts the \a collection into the collection manager.
*/
void ResourceCollectionManager::insertCollection(const ResourceCollection& collection)
{
    m_collections.insert(collection.name(), collection);
}

/*!
    Removes all occurrences of \a name from the collection manager.
*/
void ResourceCollectionManager::removeCollection(const QByteArray &name)
{
    m_collections.remove(name);
}

/*!
    Returns the collections the collection manager contains.
*/
QList<ResourceCollection> ResourceCollectionManager::collections() const
{
    return m_collections.values();
}

/*!
    Clears the contents of the collection manager.
*/
void ResourceCollectionManager::clear()
{
    m_collections.clear();
}

/*!
    Returns the number of collections in the collection manager.
*/
int ResourceCollectionManager::collectionCount() const
{
    return m_collections.count();
}

} // namespace QInstaller
