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

#include "libarchivearchive.h"

#include "directoryguard.h"
#include "fileguard.h"
#include "errors.h"
#include "globals.h"

#include <stdio.h>
#include <string.h>

#include <QApplication>
#include <QFileInfo>
#include <QDir>
#include <QTimer>

#ifdef Q_OS_WIN
#include <locale.h>
#endif

#if defined(Q_OS_WIN) && !defined(SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE)
#define SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE
#endif

namespace QInstaller {

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::ScopedPointerReaderDeleter
    \internal
*/

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::ScopedPointerWriterDeleter
    \internal
*/

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::ScopedPointerEntryDeleter
    \internal
*/

namespace ArchiveEntryPaths {

// TODO: it is expected that the filename handling will change in the major
// version jump to libarchive 4.x, and the *_w methods will disappear.

/*!
    \internal

    Returns the path name from the archive \a entry as a \c QString. The path is
    stored internally in a multistring that can contain a combination of a wide
    character or multibyte string in current locale or unicode string encoded as UTF-8.

    \note The MBS version is expected to be convertable from latin-1 which might
    not actually be the case, as the encoding depends on the current locale.
*/
static QString pathname(archive_entry *const entry)
{
    if (!entry)
        return QString();
#ifdef Q_OS_WIN
    if (const wchar_t *path = archive_entry_pathname_w(entry))
        return QString::fromWCharArray(path);
#endif
    if (const char *path = archive_entry_pathname_utf8(entry))
        return QString::fromUtf8(path);

    return QString::fromLatin1(archive_entry_pathname(entry));
}

/*!
    \internal

    Sets the path name for the archive \a entry to \a path. This function
    tries to update all variants of the string in the internal multistring
    struct.
*/
static void setPathname(archive_entry *const entry, const QString &path)
{
    if (!entry)
        return;

    // Try updating all variants at once, stops on failure
    if (archive_entry_update_pathname_utf8(entry, path.toUtf8()))
        return;

    // If that does not work, then set them individually
    archive_entry_set_pathname(entry, path.toLatin1());
    archive_entry_set_pathname_utf8(entry, path.toUtf8());
#ifdef Q_OS_WIN
    wchar_t *wpath = new wchar_t[path.length() + 1];
    path.toWCharArray(wpath);
    wpath[path.length()] = '\0';

    archive_entry_copy_pathname_w(entry, wpath);
    delete[] wpath;
#endif
}

/*!
    \internal

    Returns the source path on disk from the current archive \a entry as a \c QString.
    The path is stored internally in a multistring that can contain a combination
    of a wide character or multibyte string in current locale.

    \note The MBS version is expected to be convertable from UTF-8.
*/
static QString sourcepath(archive_entry * const entry)
{
    if (!entry)
        return QString();
#ifdef Q_OS_WIN
    if (const wchar_t *path = archive_entry_sourcepath_w(entry))
        return QString::fromWCharArray(path);
#endif
    return QString::fromUtf8(archive_entry_sourcepath(entry));
}

/*!
    \internal

    Returns the hardlink path from the current archive \a entry as a \c QString.
    The path is stored internally in a multistring that can contain a combination of a wide
    character or multibyte string in current locale or unicode string encoded as UTF-8.

    \note The MBS version is expected to be convertable from latin-1 which might
    not actually be the case, as the encoding depends on the current locale.
*/
static QString hardlink(archive_entry * const entry)
{
    if (!entry)
        return QString();
#ifdef Q_OS_WIN
    if (const wchar_t *path = archive_entry_hardlink_w(entry))
        return QString::fromWCharArray(path);
#endif
    if (const char *path = archive_entry_hardlink_utf8(entry))
        return QString::fromUtf8(path);

    return QString::fromLatin1(archive_entry_hardlink(entry));
}

/*!
    \internal

    Sets the hard link path for the archive \a entry to \a path. This function
    tries to update all variants of the string in the internal multistring
    struct.
*/
static void setHardlink(archive_entry *const entry, const QString &path)
{
    if (!entry)
        return;

    // Try updating all variants at once, stops on failure
    if (archive_entry_update_hardlink_utf8(entry, path.toUtf8()))
        return;

    // If that does not work, then set them individually
    archive_entry_set_hardlink(entry, path.toLatin1());
    archive_entry_set_hardlink_utf8(entry, path.toUtf8());
#ifdef Q_OS_WIN
    wchar_t *wpath = new wchar_t[path.length() + 1];
    path.toWCharArray(wpath);
    wpath[path.length()] = '\0';

    archive_entry_copy_hardlink_w(entry, wpath);
    delete[] wpath;
#endif
}

/*!
    \internal

    Calls a function object or pointer \a func with any number of extra
    arguments \a args. Returns the return value of the function of type T.

    On Windows, this changes the locale category LC_CTYPE from C to system
    locale. The original LC_CTYPE is restored after the function call.
    Currently the locale is unchanged on other platforms.
*/
template <typename T, typename F, typename... Args>
static T callWithSystemLocale(F func, Args... args)
{
#ifdef Q_OS_WIN
    const QByteArray oldLocale = setlocale(LC_CTYPE, "");
#endif
    T returnValue = func(std::forward<Args>(args)...);
#ifdef Q_OS_WIN
    setlocale(LC_CTYPE, oldLocale.constData());
#endif
    return returnValue;
}

/*!
    \internal

    Calls a function object or pointer \a func with any number of extra
    arguments \a args.

    On Windows, this changes the locale category LC_CTYPE from C to system
    locale. The original LC_CTYPE is restored after the function call.
    Currently the locale is unchanged on other platforms.
*/
template <typename F, typename... Args>
static void callWithSystemLocale(F func, Args... args)
{
#ifdef Q_OS_WIN
    const QByteArray oldLocale = setlocale(LC_CTYPE, "");
#endif
    func(std::forward<Args>(args)...);
#ifdef Q_OS_WIN
    setlocale(LC_CTYPE, oldLocale.constData());
#endif
}

} // namespace ArchiveEntryPaths

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::ExtractWorker
    \internal
*/

ExtractWorker::Status ExtractWorker::status() const
{
    return m_status;
}

void ExtractWorker::extract(const QString &dirPath, const quint64 totalFiles)
{
    m_status = Unfinished;
    quint64 completed = 0;

    if (!totalFiles) {
        m_status = Failure;
        emit finished(QLatin1String("The file count for current archive is null!"));
        return;
    }

    QScopedPointer<archive, ScopedPointerReaderDeleter> reader(archive_read_new());
    QScopedPointer<archive, ScopedPointerWriterDeleter> writer(archive_write_disk_new());
    archive_entry *entry = nullptr;

    LibArchiveArchive::configureReader(reader.get());
    LibArchiveArchive::configureDiskWriter(writer.get());

    DirectoryGuard targetDir(QFileInfo(dirPath).absoluteFilePath());
    try {
        const QStringList createdDirs = targetDir.tryCreate();
        // Make sure that all leading directories created get removed as well
        foreach (const QString &directory, createdDirs)
            emit currentEntryChanged(directory);

        archive_read_set_read_callback(reader.get(), readCallback);
        archive_read_set_callback_data(reader.get(), this);
        archive_read_set_seek_callback(reader.get(), seekCallback);

        int status = archive_read_open1(reader.get());
        if (status != ARCHIVE_OK) {
            m_status = Failure;
            emit finished(tr("Cannot open archive for reading: %1")
                .arg(LibArchiveArchive::errorStringWithCode(reader.get())));
            return;
        }

        forever {
            if (m_status == Canceled) {
                emit finished(QLatin1String("Extract canceled."));
                return;
            }
            status = ArchiveEntryPaths::callWithSystemLocale<int>(archive_read_next_header, reader.get(), &entry);
            if (status == ARCHIVE_EOF)
                break;
            if (status != ARCHIVE_OK) {
                m_status = Failure;
                emit finished(tr("Cannot read entry header: %1")
                    .arg(LibArchiveArchive::errorStringWithCode(reader.get())));
                return;
            }
            const QString current = ArchiveEntryPaths::callWithSystemLocale<QString>(ArchiveEntryPaths::pathname, entry);
            const QString outputPath = dirPath + QDir::separator() + current;
            ArchiveEntryPaths::callWithSystemLocale(&ArchiveEntryPaths::setPathname, entry, outputPath);

            const QString hardlink = ArchiveEntryPaths::callWithSystemLocale<QString>(ArchiveEntryPaths::hardlink, entry);
            if (!hardlink.isEmpty()) {
                const QString hardLinkPath = dirPath + QDir::separator() + hardlink;
                ArchiveEntryPaths::callWithSystemLocale(ArchiveEntryPaths::setHardlink, entry, hardLinkPath);
            }

            emit currentEntryChanged(outputPath);
            if (!writeEntry(reader.get(), writer.get(), entry))
                return;

            ++completed;
            emit completedChanged(completed, totalFiles);

            qApp->processEvents();
        }
    } catch (const Error &e) {
        m_status = Failure;
        emit finished(e.message());
        return;
    }
    targetDir.release();
    m_status = Success;
    emit finished();
}

void ExtractWorker::addDataBlock(const QByteArray buffer)
{
    m_buffer.append(buffer);
    emit dataReadyForRead();
}

void ExtractWorker::onFilePositionChanged(qint64 pos)
{
    m_lastPos = pos;
    emit seekReady();
}

void ExtractWorker::cancel()
{
    m_status = Canceled;
}

ssize_t ExtractWorker::readCallback(archive *reader, void *caller, const void **buff)
{
    Q_UNUSED(reader)

    ExtractWorker *obj;
    if (!(obj = static_cast<ExtractWorker *>(caller)))
        return ARCHIVE_FATAL;

    QByteArray *buffer = &obj->m_buffer;
    if (!buffer->isEmpty())
        buffer->clear();

    emit obj->dataBlockRequested();

    // It's a bit bad that we have to wait here, but libarchive doesn't
    // provide an event based reading method.
    {
        QEventLoop loop;
        QTimer::singleShot(30000, &loop, &QEventLoop::quit);
        connect(obj, &ExtractWorker::dataReadyForRead, &loop, &QEventLoop::quit);
        connect(obj, &ExtractWorker::dataAtEnd, &loop, &QEventLoop::quit);
        loop.exec();
    }

    if (!(*buff = static_cast<const void *>(buffer->constData())))
        return ARCHIVE_FATAL;

    return buffer->size();
}

la_int64_t ExtractWorker::seekCallback(archive *reader, void *caller, la_int64_t offset, int whence)
{
    Q_UNUSED(reader)

    ExtractWorker *obj;
    if (!(obj = static_cast<ExtractWorker *>(caller)))
        return ARCHIVE_FATAL;

    emit obj->seekRequested(static_cast<qint64>(offset), whence);

    {
        QEventLoop loop;
        QTimer::singleShot(30000, &loop, &QEventLoop::quit);
        connect(obj, &ExtractWorker::seekReady, &loop, &QEventLoop::quit);
        loop.exec();
    }

    return static_cast<la_int64_t>(obj->m_lastPos);
}

bool ExtractWorker::writeEntry(archive *reader, archive *writer, archive_entry *entry)
{
    int status;
    const void *buff;
    size_t size;
    int64_t offset;

    const QString entryPath = ArchiveEntryPaths::callWithSystemLocale
        <QString>(ArchiveEntryPaths::pathname, entry);

    FileGuardLocker locker(entryPath, FileGuard::globalObject());

    status = archive_write_header(writer, entry);
    if (status != ARCHIVE_OK) {
        emit finished(tr("Cannot write entry \"%1\" to disk: %2")
            .arg(entryPath, LibArchiveArchive::errorStringWithCode(writer)));
        return false;
    }

    forever {
        status = archive_read_data_block(reader, &buff, &size, &offset);
        if (status == ARCHIVE_EOF) {
            status = archive_write_finish_entry(writer);
            if (status == ARCHIVE_OK)
                return true;
        }
        if (status != ARCHIVE_OK) {
            m_status = Failure;
            emit finished(tr("Cannot write entry \"%1\" to disk: %2")
                .arg(entryPath, LibArchiveArchive::errorStringWithCode(reader)));
            return false;
        }
        status = archive_write_data_block(writer, buff, size, offset);
        if (status != ARCHIVE_OK) {
            m_status = Failure;
            emit finished(tr("Cannot write entry \"%1\" to disk: %2")
                .arg(entryPath, LibArchiveArchive::errorStringWithCode(writer)));
            return false;
        }
    }
}

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::LibArchiveArchive
    \brief The LibArchiveArchive class represents an archive file
           handled with libarchive archive and compression library.

