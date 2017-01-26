/**************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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
#include "lib7z_facade.h"

#include "errors.h"
#include "fileio.h"

#ifndef Q_OS_WIN
#   include "StdAfx.h"
#endif

#include "Common/MyInitGuid.h"

#include "7zip/Archive/IArchive.h"
#include "7zip/UI/Common/OpenArchive.h"
#include "7zip/UI/Common/Update.h"

#include "Windows/FileIO.h"
#include "Windows/PropVariant.h"
#include "Windows/PropVariantConversions.h"

#include <QDir>
#include <QFileInfo>
#include <QIODevice>
#include <QtCore/QMutexLocker>
#include <QPointer>
#include <QTemporaryFile>
#include <QReadWriteLock>

#ifdef _MSC_VER
#pragma warning(disable:4297)
#endif

#ifdef Q_OS_WIN

#include <time.h>
#define FILE_ATTRIBUTE_UNIX_EXTENSION   0x8000   /* trick for Unix */
#define S_IFMT  00170000
#define S_IFLNK 0120000
#define S_ISLNK(m) (((m) & S_IFMT) == S_IFLNK)

typedef BOOL (WINAPI *CREATEHARDLINK)(LPCSTR dst, LPCSTR str, LPSECURITY_ATTRIBUTES sa);

bool CreateHardLinkWrapper(const QString &dest, const QString &file)
{
    static HMODULE module = 0;
    static CREATEHARDLINK proc = 0;

    if (module == 0)
        module = LoadLibrary(L"kernel32.dll");
    if (module == 0)
        return false;
    if (proc == 0)
        proc = (CREATEHARDLINK) GetProcAddress(module, "CreateHardLinkA");
    if (proc == 0)
        return false;
    QString target = file;
    if (!QFileInfo(file).isAbsolute())
    {
        target = QFileInfo(dest).dir().absoluteFilePath(file);
    }
    const QString link = QDir::toNativeSeparators(dest);
    const QString existingFile = QDir::toNativeSeparators(target);
    return proc(link.toLocal8Bit(), existingFile.toLocal8Bit(), 0);
}

#else
#include <sys/stat.h>
#endif

#include <memory>

#include <cassert>

using namespace Lib7z;
using namespace NWindows;


namespace Lib7z {
    Q_GLOBAL_STATIC(QReadWriteLock, lastErrorReadWriteLock)
    Q_GLOBAL_STATIC(QString, getLastErrorString)

    QString lastError()
    {
        QReadLocker locker(lastErrorReadWriteLock());
        Q_UNUSED(locker)
        return *getLastErrorString();
    }

    void setLastError(const QString &errorString)
    {
        QWriteLocker locker(lastErrorReadWriteLock());
        Q_UNUSED(locker)
        *getLastErrorString() = errorString;
    }
}

namespace {
/**
* RAII class to create a directory (tryCreate()) and delete it on destruction unless released.
*/
struct DirectoryGuard {
    explicit DirectoryGuard(const QString &path)
        : m_path(path)
        , m_created(false)
        , m_released(false)
    {
        m_path.replace(QLatin1Char('\\'), QLatin1Char('/'));
    }

    ~DirectoryGuard()
    {
        if (!m_created || m_released)
            return;
        QDir dir(m_path);
        if (!dir.rmdir(m_path))
            qWarning() << "Could not delete directory " << m_path;
    }

    /**
    * Tries to create the directorie structure.
    * Returns a list of every directory created.
    */
    QStringList tryCreate() {
        if (m_path.isEmpty())
            return QStringList();

        const QFileInfo fi(m_path);
        if (fi.exists() && fi.isDir())
            return QStringList();
        if (fi.exists() && !fi.isDir()) {
            throw SevenZipException(QCoreApplication::translate("DirectoryGuard",
                "Path exists but is not a folder: %1").arg(m_path));
        }
        QStringList created;

        QDir toCreate(m_path);
        while (!toCreate.exists())
        {
            QString p = toCreate.absolutePath();
            created.push_front(p);
            p = p.section(QLatin1Char('/'), 0, -2);
            toCreate = QDir(p);
        }

        QDir dir(m_path);
        m_created = dir.mkpath(m_path);
        if (!m_created) {
            throw SevenZipException(QCoreApplication::translate("DirectoryGuard",
                "Could not create folder: %1").arg(m_path));
        }
        return created;
    }

    void release() {
        m_released = true;
    }

    QString m_path;
    bool m_created;
    bool m_released;
};
}

static UString QString2UString(const QString &str)
{
    return str.toStdWString().c_str();
}

static QString UString2QString(const UString& str)
{
    return QString::fromStdWString(static_cast<const wchar_t*>(str));
}

static QString generateTempFileName()
{
    QTemporaryFile tmp;
    if (!tmp.open()) {
        throw SevenZipException(QCoreApplication::translate("QInstaller",
            "Could not create temporary file"));
    }
    return QDir::toNativeSeparators(tmp.fileName());
}

/*
static QStringList UStringVector2QStringList(const UStringVector& vec)
{
QStringList res;
for (int i = 0; i < vec.Size(); ++i)
res += UString2QString(vec[i]);
return res;
}
*/

static NCOM::CPropVariant readProperty(IInArchive* archive, int index, int propId)
{
    NCOM::CPropVariant prop;
    if (archive->GetProperty(index, propId, &prop) != S_OK) {
        throw SevenZipException(QCoreApplication::translate("QInstaller",
            "Could not retrieve property %1 for item %2").arg(QString::number(propId),
            QString::number(index)));
    }
    return prop;
}

static bool IsFileTimeZero(const FILETIME *lpFileTime)
{
    return (lpFileTime->dwLowDateTime == 0) && (lpFileTime->dwHighDateTime == 0);
}

