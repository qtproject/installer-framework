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

#include "abstractarchive.h"

namespace QInstaller {

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::ArchiveEntry
    \brief The ArchiveEntry struct represents an entry in an archive file,
           which can be for example a file or a directory.
*/

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::AbstractArchive
    \brief The AbstractArchive class is the base class for classes representing
           different archive files. It cannot be instantiated on its own but
           defines the API and provides common functionality when subclassed.
*/

/*!
    \enum AbstractArchive::CompressionLevel
    This enum holds the possible values for archive compression level.

    \value Non
    \value Fastest
    \value Fast
    \value Normal
    \value Maximum
    \value Ultra
*/

/*!
    \fn QInstaller::AbstractArchive::currentEntryChanged(const QString &filename)

    Current entry changed to \a filename. Subclasses should emit this signal whenever
    the entry to process is changed.
*/

/*!
    \fn QInstaller::AbstractArchive::completedChanged(const quint64 completed, const quint64 total)

    The ratio of \a completed entries from \a total changed. Subclasses should emit
    this whenever the progress changes.
*/

/*!
    \fn QInstaller::AbstractArchive::cancel()

    Cancels current operation. A subclass should implement this slot.
*/

/*!
    \fn QInstaller::AbstractArchive::close()

    Closes the archive. A subclass should implement this method.
*/

/*!
    \fn QInstaller::AbstractArchive::create(const QStringList &data)

    Creates an archive from \a data. Returns \c true on success;
    \c false otherwise. A subclass should implement this method.
*/

/*!
    \fn QInstaller::AbstractArchive::extract(const QString &dirPath)

    Extracts the archive to \a dirPath. Returns \c true on success;
    \c false otherwise. A subclass should implement this method.
*/

/*!
    \fn QInstaller::AbstractArchive::extract(const QString &dirPath, const quint64 totalFiles)

    Extracts the contents of an archive to \a dirPath with precalculated
    count of \a totalFiles. Returns \c true on success; \c false otherwise.
    A subclass should implement this method.
*/

/*!
    \fn QInstaller::AbstractArchive::isSupported()

    Returns \c true if the archive is supported; \c false otherwise.
    A subclass should implement this method.
*/

/*!
    \fn QInstaller::AbstractArchive::list()

    Returns a list of entries in this archive. A subclass should implement this method.
*/

/*!
    \fn QInstaller::AbstractArchive::open(QIODevice::OpenMode mode)

    Opens the file device for an archive in \a mode. Returns \c true on success;
    \c false otherwise. A subclass should implement this method.
*/

/*!
    \fn QInstaller::AbstractArchive::setFilename(const QString &filename)

    Sets the \a filename for the archive. A subclass should implement this method.
*/

/*!
    Constructs a new archive object with \a parent as parent. Cannot be
    called directly but instead from subclass constructors.
*/
AbstractArchive::AbstractArchive(QObject *parent)
    : QObject(parent)
    , m_compressionLevel(CompressionLevel::Normal)
{
}

/*!
    Virtual destructor for \c AbstractArchive.
*/
AbstractArchive::~AbstractArchive()
{
}

/*!
    Returns a human-readable description of the last error that occurred.
*/
QString AbstractArchive::errorString() const
{
    return m_error;
}

/*!
    Sets the compression level for new archives to \a level.
*/
void AbstractArchive::setCompressionLevel(const CompressionLevel level)
{
    m_compressionLevel = level;
}

/*!
    Sets a human-readable description of the current \a error.
*/
void AbstractArchive::setErrorString(const QString &error)
{
    m_error = error;
}

/*!
    Returns the current compression level.
*/
AbstractArchive::CompressionLevel AbstractArchive::compressionLevel() const
{
    return m_compressionLevel;
}

/*!
    Reads an \a entry from the specified \a istream. Returns a reference to \a istream.
*/
QDataStream &operator>>(QDataStream &istream, ArchiveEntry &entry)
{
    istream >> entry.path >> entry.utcTime >> entry.isDirectory
        >> entry.uncompressedSize >> entry.permissions_mode >> entry.permissions_enum;

    return istream;
}

/*!
    Writes an \a entry to the specified \a ostream. Returns a reference to \a ostream.
*/
QDataStream &operator<<(QDataStream &ostream, const ArchiveEntry &entry)
{
    ostream << entry.path << entry.utcTime << entry.isDirectory
        << entry.uncompressedSize << entry.permissions_mode << entry.permissions_enum;

    return ostream;
}

/*!
    Returns \c true if left-hand-side entry \a lhs is equal to right-hand-size entry \a rhs.
*/
bool operator==(const ArchiveEntry &lhs, const ArchiveEntry &rhs)
{
    return lhs.path == rhs.path
        && lhs.utcTime == rhs.utcTime
        && lhs.isDirectory == rhs.isDirectory
        && lhs.compressedSize == rhs.compressedSize
        && lhs.uncompressedSize == rhs.uncompressedSize
        && lhs.permissions_mode == rhs.permissions_mode
        && (lhs.permissions_enum == rhs.permissions_enum // ignore invalid permissions
        || lhs.permissions_enum == static_cast<QFile::Permissions>(-1)
        || rhs.permissions_enum == static_cast<QFile::Permissions>(-1));
}

} // namespace QInstaller
