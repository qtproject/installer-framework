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
#include "fileutils.h"

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

#include <errno.h>

#ifdef Q_OS_UNIX
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

using namespace QInstaller;


// -- TempDirDeleter

TempDirDeleter::TempDirDeleter(const QString &path)
{
    m_paths.insert(path);
}

TempDirDeleter::TempDirDeleter(const QStringList &paths)
    : m_paths(paths.toSet())
{
}

TempDirDeleter::~TempDirDeleter()
{
    releaseAndDeleteAll();
}

QStringList TempDirDeleter::paths() const
{
    return m_paths.toList();
}

void TempDirDeleter::add(const QString &path)
{
    m_paths.insert(path);
}

void TempDirDeleter::add(const QStringList &paths)
{
    m_paths += paths.toSet();
}

void TempDirDeleter::releaseAll()
{
    m_paths.clear();
}

void TempDirDeleter::release(const QString &path)
{
    m_paths.remove(path);
}

void TempDirDeleter::passAndReleaseAll(TempDirDeleter &tdd)
{
    tdd.m_paths = m_paths;
    releaseAll();
}

void TempDirDeleter::passAndRelease(TempDirDeleter &tdd, const QString &path)
{
    tdd.add(path);
    release(path);
}

void TempDirDeleter::releaseAndDeleteAll()
{
    foreach (const QString &path, m_paths)
        releaseAndDelete(path);
}

void TempDirDeleter::releaseAndDelete(const QString &path)
{
    if (m_paths.contains(path)) {
        try {
            m_paths.remove(path);
            removeDirectory(path);
        } catch (const Error &e) {
            qCritical() << Q_FUNC_INFO << "Exception caught:" << e.message();
        } catch (...) {
            qCritical() << Q_FUNC_INFO << "Unknown exception caught.";
        }
    }
}

QString QInstaller::humanReadableSize(const qint64 &size, int precision)
{
    double sizeAsDouble = size;
    static QStringList measures;
    if (measures.isEmpty())
        measures << QCoreApplication::translate("QInstaller", "bytes")
                 << QCoreApplication::translate("QInstaller", "KiB")
                 << QCoreApplication::translate("QInstaller", "MiB")
                 << QCoreApplication::translate("QInstaller", "GiB")
                 << QCoreApplication::translate("QInstaller", "TiB")
                 << QCoreApplication::translate("QInstaller", "PiB")
                 << QCoreApplication::translate("QInstaller", "EiB")
                 << QCoreApplication::translate("QInstaller", "ZiB")
                 << QCoreApplication::translate("QInstaller", "YiB");

    QStringListIterator it(measures);
    QString measure(it.next());

    while (sizeAsDouble >= 1024.0 && it.hasNext()) {
        measure = it.next();
        sizeAsDouble /= 1024.0;
    }
    return QString::fromLatin1("%1 %2").arg(sizeAsDouble, 0, 'f', precision).arg(measure);
}



// -- read, write operations

bool QInstaller::isLocalUrl(const QUrl &url)
{
    return url.scheme().isEmpty() || url.scheme().toLower() == QLatin1String("file");
}

QString QInstaller::pathFromUrl(const QUrl &url)
{
    if (isLocalUrl(url))
        return url.toLocalFile();
    const QString str = url.toString();
    if (url.scheme() == QLatin1String("resource"))
        return str.mid(QString::fromLatin1("resource").length());
    return str;
}

void QInstaller::removeFiles(const QString &path, bool ignoreErrors)
{
    const QFileInfoList entries = QDir(path).entryInfoList(QDir::AllEntries | QDir::Hidden);
    foreach (const QFileInfo &fi, entries) {
        if (fi.isSymLink() || fi.isFile()) {
            QFile f(fi.filePath());
            if (!f.remove()) {
                const QString errorMessage = QCoreApplication::translate("QInstaller",
                    "Could not remove file %1: %2").arg(f.fileName(), f.errorString());
                if (!ignoreErrors)
                    throw Error(errorMessage);
                qWarning() << errorMessage;
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
                "Could not remove folder %1: %2").arg(dir, errnoToQString(errno));
            if (!ignoreErrors)
                throw Error(errorMessage);
            qWarning() << errorMessage;
        }
    }
}

