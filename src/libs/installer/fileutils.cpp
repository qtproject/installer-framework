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
#include "fileutils.h"

#include "globals.h"
#include "constants.h"
#include "fileio.h"
#include <errors.h>

#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QDirIterator>
#include <QtCore/QEventLoop>
#include <QtCore/QTemporaryFile>
#include <QtCore/QThread>
#include <QtCore/QUrl>
#include <QtCore/QCoreApplication>
#include <QImageReader>
#include <QRandomGenerator>
#include <QGuiApplication>
#include <QScreen>

#include <errno.h>

#ifdef Q_OS_UNIX
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

using namespace QInstaller;

/*!
    \enum QInstaller::DefaultFilePermissions

    \value NonExecutable
           Default permissions for a non-executable file.
    \value Executable
           Default permissions for an executable file.
*/

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::TempPathDeleter
    \internal
*/

// -- TempDirDeleter

TempPathDeleter::TempPathDeleter(const QString &path)
{
    m_paths.insert(path);
}

TempPathDeleter::TempPathDeleter(const QStringList &paths)
    : m_paths(QSet<QString>(paths.begin(), paths.end()))
{
}

TempPathDeleter::~TempPathDeleter()
{
    releaseAndDeleteAll();
}

QStringList TempPathDeleter::paths() const
{
    return m_paths.values();
}

void TempPathDeleter::add(const QString &path)
{
    m_paths.insert(path);
}

void TempPathDeleter::add(const QStringList &paths)
{
    m_paths += QSet<QString>(paths.begin(), paths.end());
}

void TempPathDeleter::releaseAndDeleteAll()
{
    foreach (const QString &path, m_paths)
        releaseAndDelete(path);
}

void TempPathDeleter::releaseAndDelete(const QString &path)
{
    if (m_paths.contains(path)) {
        try {
            m_paths.remove(path);
            if (QFileInfo(path).isDir()) {
                removeDirectory(path);
                return;
            }
            QFile file(path);
            if (file.exists() && !file.remove()) {
                throw Error(QCoreApplication::translate("QInstaller",
                    "Cannot remove file \"%1\": %2").arg(file.fileName(), file.errorString()));
            }
        } catch (const Error &e) {
            qCritical() << Q_FUNC_INFO << "Exception caught:" << e.message();
        } catch (...) {
            qCritical() << Q_FUNC_INFO << "Unknown exception caught.";
        }
    }
}

/*!
    Returns the given \a size in a measuring unit suffixed human readable format,
    with \a precision marking the number of shown decimals.
*/
QString QInstaller::humanReadableSize(const qint64 &size, int precision)
{
    double sizeAsDouble = size;
    static QStringList measures;
    if (measures.isEmpty())
        measures << QCoreApplication::translate("QInstaller", "bytes")
                 << QCoreApplication::translate("QInstaller", "KB")
                 << QCoreApplication::translate("QInstaller", "MB")
                 << QCoreApplication::translate("QInstaller", "GB")
                 << QCoreApplication::translate("QInstaller", "TB")
                 << QCoreApplication::translate("QInstaller", "PB")
                 << QCoreApplication::translate("QInstaller", "EB")
                 << QCoreApplication::translate("QInstaller", "ZB")
                 << QCoreApplication::translate("QInstaller", "YB");

    QStringListIterator it(measures);
    QString measure(it.next());

    while (sizeAsDouble >= 1024.0 && it.hasNext()) {
        measure = it.next();
        sizeAsDouble /= 1024.0;
    }
    return QString::fromLatin1("%1 %2").arg(sizeAsDouble, 0, 'f', precision).arg(measure);
}



// -- read, write operations

/*!
    \internal
*/
bool QInstaller::isLocalUrl(const QUrl &url)
{
    return url.scheme().isEmpty() || url.scheme().toLower() == QLatin1String("file");
}

/*!
    \internal
*/
QString QInstaller::pathFromUrl(const QUrl &url)
{
    if (isLocalUrl(url))
        return url.toLocalFile();
    const QString str = url.toString();
    if (url.scheme() == QLatin1String("resource"))
        return str.mid(QString::fromLatin1("resource").length());
    return str;
}