    In addition to extracting data from the underlying file device,
    the class supports a non-blocking mode of extracting from an external
    data source. When using this mode, the calling client must pass the data
    to be read in chunks of arbitrary size, and inform the object when there
    is no more data to read.
*/

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::LibArchiveArchive::ArchiveData
    \brief Bundles a file device and associated read buffer for access
           as client data in libarchive callbacks.
*/

/*!
    \fn QInstaller::LibArchiveArchive::dataBlockRequested()

    Emitted when the worker object requires more data to continue extracting.
*/

/*!
    \fn QInstaller::LibArchiveArchive::seekRequested(qint64 offset, int whence)

    Emitted when the worker object requires a seek to \a offset to continue extracting.
    The \a whence value defines the starting position for \a offset.
*/

/*!
    \fn QInstaller::LibArchiveArchive::workerFinished()

    Emitted when the worker object finished extracting an archive.
*/

/*!
    \fn QInstaller::LibArchiveArchive::workerAboutToExtract(const QString &dirPath, const quint64 totalFiles)

    Emitted when the worker object is about to extract \a totalFiles
    from an archive to \a dirPath.
*/

/*!
    \fn QInstaller::LibArchiveArchive::workerAboutToAddDataBlock(const QByteArray buffer)

    Emitted when the worker object is about to add and read the data block in \a buffer.
*/

/*!
    \fn QInstaller::LibArchiveArchive::workerAboutToSetDataAtEnd()

    Emitted when the worker object is about to set data-at-end, meaning there
    will be no further read requests for the calling client.
*/

/*!
    \fn QInstaller::LibArchiveArchive::workerAboutToCancel()

    Emitted when the worker object is about to cancel extracting.
*/

/*!
    \fn QInstaller::LibArchiveArchive::workerAboutToSetFilePosition(qint64 pos)

    Emitted when the worker object is about to set new file position \a pos.
*/

/*!
    Constructs an archive object representing an archive file
    specified by \a filename with \a parent as the parent object.
*/
LibArchiveArchive::LibArchiveArchive(const QString &filename, QObject *parent)
    : AbstractArchive(parent)
    , m_data(new ArchiveData())
    , m_cancelScheduled(false)
{
    LibArchiveArchive::setFilename(filename);
    initExtractWorker();
}

/*!
    Constructs an archive object with the given \a parent.
*/
LibArchiveArchive::LibArchiveArchive(QObject *parent)
    : AbstractArchive(parent)
    , m_data(new ArchiveData())
    , m_cancelScheduled(false)
{
    initExtractWorker();
}

/*!
    Destroys the instance and releases resources.
*/
LibArchiveArchive::~LibArchiveArchive()
{
    m_workerThread.quit();
    m_workerThread.wait();

    delete m_data;
}

/*!
    \reimp

    Opens the underlying file device using \a mode. Returns \c true if
    succesfull; otherwise \c false.
*/
bool LibArchiveArchive::open(QIODevice::OpenMode mode)
{
    if (!m_data->file.open(mode)) {
        setErrorString(m_data->file.errorString());
        return false;
    }
    return true;
}

/*!
    \reimp

    Closes the underlying file device.
*/
void LibArchiveArchive::close()
{
    m_data->file.close();
}

/*!
    \reimp

    Sets the \a filename of the underlying file device.
*/
void LibArchiveArchive::setFilename(const QString &filename)
{
    m_data->file.setFileName(filename);
}

/*!
    \reimp

    Extracts the contents of this archive to \a dirPath.
    Returns \c true on success; \c false otherwise.
*/
bool LibArchiveArchive::extract(const QString &dirPath)
{
    return extract(dirPath, totalFiles());
}

/*!
    \reimp

    Extracts the contents of this archive to \a dirPath with
    precalculated count of \a totalFiles. Returns \c true on
    success; \c false otherwise.
*/
bool LibArchiveArchive::extract(const QString &dirPath, const quint64 totalFiles)
{
    m_cancelScheduled = false;
    quint64 completed = 0;
    if (!totalFiles) {
        setErrorString(QLatin1String("The file count for current archive is null!"));
        return false;
    }

    QScopedPointer<archive, ScopedPointerReaderDeleter> reader(archive_read_new());
    QScopedPointer<archive, ScopedPointerWriterDeleter> writer(archive_write_disk_new());
    archive_entry *entry = nullptr;

    configureReader(reader.get());
    configureDiskWriter(writer.get());

    DirectoryGuard targetDir(QFileInfo(dirPath).absoluteFilePath());
    try {
        const QStringList createdDirs = targetDir.tryCreate();
        // Make sure that all leading directories created get removed as well
        foreach (const QString &directory, createdDirs)
            emit currentEntryChanged(directory);

        int status = archiveReadOpenWithCallbacks(reader.get());
        if (status != ARCHIVE_OK) {
            throw Error(tr("Cannot open archive for reading: %1")
                .arg(errorStringWithCode(reader.get())));
        }

        forever {
            if (m_cancelScheduled)
                throw Error(QLatin1String("Extract canceled."));

            status = ArchiveEntryPaths::callWithSystemLocale<int>(archive_read_next_header, reader.get(), &entry);
            if (status == ARCHIVE_EOF)
                break;
            if (status != ARCHIVE_OK) {
                throw Error(tr("Cannot read entry header: %1")
                    .arg(errorStringWithCode(reader.get())));
            }

            const QString current = ArchiveEntryPaths::callWithSystemLocale<QString>(ArchiveEntryPaths::pathname, entry);
            const QString outputPath = dirPath + QDir::separator() + current;
            ArchiveEntryPaths::callWithSystemLocale(ArchiveEntryPaths::setPathname, entry, outputPath);

            const QString hardlink = ArchiveEntryPaths::callWithSystemLocale<QString>(ArchiveEntryPaths::hardlink, entry);
            if (!hardlink.isEmpty()) {
                const QString hardLinkPath = dirPath + QDir::separator() + hardlink;
                ArchiveEntryPaths::callWithSystemLocale(ArchiveEntryPaths::setHardlink, entry, hardLinkPath);
            }

            emit currentEntryChanged(outputPath);
            if (!writeEntry(reader.get(), writer.get(), entry)) {
                throw Error(tr("Cannot write entry \"%1\" to disk: %2")
                    .arg(outputPath, errorString())); // appropriate error string set in writeEntry()
            }

            ++completed;
            emit completedChanged(completed, totalFiles);

            qApp->processEvents();
        }
    } catch (const Error &e) {
        setErrorString(e.message());
        m_data->file.seek(0);
        return false;
    }
    targetDir.release();
    m_data->file.seek(0);
    return true;
}

/*!
    \reimp

    Packages the given \a data into the archive and creates the file on disk.
*/
bool LibArchiveArchive::create(const QStringList &data)
{
    QScopedPointer<archive, ScopedPointerWriterDeleter> writer(archive_write_new());
    configureWriter(writer.get());

    QStringList globbedData;
    for (auto &dataEntry : data) { // Expand glob pattern entries with proper filenames
        if (!dataEntry.contains(QLatin1Char('*'))) {
            globbedData.append(dataEntry);
            continue;
        }
        const QFileInfo entryInfo(dataEntry);
        if (entryInfo.path().contains(QLatin1Char('*'))) {
            setErrorString(QString::fromLatin1("Invalid argument \"%1\": glob patterns "
                "are not supported between directory paths.").arg(dataEntry));
            return false;
        }
        const QDir parentDir = entryInfo.dir();
        const QList<QFileInfo> infoList = parentDir.entryInfoList(QStringList()
            << entryInfo.fileName(), QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot);

        for (auto &info : infoList)
            globbedData.append(info.absoluteFilePath());
    }

    try {
        int status;
#ifdef Q_OS_WIN
        QScopedPointer<wchar_t, QScopedPointerArrayDeleter<wchar_t>> fileName_w(
            new wchar_t[m_data->file.fileName().length() + 1]);

        m_data->file.fileName().toWCharArray(fileName_w.get());
        fileName_w.get()[m_data->file.fileName().length()] = '\0';

        if ((status = archive_write_open_filename_w(writer.get(), fileName_w.get()))) {
            throw Error(tr("Cannot open file \"%1\" for writing: %2")
                .arg(m_data->file.fileName(), errorStringWithCode(writer.get())));
        }
#else
        if ((status = archive_write_open_filename(writer.get(), m_data->file.fileName().toUtf8()))) {
            throw Error(tr("Cannot open file \"%1\" for writing: %2")
                .arg(m_data->file.fileName(), errorStringWithCode(writer.get())));
        }
#endif
        for (auto &dataEntry : globbedData) {
            QScopedPointer<archive, ScopedPointerReaderDeleter> reader(archive_read_disk_new());
            configureDiskReader(reader.get());

#ifdef Q_OS_WIN
            QScopedPointer<wchar_t, QScopedPointerArrayDeleter<wchar_t>> dataEntry_w(
                new wchar_t[dataEntry.length() + 1]);

            dataEntry.toWCharArray(dataEntry_w.get());
            dataEntry_w.get()[dataEntry.length()] = '\0';

            if ((status = archive_read_disk_open_w(reader.get(), dataEntry_w.get()))) {
                throw Error(tr("Cannot open file \"%1\" for reading: %2")
                    .arg(dataEntry, errorStringWithCode(reader.get())));
            }
#else
            if ((status = archive_read_disk_open(reader.get(), dataEntry.toUtf8()))) {
                throw Error(tr("Cannot open file \"%1\" for reading: %2")
                    .arg(dataEntry, errorStringWithCode(reader.get())));
            }
#endif
            QDir basePath = QFileInfo(dataEntry).dir();
            forever {
                QScopedPointer<archive_entry, ScopedPointerEntryDeleter> entry(archive_entry_new());
                status = ArchiveEntryPaths::callWithSystemLocale<int>(archive_read_next_header2, reader.get(), entry.get());
                if (status == ARCHIVE_EOF)
                    break;
                if (status != ARCHIVE_OK) {
                    throw Error(tr("Cannot read entry header: %1")
                        .arg(errorStringWithCode(reader.get())));
                }

                const QFileInfo fileOrDir(pathWithoutNamespace(
                    ArchiveEntryPaths::callWithSystemLocale<QString>(ArchiveEntryPaths::sourcepath, entry.get())));
                // Set new path name in archive, otherwise we add all directories from absolute path
                const QString newPath = basePath.relativeFilePath(fileOrDir.filePath());
                ArchiveEntryPaths::callWithSystemLocale(ArchiveEntryPaths::setPathname, entry.get(), newPath);

                archive_read_disk_descend(reader.get());
                status = archive_write_header(writer.get(), entry.get());
                if (status < ARCHIVE_OK) {
                    throw Error(tr("Cannot write entry header for \"%1\": %2")
                        .arg(fileOrDir.filePath(), errorStringWithCode(writer.get())));
                }
                if (fileOrDir.isDir() || archive_entry_size(entry.get()) == 0)
                    continue; // nothing to copy

                QFile file(pathWithoutNamespace(ArchiveEntryPaths::callWithSystemLocale<QString>(
                    ArchiveEntryPaths::sourcepath, entry.get())));
                if (!file.open(QIODevice::ReadOnly)) {
                    throw Error(tr("Cannot open file \"%1\" for reading: %2")
                        .arg(file.fileName(), file.errorString()));
                }

                QByteArray buffer;
                constexpr qint64 blockSize = 4 * 1024;
                buffer.resize(blockSize);

                ssize_t bytesRead = readData(&file, buffer.data(), blockSize);
                while (bytesRead > 0) {
                    archive_write_data(writer.get(), buffer.constData(), blockSize);
                    bytesRead = readData(&file, buffer.data(), blockSize);
                }
                file.close();
            }
        }
    } catch (const Error &e) {
        setErrorString(e.message());
        return false;
    }
    return true;
}

/*!
    \reimp

    Returns the contents of this archive as an array of \c ArchiveEntry objects.
    On failure, returns an empty array.
*/
QVector<ArchiveEntry> LibArchiveArchive::list()
{
    QScopedPointer<archive, ScopedPointerReaderDeleter> reader(archive_read_new());
    archive_entry *entry = nullptr;

    configureReader(reader.get());

    QVector<ArchiveEntry> entries;
    try {
        int status = archiveReadOpenWithCallbacks(reader.get());
        if (status != ARCHIVE_OK) {
            throw Error(tr("Cannot open archive for reading: %1")
                .arg(errorStringWithCode(reader.get())));
        }

        forever {
            status = ArchiveEntryPaths::callWithSystemLocale<int>(archive_read_next_header, reader.get(), &entry);
            if (status == ARCHIVE_EOF)
                break;
            if (status != ARCHIVE_OK) {
                throw Error(tr("Cannot read entry header: %1")
                    .arg(errorStringWithCode(reader.get())));
            }

            ArchiveEntry archiveEntry;
            archiveEntry.path = ArchiveEntryPaths::callWithSystemLocale<QString>(ArchiveEntryPaths::pathname, entry);
            archiveEntry.utcTime = QDateTime::fromSecsSinceEpoch(archive_entry_mtime(entry), Qt::UTC);
            archiveEntry.isDirectory = (archive_entry_filetype(entry) == AE_IFDIR);
            archiveEntry.isSymbolicLink = (archive_entry_filetype(entry) == AE_IFLNK);
            archiveEntry.uncompressedSize = archive_entry_size(entry);
            archiveEntry.permissions_mode = archive_entry_perm(entry);

            entries.append(archiveEntry);
        }
    } catch (const Error &e) {
        setErrorString(e.message());
        m_data->file.seek(0);
        return QVector<ArchiveEntry>();
    }
    m_data->file.seek(0);
    return entries;
}

/*!
    \reimp

    Returns \c true if the current archive is of a supported format;
    \c false otherwise.
*/
bool LibArchiveArchive::isSupported()
{
    QScopedPointer<archive, ScopedPointerReaderDeleter> reader(archive_read_new());
    configureReader(reader.get());

    try {
        const int status = archiveReadOpenWithCallbacks(reader.get());
        if (status != ARCHIVE_OK) {
            throw Error(tr("Cannot open archive for reading: %1")
                .arg(errorStringWithCode(reader.get())));
        }
    } catch (const Error &e) {
        setErrorString(e.message());
        m_data->file.seek(0);
        return false;
    }
    m_data->file.seek(0);
    return true;
}

/*!
    Requests to extract the archive to \a dirPath with \a totalFiles
    in a separate thread with a worker object.
*/
void LibArchiveArchive::workerExtract(const QString &dirPath, const quint64 totalFiles)
{
    emit workerAboutToExtract(dirPath, totalFiles);
}

/*!
    Adds data to be read by the worker object in \a buffer.
*/
void LibArchiveArchive::workerAddDataBlock(const QByteArray buffer)
{
    emit workerAboutToAddDataBlock(buffer);
}

/*!
    Signals the worker object that the client data is at end.
*/
void LibArchiveArchive::workerSetDataAtEnd()
{
    emit workerAboutToSetDataAtEnd();
}

/*!
    Signals the worker object that the client file position changed to \a pos.
*/
void LibArchiveArchive::workerSetFilePosition(qint64 pos)
{
    emit workerAboutToSetFilePosition(pos);
}

/*!
    Cancels the extract in progress for the worker object.
*/
void LibArchiveArchive::workerCancel()
{
    emit workerAboutToCancel();
}

/*!
    Returns the status of the worker object.
*/
ExtractWorker::Status LibArchiveArchive::workerStatus() const
{
    return m_worker.status();
}

/*!
    \reimp

    Cancels the extract in progress.
*/
void LibArchiveArchive::cancel()
{
    m_cancelScheduled = true;
}

/*!
    \internal
*/
void LibArchiveArchive::onWorkerFinished(const QString &errorString)
{
    setErrorString(errorString);
    emit workerFinished();
}

/*!
    \internal
*/
void LibArchiveArchive::configureReader(archive *archive)
{
    archive_read_support_filter_bzip2(archive);
    archive_read_support_filter_gzip(archive);
    archive_read_support_filter_xz(archive);

    archive_read_support_format_tar(archive);
    archive_read_support_format_zip(archive);
    archive_read_support_format_7zip(archive);
}

/*!
    \internal
*/
void LibArchiveArchive::configureWriter(archive *archive)
{
    const QString fileName = m_data->file.fileName();
    if (fileName.endsWith(QLatin1String(".qbsp"), Qt::CaseInsensitive)) {
        // The Qt board support package file extension is really a 7z.
        archive_write_set_format_7zip(archive);
    } else {
        archive_write_set_format_filter_by_ext(archive, fileName.toUtf8());
    }

    const QByteArray charset = "hdrcharset=UTF-8";
    // not checked as this is ignored on some archive formats like 7z
    archive_write_set_options(archive, charset);

    if (compressionLevel() == CompressionLevel::Normal)
        return;

    const QByteArray compression = "compression-level=" + QString::number(compressionLevel()).toLatin1();
    if (archive_write_set_options(archive, compression.constData())) { // not fatal
        qCWarning(QInstaller::lcInstallerInstallLog) << "Could not set option" << compression
            << "for archive" << m_data->file.fileName() << ":" << errorStringWithCode(archive);
    }
}

/*!
    \internal
*/
void LibArchiveArchive::configureDiskReader(archive *archive)
{
    archive_read_disk_set_standard_lookup(archive);
}

/*!
    \internal
*/
void LibArchiveArchive::configureDiskWriter(archive *archive)
{
    constexpr int flags = ARCHIVE_EXTRACT_TIME
        | ARCHIVE_EXTRACT_PERM
        | ARCHIVE_EXTRACT_ACL
        | ARCHIVE_EXTRACT_FFLAGS;

    archive_write_disk_set_options(archive, flags);
    archive_write_disk_set_standard_lookup(archive);
}

/*!
    \internal
*/
void LibArchiveArchive::initExtractWorker()
{
    m_worker.moveToThread(&m_workerThread);

    connect(this, &LibArchiveArchive::workerAboutToExtract, &m_worker, &ExtractWorker::extract);
    connect(this, &LibArchiveArchive::workerAboutToAddDataBlock, &m_worker, &ExtractWorker::addDataBlock);
    connect(this, &LibArchiveArchive::workerAboutToSetDataAtEnd, &m_worker, &ExtractWorker::dataAtEnd);
    connect(this, &LibArchiveArchive::workerAboutToSetFilePosition, &m_worker, &ExtractWorker::onFilePositionChanged);
    connect(this, &LibArchiveArchive::workerAboutToCancel, &m_worker, &ExtractWorker::cancel);

    connect(&m_worker, &ExtractWorker::dataBlockRequested, this, &LibArchiveArchive::dataBlockRequested);
    connect(&m_worker, &ExtractWorker::seekRequested, this, &LibArchiveArchive::seekRequested);
    connect(&m_worker, &ExtractWorker::finished, this, &LibArchiveArchive::onWorkerFinished);

    connect(&m_worker, &ExtractWorker::currentEntryChanged, this, &LibArchiveArchive::currentEntryChanged);
    connect(&m_worker, &ExtractWorker::completedChanged, this, &LibArchiveArchive::completedChanged);

    m_workerThread.start();
}

/*!
    \internal

    Sets default callbacks for archive \a reader and opens for reading.
*/
int LibArchiveArchive::archiveReadOpenWithCallbacks(archive *reader)
{
    archive_read_set_read_callback(reader, readCallback);
    archive_read_set_callback_data(reader, m_data);
    archive_read_set_seek_callback(reader, seekCallback);

    return archive_read_open1(reader);
}

/*!
    Writes the current \a entry header, then pulls data from the archive \a reader
    and writes it to the \a writer handle.
*/
bool LibArchiveArchive::writeEntry(archive *reader, archive *writer, archive_entry *entry)
{
    int status;
    const void *buff;
    size_t size;
    int64_t offset;

    const QString entryPath = ArchiveEntryPaths::callWithSystemLocale
        <QString>(ArchiveEntryPaths::pathname, entry);

    FileGuardLocker locker(entryPath, FileGuard::globalObject());

    status = archive_write_header(writer, entry);
    if (status != ARCHIVE_OK) {
        setErrorString(errorStringWithCode(writer));
        return false;
    }

    forever {
        status = archive_read_data_block(reader, &buff, &size, &offset);
        if (status == ARCHIVE_EOF) {
            status = archive_write_finish_entry(writer);
            if (status == ARCHIVE_OK)
                return true;
        }
        if (status != ARCHIVE_OK) {
            setErrorString(errorStringWithCode(reader));
            return false;
        }
        status = archive_write_data_block(writer, buff, size, offset);
        if (status != ARCHIVE_OK) {
            setErrorString(errorStringWithCode(writer));
            return false;
        }
    }
}

/*!
    \internal

    Reads \a data from the current position of \a file. The maximum bytes to
    read are specified by \a maxSize. Returns the amount of bytes read.
*/
qint64 LibArchiveArchive::readData(QFile *file, char *data, qint64 maxSize)
{
    if (!file->isOpen() || file->isSequential())
        return ARCHIVE_FATAL;

    if (file->atEnd() && file->seek(0))
        return ARCHIVE_OK;

    const qint64 bytesRead = file->read(data, maxSize);
    if (bytesRead == -1)
        return ARCHIVE_FATAL;

    return bytesRead;
}

/*!
    \internal

    Called by libarchive when new data is needed. Reads data from the file device
    in \a archiveData into the buffer referenced by \a buff. Returns the number of bytes read.
*/
ssize_t LibArchiveArchive::readCallback(archive *reader, void *archiveData, const void **buff)
{
    Q_UNUSED(reader)
    constexpr qint64 blockSize = 1024 * 1024; // 1MB

    ArchiveData *data;
    if (!(data = static_cast<ArchiveData *>(archiveData)))
        return ARCHIVE_FATAL;

    if (!data->buffer.isEmpty())
        data->buffer.clear();

    if (data->buffer.size() != blockSize)
        data->buffer.resize(blockSize);

    if (!(*buff = static_cast<const void *>(data->buffer.constData())))
        return ARCHIVE_FATAL;

    // Doesn't matter if the buffer size exceeds the actual data read,
    // the return value indicates the length of relevant bytes.
    return readData(&data->file, data->buffer.data(), data->buffer.size());
}

/*!
    \internal

    Seeks to specified \a offset in the file device in \a archiveData and returns the position.
    Possible \a whence values are \c SEEK_SET, \c SEEK_CUR, and \c SEEK_END. Returns
    \c ARCHIVE_FATAL if the seek fails.
*/
la_int64_t LibArchiveArchive::seekCallback(archive *reader, void *archiveData, la_int64_t offset, int whence)
{
    Q_UNUSED(reader)

    ArchiveData *data;
    if (!(data = static_cast<ArchiveData *>(archiveData)))
        return ARCHIVE_FATAL;

    if (!data->file.isOpen() || data->file.isSequential())
        return ARCHIVE_FATAL;

    switch (whence) {
    case SEEK_SET: // moves file pointer position to the beginning of the file
        if (!data->file.seek(offset))
            return ARCHIVE_FATAL;
        break;
    case SEEK_CUR: // moves file pointer position to given location
        if (!data->file.seek(data->file.pos() + offset))
            return ARCHIVE_FATAL;
        break;
    case SEEK_END: // moves file pointer position to the end of file
        if (!data->file.seek(data->file.size() + offset))
            return ARCHIVE_FATAL;
        break;
    default:
        return ARCHIVE_FATAL;
    }
    return data->file.pos();
}

/*!
    Returns the \a path to a file or directory, without the Win32 namespace prefix.
    On Unix platforms, the \a path is returned unaltered.
*/
QString LibArchiveArchive::pathWithoutNamespace(const QString &path)
{
    QString aPath = path;
#ifdef Q_OS_WIN
    if (aPath.size() > 4 && aPath.at(0) == QLatin1Char('\\')
            && aPath.at(2) == QLatin1Char('?') && aPath.at(3) == QLatin1Char('\\')) {
        aPath = aPath.mid(4);
    }
#endif
    return aPath;
}

/*!
    Returns an error message and a textual representaion of the numeric error code
    indicating the reason for the most recent error return for the \a archive object.
*/
QString LibArchiveArchive::errorStringWithCode(archive *const archive)
{
    if (!archive)
        return QString();

    return QString::fromLatin1("%1: %2.").arg(
        QLatin1String(archive_error_string(archive)),
        QLatin1String(strerror(archive_errno(archive)))
    );
}

/*!
    Returns the number of files in this archive.
*/
quint64 LibArchiveArchive::totalFiles()
{
    QScopedPointer<archive, ScopedPointerReaderDeleter> reader(archive_read_new());
    archive_entry *entry = nullptr;
    quint64 files = 0;

    configureReader(reader.get());

    try {
        int status = archiveReadOpenWithCallbacks(reader.get());
        if (status != ARCHIVE_OK) {
            throw Error(tr("Cannot open archive for reading: %1")
                .arg(errorStringWithCode(reader.get())));
        }

        forever {
            status = ArchiveEntryPaths::callWithSystemLocale<int>(archive_read_next_header, reader.get(), &entry);
            if (status == ARCHIVE_EOF)
                break;
            if (status != ARCHIVE_OK) {
                throw Error(tr("Cannot read entry header: %1")
                    .arg(errorStringWithCode(reader.get())));
            }

            ++files;
        }
    } catch (const Error &e) {
        setErrorString(e.message());
        m_data->file.seek(0);
        return 0;
    }
    m_data->file.seek(0);
    return files;
}

} // namespace QInstaller
