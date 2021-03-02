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

#include "libarchivewrapper_p.h"

#include "globals.h"

#include <QFileInfo>

namespace QInstaller {

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::LibArchiveWrapperPrivate
    \internal
*/

/*!
    \fn QInstaller::ArchiveWrapper::dataBlockRequested()

    Emitted when the server process has requested another data block.
*/

/*!
    \fn QInstaller::ArchiveWrapper::remoteWorkerFinished()

    Emitted when the server process has finished extracting an archive.
*/

/*!
    Constructs an archive object representing an archive file
    specified by \a filename.
*/
LibArchiveWrapperPrivate::LibArchiveWrapperPrivate(const QString &filename)
    : RemoteObject(QLatin1String(Protocol::AbstractArchive))
{
    init();
    LibArchiveWrapperPrivate::setFilename(filename);
}

/*!
    Constructs the object.
*/
LibArchiveWrapperPrivate::LibArchiveWrapperPrivate()
    : RemoteObject(QLatin1String(Protocol::AbstractArchive))
{
    init();
}

/*!
    Destroys the instance.
*/
LibArchiveWrapperPrivate::~LibArchiveWrapperPrivate()
{
    m_timer.stop();
}

/*!
    Opens the file device using \a mode.
    Returns \c true if succesfull; otherwise \c false.
*/
bool LibArchiveWrapperPrivate::open(QIODevice::OpenMode mode)
{
    return m_archive.open(mode);
}

/*!
    Closes the file device.
*/
void LibArchiveWrapperPrivate::close()
{
    m_archive.close();
}

/*!
    Sets the \a filename for the archive.

    If the remote connection is active, the same method is called by the server.
*/
void LibArchiveWrapperPrivate::setFilename(const QString &filename)
{
    if (connectToServer()) {
        m_lock.lockForWrite();
        callRemoteMethod(QLatin1String(Protocol::AbstractArchiveSetFilename), filename, dummy);
        m_lock.unlock();
    }
    m_archive.setFilename(filename);
}

/*!
    Returns a human-readable description of the last error that occurred.

    If the remote connection is active, the method is called by the server instead.
*/
QString LibArchiveWrapperPrivate::errorString() const
{
    if ((const_cast<LibArchiveWrapperPrivate *>(this))->connectToServer()) {
        m_lock.lockForWrite();
        const QString errorString
            = callRemoteMethod<QString>(QLatin1String(Protocol::AbstractArchiveErrorString));
        m_lock.unlock();
        return errorString;
    }
    return m_archive.errorString();
}

/*!
    Extracts the contents of this archive to \a dirPath with
    an optional precalculated count of \a totalFiles. Returns \c true
    on success; \c false otherwise.

    If the remote connection is active, the method is called by the server instead,
    with the client starting a new event loop waiting for the extraction to finish.
*/
bool LibArchiveWrapperPrivate::extract(const QString &dirPath, const quint64 totalFiles)
{
    if (connectToServer()) {
        m_lock.lockForWrite();
        callRemoteMethod(QLatin1String(Protocol::AbstractArchiveExtract), dirPath, totalFiles);
        m_lock.unlock();
        {
            QEventLoop loop;
            connect(this, &LibArchiveWrapperPrivate::remoteWorkerFinished, &loop, &QEventLoop::quit);
            loop.exec();
        }
        return (workerStatus() == ExtractWorker::Success);
    }
    return m_archive.extract(dirPath, totalFiles);
}

/*!
    Packages the given \a data into the archive and creates the file on disk.
    Returns \c true on success; \c false otherwise.

    If the remote connection is active, the method is called by the server instead.
*/
bool LibArchiveWrapperPrivate::create(const QStringList &data)
{
    if (connectToServer()) {
        m_lock.lockForWrite();
        const bool success
            = callRemoteMethod<bool>(QLatin1String(Protocol::AbstractArchiveCreate), data);
        m_lock.unlock();
        return success;
    }
    return m_archive.create(data);
}

/*!
    Returns the contents of this archive as an array of \c ArchiveEntry objects.
    On failure, returns an empty array.
*/
QVector<ArchiveEntry> LibArchiveWrapperPrivate::list()
{
    return m_archive.list();
}

/*!
    Returns \c true if the current archive is of a supported format;
    \c false otherwise.
*/
bool LibArchiveWrapperPrivate::isSupported()
{
    return m_archive.isSupported();
}

/*!
    Sets the compression level for new archives to \a level.

    If the remote connection is active, the method is called by the server instead.
*/
void LibArchiveWrapperPrivate::setCompressionLevel(const AbstractArchive::CompressionLevel level)
{
    if (connectToServer()) {
        m_lock.lockForWrite();
        callRemoteMethod(QLatin1String(Protocol::AbstractArchiveSetCompressionLevel), level, dummy);
        m_lock.unlock();
        return;
    }
    m_archive.setCompressionLevel(level);
}

/*!
    Cancels the extract operation in progress.

    If the remote connection is active, the method is called by the server instead.
*/
void LibArchiveWrapperPrivate::cancel()
{
    if (connectToServer()) {
        m_lock.lockForWrite();
        callRemoteMethod(QLatin1String(Protocol::AbstractArchiveCancel));
        m_lock.unlock();
        return;
    }
    m_archive.cancel();
}

/*!
    Calls a remote method to get the associated queued signals from the server.
    Signals are then processed and emitted client-side.
*/
void LibArchiveWrapperPrivate::processSignals()
{
    if (!isConnectedToServer())
        return;

    if (!m_lock.tryLockForRead())
        return;

    QList<QVariant> receivedSignals =
        callRemoteMethod<QList<QVariant>>(QString::fromLatin1(Protocol::GetAbstractArchiveSignals));

    m_lock.unlock();
    while (!receivedSignals.isEmpty()) {
        const QString name = receivedSignals.takeFirst().toString();
        if (name == QLatin1String(Protocol::AbstractArchiveSignalCurrentEntryChanged)) {
            emit currentEntryChanged(receivedSignals.takeFirst().toString());
        } else if (name == QLatin1String(Protocol::AbstractArchiveSignalCompletedChanged)) {
            const quint64 completed = receivedSignals.takeFirst().value<quint64>();
            const quint64 total = receivedSignals.takeFirst().value<quint64>();
            emit completedChanged(completed, total);
        } else if (name == QLatin1String(Protocol::AbstractArchiveSignalDataBlockRequested)) {
            emit dataBlockRequested();
        } else if (name == QLatin1String(Protocol::AbstractArchiveSignalWorkerFinished)) {
            emit remoteWorkerFinished();
        }
    }
}

/*!
    Reads a block of data from the current position of the underlying file device.
*/
void LibArchiveWrapperPrivate::onDataBlockRequested()
{
    constexpr quint64 blockSize = 10 * 1024 * 1024; // 10MB

    QFile *const file = &m_archive.m_data->file;
    if (!file->isOpen() || file->isSequential()) {
        qCWarning(QInstaller::lcInstallerInstallLog) << file->errorString();
        setClientDataAtEnd();
        return;
    }
    if (file->atEnd() && file->seek(0)) {
        setClientDataAtEnd();
        return;
    }

    QByteArray *const buff = &m_archive.m_data->buffer;
    if (!buff->isEmpty())
        buff->clear();

    if (buff->size() != blockSize)
        buff->resize(blockSize);

    const qint64 bytesRead = file->read(buff->data(), blockSize);
    if (bytesRead == -1) {
        qCWarning(QInstaller::lcInstallerInstallLog) << file->errorString();
        setClientDataAtEnd();
        return;
    }
    // The read callback in ExtractWorker class expects the buffer size to
    // match the number of bytes read. Some formats will fail if the buffer
    // is larger than the actual data.
    if (buff->size() != bytesRead)
        buff->resize(bytesRead);

    addDataBlock(*buff);
}

/*!
    Starts the timer to process server-side signals and connects handler
    signals for the matching signals of the wrapper object.
*/
void LibArchiveWrapperPrivate::init()
{
    m_timer.start(250);
    QObject::connect(&m_timer, &QTimer::timeout,
                     this, &LibArchiveWrapperPrivate::processSignals);

    QObject::connect(&m_archive, &LibArchiveArchive::currentEntryChanged,
                     this, &LibArchiveWrapperPrivate::currentEntryChanged);
    QObject::connect(&m_archive, &LibArchiveArchive::completedChanged,
                     this, &LibArchiveWrapperPrivate::completedChanged);

    QObject::connect(this, &LibArchiveWrapperPrivate::dataBlockRequested,
                     this, &LibArchiveWrapperPrivate::onDataBlockRequested);
}

/*!
    Calls a remote method to add a \a buffer for reading.
*/
void LibArchiveWrapperPrivate::addDataBlock(const QByteArray &buffer)
{
    if (connectToServer()) {
        m_lock.lockForWrite();
        callRemoteMethod(QLatin1String(Protocol::AbstractArchiveAddDataBlock), buffer, dummy);
        m_lock.unlock();
    }
}

/*!
    Calls a remote method to inform that the client has finished
    reading the current file.
*/
void LibArchiveWrapperPrivate::setClientDataAtEnd()
{
    if (connectToServer()) {
        m_lock.lockForWrite();
        callRemoteMethod(QLatin1String(Protocol::AbstractArchiveSetClientDataAtEnd));
        m_lock.unlock();
    }
}

/*!
    Calls a remote method to retrieve and return the status of
    the extract worker on a server process.
*/
ExtractWorker::Status LibArchiveWrapperPrivate::workerStatus() const
{
    ExtractWorker::Status status = ExtractWorker::Unfinished;
    if ((const_cast<LibArchiveWrapperPrivate *>(this))->connectToServer()) {
        m_lock.lockForWrite();
        status = static_cast<ExtractWorker::Status>(
            callRemoteMethod<qint32>(QLatin1String(Protocol::AbstractArchiveWorkerStatus)));
        m_lock.unlock();
    }
    return status;
}

} // namespace QInstaller
