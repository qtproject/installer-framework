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

#include "link.h"
#include "utils.h"
#include "globals.h"

#include <QFileInfo>
#include <QDir>
#include <QDebug>

#ifdef Q_OS_UNIX
#include <unistd.h>
#endif

#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <winioctl.h>

#ifndef Q_CC_MINGW
# include <strsafe.h>
#endif

#if !defined(REPARSE_DATA_BUFFER_HEADER_SIZE)

typedef struct _REPARSE_DATA_BUFFER {
    ULONG  ReparseTag;
    USHORT ReparseDataLength;
    USHORT Reserved;
    union {
        struct {
            USHORT SubstituteNameOffset;
            USHORT SubstituteNameLength;
            USHORT PrintNameOffset;
            USHORT PrintNameLength;
            ULONG  Flags;
            WCHAR  PathBuffer[1];
        } SymbolicLinkReparseBuffer;
        struct {
            USHORT SubstituteNameOffset;
            USHORT SubstituteNameLength;
            USHORT PrintNameOffset;
            USHORT PrintNameLength;
            WCHAR  PathBuffer[1];
        } MountPointReparseBuffer;
        struct {
            UCHAR DataBuffer[1];
        } GenericReparseBuffer;
    };
} REPARSE_DATA_BUFFER, *PREPARSE_DATA_BUFFER;
#define REPARSE_DATA_BUFFER_HEADER_SIZE FIELD_OFFSET(REPARSE_DATA_BUFFER, GenericReparseBuffer)

#endif