/*!
    \internal
*/
void QInstaller::removeFiles(const QString &path, bool ignoreErrors)
{
    const QFileInfoList entries = QDir(path).entryInfoList(QDir::AllEntries | QDir::Hidden);
    foreach (const QFileInfo &fi, entries) {
        if (fi.isSymLink() || fi.isFile()) {
            QString filePath = fi.filePath();
            QFile f(filePath);
            bool ok = f.remove();
            if (!ok) { //ReadOnly can prevent removing in Windows. Change permission and try again.
                const QFile::Permissions permissions = f.permissions();
                if (!(permissions & QFile::WriteUser)) {
                    ok = f.setPermissions(filePath, permissions | QFile::WriteUser)
                            && f.remove(filePath);
                }
                if (!ok) {
                    const QString errorMessage = QCoreApplication::translate("QInstaller",
                        "Cannot remove file \"%1\": %2").arg(
                                QDir::toNativeSeparators(f.fileName()), f.errorString());
                    if (!ignoreErrors)
                        throw Error(errorMessage);
                    qCWarning(QInstaller::lcInstallerInstallLog).noquote() << errorMessage;
                }
            }
        }
    }
}

static QString errnoToQString(int error)
{
#if defined(Q_OS_WIN) && !defined(Q_CC_MINGW)
    char msg[128];
    if (strerror_s(msg, sizeof msg, error) != 0)
        return QString::fromLocal8Bit(msg);
    return QString();
#else
    return QString::fromLocal8Bit(strerror(error));
#endif
}

/*!
    \internal

    Removes the directory at \a path recursively.
    @param path The directory to remove
    @param ignoreErrors if @p true, errors will be silently ignored. Otherwise an exception will be thrown
        if removing fails.

    @throws QInstaller::Error if the directory cannot be removed and ignoreErrors is @p false
*/
void QInstaller::removeDirectory(const QString &path, bool ignoreErrors)
{
    if (path.isEmpty()) // QDir("") points to the working directory! We never want to remove that one.
        return;

    QStringList dirs;
    QDirIterator it(path, QDir::NoDotAndDotDot | QDir::Dirs | QDir::NoSymLinks | QDir::Hidden,
        QDirIterator::Subdirectories);
    while (it.hasNext()) {
        dirs.prepend(it.next());
        removeFiles(dirs.at(0), ignoreErrors);
    }

    QDir d;
    dirs.append(path);
    removeFiles(path, ignoreErrors);
    foreach (const QString &dir, dirs) {
        errno = 0;
        if (d.exists(path) && !d.rmdir(dir)) {
            const QString errorMessage = QCoreApplication::translate("QInstaller",
                "Cannot remove directory \"%1\": %2").arg(QDir::toNativeSeparators(dir),
                                                          errnoToQString(errno));
            if (!ignoreErrors)
                throw Error(errorMessage);
            qCWarning(QInstaller::lcInstallerInstallLog).noquote() << errorMessage;
        }
    }
}

class RemoveDirectoryThread : public QThread
{
public:
    explicit RemoveDirectoryThread(const QString &path, bool ignoreErrors = false, QObject *parent = nullptr)
        : QThread(parent)
        , p(path)
        , ignore(ignoreErrors)
    {
        setObjectName(QLatin1String("RemoveDirectory"));
    }

    const QString &error() const
    {
        return err;
    }

protected:
    /*
     \reimp
     */
    void run()
    {
        try {
            removeDirectory(p, ignore);
        } catch (const Error &e) {
            err = e.message();
        }
    }

private:
    QString err;
    const QString p;
    const bool ignore;
};

/*!
    \internal
*/
void QInstaller::removeDirectoryThreaded(const QString &path, bool ignoreErrors)
{
    RemoveDirectoryThread thread(path, ignoreErrors);
    QEventLoop loop;
    QObject::connect(&thread, &RemoveDirectoryThread::finished, &loop, &QEventLoop::quit);
    thread.start();
    loop.exec();
    if (!thread.error().isEmpty())
        throw Error(thread.error());
}

/*!
    Removes system generated files from \a path on Windows and macOS. Does nothing on Linux.
*/
void QInstaller::removeSystemGeneratedFiles(const QString &path)
{
    if (path.isEmpty())
        return;
#if defined Q_OS_MACOS
    QFile::remove(path + QLatin1String("/.DS_Store"));
#elif defined Q_OS_WIN
    QFile::remove(path + QLatin1String("/Thumbs.db"));
#endif
}