static bool IsDST(const QDateTime& datetime = QDateTime())
{
    const time_t seconds = static_cast< time_t >(datetime.isValid() ? datetime.toTime_t()
        : QDateTime::currentDateTime().toTime_t());
#if defined(Q_OS_WIN) && !defined(Q_CC_MINGW)
    struct tm t;
    localtime_s(&t, &seconds);
#else
    const struct tm &t = *localtime(&seconds);
#endif
    return t.tm_isdst;
}

static bool getFileTimeFromProperty(IInArchive* archive, int index, int propId, FILETIME *fileTime)
{
    const NCOM::CPropVariant prop = readProperty(archive, index, propId);
    if (prop.vt != VT_FILETIME) {
        throw SevenZipException(QCoreApplication::translate("QInstaller",
            "Property %1 for item %2 not of type VT_FILETIME but %3").arg(QString::number(propId),
            QString::number(index), QString::number(prop.vt)));
    }
    *fileTime = prop.filetime;

    if (IsFileTimeZero(fileTime))
        return false;
    return true;
}

static
QDateTime getDateTimeProperty(IInArchive* archive, int index, int propId, const QDateTime& defaultValue)
{
    FILETIME fileTime;
    if (!getFileTimeFromProperty(archive, index, propId, &fileTime))
        return defaultValue;

    FILETIME localFileTime;
    if (!FileTimeToLocalFileTime(&fileTime, &localFileTime)) {
        throw SevenZipException(QCoreApplication::translate("QInstaller",
            "Could not convert file time to local time"));
    }
    SYSTEMTIME st;
    if (!BOOLToBool(FileTimeToSystemTime(&localFileTime, &st))) {
        throw SevenZipException(QCoreApplication::translate("QInstaller",
            "Could not convert local file time to system time"));
    }
    const QDate date(st.wYear, st.wMonth, st.wDay);
    const QTime time(st.wHour, st.wMinute, st.wSecond);
    QDateTime result(date, time);

    // fix daylight saving time
    const bool dst = IsDST();
    if (dst != IsDST(result))
        result = result.addSecs(dst ? -3600 : 3600);

    return result;
}

static quint64 getUInt64Property(IInArchive* archive, int index, int propId, quint64 defaultValue=0)
{
    const NCOM::CPropVariant prop = readProperty(archive, index, propId);
    if (prop.vt == VT_EMPTY)
        return defaultValue;
    return static_cast<quint64>(ConvertPropVariantToUInt64(prop));
}

static quint32 getUInt32Property(IInArchive* archive, int index, int propId, quint32 defaultValue=0)
{
    const NCOM::CPropVariant prop = readProperty(archive, index, propId);
    if (prop.vt == VT_EMPTY)
        return defaultValue;
    return static_cast< quint32 >(prop.ulVal);
}

static QFile::Permissions getPermissions(IInArchive* archive, int index, bool* hasPermissions = 0)
{
    quint32 attributes = getUInt32Property(archive, index, kpidAttrib, 0);
    QFile::Permissions permissions = 0;
    if (attributes & FILE_ATTRIBUTE_UNIX_EXTENSION) {
        if (hasPermissions != 0)
            *hasPermissions = true;
        // filter the unix permissions
        attributes = (attributes >> 16) & 0777;
        permissions |= static_cast< QFile::Permissions >((attributes & 0700) << 2);  // owner rights
        permissions |= static_cast< QFile::Permissions >((attributes & 0070) << 1);  // group
        permissions |= static_cast< QFile::Permissions >((attributes & 0007) << 0);  // and world rights
    } else if (hasPermissions != 0) {
        *hasPermissions = false;
    }
    return permissions;
}

namespace Lib7z {
class QIODeviceSequentialOutStream : public ISequentialOutStream, public CMyUnknownImp
{
public:
    enum Behavior {
        KeepDeviceUntouched,
        CloseAndDeleteDevice
    };

    MY_UNKNOWN_IMP
    explicit QIODeviceSequentialOutStream(QIODevice* device, Behavior behavior);
    ~QIODeviceSequentialOutStream();
    QString errorString() const;

    /* reimp */ STDMETHOD(Write)(const void* data, UInt32 size, UInt32* processedSize);

private:
    Behavior m_behavior;
    QString m_errorString;
    QPointer<QIODevice> m_device;
};

QIODeviceSequentialOutStream::QIODeviceSequentialOutStream(QIODevice* device, Behavior behavior)
    : ISequentialOutStream()
    , CMyUnknownImp()
    , m_behavior(behavior)
    , m_device(device)
{
    Q_ASSERT(m_device);

    if (!device->isOpen() && !m_device->open(QIODevice::WriteOnly))
        m_errorString = m_device->errorString();
}

QIODeviceSequentialOutStream::~QIODeviceSequentialOutStream()
{
    if (m_behavior == CloseAndDeleteDevice) {
        m_device->close();
        delete m_device;
        m_device = 0;
    }
}

QString QIODeviceSequentialOutStream::errorString() const
{
    return m_errorString;
}

HRESULT QIODeviceSequentialOutStream::Write(const void* data, UInt32 size, UInt32* processedSize)
{
    if (!m_device) {
        if (processedSize)
            *processedSize = 0;
        m_errorString = QCoreApplication::translate("QIODeviceSequentialOutStream",
            "No device set for output stream");
        return E_FAIL;
    }
    if (!m_device->isOpen()) {
        const bool opened = m_device->open(QIODevice::WriteOnly);
        if (!opened) {
            if (processedSize)
                *processedSize = 0;
            m_errorString = m_device->errorString();
            return E_FAIL;
        }
    }

    const qint64 written = m_device->write(reinterpret_cast<const char*>(data), size);
    if (written == -1) {
        if (processedSize)
            *processedSize = 0;
        m_errorString = m_device->errorString();
        return E_FAIL;
    }

    if (processedSize)
        *processedSize = written;
    m_errorString = QString();
    return S_OK;
}

class QIODeviceInStream : public IInStream, public CMyUnknownImp
{
public:
    MY_UNKNOWN_IMP

    explicit QIODeviceInStream(QIODevice* device)  : IInStream(), CMyUnknownImp(), m_device(device)
    {
        assert(m_device);
        assert(!m_device->isSequential());
    }