class FileHandleWrapper
{
public:
    FileHandleWrapper(const QString &path)
        : m_dirHandle(INVALID_HANDLE_VALUE)
    {
        QString normalizedPath = QString(path).replace(QLatin1Char('/'), QLatin1Char('\\'));
        m_dirHandle = CreateFile((wchar_t*)normalizedPath.utf16(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
            OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr);

        if (m_dirHandle == INVALID_HANDLE_VALUE) {
            qCWarning(QInstaller::lcInstallerInstallLog) << "Cannot open" << path << ":"
                << QInstaller::windowsErrorString(GetLastError());
        }
    }

    ~FileHandleWrapper() {
        if (m_dirHandle != INVALID_HANDLE_VALUE)
            CloseHandle(m_dirHandle);
    }

    HANDLE handle() {
        return m_dirHandle;
    }

private:
    HANDLE m_dirHandle;
};

QString readWindowsSymLink(const QString &path)
{
    QString result;
    FileHandleWrapper dirHandle(path);
    if (dirHandle.handle() != INVALID_HANDLE_VALUE) {
        REPARSE_DATA_BUFFER* reparseStructData = (REPARSE_DATA_BUFFER*)calloc(1, MAXIMUM_REPARSE_DATA_BUFFER_SIZE);
        DWORD bytesReturned = 0;
        if (::DeviceIoControl(dirHandle.handle(), FSCTL_GET_REPARSE_POINT, nullptr, 0, reparseStructData,
            MAXIMUM_REPARSE_DATA_BUFFER_SIZE, &bytesReturned, 0)) {
                if (reparseStructData->ReparseTag == IO_REPARSE_TAG_MOUNT_POINT) {
                    int length = reparseStructData->MountPointReparseBuffer.SubstituteNameLength / sizeof(wchar_t);
                    int offset = reparseStructData->MountPointReparseBuffer.SubstituteNameOffset / sizeof(wchar_t);
                    const wchar_t* PathBuffer = &reparseStructData->MountPointReparseBuffer.PathBuffer[offset];
                    result = QString::fromWCharArray(PathBuffer, length);
                } else if (reparseStructData->ReparseTag == IO_REPARSE_TAG_SYMLINK) {
                    int length = reparseStructData->SymbolicLinkReparseBuffer.SubstituteNameLength / sizeof(wchar_t);
                    int offset = reparseStructData->SymbolicLinkReparseBuffer.SubstituteNameOffset / sizeof(wchar_t);
                    const wchar_t* PathBuffer = &reparseStructData->SymbolicLinkReparseBuffer.PathBuffer[offset];
                    result = QString::fromWCharArray(PathBuffer, length);
                }
                // cut-off "//?/" and "/??/"
                if (result.size() > 4 && result.at(0) == QLatin1Char('\\') && result.at(2) == QLatin1Char('?') && result.at(3) == QLatin1Char('\\'))
                    result = result.mid(4);
        }
        free(reparseStructData);
    }
    return result;
}

Link createJunction(const QString &linkPath, const QString &targetPath)
{
    if (!QDir().mkpath(linkPath)) {
        qCWarning(QInstaller::lcInstallerInstallLog) << "Cannot create the mount directory" << linkPath;
        return Link(linkPath);
    }
    FileHandleWrapper dirHandle(linkPath);
    if (dirHandle.handle() == INVALID_HANDLE_VALUE) {
        qCWarning(QInstaller::lcInstallerInstallLog) << "Cannot open" << linkPath << ":"
            << QInstaller::windowsErrorString(GetLastError());
        return Link(linkPath);
    }

    const QString szDestDir = QString::fromLatin1("\\??\\%1").arg(targetPath).replace(QLatin1Char('/'),
        QLatin1Char('\\'));

    // Get string length in bytes, not in characters count
    const size_t destDirBytes = szDestDir.size() * sizeof(ushort);

    // Allocates a block of memory for an array of num elements(1) and initializes all its bits to zero.
    REPARSE_DATA_BUFFER* reparseStructData = (REPARSE_DATA_BUFFER*)calloc(1,
        MAXIMUM_REPARSE_DATA_BUFFER_SIZE);
    reparseStructData->Reserved = 0;
    reparseStructData->ReparseTag = IO_REPARSE_TAG_MOUNT_POINT;
    reparseStructData->MountPointReparseBuffer.PrintNameLength = 0;
    reparseStructData->MountPointReparseBuffer.SubstituteNameOffset = 0;
    reparseStructData->MountPointReparseBuffer.SubstituteNameLength = destDirBytes;
    reparseStructData->MountPointReparseBuffer.PrintNameOffset = destDirBytes + sizeof(WCHAR);

    uint spaceAfterGeneralData = sizeof(USHORT) * 5 + sizeof(WCHAR); //should be 12
    reparseStructData->ReparseDataLength = destDirBytes + spaceAfterGeneralData;

#ifndef Q_CC_MINGW
    StringCchCopy(reparseStructData->MountPointReparseBuffer.PathBuffer, 1024, (wchar_t*)szDestDir.utf16());
#else
    wcsncpy(reparseStructData->MountPointReparseBuffer.PathBuffer, (wchar_t*)szDestDir.utf16(), 1024);
#endif

    DWORD bytesReturned;
    if (!::DeviceIoControl(dirHandle.handle(), FSCTL_SET_REPARSE_POINT, reparseStructData,
        reparseStructData->ReparseDataLength + REPARSE_DATA_BUFFER_HEADER_SIZE, nullptr, 0,
        &bytesReturned, nullptr)) {
           qCWarning(QInstaller::lcInstallerInstallLog) << "Cannot set the reparse point for" << linkPath << "to" << targetPath
                       << ":" << QInstaller::windowsErrorString(GetLastError());
    }
    return Link(linkPath);
}

bool removeJunction(const QString &path)
{
    // Allocates a block of memory for an array of num elements(1) and initializes all its bits to zero.
    REPARSE_DATA_BUFFER* reparseStructData = (REPARSE_DATA_BUFFER*)calloc(1,
        MAXIMUM_REPARSE_DATA_BUFFER_SIZE);

    reparseStructData->ReparseTag = IO_REPARSE_TAG_MOUNT_POINT;

    { // extra scope because we need to close the dirHandle before we can remove that directory
        FileHandleWrapper dirHandle(path);

        DWORD bytesReturned;
        if (!::DeviceIoControl(dirHandle.handle(), FSCTL_DELETE_REPARSE_POINT, reparseStructData,
            REPARSE_GUID_DATA_BUFFER_HEADER_SIZE, nullptr, 0,
            &bytesReturned, nullptr)) {

            qCWarning(QInstaller::lcInstallerInstallLog) << "Cannot remove the reparse point" << path << ":"
                << QInstaller::windowsErrorString(GetLastError());
            return false;
        }
    }

    return QDir().rmdir(path);
}
#else
Link createLnSymlink(const QString &linkPath, const QString &targetPath)
{
    int linkedError = symlink(QFileInfo(targetPath).absoluteFilePath().toUtf8(),
        QFileInfo(linkPath).absoluteFilePath().toUtf8());
    if (linkedError != 0) {
        qCWarning(QInstaller::lcInstallerInstallLog) << "Cannot create a symlink from" << linkPath << "to"
            << targetPath << ":" << linkedError;
    }


    return Link(linkPath);
}

bool removeLnSymlink(const QString &path)
{
    return QFile::remove(path);
}

#endif

Link::Link(const QString &path) : m_path(path)
{
}

Link Link::create(const QString &link, const QString &targetPath)
{
    QStringList pathParts = QFileInfo(link).absoluteFilePath().split(QLatin1Char('/'));
    pathParts.removeLast();
    QString linkPath = pathParts.join(QLatin1String("/"));
    bool linkPathExists = QFileInfo(linkPath).exists();
    if (!linkPathExists)
        linkPathExists = QDir().mkpath(linkPath);
    if (!linkPathExists) {
        qCWarning(QInstaller::lcInstallerInstallLog) << "Cannot create the needed directories" << link;
        return Link(link);
    }

#ifdef Q_OS_WIN
    if (QFileInfo(targetPath).isDir())
        return createJunction(link, targetPath);

    qCWarning(QInstaller::lcInstallerInstallLog) << "At the moment the" << Q_FUNC_INFO
        << "can not create anything else as " << "junctions for directories under windows";
    return Link(link);
#else
    return createLnSymlink(link, targetPath);
#endif
}

QString Link::targetPath() const
{
#ifdef Q_OS_WIN
    return readWindowsSymLink(m_path);
#else
    return QFileInfo(m_path).symLinkTarget();
#endif
}

bool Link::exists()
{
#ifdef Q_OS_WIN
    return QFileInfo(m_path).exists();
#else
    return QFileInfo(m_path).isSymLink();
#endif
}

bool Link::targetExists()
{
    return QFileInfo(targetPath()).exists();
}

bool Link::isValid()
{
    return targetExists() && QFileInfo(m_path).exists();
}

bool Link::remove()
{
    if (!exists())
        return false;
#ifdef Q_OS_WIN
    return removeJunction(m_path);
#else
    return removeLnSymlink(m_path);
#endif
}