/*!
    Sets permissions of file or directory specified by \a fileName to \c 644 or \c 755
    based by the value of \a permissions.

    Returns \c true on success, \c false otherwise.
*/
bool QInstaller::setDefaultFilePermissions(const QString &fileName, DefaultFilePermissions permissions)
{
    QFile file(fileName);
    return setDefaultFilePermissions(&file, permissions);
}

/*!
    Sets permissions of file or directory specified by \a file to \c 644 or \c 755
    based by the value of \a permissions. This is effective only on Unix platforms
    as \c setPermissions() does not manipulate ACLs. On Windows NTFS volumes this
    only unsets the legacy read-only flag regardless of the value of \a permissions.

    Returns \c true on success, \c false otherwise.
*/
bool QInstaller::setDefaultFilePermissions(QFile *file, DefaultFilePermissions permissions)
{
    if (!file->exists()) {
        qCWarning(QInstaller::lcInstallerInstallLog) << "Target" << file->fileName() << "does not exists.";
        return false;
    }
    if (file->permissions() == static_cast<QFileDevice::Permission>(permissions))
        return true;

    if (!file->setPermissions(static_cast<QFileDevice::Permission>(permissions))) {
        qCWarning(QInstaller::lcInstallerInstallLog) << "Cannot set default permissions for target"
                   << file->fileName() << ":" << file->errorString();
        return false;
    }
    return true;
}

/*!
    \internal
*/
void QInstaller::copyDirectoryContents(const QString &sourceDir, const QString &targetDir)
{
    Q_ASSERT(QFileInfo(sourceDir).isDir());
    Q_ASSERT(!QFileInfo::exists(targetDir) || QFileInfo(targetDir).isDir());
    if (!QDir().mkpath(targetDir)) {
        throw Error(QCoreApplication::translate("QInstaller", "Cannot create directory \"%1\".")
            .arg(QDir::toNativeSeparators(targetDir)));
    }
    QDirIterator it(sourceDir, QDir::NoDotAndDotDot | QDir::AllEntries);
    while (it.hasNext()) {
        const QFileInfo i(it.next());
        if (i.isDir()) {
            copyDirectoryContents(QDir(sourceDir).absoluteFilePath(i.fileName()),
                QDir(targetDir).absoluteFilePath(i.fileName()));
        } else {
            QFile f(i.filePath());
            const QString target = QDir(targetDir).absoluteFilePath(i.fileName());
            if (!f.copy(target)) {
                throw Error(QCoreApplication::translate("QInstaller",
                    "Cannot copy file from \"%1\" to \"%2\": %3").arg(
                                QDir::toNativeSeparators(f.fileName()),
                                QDir::toNativeSeparators(target),
                                f.errorString()));
            }
        }
    }
}

/*!
    \internal
*/
void QInstaller::moveDirectoryContents(const QString &sourceDir, const QString &targetDir)
{
    Q_ASSERT(QFileInfo(sourceDir).isDir());
    Q_ASSERT(!QFileInfo::exists(targetDir) || QFileInfo(targetDir).isDir());
    if (!QDir().mkpath(targetDir)) {
        throw Error(QCoreApplication::translate("QInstaller", "Cannot create directory \"%1\".")
            .arg(QDir::toNativeSeparators(targetDir)));
    }
    QDirIterator it(sourceDir, QDir::NoDotAndDotDot | QDir::AllEntries);
    while (it.hasNext()) {
        const QFileInfo i(it.next());
        if (i.isDir()) {
            // only copy directories that are not the target to avoid loop dir creations
            QString newSource = QDir(sourceDir).absoluteFilePath(i.fileName());
            if (QDir(newSource) != QDir(targetDir)) {
                moveDirectoryContents(newSource, QDir(targetDir).absoluteFilePath(i.fileName()));
            }
        } else {
            QFile f(i.filePath());
            const QString target = QDir(targetDir).absoluteFilePath(i.fileName());
            if (!f.rename(target)) {
                throw Error(QCoreApplication::translate("QInstaller",
                    "Cannot move file from \"%1\" to \"%2\": %3").arg(
                                QDir::toNativeSeparators(f.fileName()),
                                QDir::toNativeSeparators(target),
                                f.errorString()));
            }
        }
    }
}