    /* reimp */ STDMETHOD(Read)(void* data, UInt32 size, UInt32* processedSize)
    {
        assert(m_device);
        assert(m_device->isReadable());
        const qint64 actual = m_device->read(reinterpret_cast<char*>(data), size);
        Q_ASSERT(actual != 0 || m_device->atEnd());
        if (processedSize)
            *processedSize = actual;
        return actual >= 0 ? S_OK : E_FAIL;
    }

    /* reimp */ STDMETHOD(Seek)(Int64 offset, UInt32 seekOrigin, UInt64* newPosition)
    {
        assert(m_device);
        assert(!m_device->isSequential());
        assert(m_device->isReadable());
        if (seekOrigin > STREAM_SEEK_END)
            return STG_E_INVALIDFUNCTION;
        UInt64 np = 0;
        switch(seekOrigin) {
        case STREAM_SEEK_SET:
            np = offset;
            break;
        case STREAM_SEEK_CUR:
            np = m_device->pos() + offset;
            break;
        case STREAM_SEEK_END:
            np = m_device->size() + offset;
            break;
        default:
            return STG_E_INVALIDFUNCTION;
        }

        np = qBound(static_cast<UInt64>(0), np, static_cast<UInt64>(m_device->size() - 1));
        const bool ok = m_device->seek(np);
        if (newPosition)
            *newPosition = np;
        return ok ? S_OK : E_FAIL;
    }

private:
    QPointer<QIODevice> m_device;
};
}

File::File()
    : permissions(0)
    , uncompressedSize(0)
    , compressedSize(0)
    , isDirectory(false)
{
}

QVector<File> File::subtreeInPreorder() const
{
    QVector<File> res;
    res += *this;
    Q_FOREACH (const File& child, children)
        res += child.subtreeInPreorder();
    return res;
}

bool File::operator<(const File& other) const
{
    if (path != other.path)
        return path < other.path;
    if (mtime != other.mtime)
        return mtime < other.mtime;
    if (uncompressedSize != other.uncompressedSize)
        return uncompressedSize < other.uncompressedSize;
    if (compressedSize != other.compressedSize)
        return compressedSize < other.compressedSize;
    if (isDirectory != other.isDirectory)
        return !isDirectory;
    if (permissions != other.permissions)
        return permissions < other.permissions;
    return false;
}

bool File::operator==(const File& other) const
{
    return mtime == other.mtime
        && path == other.path
        && uncompressedSize == other.uncompressedSize
        && compressedSize == other.compressedSize
        && isDirectory == other.isDirectory
        && children == other.children
        && (permissions == other.permissions
        || permissions == static_cast< QFile::Permissions >(-1)
        || other.permissions == static_cast< QFile::Permissions >(-1));
}

QByteArray Lib7z::formatKeyValuePairs(const QVariantList& l)
{
    assert(l.size() % 2 == 0);
    QByteArray res;
    for (QVariantList::ConstIterator it = l.constBegin(); it != l.constEnd(); ++it) {
        if (!res.isEmpty())
            res += ", ";
        res += qPrintable(it->toString()) + QByteArray(" = ");
        ++it;
        res += qPrintable(it->toString());
    }
    return res;
}

class Job::Private
{
public:
    Private()
        : error(Lib7z::NoError)
    {}

    int error;
    QString errorString;
};

Job::Job(QObject* parent)
    : QObject(parent)
    , QRunnable()
    , d(new Private)
{
}

Job::~Job()
{
    delete d;
}

void Job::emitResult()
{
    emit finished(this);
}

void Job::setError(int code)
{
    d->error = code;
}

void Job::setErrorString(const QString &str)
{
    d->errorString = str;
}

void Job::emitProgress(qint64 completed, qint64 total)
{
    emit progress(completed, total);
}

int Job::error() const
{
    return d->error;
}

bool Job::hasError() const
{
    return d->error != NoError;
}

void Job::run()
{
    doStart();
}

QString Job::errorString() const
{
    return d->errorString;
}

void Job::start()
{
    QMetaObject::invokeMethod(this, "doStart", Qt::QueuedConnection);
}

class ListArchiveJob::Private
{
public:
    QPointer<QFileDevice> archive;
    QVector<File> files;
};

ListArchiveJob::ListArchiveJob(QObject* parent)
    : Job(parent)
    , d(new Private)
{
}

ListArchiveJob::~ListArchiveJob()
{
    delete d;
}

QFileDevice* ListArchiveJob::archive() const
{
    return d->archive;
}

void ListArchiveJob::setArchive(QFileDevice* device)
{
    d->archive = device;
}

QVector<File> ListArchiveJob::index() const
{
    return d->files;
}

class OpenArchiveInfo
{
private:
    OpenArchiveInfo(QFileDevice* device)
        : codecs(new CCodecs)
    {
        if (codecs->Load() != S_OK) {
            throw SevenZipException(QCoreApplication::translate("OpenArchiveInfo",
                "Could not load codecs"));
        }
        if (!codecs->FindFormatForArchiveType(L"", formatIndices)) {
            throw SevenZipException(QCoreApplication::translate("OpenArchiveInfo",
                "Could not retrieve default format"));
        }
        stream = new QIODeviceInStream(device);
        if (archiveLink.Open2(codecs.data(), formatIndices, false, stream, UString(), 0) != S_OK) {
            throw SevenZipException(QCoreApplication::translate("OpenArchiveInfo",
                "Could not open archive"));
        }
        if (archiveLink.Arcs.Size() == 0)
            throw SevenZipException(QCoreApplication::translate("OpenArchiveInfo", "No CArc found"));

        m_cleaner = new OpenArchiveInfoCleaner();
        m_cleaner->moveToThread(device->thread());
        QObject::connect(device, SIGNAL(destroyed(QObject*)), m_cleaner, SLOT(deviceDestroyed(QObject*)));
    }

public:
    ~OpenArchiveInfo()
    {
        m_cleaner->deleteLater();
    }

