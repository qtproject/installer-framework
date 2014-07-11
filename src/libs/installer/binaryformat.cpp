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

namespace QInstallerCreator {

/*!
    \class Archive
    \brief The Archive class provides an interface for reading from an underlying device.

    Archive is an interface for reading inside a binary file, but is not supposed to write to
    the binary it wraps. The Archive class is created by either passing a path to an already
    zipped archive or by passing an identifier and segment inside an already existing binary,
    passed as device.

    The archive name can be set at any time using setName(). The segment passed inside the
    constructor represents the offset and size of the archive inside the binary file.
*/

/*!
    Creates an archive providing the data in \a path.
 */
Archive::Archive(const QString &path)
    : m_device(0)
    , m_name(QFileInfo(path).fileName().toUtf8())
{
    m_inputFile.setFileName(path);
}

/*!
    Creates an archive identified by \a identifier providing a data \a segment within a \a device.
 */
Archive::Archive(const QByteArray &identifier, const QSharedPointer<QFile> &device, const Range<qint64> &segment)
    : m_device(device)
    , m_segment(segment)
    , m_name(identifier)
{
}

Archive::~Archive()
{
    if (isOpen())
        close();
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

    if (!m_inputFile.open(mode)) {
        setErrorString(m_inputFile.errorString());
        return false;
    }
    setOpenMode(mode);
    return true;
}

/*!
    \reimp
 */
qint64 Archive::size() const
{
    if (m_device != 0)
        return m_segment.length();
    return m_inputFile.size();
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

void Archive::copyData(Archive *archive, QFileDevice *out)
{
    qint64 left = archive->size();
    char data[4096];
    while (left > 0) {
        const qint64 len = qMin<qint64>(left, 4096);
        const qint64 bytesRead = archive->read(data, len);
        if (bytesRead != len) {
            throw QInstaller::Error(tr("Read failed after % 1 bytes: % 2")
                .arg(QString::number(archive->size() - left), archive->errorString()));
        }
        const qint64 bytesWritten = out->write(data, len);
        if (bytesWritten != len) {
            throw QInstaller::Error(tr("Write failed after % 1 bytes: % 2")
                .arg(QString::number(archive->size() - left), out->errorString()));
        }
        left -= len;
    }
}


// -- Component

QByteArray Component::name() const
{
    return m_name;
}

void Component::setName(const QByteArray &ba)
{
    m_name = ba;
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
    QInstaller::appendInt64(out, m_binarySegment.start());
    QInstaller::appendInt64(out, m_binarySegment.length());
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
            throw QInstaller::Error(tr("Could not open archive %1: %2")
                .arg(QString::fromUtf8(archive->name()), archive->errorString()));
        }

        const qint64 expectedStart = starts.takeFirst();
        const qint64 actualStart = out->pos() + offset;
        Q_UNUSED(expectedStart);
        Q_UNUSED(actualStart);
        Q_ASSERT(expectedStart == actualStart);
        archive->copyData(out);
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

} // namespace QInstallerCreator