/*!
    \internal
*/
void QInstaller::mkdir(const QString &path)
{
    errno = 0;
    if (!QDir().mkdir(QFileInfo(path).absoluteFilePath())) {
        throw Error(QCoreApplication::translate("QInstaller", "Cannot create directory \"%1\": %2")
            .arg(QDir::toNativeSeparators(path), errnoToQString(errno)));
    }
}

/*!
    \internal
*/
void QInstaller::mkpath(const QString &path)
{
    errno = 0;
    if (!QDir().mkpath(QFileInfo(path).absoluteFilePath())) {
        throw Error(QCoreApplication::translate("QInstaller", "Cannot create directory \"%1\": %2")
            .arg(QDir::toNativeSeparators(path), errnoToQString(errno)));
    }
}

/*!
    \internal

    Creates directory \a path including all parent directories. Return \c true on
    success, \c false otherwise.

    On Windows \c QDir::mkpath() doesn't check if the leading directories were created
    elsewhere (i.e. in another thread) after the initial check that the given path
    requires creating also parent directories, and returns \c false.

    On Unix platforms this case is handled different by QFileSystemEngine though,
    which checks for \c EEXIST error code in case any of the recursive directories
    could not be created.

    Compared to \c QInstaller::mkpath() and \c QDir::mkpath() this function checks if
    each parent directory to-be-created were created elsewhere.
*/
bool QInstaller::createDirectoryWithParents(const QString &path)
{
    if (path.isEmpty())
        return false;

    QFileInfo dirInfo(path);
    if (dirInfo.exists() && dirInfo.isDir())
        return true;

    // bail out if we are at the root directory
    if (dirInfo.isRoot())
        return false;

    QDir dir(path);
    if (dir.exists() || dir.mkdir(path))
        return true;

    // mkdir failed, try to create the parent directory
    if (!createDirectoryWithParents(dirInfo.absolutePath()))
        return false;

    // now try again
    if (dir.exists() || dir.mkdir(path))
        return true;

    // directory may be have also been created elsewhere
    return (dirInfo.exists() && dirInfo.isDir());
}

/*!
    \internal

    Generates and returns a temporary file name. The name can start with
    a template \a templ.
*/
QString QInstaller::generateTemporaryFileName(const QString &templ)
{
    if (templ.isEmpty()) {
        QTemporaryFile f;
        if (!f.open()) {
            throw Error(QCoreApplication::translate("QInstaller",
                "Cannot open temporary file: %1").arg(f.errorString()));
        }
        return f.fileName();
    }

    static const QString characters = QLatin1String("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890");
    QString suffix;
    for (int i = 0; i < 5; ++i)
        suffix += characters[QRandomGenerator::global()->generate() % characters.length()];

    const QString tmp = QLatin1String("%1.tmp.%2.%3");
    int count = 1;
    while (QFile::exists(tmp.arg(templ, suffix).arg(count)))
        ++count;

    QFile f(tmp.arg(templ, suffix).arg(count));
    if (!f.open(QIODevice::WriteOnly)) {
        throw Error(QCoreApplication::translate("QInstaller",
            "Cannot open temporary file for template %1: %2").arg(templ, f.errorString()));
    }
    f.remove();
    return f.fileName();
}

#ifdef Q_OS_WIN
#include <qt_windows.h>

/*!
    \internal
*/
QString QInstaller::getShortPathName(const QString &name)
{
    if (name.isEmpty())
        return name;

    // Determine length, then convert.
    const LPCTSTR nameC = reinterpret_cast<LPCTSTR>(name.utf16()); // MinGW
    const DWORD length = GetShortPathName(nameC, nullptr, 0);
    if (length == 0)
        return name;
    QScopedArrayPointer<TCHAR> buffer(new TCHAR[length]);
    GetShortPathName(nameC, buffer.data(), length);
    const QString rc = QString::fromUtf16(reinterpret_cast<const ushort *>(buffer.data()), length - 1);
    return rc;
}