    static OpenArchiveInfo* value(QFileDevice* device)
    {
        QMutexLocker _(&m_mutex);
        if (!instances.contains(device))
            instances.insert(device, new OpenArchiveInfo(device));
        return instances.value(device);
    }

    static OpenArchiveInfo* take(QFileDevice *device)
    {
        QMutexLocker _(&m_mutex);
        if (instances.contains(device))
            return instances.take(device);
        return 0;
    }

    CArchiveLink archiveLink;

private:
    CIntVector formatIndices;
    CMyComPtr<IInStream> stream;
    QScopedPointer<CCodecs> codecs;
    OpenArchiveInfoCleaner *m_cleaner;

    static QMutex m_mutex;
    static QHash< QIODevice*, OpenArchiveInfo* > instances;
};

QMutex OpenArchiveInfo::m_mutex;
QHash< QIODevice*, OpenArchiveInfo* > OpenArchiveInfo::instances;

void OpenArchiveInfoCleaner::deviceDestroyed(QObject* dev)
{
    QFileDevice* device = static_cast<QFileDevice*>(dev);
    Q_ASSERT(device);
    delete OpenArchiveInfo::take(device);
}

QVector<File> Lib7z::listArchive(QFileDevice* archive)
{
    assert(archive);
    try {
        const OpenArchiveInfo* const openArchive = OpenArchiveInfo::value(archive);

        QVector<File> flat;

        for (int i = 0; i < openArchive->archiveLink.Arcs.Size(); ++i) {
            const CArc& arc = openArchive->archiveLink.Arcs[i];
            IInArchive* const arch = arc.Archive;

            UInt32 numItems = 0;
            if (arch->GetNumberOfItems(&numItems) != S_OK) {
                throw SevenZipException(QCoreApplication::translate("Lib7z",
                    "Could not retrieve number of items in archive"));
            }
            flat.reserve(flat.size() + numItems);
            for (uint item = 0; item < numItems; ++item) {
                UString s;
                if (arc.GetItemPath(item, s) != S_OK) {
                    throw SevenZipException(QCoreApplication::translate("Lib7z",
                        "Could not retrieve path of archive item %1").arg(item));
                }
                File f;
                f.archiveIndex.setX(i);
                f.archiveIndex.setY(item);
                f.path = UString2QString(s).replace(QLatin1Char('\\'), QLatin1Char('/'));
                IsArchiveItemFolder(arch, item, f.isDirectory);
                f.permissions = getPermissions(arch, item);
                f.mtime = getDateTimeProperty(arch, item, kpidMTime, QDateTime());
                f.uncompressedSize = getUInt64Property(arch, item, kpidSize, 0);
                f.compressedSize = getUInt64Property(arch, item, kpidPackSize, 0);
                flat.push_back(f);
                qApp->processEvents();
            }
        }
        return flat;
    } catch (const SevenZipException& e) {
        throw e;
    } catch (const char *err) {
        throw SevenZipException(err);
    } catch (...) {
        throw SevenZipException(QCoreApplication::translate("Lib7z",
            "Unknown exception caught (%1)").arg(QString::fromLatin1(Q_FUNC_INFO)));
    }
    return QVector<File>(); // never reached
}

void ListArchiveJob::doStart()
{
    try {
        if (!d->archive)
            throw SevenZipException(tr("Could not list archive: QIODevice already destroyed."));
        d->files = listArchive(d->archive);
    } catch (const SevenZipException& e) {
        setError(Failed);
        setErrorString(e.message());
    } catch (...) {
        setError(Failed);
        setErrorString(tr("Unknown exception caught (%1)").arg(tr("Failed")));
    }
    emitResult();
}

class Lib7z::ExtractCallbackImpl : public IArchiveExtractCallback, public CMyUnknownImp
{
public:
    MY_UNKNOWN_IMP
        explicit ExtractCallbackImpl(ExtractCallback* qq)
        : q(qq)
        , currentIndex(0)
        , arc(0)
        , total(0)
        , completed(0)
        , device(0)
    {
    }

    void setTarget(QIODevice* dev)
    {
        device = dev;
    }

    void setTarget(const QString &targetDirectory)
    {
        targetDir = targetDirectory;
    }

    // this method will be called by CFolderOutStream::OpenFile to stream via
    // CDecoder::CodeSpec extracted content to an output stream.
    /* reimp */ STDMETHOD(GetStream)(UInt32 index, ISequentialOutStream** outStream, Int32 askExtractMode)
    {
        Q_UNUSED(askExtractMode)
        *outStream = 0;
        if (device != 0) {
            QIODeviceSequentialOutStream *qOutStream = new QIODeviceSequentialOutStream(device,
                QIODeviceSequentialOutStream::KeepDeviceUntouched);
            if (!qOutStream->errorString().isEmpty()) {
                Lib7z::setLastError(qOutStream->errorString());
                return E_FAIL;
            }
            CMyComPtr<ISequentialOutStream> stream = qOutStream;
            *outStream = stream.Detach();
            return S_OK;
        } else if (!targetDir.isEmpty()) {
            assert(arc);

            currentIndex = index;

            UString s;
            if (arc->GetItemPath(index, s) != S_OK) {
                Lib7z::setLastError(QCoreApplication::translate("ExtractCallbackImpl",
                    "Could not retrieve path of archive item %1").arg(index));
                return E_FAIL;
            }

            const QString path = UString2QString(s).replace(QLatin1Char('\\'), QLatin1Char('/'));
            const QFileInfo fi(QString::fromLatin1("%1/%2").arg(targetDir, path));

            DirectoryGuard guard(fi.absolutePath());
            const QStringList directories = guard.tryCreate();

            bool isDir = false;
            IsArchiveItemFolder(arc->Archive, index, isDir);
            if (isDir)
                QDir(fi.absolutePath()).mkdir(fi.fileName());

            // this makes sure that all directories created get removed as well
            foreach (const QString &directory, directories)
                q->setCurrentFile(directory);

            if (!isDir && !q->prepareForFile(fi.absoluteFilePath()))
                return E_FAIL;

            q->setCurrentFile(fi.absoluteFilePath());

            if (!isDir) {
#ifndef Q_OS_WIN
                // do not follow symlinks, so we need to remove an existing one
                if (fi.isSymLink() && (!QFile::remove(fi.absoluteFilePath()))) {
                    Lib7z::setLastError(QCoreApplication::translate("ExtractCallbackImpl",
                        "Could not remove already existing symlink. %1").arg(fi.absoluteFilePath()));
                    return E_FAIL;
                }
#endif
                QIODeviceSequentialOutStream *qOutStream = new QIODeviceSequentialOutStream(
                    new QFile(fi.absoluteFilePath()), QIODeviceSequentialOutStream::CloseAndDeleteDevice);
                if (!qOutStream->errorString().isEmpty()) {
                    Lib7z::setLastError(QCoreApplication::translate("ExtractCallbackImpl",
                        "Could not open file: %1 (%2)").arg(fi.absoluteFilePath(),
                        qOutStream->errorString()));
                    return E_FAIL;
                }
                CMyComPtr<ISequentialOutStream> stream = qOutStream;
                *outStream = stream;
                stream.Detach();
            }

            guard.release();
            return S_OK;
        }
        return E_FAIL;
    }

