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

#include "lib7z_facade.h"

#include "errors.h"
#include "fileio.h"

#include "lib7z_create.h"
#include "lib7z_extract.h"
#include "lib7z_list.h"
#include "lib7z_guid.h"
#include "globals.h"
#include "directoryguard.h"
#include "fileguard.h"

#ifndef Q_OS_WIN
#   include "StdAfx.h"
#endif

#include <7zCrc.h>

#include <7zip/Archive/IArchive.h>

#include <7zip/UI/Common/ArchiveCommandLine.h>
#include <7zip/UI/Common/OpenArchive.h>

#include <Windows/FileDir.h>
#include <Windows/FileIO.h>
#include <Windows/PropVariant.h>
#include <Windows/PropVariantConv.h>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QIODevice>
#include <QPointer>
#include <QReadWriteLock>
#include <QTemporaryFile>

#include <mutex>
#include <memory>

#ifdef Q_OS_WIN
HINSTANCE g_hInstance = nullptr;

# define S_IFMT 00170000
# define S_IFLNK 0120000
# define S_ISLNK(m) (((m) & S_IFMT) == S_IFLNK)
# define FILE_ATTRIBUTE_UNIX_EXTENSION   0x8000   /* trick for Unix */
# if !defined(Q_CC_MINGW)
#  include <time.h> // for localtime_s
# endif
#else
extern "C" int global_use_utf16_conversion;

#include <myWindows/config.h>
#include <sys/stat.h>
#endif

namespace NArchive {
    namespace N7z {
        void registerArcDec7z();
    }
    namespace NXz {
        void registerArcxz();
    }
    namespace NSplit {
        void registerArcSplit();
    }
    namespace NLzma {
        namespace NLzmaAr {
            void registerArcLzma();
        }
        namespace NLzma86Ar {
            void registerArcLzma86();
        }
    }
}
using namespace NWindows;

void registerCodecBCJ();
void registerCodecBCJ2();

void registerCodecLZMA();
void registerCodecLZMA2();

void registerCodecCopy();
void registerCodecDelta();
void registerCodecBranch();
void registerCodecByteSwap();