/*!
    \internal
*/
QString QInstaller::getLongPathName(const QString &name)
{
    if (name.isEmpty())
        return name;

    // Determine length, then convert.
    const LPCTSTR nameC = reinterpret_cast<LPCTSTR>(name.utf16()); // MinGW
    const DWORD length = GetLongPathName(nameC, nullptr, 0);
    if (length == 0)
        return name;
    QScopedArrayPointer<TCHAR> buffer(new TCHAR[length]);
    GetLongPathName(nameC, buffer.data(), length);
    const QString rc = QString::fromUtf16(reinterpret_cast<const ushort *>(buffer.data()), length - 1);
    return rc;
}

/*!
    \internal

    Makes sure that capitalization of directory specified by \a name is canonical.
*/
QString QInstaller::normalizePathName(const QString &name)
{
    QString canonicalName = getShortPathName(name);
    if (canonicalName.isEmpty())
        return name;
    canonicalName = getLongPathName(canonicalName);
    if (canonicalName.isEmpty())
        return name;
    // Upper case drive letter
    if (canonicalName.size() > 2 && canonicalName.at(1) == QLatin1Char(':'))
        canonicalName[0] = canonicalName.at(0).toUpper();
    return canonicalName;
}

#pragma pack(push)
#pragma pack(2)

typedef struct {
    BYTE bWidth; // Width, in pixels, of the image
    BYTE bHeight; // Height, in pixels, of the image
    BYTE bColorCount; // Number of colors in image (0 if >=8bpp)
    BYTE bReserved; // Reserved
    WORD wPlanes; // Color Planes
    WORD wBitCount; // Bits per pixel
    DWORD dwBytesInRes; // how many bytes in this resource?
    DWORD dwImageOffset; // the ID
} ICONDIRENTRY;

typedef struct {
    WORD idReserved; // Reserved (must be 0)
    WORD idType; // Resource type (1 for icons)
    WORD idCount; // How many images?
    ICONDIRENTRY idEntries[1]; // The entries for each image
} ICONDIR;

typedef struct {
    BYTE bWidth; // Width, in pixels, of the image
    BYTE bHeight; // Height, in pixels, of the image
    BYTE bColorCount; // Number of colors in image (0 if >=8bpp)
    BYTE bReserved; // Reserved
    WORD wPlanes; // Color Planes
    WORD wBitCount; // Bits per pixel
    DWORD dwBytesInRes; // how many bytes in this resource?
    WORD nID; // the ID
} GRPICONDIRENTRY, *LPGRPICONDIRENTRY;

typedef struct {
    WORD idReserved; // Reserved (must be 0)
    WORD idType; // Resource type (1 for icons)
    WORD idCount; // How many images?
    GRPICONDIRENTRY idEntries[1]; // The entries for each image
} GRPICONDIR, *LPGRPICONDIR;


#pragma pack(pop)

/*!
    \internal

    Sets the .ico file at \a icon as application icon for \a application.
*/
void QInstaller::setApplicationIcon(const QString &application, const QString &icon)
{
    QFile iconFile(icon);
    if (!iconFile.open(QIODevice::ReadOnly)) {
        qCWarning(QInstaller::lcInstallerInstallLog) << "Cannot use" << icon << "as an application icon:"
            << iconFile.errorString();
        return;
    }

    if (QImageReader::imageFormat(icon) != "ico") {
        qCWarning(QInstaller::lcInstallerInstallLog) << "Cannot use" << icon << "as an application icon, "
            "unsupported format" << QImageReader::imageFormat(icon).constData();
        return;
    }

    QByteArray temp = iconFile.readAll();
    ICONDIR* ig = reinterpret_cast<ICONDIR*> (temp.data());

    DWORD newSize = sizeof(GRPICONDIR) + sizeof(GRPICONDIRENTRY) * (ig->idCount - 1);
    GRPICONDIR* newDir = reinterpret_cast< GRPICONDIR* >(new char[newSize]);
    newDir->idReserved = ig->idReserved;
    newDir->idType = ig->idType;
    newDir->idCount = ig->idCount;

    HANDLE updateRes = BeginUpdateResourceW((wchar_t*)QDir::toNativeSeparators(application).utf16(), false);
    for (int i = 0; i < ig->idCount; ++i) {
        char* temp1 = temp.data() + ig->idEntries[i].dwImageOffset;
        DWORD size1 = ig->idEntries[i].dwBytesInRes;

        newDir->idEntries[i].bWidth = ig->idEntries[i].bWidth;
        newDir->idEntries[i].bHeight = ig->idEntries[i].bHeight;
        newDir->idEntries[i].bColorCount = ig->idEntries[i].bColorCount;
        newDir->idEntries[i].bReserved = ig->idEntries[i].bReserved;
        newDir->idEntries[i].wPlanes = ig->idEntries[i].wPlanes;
        newDir->idEntries[i].wBitCount = ig->idEntries[i].wBitCount;
        newDir->idEntries[i].dwBytesInRes = ig->idEntries[i].dwBytesInRes;
        newDir->idEntries[i].nID = i + 1;

        UpdateResourceW(updateRes, RT_ICON, MAKEINTRESOURCE(i + 1),
            MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), temp1, size1);
    }

    UpdateResourceW(updateRes, RT_GROUP_ICON, L"IDI_ICON1", MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
        newDir, newSize);

    delete [] newDir;

    EndUpdateResourceW(updateRes, false);
}