    /* reimp */ STDMETHOD(PrepareOperation)(Int32 askExtractMode)
    {
        Q_UNUSED(askExtractMode)
        return S_OK;
    }

    /* reimp */ STDMETHOD(SetOperationResult)(Int32 resultEOperationResult)
    {
        Q_UNUSED(resultEOperationResult)

        if (!targetDir.isEmpty()) {
            bool hasPerm = false;
            const QFile::Permissions permissions = getPermissions(arc->Archive, currentIndex, &hasPerm);

            UString s;
            if (arc->GetItemPath(currentIndex, s) != S_OK) {
                Lib7z::setLastError(QCoreApplication::translate("ExtractCallbackImpl",
                    "Could not retrieve path of archive item %1").arg(currentIndex));
                return E_FAIL;
            }
            const QString path = UString2QString(s).replace(QLatin1Char('\\'), QLatin1Char('/'));
            const QString absFilePath = QFileInfo(QString::fromLatin1("%1/%2").arg(targetDir, path))
                .absoluteFilePath();

            // do we have a symlink?
            const quint32 attributes = getUInt32Property(arc->Archive, currentIndex, kpidAttrib, 0);
            struct stat stat_info;
            stat_info.st_mode = attributes >> 16;
            if (S_ISLNK(stat_info.st_mode)) {
#ifdef Q_OS_WIN
                qFatal(QString::fromLatin1("Creating a link from archive is not implemented for windows. "
                    "Link filename: %1").arg(absFilePath).toLatin1());
                // TODO
//                if (!CreateHardLinkWrapper(absFilePath, QLatin1String(symlinkTarget))) {
//                    return S_FALSE;
//                }
#else
                QFileInfo symlinkPlaceHolderFileInfo(absFilePath);
                if (symlinkPlaceHolderFileInfo.isSymLink()) {
                    Lib7z::setLastError(QCoreApplication::translate("ExtractCallbackImpl",
                        "Could not create symlink at '%1'. Another one is already existing.")
                        .arg(absFilePath));
                    return E_FAIL;
                }
                QFile symlinkPlaceHolderFile(absFilePath);
                if (!symlinkPlaceHolderFile.open(QIODevice::ReadOnly)) {
                    Lib7z::setLastError(QCoreApplication::translate("ExtractCallbackImpl",
                        "Could not read symlink target from file '%1'.").arg(absFilePath));
                    return E_FAIL;
                }

                const QByteArray symlinkTarget = symlinkPlaceHolderFile.readAll();
                symlinkPlaceHolderFile.close();
                symlinkPlaceHolderFile.remove();
                QFile targetFile(QString::fromLatin1(symlinkTarget));
                if (!targetFile.link(absFilePath)) {
                    Lib7z::setLastError(QCoreApplication::translate("ExtractCallbackImpl",
                        "Could not create symlink at %1. %2").arg(absFilePath,
                        targetFile.errorString()));
                    return E_FAIL;
                }
                return S_OK;
#endif
            }

            try {
                if (!absFilePath.isEmpty()) {
                    // This might fail for archives without all properties, we can only be sure about
                    // modification time, as it's always stored by default in 7z archives. Also note that
                    // we restore modification time on Unix only, as access time and change time are
                    // supposed to be set to the time of installation.
                    FILETIME mTime;
                    if (getFileTimeFromProperty(arc->Archive, currentIndex, kpidMTime, &mTime)) {
                        NWindows::NFile::NIO::COutFile file;
                        if (file.Open(QString2UString(absFilePath), 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL))
                            file.SetTime(&mTime, &mTime, &mTime);
                    }
#ifdef Q_OS_WIN
                    FILETIME cTime, aTime;
                    bool success = getFileTimeFromProperty(arc->Archive, currentIndex, kpidCTime, &cTime);
                    if (success && getFileTimeFromProperty(arc->Archive, currentIndex, kpidATime, &aTime)) {
                        NWindows::NFile::NIO::COutFile file;
                        if (file.Open(QString2UString(absFilePath), 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL))
                            file.SetTime(&cTime, &aTime, &mTime);
                    }
#endif
                }
            } catch (...) {}

            if (hasPerm)
                QFile::setPermissions(absFilePath, permissions);
        }

        return S_OK;
    }

    /* reimp */ STDMETHOD(SetTotal)(UInt64 t)
    {
        total = t;
        return S_OK;
    }

    /* reimp */ STDMETHOD(SetCompleted)(const UInt64* c)
    {
        completed = *c;
        if (total > 0)
            return q->setCompleted(completed, total);
        return S_OK;
    }