class RemoveDirectoryThread : public QThread
{
public:
    explicit RemoveDirectoryThread(const QString &path, bool ignoreErrors = false, QObject *parent = 0)
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
    /*!
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

void QInstaller::removeDirectoryThreaded(const QString &path, bool ignoreErrors)
{
    RemoveDirectoryThread thread(path, ignoreErrors);
    QEventLoop loop;
    QObject::connect(&thread, SIGNAL(finished()), &loop, SLOT(quit()));
    thread.start();
    loop.exec();
    if (!thread.error().isEmpty())
        throw Error(thread.error());
}

void QInstaller::removeSystemGeneratedFiles(const QString &path)
{
    if (path.isEmpty())
        return;
#if defined Q_OS_OSX
    QFile::remove(path + QLatin1String("/.DS_Store"));
#elif defined Q_OS_WIN
    QFile::remove(path + QLatin1String("/Thumbs.db"));
#endif
}

void QInstaller::copyDirectoryContents(const QString &sourceDir, const QString &targetDir)
{
    Q_ASSERT(QFileInfo(sourceDir).isDir());
    Q_ASSERT(!QFileInfo(targetDir).exists() || QFileInfo(targetDir).isDir());
    if (!QDir().mkpath(targetDir)) {
        throw Error(QCoreApplication::translate("QInstaller", "Could not create folder %1")
            .arg(targetDir));
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
                    "Could not copy file from %1 to %2: %3").arg(f.fileName(), target,
                    f.errorString()));
            }
        }
    }
}

void QInstaller::moveDirectoryContents(const QString &sourceDir, const QString &targetDir)
{
    Q_ASSERT(QFileInfo(sourceDir).isDir());
    Q_ASSERT(!QFileInfo(targetDir).exists() || QFileInfo(targetDir).isDir());
    if (!QDir().mkpath(targetDir)) {
        throw Error(QCoreApplication::translate("QInstaller", "Could not create folder %1")
            .arg(targetDir));
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
                    "Could not move file from %1 to %2: %3").arg(f.fileName(), target,
                    f.errorString()));
            }
        }
    }
}

void QInstaller::mkdir(const QString &path)
{
    errno = 0;
    if (!QDir().mkdir(QFileInfo(path).absoluteFilePath())) {
        throw Error(QCoreApplication::translate("QInstaller", "Could not create folder %1: %2")
            .arg(path, errnoToQString(errno)));
    }
}

void QInstaller::mkpath(const QString &path)
{
    errno = 0;
    if (!QDir().mkpath(QFileInfo(path).absoluteFilePath())) {
        throw Error(QCoreApplication::translate("QInstaller", "Could not create folder %1: %2")
            .arg(path, errnoToQString(errno)));
    }
}

QString QInstaller::generateTemporaryFileName(const QString &templ)
{
    if (templ.isEmpty()) {
        QTemporaryFile f;
        if (!f.open()) {
            throw Error(QCoreApplication::translate("QInstaller",
                "Could not open temporary file: %1").arg(f.errorString()));
        }
        return f.fileName();
    }

    static const QString characters = QLatin1String("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890");
    QString suffix;
    qsrand(qrand() * QDateTime::currentDateTime().toTime_t());
    for (int i = 0; i < 5; ++i)
        suffix += characters[qrand() % characters.length()];

    const QString tmp = QLatin1String("%1.tmp.%2.%3");
    int count = 1;
    while (QFile::exists(tmp.arg(templ, suffix).arg(count)))
        ++count;

    QFile f(tmp.arg(templ, suffix).arg(count));
    if (!f.open(QIODevice::WriteOnly)) {
        throw Error(QCoreApplication::translate("QInstaller",
            "Could not open temporary file for template %1: %2").arg(templ, f.errorString()));
    }
    f.remove();
    return f.fileName();
}

#ifdef Q_OS_WIN
#include <qt_windows.h>

QString QInstaller::getShortPathName(const QString &name)
{
    if (name.isEmpty())
        return name;

    // Determine length, then convert.
    const LPCTSTR nameC = reinterpret_cast<LPCTSTR>(name.utf16()); // MinGW
    const DWORD length = GetShortPathName(nameC, NULL, 0);
    if (length == 0)
        return name;
    QScopedArrayPointer<TCHAR> buffer(new TCHAR[length]);
    GetShortPathName(nameC, buffer.data(), length);
    const QString rc = QString::fromUtf16(reinterpret_cast<const ushort *>(buffer.data()), length - 1);
    return rc;
}

QString QInstaller::getLongPathName(const QString &name)
{
    if (name.isEmpty())
        return name;

    // Determine length, then convert.
    const LPCTSTR nameC = reinterpret_cast<LPCTSTR>(name.utf16()); // MinGW
    const DWORD length = GetLongPathName(nameC, NULL, 0);
    if (length == 0)
        return name;
    QScopedArrayPointer<TCHAR> buffer(new TCHAR[length]);
    GetLongPathName(nameC, buffer.data(), length);
    const QString rc = QString::fromUtf16(reinterpret_cast<const ushort *>(buffer.data()), length - 1);
    return rc;
}

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

void QInstaller::setApplicationIcon(const QString &application, const QString &icon)
{
    QFile iconFile(icon);
    if (!iconFile.open(QIODevice::ReadOnly)) {
        qWarning() << QString::fromLatin1("Could not use '%1' as application icon: %2.")
            .arg(icon, iconFile.errorString());
        return;
    }

    if (QImageReader::imageFormat(icon) != "ico") {
        qWarning() << QString::fromLatin1("Could not use '%1' as application icon, unsupported format %2.")
            .arg(icon, QLatin1String(QImageReader::imageFormat(icon)));
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

bool QInstaller::isInBundle(const QString &path, QString *bundlePath)
{
#ifdef Q_OS_OSX
    QFileInfo fi = QFileInfo(path).absoluteFilePath();
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
