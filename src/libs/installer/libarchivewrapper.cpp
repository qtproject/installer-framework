/**************************************************************************
**
** Copyright (C) 2022 The Qt Company Ltd.
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

#include "libarchivewrapper.h"
#include "libarchivewrapper_p.h"

namespace QInstaller {

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::LibArchiveWrapper
    \brief The LibArchiveWrapper class provides an interface for interacting
           with archives handled using the libarchive archive and compression library.

    The invoked archive operations are performed in a normal user mode, or in an
    on-demand elevated user mode through the remote client-server protocol of the framework.

    This class is not thread-safe; extra care should be taken especially when
    elevated mode is active to not call its functions from any thread other than
    where the object was created.
*/

/*!
    Constructs an archive object representing an archive file
    specified by \a filename with \a parent as the parent object.
*/
LibArchiveWrapper::LibArchiveWrapper(const QString &filename, QObject *parent)
    : AbstractArchive(parent)
    , d(new LibArchiveWrapperPrivate(filename))
{
    connect(d, &LibArchiveWrapperPrivate::currentEntryChanged,
        this, &LibArchiveWrapper::currentEntryChanged);
    connect(d, &LibArchiveWrapperPrivate::completedChanged,
        this, &LibArchiveWrapper::completedChanged);
}

/*!
    Constructs an archive object with the given \a parent.
*/
LibArchiveWrapper::LibArchiveWrapper(QObject *parent)
    : AbstractArchive(parent)
    , d(new LibArchiveWrapperPrivate())
{
    connect(d, &LibArchiveWrapperPrivate::currentEntryChanged,
        this, &LibArchiveWrapper::currentEntryChanged);
    connect(d, &LibArchiveWrapperPrivate::completedChanged,
        this, &LibArchiveWrapper::completedChanged);
}

/*!
    Destroys the instance.
*/
LibArchiveWrapper::~LibArchiveWrapper()
{
    delete d;
}

/*!
    Opens the file device using \a mode.
    Returns \c true if succesfull; otherwise \c false.
*/
bool LibArchiveWrapper::open(QIODevice::OpenMode mode)
{
    return d->open(mode);
}

/*!
    Closes the file device.
*/
void LibArchiveWrapper::close()
{
    d->close();
}

/*!
    Sets the \a filename for the archive.

    If the remote connection is active, the same method is called by the server.
*/
void LibArchiveWrapper::setFilename(const QString &filename)
{
    d->setFilename(filename);
}

/*!
    Returns a human-readable description of the last error that occurred.

    If the remote connection is active, the method is called by the server instead.
*/
QString LibArchiveWrapper::errorString() const
{
    return d->errorString();
}

/*!
    Extracts the contents of this archive to \a dirPath. Returns \c true
    on success; \c false otherwise.

    If the remote connection is active, the method is called by the server instead,
    with the client starting a new event loop waiting for the extraction to finish.
*/
bool LibArchiveWrapper::extract(const QString &dirPath)
{
    return d->extract(dirPath);
}

/*!
    Extracts the contents of this archive to \a dirPath with
    precalculated count of \a totalFiles. Returns \c true
    on success; \c false otherwise.

    If the remote connection is active, the method is called by the server instead,
    with the client starting a new event loop waiting for the extraction to finish.
*/
bool LibArchiveWrapper::extract(const QString &dirPath, const quint64 totalFiles)
{
    return d->extract(dirPath, totalFiles);
}

/*!
    Packages the given \a data into the archive and creates the file on disk.
    Returns \c true on success; \c false otherwise.

    If the remote connection is active, the method is called by the server instead.
*/
bool LibArchiveWrapper::create(const QStringList &data)
{
    return d->create(data);
}

/*!
    Returns the contents of this archive as an array of \c ArchiveEntry objects.
    On failure, returns an empty array.
*/
QVector<ArchiveEntry> LibArchiveWrapper::list()
{
    return d->list();
}

/*!
    Returns \c true if the current archive is of a supported format;
    \c false otherwise.
*/
bool LibArchiveWrapper::isSupported()
{
    return d->isSupported();
}

/*!
    Sets the compression level for new archives to \a level.
*/
void LibArchiveWrapper::setCompressionLevel(const CompressionLevel level)
{
    d->setCompressionLevel(level);
}

/*!
    Cancels the extract operation in progress.

    If the remote connection is active, the method is called by the server instead.
*/
void LibArchiveWrapper::cancel()
{
    d->cancel();
}

} // namespace QInstaller