    void setArchive(const CArc* archive)
    {
        arc = archive;
    }

private:
    ExtractCallback* const q;
    UInt32 currentIndex;
    const CArc* arc;
    UInt64 total;
    UInt64 completed;
    QPointer<QIODevice> device;
    QString targetDir;
};


class Lib7z::ExtractCallbackPrivate
{
public:
    explicit ExtractCallbackPrivate(ExtractCallback* qq)
    {
        impl = new ExtractCallbackImpl(qq);
    }

    CMyComPtr<ExtractCallbackImpl> impl;
};

ExtractCallback::ExtractCallback()
    : d(new ExtractCallbackPrivate(this))
{
}

ExtractCallback::~ExtractCallback()
{
    delete d;
}

ExtractCallbackImpl* ExtractCallback::impl()
{
    return d->impl;
}

const ExtractCallbackImpl* ExtractCallback::impl() const
{
    return d->impl;
}

void ExtractCallback::setTarget(QFileDevice* dev)
{
    d->impl->setTarget(dev);
}

void ExtractCallback::setTarget(const QString &dir)
{
    d->impl->setTarget(dir);
}

HRESULT ExtractCallback::setCompleted(quint64, quint64)
{
    return S_OK;
}

void ExtractCallback::setCurrentFile(const QString&)
{
}

bool ExtractCallback::prepareForFile(const QString&)
{
    return true;
}

class Lib7z::ExtractCallbackJobImpl : public ExtractCallback
{
public:
    explicit ExtractCallbackJobImpl(ExtractItemJob* j)
        : ExtractCallback()
        , job(j)
    {}

private:
    /* reimp */ HRESULT setCompleted(quint64 c, quint64 t)
    {
        emit job->progress(c, t);
        return S_OK;
    }

    ExtractItemJob* const job;
};

class Lib7z::UpdateCallbackImpl : public IUpdateCallbackUI2, public CMyUnknownImp
{
public:
    MY_UNKNOWN_IMP
    UpdateCallbackImpl()
    {
    }
    virtual ~UpdateCallbackImpl()
    {
    }
    /**
    * \reimp
    */
    HRESULT SetTotal(UInt64)
    {
        return S_OK;
    }
    /**
    * \reimp
    */
    HRESULT SetCompleted(const UInt64*)
    {
        return S_OK;
    }
    HRESULT SetRatioInfo(const UInt64*, const UInt64*)
    {
        return S_OK;
    }
    HRESULT CheckBreak()
    {
        return S_OK;
    }
    HRESULT Finilize()
    {
        return S_OK;
    }
    HRESULT SetNumFiles(UInt64)
    {
        return S_OK;
    }
    HRESULT GetStream(const wchar_t*, bool)
    {
        return S_OK;
    }
    HRESULT OpenFileError(const wchar_t*, DWORD)
    {
        return S_OK;
    }
    HRESULT CryptoGetTextPassword2(Int32* passwordIsDefined, OLECHAR** password)
    {
        *password = 0;
        *passwordIsDefined = false;
        return S_OK;
    }
    HRESULT CryptoGetTextPassword(OLECHAR**)
    {
        return E_NOTIMPL;
    }
    HRESULT OpenResult(const wchar_t*, LONG)
    {
        return S_OK;
    }
    HRESULT StartScanning()
    {
        return S_OK;
    }
    HRESULT ScanProgress(UInt64, UInt64, const wchar_t*)
    {
        return S_OK;
    }
    HRESULT CanNotFindError(const wchar_t*, DWORD)
    {
        return S_OK;
    }
    HRESULT FinishScanning()
    {
        return S_OK;
    }
    HRESULT StartArchive(const wchar_t*, bool)
    {
        return S_OK;
    }
    HRESULT FinishArchive()
    {
        return S_OK;
    }

    /**
    * \reimp
    */
    HRESULT SetOperationResult(Int32)
    {
        // TODO!
        return S_OK;
    }
    void setSourcePaths(const QStringList &paths)
    {
        sourcePaths = paths;
    }
    void setTarget(QIODevice* archive)
    {
        target = archive;
    }

private:
    QIODevice* target;
    QStringList sourcePaths;
};

class Lib7z::UpdateCallbackPrivate
{
public:
    UpdateCallbackPrivate()
    {
        m_impl = new UpdateCallbackImpl;
    }

    UpdateCallbackImpl* impl()
    {
        return m_impl;
    }

private:
    CMyComPtr< UpdateCallbackImpl > m_impl;
};

UpdateCallback::UpdateCallback()
    : d(new UpdateCallbackPrivate)
{
}

UpdateCallback::~UpdateCallback()
{
    delete d;
}

UpdateCallbackImpl* UpdateCallback::impl()
{
    return d->impl();
}

void UpdateCallback::setSourcePaths(const QStringList &paths)
{
    d->impl()->setSourcePaths(paths);
}

void UpdateCallback::setTarget(QFileDevice* target)
{
    d->impl()->setTarget(target);
}

class ExtractItemJob::Private
{
public:
    Private(ExtractItemJob* qq)
        : q(qq)
        , target(0)
        , callback(new ExtractCallbackJobImpl(q))
    {
    }

    ExtractItemJob* q;
    File item;
    QPointer<QFileDevice> archive;
    QString targetDirectory;
    QFileDevice* target;
    ExtractCallback* callback;
};

ExtractItemJob::ExtractItemJob(QObject* parent)
    : Job(parent)
    , d(new Private(this))
{
}

ExtractItemJob::~ExtractItemJob()
{
    delete d;
}

File ExtractItemJob::item() const
{
    return d->item;
}

void ExtractItemJob::setItem(const File& item)
{
    d->item = item;
}

QFileDevice* ExtractItemJob::archive() const
{
    return d->archive;
}

void ExtractItemJob::setArchive(QFileDevice* archive)
{
    d->archive = archive;
}

QString ExtractItemJob::targetDirectory() const
{
    return d->targetDirectory;
}