static quint64 symlinkSizeWin(const QString &path)
{
    WIN32_FILE_ATTRIBUTE_DATA fileAttributeData;
    if (GetFileAttributesEx((wchar_t*)(path.utf16()), GetFileExInfoStandard, &fileAttributeData) == FALSE)
        return quint64(0);

    LARGE_INTEGER size;
    size.LowPart = fileAttributeData.nFileSizeLow;
    size.HighPart = fileAttributeData.nFileSizeHigh;
    return quint64(size.QuadPart);
}

#endif

/*!
    \internal
*/
quint64 QInstaller::fileSize(const QFileInfo &info)
{
    if (!info.isSymLink())
        return info.size();

#ifndef Q_OS_WIN
    struct stat buffer;
    if (lstat(qPrintable(info.absoluteFilePath()), &buffer) != 0)
        return quint64(0);
    return quint64(buffer.st_size);
#else
    return symlinkSizeWin(info.absoluteFilePath());
#endif
}

/*!
    \internal

    Returns \c true if a file specified by \a path points to a bundle. The
    absolute path for the bundle can be retrieved with \a bundlePath. Works only on macOS.
*/
bool QInstaller::isInBundle(const QString &path, QString *bundlePath)
{
#ifdef Q_OS_MACOS
    QFileInfo fi(QFileInfo(path).absoluteFilePath());
    while (!fi.isRoot()) {
        if (fi.isBundle()) {
            if (bundlePath)
                *bundlePath = fi.absoluteFilePath();
            return true;
        }
        fi.setFile(fi.path());
    }
#else
    Q_UNUSED(path)
    Q_UNUSED(bundlePath)
#endif
    return false;
}

/*!
    Replaces the path \a before with the path \a after at the beginning of \a path and returns
    the replaced path. If \a before cannot be found in \a path, the original value is returned.
    If \a cleanPath is \c true, path is returned with directory separators normalized (that is,
    platform-native separators converted to "/") and redundant ones removed, and "."s and ".."s
    resolved (as far as possible). If \a cleanPath is \c false, path is returned as such. Default
    value is \c true.
*/
QString QInstaller::replacePath(const QString &path, const QString &before, const QString &after, bool cleanPath)
{
    if (path.isEmpty() || before.isEmpty())
        return path;

    QString pathToPatch = QDir::cleanPath(path);
    const QString pathToReplace = QDir::cleanPath(before);
    if (pathToPatch.startsWith(pathToReplace)) {
        if (cleanPath)
            return QDir::cleanPath(after) + pathToPatch.mid(pathToReplace.size());
        else
            return after + path.mid(before.size());
    }
    return path;
}

/*!
    Replaces \a imagePath with high dpi image. If high dpi image is not provided or
    high dpi screen is not in use, the original value is returned.
*/
void QInstaller::replaceHighDpiImage(QString &imagePath)
{
    if (QGuiApplication::primaryScreen()->devicePixelRatio() >= 2 ) {
        QFileInfo fi(imagePath);
        QString highdpiPixmap = fi.absolutePath() + QLatin1Char('/') + fi.baseName() + scHighDpi + fi.suffix();
        if (QFileInfo::exists(highdpiPixmap))
            imagePath = highdpiPixmap;
    }
}