namespace Lib7z {

/*!
    \inmodule Lib7z
    \class Lib7z::File
    \internal
*/

/*!
    \inmodule Lib7z
    \class Lib7z::UpdateCallback
    \internal
*/

/*!
    \inmodule Lib7z
    \class Lib7z::SevenZipException
    \brief The SevenZipException provides a class for lib7z exceptions.
*/

/*!
    \fn explicit Lib7z::SevenZipException::SevenZipException(const QString &msg)

    Constructs an instance of SevenZipException with error message \a msg.
*/

/*!
    \fn explicit Lib7z::SevenZipException::SevenZipException(const char *msg)

    Constructs an instance of SevenZipException with error message \a msg.
*/

/*!
    \inmodule Lib7z
    \class Lib7z::PercentPrinter
    \brief The PercentPrinter class displays the archiving process.
*/

/*!
    \fn void Lib7z::PercentPrinter::PrintRatio()

    Prints ratio.
*/

/*!
    \fn void Lib7z::PercentPrinter::ClosePrint()

    Closes print.
*/

/*!
    \fn void Lib7z::PercentPrinter::RePrintRatio()

    Reprints ratio.
*/

/*!
    \fn void Lib7z::PercentPrinter::PrintNewLine()

    Prints new line.
*/

/*!
    \fn void Lib7z::PercentPrinter::PrintString(const char *s)

   Prints string \a s.
*/

/*!
    \fn void Lib7z::PercentPrinter::PrintString(const wchar_t *s)

    Prints string \a s.
*/

// -- 7z init codecs, archives

std::once_flag gOnceFlag;

/*!
    Initializes 7z and registers codecs and compression methods.
*/
void initSevenZ()
{
    std::call_once(gOnceFlag, [] {
        CrcGenerateTable();

        registerCodecBCJ();
        registerCodecBCJ2();

        registerCodecLZMA();
        registerCodecLZMA2();

        registerCodecCopy();
        registerCodecDelta();
        registerCodecBranch();
        registerCodecByteSwap();

        NArchive::N7z::registerArcDec7z();
        NArchive::NXz::registerArcxz();
        NArchive::NSplit::registerArcSplit();
        NArchive::NLzma::NLzmaAr::registerArcLzma();
        NArchive::NLzma::NLzma86Ar::registerArcLzma86();

#ifndef Q_OS_WIN
# ifdef ENV_HAVE_LOCALE
        const QByteArray locale = qgetenv("LC_ALL").toUpper();
        if (!locale.isEmpty() && (locale != "C") && (locale != "POSIX"))
            global_use_utf16_conversion = 1;
# elif defined(LOCALE_IS_UTF8)
        global_use_utf16_conversion = 1;
# else
        global_use_utf16_conversion = 0;
# endif
#endif
    });
}


// -- error handling

Q_GLOBAL_STATIC(QString, getLastErrorString)
Q_GLOBAL_STATIC(QReadWriteLock, lastErrorReadWriteLock)

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

QString errorMessageFrom7zResult(const LONG  &extractResult)
{
    if (!lastError().isEmpty())
        return lastError();

    QString errorMessage = QCoreApplication::translate("Lib7z", "Internal code: %1");
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
            errorMessage = QCoreApplication::translate("Lib7z", "Not enough memory");
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

static UString QString2UString(const QString &str)
{
    return str.toStdWString().c_str();
}

static QString UString2QString(const UString& str)
{
    return QString::fromStdWString(static_cast<const wchar_t*>(str));
}

static NCOM::CPropVariant readProperty(IInArchive *archive, int index, int propId)
{
    NCOM::CPropVariant prop;
    if (archive->GetProperty(index, propId, &prop) != S_OK) {
        throw SevenZipException(QCoreApplication::translate("Lib7z",
            "Cannot retrieve property %1 for item %2.").arg(QString::number(propId),
            QString::number(index)));
    }
    return prop;
}

static bool IsFileTimeZero(const FILETIME *lpFileTime)
{
    return (lpFileTime->dwLowDateTime == 0) && (lpFileTime->dwHighDateTime == 0);
}

static bool getFileTimeFromProperty(IInArchive* archive, int index, int propId, FILETIME *ft)
{
    const NCOM::CPropVariant prop = readProperty(archive, index, propId);
    if (prop.vt != VT_FILETIME) {
        throw SevenZipException(QCoreApplication::translate("Lib7z",
            "Property %1 for item %2 not of type VT_FILETIME but %3.").arg(QString::number(propId),
            QString::number(index), QString::number(prop.vt)));
    }
    *ft = prop.filetime;
    return !IsFileTimeZero(ft);
}

static bool getDateTimeProperty(IInArchive *arc, int index, int id, QDateTime *value)
{
    FILETIME ft7z;
    if (!getFileTimeFromProperty(arc, index, id, &ft7z))
        return false;

    SYSTEMTIME st;
    if (!BOOLToBool(FileTimeToSystemTime(&ft7z, &st))) {
        throw SevenZipException(QCoreApplication::translate("Lib7z",
            "Cannot convert UTC file time to system time."));
    }
    *value = QDateTime(QDate(st.wYear, st.wMonth, st.wDay), QTime(st.wHour, st.wMinute,
        st.wSecond), Qt::UTC);
    return value->isValid();
}

static quint64 getUInt64Property(IInArchive *archive, int index, int propId, quint64 defaultValue)
{
    UInt64 value;
    if (ConvertPropVariantToUInt64(readProperty(archive, index, propId), value))
        return value;
    return defaultValue;
}

static quint32 getUInt32Property(IInArchive *archive, int index, int propId, quint32 defaultValue)
{
    const NCOM::CPropVariant prop = readProperty(archive, index, propId);
    if (prop.vt == VT_EMPTY)
        return defaultValue;
    return static_cast<quint32>(prop.ulVal);
}

static QFile::Permissions getPermissions(IInArchive *archive, int index, bool *hasPermissions)
{
    quint32 attributes = getUInt32Property(archive, index, kpidAttrib, 0);
    QFile::Permissions permissions = QFile::Permissions();
    if (attributes & FILE_ATTRIBUTE_UNIX_EXTENSION) {
        if (hasPermissions != nullptr)
            *hasPermissions = true;
        // filter the Unix permissions
        attributes = (attributes >> 16) & 0777;
        permissions |= static_cast<QFile::Permissions>((attributes & 0700) << 2);  // owner rights
        permissions |= static_cast<QFile::Permissions>((attributes & 0070) << 1);  // group
        permissions |= static_cast<QFile::Permissions>((attributes & 0007) << 0);  // and world rights
    } else if (hasPermissions != nullptr) {
        *hasPermissions = false;
    }
    return permissions;
}

#define LIB7Z_ASSERTS(X, MODE) \
    Q_ASSERT(X); \
    Q_ASSERT(X->isOpen()); \
    Q_ASSERT(X->is ## MODE()); \
    Q_ASSERT(!X->isSequential());

class QIODeviceSequentialOutStream : public ISequentialOutStream, public CMyUnknownImp
{
    Q_DISABLE_COPY(QIODeviceSequentialOutStream)

public:
    MY_UNKNOWN_IMP

    explicit QIODeviceSequentialOutStream(std::unique_ptr<QIODevice> device)
        : ISequentialOutStream()
        , m_device(std::move(device))
    {
        LIB7Z_ASSERTS(m_device, Writable)
    }

    QString errorString() const {
        return m_errorString;
    }

    STDMETHOD(Write)(const void *data, UInt32 size, UInt32 *processedSize)
    {
        if (processedSize)
            *processedSize = 0;

        const qint64 written = m_device->write(reinterpret_cast<const char*>(data), size);
        if (written == -1) {
            m_errorString = m_device->errorString();
            return E_FAIL;
        }

        if (processedSize)
            *processedSize = written;
        m_errorString.clear();
        return S_OK;
    }

private:
    QString m_errorString;
    std::unique_ptr<QIODevice> m_device;
};

class QIODeviceInStream : public IInStream, public CMyUnknownImp
{
    Q_DISABLE_COPY(QIODeviceInStream)

public:
    MY_UNKNOWN_IMP

    explicit QIODeviceInStream(QIODevice *device)
        : IInStream()
        , CMyUnknownImp()
        , m_device(device)
    {
        LIB7Z_ASSERTS(m_device, Readable)
    }

    STDMETHOD(Read)(void *data, UInt32 size, UInt32 *processedSize)
    {
        if (m_device.isNull())
            return E_FAIL;

        const qint64 actual = m_device->read(reinterpret_cast<char*>(data), size);
        Q_ASSERT(actual != 0 || m_device->atEnd());
        if (processedSize)
            *processedSize = actual;
        return actual >= 0 ? S_OK : E_FAIL;
    }

    STDMETHOD(Seek)(Int64 offset, UInt32 seekOrigin, UInt64 *newPosition)
    {
        if (m_device.isNull())
            return E_FAIL;
        if (seekOrigin > STREAM_SEEK_END)
            return STG_E_INVALIDFUNCTION;
        UInt64 np = 0;
        switch (seekOrigin) {
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

        np = qBound(static_cast<UInt64>(0), np, static_cast<UInt64>(m_device->size()));
        const bool ok = m_device->seek(np);
        if (newPosition)
            *newPosition = np;
        return ok ? S_OK : E_FAIL;
    }

private:
    QPointer<QIODevice> m_device;
};

/*!
   Returns a list of files belonging to an \a archive.
*/
QVector<File> listArchive(QFileDevice *archive)
{
    LIB7Z_ASSERTS(archive, Readable)

    const qint64 initialPos = archive->pos();
    try {
        CCodecs codecs;
        if (codecs.Load() != S_OK)
            throw SevenZipException(QCoreApplication::translate("Lib7z", "Cannot load codecs."));

        COpenOptions op;
        op.codecs = &codecs;

        CObjectVector<COpenType> types;
        op.types = &types;  // Empty, because we use a stream.

        CIntVector excluded;
        excluded.Add(codecs.FindFormatForExtension(
            QString2UString(QLatin1String("xz")))); // handled by libarchive
        op.excludedFormats = &excluded;

        const CMyComPtr<IInStream> stream = new QIODeviceInStream(archive);
        op.stream = stream; // CMyComPtr is needed, otherwise it crashes in OpenStream().

        CObjectVector<CProperty> properties;
        op.props = &properties;

        CArchiveLink archiveLink;
        if (archiveLink.Open2(op, nullptr) != S_OK) {
            throw SevenZipException(QCoreApplication::translate("Lib7z",
                "Cannot open archive \"%1\".").arg(archive->fileName()));
        }

        QVector<File> flat;
        for (unsigned i = 0; i < archiveLink.Arcs.Size(); ++i) {
            IInArchive *const arch = archiveLink.Arcs[i].Archive;
            UInt32 numItems = 0;
            if (arch->GetNumberOfItems(&numItems) != S_OK) {
                throw SevenZipException(QCoreApplication::translate("Lib7z",
                    "Cannot retrieve number of items in archive."));
            }
            flat.reserve(flat.size() + numItems);
            for (uint item = 0; item < numItems; ++item) {
                UString s;
                if (archiveLink.Arcs[i].GetItemPath(item, s) != S_OK) {
                    throw SevenZipException(QCoreApplication::translate("Lib7z",
                        "Cannot retrieve path of archive item \"%1\".").arg(item));
                }
                File f;
                f.archiveIndex.setX(i);
                f.archiveIndex.setY(item);
                f.path = UString2QString(s).replace(QLatin1Char('\\'), QLatin1Char('/'));
                Archive_IsItem_Folder(arch, item, f.isDirectory);
                Archive_GetItemBoolProp(arch, item, kpidSymLink, f.isSymbolicLink);
                f.permissions_enum = getPermissions(arch, item, nullptr);
                getDateTimeProperty(arch, item, kpidMTime, &(f.utcTime));
                f.uncompressedSize = getUInt64Property(arch, item, kpidSize, 0);
                f.compressedSize = getUInt64Property(arch, item, kpidPackSize, 0);
                flat.append(f);
            }
        }
        return flat;
    } catch (const char *err) {
        archive->seek(initialPos);
        throw SevenZipException(err);
    } catch (const SevenZipException &e) {
        archive->seek(initialPos);
        throw e; // re-throw unmodified
    } catch (...) {
        archive->seek(initialPos);
        throw SevenZipException(QCoreApplication::translate("Lib7z",
            "Unknown exception caught (%1).").arg(QString::fromLatin1(Q_FUNC_INFO)));
    }
    return QVector<File>(); // never reached
}

/*!
    \inmodule Lib7z
    \class Lib7z::ExtractCallback
    \brief Provides a callback for archive extraction.
*/

/*!
    \internal
*/
STDMETHODIMP ExtractCallback::SetTotal(UInt64 t)
{
    total = t;
    return S_OK;
}

/*!
    \internal
*/
STDMETHODIMP ExtractCallback::SetCompleted(const UInt64 *c)
{
    completed = *c;
    if (total > 0)
        return setCompleted(completed, total);
    return S_OK;
}

/*!
    \internal

    This method will be called by CFolderOutStream::OpenFile to stream via
    CDecoder::CodeSpec extracted content to an output stream.
*/
STDMETHODIMP ExtractCallback::GetStream(UInt32 index, ISequentialOutStream **outStream, Int32 /*askExtractMode*/)
{
    *outStream = nullptr;
    if (targetDir.isEmpty())
        return E_FAIL;

    Q_ASSERT(arc);
    currentIndex = index;

    UString s;
    if (arc->GetItemPath(index, s) != S_OK) {
        setLastError(QCoreApplication::translate("ExtractCallbackImpl",
            "Cannot retrieve path of archive item %1.").arg(index));
        return E_FAIL;
    }

    const QFileInfo fi(QString::fromLatin1("%1/%2").arg(targetDir, UString2QString(s)));

    QInstaller::DirectoryGuard guard(fi.absolutePath());
    const QStringList directories = guard.tryCreate();

    bool isDir = false;
    Archive_IsItem_Folder(arc->Archive, index, isDir);
    if (isDir)
        QDir(fi.absolutePath()).mkdir(fi.fileName());

    // this makes sure that all directories created get removed as well
    foreach (const QString &directory, directories)
        setCurrentFile(directory);

    QScopedPointer<QInstaller::FileGuardLocker> locker(nullptr);
    if (!isDir) {
        locker.reset(new QInstaller::FileGuardLocker(
            fi.absoluteFilePath(), QInstaller::FileGuard::globalObject()));
    }
    if (!isDir && !prepareForFile(fi.absoluteFilePath()))
        return E_FAIL;

    setCurrentFile(fi.absoluteFilePath());

    if (!isDir) {
#ifndef Q_OS_WIN
        // do not follow symlinks, so we need to remove an existing one
        if (fi.isSymLink() && (!QFile::remove(fi.absoluteFilePath()))) {
            setLastError(QCoreApplication::translate("ExtractCallbackImpl",
                "Cannot remove already existing symlink %1.").arg(fi.absoluteFilePath()));
            return E_FAIL;
        }
#endif
        std::unique_ptr<QFile> file(new QFile(fi.absoluteFilePath()));
        if (!file->open(QIODevice::WriteOnly)) {
            setLastError(QCoreApplication::translate("ExtractCallbackImpl",
                                                     "Cannot open file \"%1\" for writing: %2").arg(
                             QDir::toNativeSeparators(fi.absoluteFilePath()), file->errorString()));
            return E_FAIL;
        }
        CMyComPtr<ISequentialOutStream> stream =
            new QIODeviceSequentialOutStream(std::move(file));
        *outStream = stream.Detach(); // CMyComPtr is needed, otherwise it crashes in Write().
    }

    guard.release();
    return S_OK;
}

/*!
    \internal
*/
STDMETHODIMP ExtractCallback::PrepareOperation(Int32 /*askExtractMode*/)
{
    return S_OK;
}

/*!
    \internal
*/
STDMETHODIMP ExtractCallback::SetOperationResult(Int32 /*resultEOperationResult*/)
{
    if (targetDir.isEmpty())
        return S_OK;

    UString s;
    if (arc->GetItemPath(currentIndex, s) != S_OK) {
        setLastError(QCoreApplication::translate("ExtractCallbackImpl",
            "Cannot retrieve path of archive item %1.").arg(currentIndex));
        return E_FAIL;
    }

    const QString absFilePath = QFileInfo(QString::fromLatin1("%1/%2").arg(targetDir,
        UString2QString(s).replace(QLatin1Char('\\'), QLatin1Char('/')))).absoluteFilePath();

    // do we have a symlink?
    const quint32 attributes = getUInt32Property(arc->Archive, currentIndex, kpidAttrib, 0);
    struct stat stat_info;
    stat_info.st_mode = attributes >> 16;
    if (S_ISLNK(stat_info.st_mode)) {
#ifdef Q_OS_WIN
        qFatal(QString::fromLatin1("Creating a link from archive is not implemented for "
            "windows. Link filename: %1").arg(absFilePath).toLatin1());
        // TODO
        //if (!NFile::NDir::MyCreateHardLink(CFSTR(QDir::toNativeSeparators(absFilePath).utf16()),
        //    CFSTR(QDir::toNativeSeparators(symlinkTarget).utf16())) {
        //    return S_FALSE;
        //}
#else
        QFileInfo symlinkPlaceHolderFileInfo(absFilePath);
        if (symlinkPlaceHolderFileInfo.isSymLink()) {
            setLastError(QCoreApplication::translate("ExtractCallbackImpl",
                "Cannot create symlink at \"%1\". Another one is already existing.")
                .arg(absFilePath));
            return E_FAIL;
        }
        QFile symlinkPlaceHolderFile(absFilePath);
        if (!symlinkPlaceHolderFile.open(QIODevice::ReadOnly)) {
            setLastError(QCoreApplication::translate("ExtractCallbackImpl",
                "Cannot read symlink target from file \"%1\".").arg(absFilePath));
            return E_FAIL;
        }

        const QByteArray symlinkTarget = symlinkPlaceHolderFile.readAll();
        symlinkPlaceHolderFile.close();
        symlinkPlaceHolderFile.remove();
        QFile targetFile(QString::fromLatin1(symlinkTarget));
        if (!targetFile.link(absFilePath)) {
            setLastError(QCoreApplication::translate("ExtractCallbackImpl",
                "Cannot create symlink at %1: %2").arg(absFilePath,
                targetFile.errorString()));
            return E_FAIL;
        }
        return S_OK;
#endif
    }

    try {   // Note: This part might also fail while running a elevated installation.
        if (!absFilePath.isEmpty()) {
            // This might fail for archives without all properties, we can only be sure
            // about modification time, as it's always stored by default in 7z archives.
            // Also note that we restore modification time on Unix only, as access time
            // and change time are supposed to be set to the time of installation.
            FILETIME mTime;
            const UString fileName = QString2UString(absFilePath);
            if (getFileTimeFromProperty(arc->Archive, currentIndex, kpidMTime, &mTime)) {
                NWindows::NFile::NIO::COutFile file;
                if (file.Open(fileName, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL))
                    file.SetTime(&mTime, &mTime, &mTime);
            }
#ifdef Q_OS_WIN
            FILETIME cTime, aTime;
            if (getFileTimeFromProperty(arc->Archive, currentIndex, kpidCTime, &cTime)
                && getFileTimeFromProperty(arc->Archive, currentIndex, kpidATime, &aTime)) {
                    NWindows::NFile::NIO::COutFile file;
                    if (file.Open(fileName, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL))
                        file.SetTime(&cTime, &aTime, &mTime);
            }
#endif
        }
    } catch (...) {}

    bool hasPerm = false;
    const QFile::Permissions permissions = getPermissions(arc->Archive, currentIndex, &hasPerm);
    if (hasPerm)
        QFile::setPermissions(absFilePath, permissions);
    return S_OK;
}

/*!
    \enum Lib7z::TmpFile

    This enum type holds the temp file mode:

    \value  No
            File is not a temporary file.
    \value  Yes
            File is a tmp file.
*/

/*!
    \typedef Lib7z::Compression

    Synonym for QInstaller::CompressionLevel
*/

/*!
    \typedef Lib7z::File

    Synonym for QInstaller::ArchiveEntry
*/

/*!
    \namespace Lib7z
    \inmodule QtInstallerFramework
    \brief  The Lib7z namespace contains miscellaneous identifiers used throughout the Lib7z library.
*/

/*!
    \fn virtual bool Lib7z::ExtractCallback::prepareForFile(const QString &filename)

    Implement to prepare for file \a filename to be extracted, e.g. by renaming existing files.
    Return \c true if the preparation was successful and extraction can be continued. If \c false
    is returned, the extraction will be aborted. The default implementation returns \c true.
*/

/*!
    \fn bool Lib7z::ExtractCallback::setArchive(CArc *carc)

    Sets \a carc as archive.
*/

/*!
   \fn void Lib7z::ExtractCallback::setTarget(const QString &dir)

    Sets the target directory to \a dir.
*/

/*!
    \fn void Lib7z::ExtractCallback::setCurrentFile(const QString &filename)

    Sets the current file to \a filename.
*/

/*!
    \fn virtual Lib7z::ExtractCallback::setCompleted(quint64 completed, quint64 total)

    Always returns \c true.
*/

/*!
    \fn ULONG Lib7z::ExtractCallback::AddRef()

    \internal
*/

/*!
    \fn LONG Lib7z::ExtractCallback::QueryInterface(const GUID &iid, void **outObject)

    \internal
*/

/*!
    \fn ULONG Lib7z::ExtractCallback::Release()

    \internal
*/

// -- UpdateCallback

HRESULT UpdateCallback::SetTotal(UInt64)
{
    return S_OK;
}

HRESULT UpdateCallback::SetCompleted(const UInt64*)
{
    return S_OK;
}

HRESULT UpdateCallback::SetRatioInfo(const UInt64*, const UInt64*)
{
    return S_OK;
}

HRESULT UpdateCallback::CheckBreak()
{
    return S_OK;
}

HRESULT UpdateCallback::Finilize()
{
    return S_OK;
}

HRESULT UpdateCallback::SetNumFiles(UInt64)
{
    return S_OK;
}

HRESULT UpdateCallback::GetStream(const wchar_t*, bool)
{
    return S_OK;
}

HRESULT UpdateCallback::OpenFileError(const wchar_t*, DWORD)
{
    return S_OK;
}

HRESULT UpdateCallback::CryptoGetTextPassword2(Int32 *passwordIsDefined, BSTR *password)
{
    *password = nullptr;
    *passwordIsDefined = false;
    return S_OK;
}

HRESULT UpdateCallback::CryptoGetTextPassword(BSTR *password)
{
    *password = nullptr;
    return E_NOTIMPL;
}

HRESULT UpdateCallback::OpenResult(const wchar_t*, HRESULT, const wchar_t*)
{
    return S_OK;
}

HRESULT UpdateCallback::StartScanning()
{
    return S_OK;
}

HRESULT UpdateCallback::ScanProgress(UInt64, UInt64, UInt64, const wchar_t*, bool)
{
    return S_OK;
}

HRESULT UpdateCallback::CanNotFindError(const wchar_t*, DWORD)
{
    return S_OK;
}

HRESULT UpdateCallback::FinishScanning()
{
    return S_OK;
}

HRESULT UpdateCallback::StartArchive(const wchar_t*, bool)
{
    return S_OK;
}

HRESULT UpdateCallback::FinishArchive()
{
    return S_OK;
}

HRESULT UpdateCallback::SetOperationResult(Int32)
{
    return S_OK;
}

/*
    Function to create an empty 7z container. Using a temporary file only is not working, since
    7z checks the output file for a valid signature, otherwise it rejects overwriting the file.
*/
static QString createTmp7z()
{
    QTemporaryFile file;
    if (!file.open()) {
        throw SevenZipException(QCoreApplication::translate("Lib7z", "Cannot create "
            "temporary file: %1").arg(file.errorString()));
    }

    file.write(QByteArray::fromHex("377A.BCAF.271C" // Signature.
        ".0003.8D9B.D50F.0000.0000.0000.0000.0000.0000.0000.0000.0000.0000" // Crc + Data.
    ));
    file.setAutoRemove(false);
    return file.fileName();
}

/*!
    Creates an archive using the given file device \a archive. \a sources can contain one or
    more files, one or more directories or a combination of files and folders. Also, \c * wildcard
    is supported. The value of \a level specifies the compression ratio, the default is set
    to \c 5 (Normal compression). The \a callback can be used to get information about the archive
    creation process. If no \a callback is given, an empty implementation is used.

    \note Throws SevenZipException on error.
    \note Filenames are stored case-sensitive with UTF-8 encoding.
    \note The ownership of \a callback is transferred to the function and gets delete on exit.
*/
void INSTALLER_EXPORT createArchive(QFileDevice *archive, const QStringList &sources,
    Compression level, UpdateCallback *callback)
{
    LIB7Z_ASSERTS(archive, Writable)

    const QString tmpArchive = createTmp7z();
    Lib7z::createArchive(tmpArchive, sources, TmpFile::No, level, callback);

    try {
        QFile source(tmpArchive);
        QInstaller::openForRead(&source);
        QInstaller::blockingCopy(&source, archive, source.size());
    } catch (const QInstaller::Error &error) {
        throw SevenZipException(error.message());
    }
}

/*!
    Creates an archive with the given filename \a archive. \a sources can contain one or more
    files, one or more directories or a combination of files and folders. Also, \c * wildcard
    is supported. To be able to use the function during an elevated installation, set \a mode to
    \c TmpFile::Yes. The value of \a level specifies the compression ratio, the default is set
    to \c 5 (Normal compression). The \a callback can be used to get information about the archive
    creation process. If no \a callback is given, an empty implementation is used.

    \note Throws SevenZipException on error.
    \note If \a archive exists, it will be overwritten.
    \note Filenames are stored case-sensitive with UTF-8 encoding.
    \note The ownership of \a callback is transferred to the function and gets delete on exit.
*/
void createArchive(const QString &archive, const QStringList &sources, TmpFile mode,
    Compression level, UpdateCallback *callback)
{
    try {
        QString target = archive;
        if (mode == TmpFile::Yes)
            target = createTmp7z();

        CArcCmdLineOptions options;
        try {
            UStringVector commandStrings;
            commandStrings.Add(L"a"); // mode: add
            commandStrings.Add(L"-t7z"); // type: 7z
            commandStrings.Add(L"-mtm=on"); // time: modeifier|creation|access
            commandStrings.Add(L"-mtc=on");
            commandStrings.Add(L"-mta=on");
            commandStrings.Add(L"-mmt=on"); // threads: multi-threaded
#ifdef Q_OS_WIN
            commandStrings.Add(L"-sccUTF-8"); // files: case-sensitive|UTF8
#endif
            commandStrings.Add(QString2UString(QString::fromLatin1("-mx=%1").arg(int(level)))); // compression: level
            commandStrings.Add(QString2UString(QDir::toNativeSeparators(target)));
            foreach (const QString &source, sources)
                commandStrings.Add(QString2UString(source));

            CArcCmdLineParser parser;
            parser.Parse1(commandStrings, options);
            parser.Parse2(options);
        } catch (const CArcCmdLineException &e) {
            throw SevenZipException(UString2QString(e));
        }

        CCodecs codecs;
        if (codecs.Load() != S_OK)
            throw SevenZipException(QCoreApplication::translate("Lib7z", "Cannot load codecs."));

        CObjectVector<COpenType> types;
        if (!ParseOpenTypes(codecs, options.ArcType, types))
            throw SevenZipException(QCoreApplication::translate("Lib7z", "Unsupported archive type."));

        CUpdateErrorInfo errorInfo;
        CMyComPtr<UpdateCallback> comCallback = callback == 0 ? new UpdateCallback : callback;
        const HRESULT res = UpdateArchive(&codecs, types, options.ArchiveName, options.Censor,
            options.UpdateOptions, errorInfo, nullptr, comCallback, true);

        const QFile tempFile(UString2QString(options.ArchiveName));
        if (res != S_OK || !tempFile.exists()) {
            QString errorMsg;
            if (res == S_OK) {
                errorMsg = QCoreApplication::translate("Lib7z", "Cannot create archive \"%1\"")
                    .arg(QDir::toNativeSeparators(tempFile.fileName()));
            } else {
                errorMsg = QCoreApplication::translate("Lib7z", "Cannot create archive \"%1\": %2")
                    .arg(QDir::toNativeSeparators(tempFile.fileName()), errorMessageFrom7zResult(res));
            }
            throw SevenZipException(errorMsg);
        }

        if (mode == TmpFile::Yes) {
            QFile org(archive);
            if (org.exists() && !org.remove()) {
                throw SevenZipException(QCoreApplication::translate("Lib7z", "Cannot remove "
                    "old archive \"%1\": %2").arg(QDir::toNativeSeparators(org.fileName()),
                                                org.errorString()));
            }

            QFile arc(UString2QString(options.ArchiveName));
            if(!arc.rename(archive)) {
                throw SevenZipException(QCoreApplication::translate("Lib7z", "Cannot rename "
                    "temporary archive \"%1\" to \"%2\": %3").arg(
                                            QDir::toNativeSeparators(arc.fileName()),
                                            QDir::toNativeSeparators(archive),
                                            arc.errorString()));
            }
        }
    } catch (const char *err) {
        throw SevenZipException(err);
    } catch (SevenZipException &e) {
        throw e; // re-throw unmodified
    } catch (const QInstaller::Error &err) {
        throw SevenZipException(err.message());
    } catch (...) {
        throw SevenZipException(QCoreApplication::translate("Lib7z",
            "Unknown exception caught (%1)").arg(QString::fromLatin1(Q_FUNC_INFO)));
    }
}

/*!
    Extracts the given \a archive content into target directory \a directory using the provided
    extract callback \a callback. The output filenames are deduced from the \a archive content.

    \note Throws SevenZipException on error.
    \note The ownership of \a callback is not transferred to the function.
*/
void extractArchive(QFileDevice *archive, const QString &directory, ExtractCallback *callback)
{
    LIB7Z_ASSERTS(archive, Readable)

    // Guard a given object against unwanted delete.
    CMyComPtr<ExtractCallback> externCallback = callback;

    CMyComPtr<ExtractCallback> localCallback;
    if (!externCallback) {
        callback = new ExtractCallback;
        localCallback = callback;
    }

    QInstaller::DirectoryGuard outDir(QFileInfo(directory).absolutePath());
    try {
        outDir.tryCreate();

        CCodecs codecs;
        if (codecs.Load() != S_OK)
            throw SevenZipException(QCoreApplication::translate("Lib7z", "Cannot load codecs."));

        COpenOptions op;
        op.codecs = &codecs;

        CObjectVector<COpenType> types;
        op.types = &types;  // Empty, because we use a stream.

        CIntVector excluded;
        excluded.Add(codecs.FindFormatForExtension(
            QString2UString(QLatin1String("xz")))); // handled by libarchive
        op.excludedFormats = &excluded;

        const CMyComPtr<IInStream> stream = new QIODeviceInStream(archive);
        op.stream = stream; // CMyComPtr is needed, otherwise it crashes in OpenStream().

        CObjectVector<CProperty> properties;
        op.props = &properties;

        CArchiveLink archiveLink;
        if (archiveLink.Open2(op, nullptr) != S_OK) {
            throw SevenZipException(QCoreApplication::translate("Lib7z",
                "Cannot open archive \"%1\".").arg(archive->fileName()));
        }

        callback->setTarget(directory);
        for (unsigned a = 0; a < archiveLink.Arcs.Size(); ++a) {
            callback->setArchive(&archiveLink.Arcs[a]);
            IInArchive *const arch = archiveLink.Arcs[a].Archive;

            const LONG result = arch->Extract(0, static_cast<UInt32>(-1), false, callback);
            if (result != S_OK)
                throw SevenZipException(errorMessageFrom7zResult(result));
        }
    } catch (const SevenZipException &e) {
        externCallback.Detach();
        throw e; // re-throw unmodified
    } catch (...) {
        externCallback.Detach();
        throw SevenZipException(QCoreApplication::translate("Lib7z",
            "Unknown exception caught (%1).").arg(QString::fromLatin1(Q_FUNC_INFO)));
    }
    outDir.release();
    externCallback.Detach();
}

/*!
    Returns \c true if the given \a archive is supported; otherwise returns \c false.

    \note Throws SevenZipException on error.
*/
bool isSupportedArchive(QFileDevice *archive)
{
    LIB7Z_ASSERTS(archive, Readable)

    const qint64 initialPos = archive->pos();
    try {
        CCodecs codecs;
        if (codecs.Load() != S_OK)
            throw SevenZipException(QCoreApplication::translate("Lib7z", "Cannot load codecs."));

        COpenOptions op;
        op.codecs = &codecs;

        CObjectVector<COpenType> types;
        op.types = &types;  // Empty, because we use a stream.

        CIntVector excluded;
        excluded.Add(codecs.FindFormatForExtension(
            QString2UString(QLatin1String("xz")))); // handled by libarchive
        op.excludedFormats = &excluded;

        const CMyComPtr<IInStream> stream = new QIODeviceInStream(archive);
        op.stream = stream; // CMyComPtr is needed, otherwise it crashes in OpenStream().

        CObjectVector<CProperty> properties;
        op.props = &properties;

        CArchiveLink archiveLink;
        const HRESULT result = archiveLink.Open2(op, nullptr);

        archive->seek(initialPos);
        return result == S_OK;
    } catch (const char *err) {
        archive->seek(initialPos);
        throw SevenZipException(err);
    } catch (const SevenZipException &e) {
        archive->seek(initialPos);
        throw e; // re-throw unmodified
    } catch (...) {
        archive->seek(initialPos);
        throw SevenZipException(QCoreApplication::translate("Lib7z",
            "Unknown exception caught (%1).").arg(QString::fromLatin1(Q_FUNC_INFO)));
    }
    return false; // never reached
}

/*!
    Returns \c true if the given \a archive is supported; otherwise returns \c false.

    \note Throws SevenZipException on error.
*/
bool isSupportedArchive(const QString &archive)
{
    QFile file(archive);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    return isSupportedArchive(&file);
}

} // namespace Lib7z