void ExtractItemJob::setTargetDirectory(const QString &dir)
{
    d->targetDirectory = dir;
    d->target = 0;
}

void ExtractItemJob::setTarget(QFileDevice* dev)
{
    d->target = dev;
}

namespace{
    QString errorMessageFrom7zResult(const LONG & extractResult)
    {
        if (!Lib7z::lastError().isEmpty())
            return Lib7z::lastError();

        QString errorMessage = QCoreApplication::translate("Lib7z", "internal code: %1");
        switch (extractResult) {
            case S_OK:
                qFatal("S_OK value is not a valid error code.");
            break;
            case E_NOTIMPL:
                errorMessage = errorMessage.arg(QLatin1String("E_NOTIMPL"));
            break;
            case E_NOINTERFACE:
                errorMessage = errorMessage.arg(QLatin1String("E_NOINTERFACE"));
            break;
            case E_ABORT:
                errorMessage = errorMessage.arg(QLatin1String("E_ABORT"));
            break;
            case E_FAIL:
                errorMessage = errorMessage.arg(QLatin1String("E_FAIL"));
            break;
            case STG_E_INVALIDFUNCTION:
                errorMessage = errorMessage.arg(QLatin1String("STG_E_INVALIDFUNCTION"));
            break;
            case E_OUTOFMEMORY:
                errorMessage = QCoreApplication::translate("Lib7z", "not enough memory");
            break;
            case E_INVALIDARG:
                errorMessage = errorMessage.arg(QLatin1String("E_INVALIDARG"));
            break;
            default:
                errorMessage = QCoreApplication::translate("Lib7z", "Error: %1").arg(extractResult);
            break;
        }
        return errorMessage;
    }
}

void Lib7z::createArchive(QFileDevice* archive, const QStringList &sourcePaths, UpdateCallback* callback)
{
    assert(archive);

    QScopedPointer<UpdateCallback> dummyCallback(callback ? 0 : new UpdateCallback);
    if (!callback)
        callback = dummyCallback.data();

    try {
        callback->setTarget(archive);

        QScopedPointer<CCodecs> codecs(new CCodecs);
        if (codecs->Load() != S_OK)
            throw SevenZipException(QCoreApplication::translate("Lib7z", "Could not load codecs"));

        CIntVector formatIndices;

        if (!codecs.data()->FindFormatForArchiveType(L"", formatIndices)) {
            throw SevenZipException(QCoreApplication::translate("Lib7z",
                "Could not retrieve default format"));
        }

        // yes this is crap, but there seems to be no streaming solution to this...

        const QString tempFile = generateTempFileName();

        NWildcard::CCensor censor;
        foreach (const QString &path, sourcePaths) {
            const QString cleanPath = QDir::toNativeSeparators(QDir::cleanPath(path));
            const UString nativePath = QString2UString(cleanPath);
            if (UString2QString(nativePath) != cleanPath) {
                throw SevenZipException(QCoreApplication::translate("Lib7z", "Could not convert"
                    " path: %1.").arg(path));
            }
            censor.AddItem(true /* always include item */, nativePath, false /* never recurse*/);
        }
        callback->setSourcePaths(sourcePaths);

        CArchivePath archivePath;
        archivePath.ParseFromPath(QString2UString(tempFile));

        CUpdateArchiveCommand command;
        command.ArchivePath = archivePath;
        command.ActionSet = NUpdateArchive::kAddActionSet;

        CUpdateOptions options;
        options.Commands.Add(command);
        options.ArchivePath = archivePath;
        options.MethodMode.FormatIndex = codecs->FindFormatForArchiveType(L"7z");

        // preserve creation time
        CProperty tc;
        tc.Name = UString(L"TC");
        tc.Value = UString(L"ON");
        options.MethodMode.Properties.Add(tc);

        // preserve access time
        CProperty ta;
        ta.Name = UString(L"TA");
        ta.Value = UString(L"ON");
        options.MethodMode.Properties.Add(ta);

        CUpdateErrorInfo errorInfo;
        const HRESULT res = UpdateArchive(codecs.data(), censor, options, errorInfo, 0, callback->impl());
        if (res != S_OK || !QFile::exists(tempFile)) {
            throw SevenZipException(QCoreApplication::translate("Lib7z",
                "Could not create archive %1. %2").arg(tempFile, errorMessageFrom7zResult(res)));
        }

        {
            //TODO remove temp file even if one the following throws
            QFile file(tempFile);
            QInstaller::openForRead(&file);
            QInstaller::blockingCopy(&file, archive, file.size());
        }

        QFile file(tempFile);
        if (!file.remove()) {
            qWarning("%s: Could not remove temporary file %s: %s", Q_FUNC_INFO, qPrintable(tempFile),
                qPrintable(file.errorString()));
        }
    } catch (const char *err) {
        qDebug() << err;
        throw SevenZipException(err);
    } catch (const QInstaller::Error &err) {
        throw SevenZipException(err.message());
    } catch (...) {
        throw SevenZipException(QCoreApplication::translate("Lib7z", "Unknown exception caught (%1)")
            .arg(QString::fromLatin1(Q_FUNC_INFO)));
    }
}