/*!
    Copies an internal configuration file from \a source to \a target. The XML elements,
    and their children, specified by \a elementsToRemoveTags will be removed from the \a target
    file. All relative filenames referenced in the \a source configuration file will be
    also copied to the location of the \a target file.

    Throws \c QInstaller::Error in case of failure.
*/
void QInstaller::trimmedCopyConfigData(const QString &source, const QString &target, const QStringList &elementsToRemoveTags)
{
    qCDebug(QInstaller::lcDeveloperBuild) << "Copying configuration file and associated data.";

    const QString targetPath = QFileInfo(target).absolutePath();
    if (!QDir(targetPath).exists() && !QDir().mkpath(targetPath)) {
        throw Error(QCoreApplication::translate("QInstaller",
            "Cannot create directory \"%1\".").arg(targetPath));
    }

    QFile xmlFile(source);
    if (!xmlFile.copy(target)) {
        throw Error(QCoreApplication::translate("QInstaller",
            "Cannot copy file \"%1\" to \"%2\": %3").arg(source, target, xmlFile.errorString()));
    }
    setDefaultFilePermissions(target, DefaultFilePermissions::NonExecutable);

    xmlFile.setFileName(target);
    QInstaller::openForRead(&xmlFile); // throws in case of error

    QDomDocument dom;
    dom.setContent(&xmlFile);
    xmlFile.close();

    foreach (auto elementTag, elementsToRemoveTags) {
        QDomNodeList elementsToRemove = dom.elementsByTagName(elementTag);
        for (int i = 0; i < elementsToRemove.length(); i++) {
            QDomNode elementToRemove = elementsToRemove.item(i);
            elementToRemove.parentNode().removeChild(elementToRemove);

            qCDebug(QInstaller::lcDeveloperBuild) << "Removed dom element from target file:"
                << elementToRemove.toElement().text();
        }
    }

    const QDomNodeList children = dom.documentElement().childNodes();
    copyConfigChildElements(dom, children, QFileInfo(source).absolutePath(), QFileInfo(target).absolutePath());

    QInstaller::openForWrite(&xmlFile); // throws in case of error
    QTextStream stream(&xmlFile);
    dom.save(stream, 4); // use 4 as the amount of space for indentation

    qCDebug(QInstaller::lcDeveloperBuild) << "Finished copying configuration data.";
}

/*!
    \internal

    Recursively iterates over a list of QDomNode \a objects belonging to \a dom and their
    children accordingly, searching for relative file names. Found files are copied from
    \a sourceDir to \a targetDir.

    Throws \c QInstaller::Error in case of failure.
*/
void QInstaller::copyConfigChildElements(QDomDocument &dom, const QDomNodeList &objects,
    const QString &sourceDir, const QString &targetDir)
{
    for (int i = 0; i < objects.length(); i++) {
        QDomElement domElement = objects.at(i).toElement();
        if (domElement.isNull())
            continue;

        // Iterate recursively over all child nodes
        const QDomNodeList elementChildren = domElement.childNodes();
        QInstaller::copyConfigChildElements(dom, elementChildren, sourceDir, targetDir);

        // Filename may also contain a path relative to source directory but we
        // copy it strictly into target directory without extra paths
        static const QRegularExpression regex(QLatin1String("\\\\|/|\\.|:"));
        const QString newName = domElement.text().replace(regex, QLatin1String("_"));

        const QString targetFile = targetDir + QDir::separator() + newName;
        const QFileInfo elementFileInfo = QFileInfo(sourceDir, domElement.text());

        if (!elementFileInfo.exists() || elementFileInfo.isDir())
            continue;

        domElement.replaceChild(dom.createTextNode(newName), domElement.firstChild());

        if (!QFile::copy(elementFileInfo.absoluteFilePath(), targetFile)) {
            throw Error(QCoreApplication::translate("QInstaller",
                "Cannot copy file \"%1\" to \"%2\".").arg(elementFileInfo.absoluteFilePath(), targetFile));
        }
    }
}