void Lib7z::extractFileFromArchive(QFileDevice* archive, const File& item, QFileDevice* target,
    ExtractCallback* callback)
{
    assert(archive);
    assert(target);

    std::auto_ptr<ExtractCallback> dummyCallback(callback ? 0 : new ExtractCallback);
    if (!callback)
        callback = dummyCallback.get();

    try {
        const OpenArchiveInfo* const openArchive = OpenArchiveInfo::value(archive);

        const int arcIdx = item.archiveIndex.x();
        if (arcIdx < 0 || arcIdx >= openArchive->archiveLink.Arcs.Size()) {
            throw SevenZipException(QCoreApplication::translate("Lib7z",
                "CArc index %1 out of bounds [0, %2]").arg(openArchive->archiveLink.Arcs.Size() - 1));
        }
        const CArc& arc = openArchive->archiveLink.Arcs[arcIdx];
        IInArchive* const parchive = arc.Archive;

        const UInt32 itemIdx = item.archiveIndex.y();
        UInt32 numItems = 0;
        if (parchive->GetNumberOfItems(&numItems) != S_OK) {
            throw SevenZipException(QCoreApplication::translate("Lib7z",
                "Could not retrieve number of items in archive"));
        }

        if (itemIdx >= numItems) {
            throw SevenZipException(QCoreApplication::translate("Lib7z",
                "Item index %1 out of bounds [0, %2]").arg(itemIdx).arg(numItems - 1));
        }

        UString s;
        if (arc.GetItemPath(itemIdx, s) != S_OK) {
            throw SevenZipException(QCoreApplication::translate("Lib7z",
                "Could not retrieve path of archive item %1").arg(itemIdx));
        }
        assert(item.path == UString2QString(s).replace(QLatin1Char('\\'), QLatin1Char('/')));

        callback->setTarget(target);
        const LONG extractResult = parchive->Extract(&itemIdx, 1, /*testmode=*/0, callback->impl());

        if (extractResult != S_OK)
            throw SevenZipException(errorMessageFrom7zResult(extractResult));

    } catch (const char *err) {
        throw SevenZipException(err);
    } catch (const Lib7z::SevenZipException& e) {
        throw e;
    } catch (...) {
        throw SevenZipException(QCoreApplication::translate("Lib7z", "Unknown exception caught (%1)")
            .arg(QString::fromLatin1(Q_FUNC_INFO)));
    }
}

void Lib7z::extractFileFromArchive(QFileDevice* archive, const File& item,
    const QString &targetDirectory, ExtractCallback* callback)
{
    assert(archive);

    QScopedPointer<ExtractCallback> dummyCallback(callback ? 0 : new ExtractCallback);
    if (!callback)
        callback = dummyCallback.data();

    QFileInfo fi(targetDirectory + QLatin1String("/") + item.path);
    DirectoryGuard outDir(fi.absolutePath());
    outDir.tryCreate();
    QFile out(fi.absoluteFilePath());
    if (!out.open(QIODevice::WriteOnly)) { //TODO use tmp file
        throw SevenZipException(QCoreApplication::translate("Lib7z",
            "Could not create output file for writing: %1").arg(fi.absoluteFilePath()));
    }
    callback->setTarget(&out);
    extractFileFromArchive(archive, item, &out, callback);
    if (item.permissions)
        out.setPermissions(item.permissions);
    outDir.release();
}

void Lib7z::extractArchive(QFileDevice* archive, const QString &targetDirectory,
    ExtractCallback* callback)
{
    assert(archive);

    QScopedPointer<ExtractCallback> dummyCallback(callback ? 0 : new ExtractCallback);
    if (!callback)
        callback = dummyCallback.data();

    callback->setTarget(targetDirectory);

    const QFileInfo fi(targetDirectory);
    DirectoryGuard outDir(fi.absolutePath());
    outDir.tryCreate();

    const OpenArchiveInfo* const openArchive = OpenArchiveInfo::value(archive);

    for (int a = 0; a < openArchive->archiveLink.Arcs.Size(); ++a)
    {
        const CArc& arc = openArchive->archiveLink.Arcs[a];
        IInArchive* const arch = arc.Archive;
        callback->impl()->setArchive(&arc);
        const LONG extractResult = arch->Extract(0, static_cast< UInt32 >(-1), false, callback->impl());

        if (extractResult != S_OK)
            throw SevenZipException(errorMessageFrom7zResult(extractResult));
    }

    outDir.release();
}

bool Lib7z::isSupportedArchive(const QString &archive)
{
    QFile file(archive);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    return isSupportedArchive(&file);
}

bool Lib7z::isSupportedArchive(QFileDevice* archive)
{
    assert(archive);
    assert(!archive->isSequential());
    const qint64 initialPos = archive->pos();
    try {
        QScopedPointer<CCodecs> codecs(new CCodecs);
        if (codecs->Load() != S_OK)
            throw SevenZipException(QCoreApplication::translate("Lib7z", "Could not load codecs"));

        CIntVector formatIndices;

        if (!codecs->FindFormatForArchiveType(L"", formatIndices)) {
            throw SevenZipException(QCoreApplication::translate("Lib7z",
                "Could not retrieve default format"));
        }
        CArchiveLink archiveLink;
        //CMyComPtr is needed, otherwise it crashes in OpenStream()
        const CMyComPtr<IInStream> stream = new QIODeviceInStream(archive);

        const HRESULT result = archiveLink.Open2(codecs.data(), formatIndices, /*stdInMode*/false, stream,
            UString(), 0);

        archive->seek(initialPos);
        return result == S_OK;
    } catch (const SevenZipException& e) {
        archive->seek(initialPos);
        throw e;
    } catch (const char *err) {
        archive->seek(initialPos);
        throw SevenZipException(err);
    } catch (...) {
        archive->seek(initialPos);
        throw SevenZipException(QCoreApplication::translate("Lib7z", "Unknown exception caught (%1)")
            .arg(QString::fromLatin1(Q_FUNC_INFO)));
    }
    return false; // never reached
}

void ExtractItemJob::doStart()
{
    try {
        if (!d->archive)
            throw SevenZipException(tr("Could not list archive: QIODevice not set or already destroyed."));
        if (d->target)
            extractFileFromArchive(d->archive, d->item, d->target, d->callback);
        else if (!d->item.path.isEmpty())
            extractFileFromArchive(d->archive, d->item, d->targetDirectory, d->callback);
        else
            extractArchive(d->archive, d->targetDirectory, d->callback);
    } catch (const SevenZipException& e) {
        setError(Failed);
        setErrorString(tr("Error while extracting '%1': %2").arg(d->item.path, e.message()));
    } catch (...) {
        setError(Failed);
        setErrorString(tr("Unknown exception caught (%1)").arg(tr("Failed")));
    }
    emitResult();
}
