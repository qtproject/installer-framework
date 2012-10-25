/**************************************************************************
**
** This file is part of Installer Framework
**
** Copyright (c) 2012 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (qt-info@nokia.com)
**
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** Other Usage
**
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
** If you have questions regarding the use of this file, please contact
** Nokia at qt-info@nokia.com.
**
**************************************************************************/

#include "link.h"

#include <QFileInfo>
#include <QDir>
#include <QDebug>

#ifdef Q_OS_LINUX
#include <unistd.h>
#endif

#ifdef Q_OS_WIN
#include <windows.h>
#include <strsafe.h>

// REPARSE_DATA_BUFFER structure from msdn help: http://msdn.microsoft.com/en-us/library/ff552012.aspx
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

#define _REPARSE_DATA_BUFFER_HEADER_SIZE FIELD_OFFSET(_REPARSE_DATA_BUFFER, GenericReparseBuffer)


class FileHandleWrapper
{
public:
    FileHandleWrapper(const QString &path)
        : m_dirHandle(INVALID_HANDLE_VALUE)
    {
        QString normalizedPath = QString(path).replace(QLatin1Char('/'), QLatin1Char('\\'));
        m_dirHandle = CreateFile((wchar_t*)normalizedPath.utf16(), GENERIC_READ | GENERIC_WRITE, 0, 0,
            OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, 0);

        if (m_dirHandle == INVALID_HANDLE_VALUE) {
            qWarning() << QString::fromLatin1("Could not open: '%1'; error: %2\n").arg(path).arg(GetLastError());
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
        if (::DeviceIoControl(dirHandle.handle(), FSCTL_GET_REPARSE_POINT, 0, 0, reparseStructData,
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
        qWarning() << QString::fromLatin1("Could not create the mount directory: %1").arg(
            linkPath);
        return Link(linkPath);
    }
    FileHandleWrapper dirHandle(linkPath);
    if (dirHandle.handle() == INVALID_HANDLE_VALUE) {
        qWarning() << QString::fromLatin1("Could not open: '%1'; error: %2\n").arg(linkPath).arg(
            GetLastError());
        return Link(linkPath);
    }

    TCHAR szDestDir[1024] = L"\\??\\"; //you need this to create valid unicode junctions

    QString normalizedTargetPath = QString(targetPath).replace(QLatin1Char('/'), QLatin1Char('\\'));
    //now we add the real absolute path
    StringCchCat(szDestDir, 1024, (wchar_t*)normalizedTargetPath.utf16());

    // Allocates a block of memory for an array of num elements(1) and initializes all its bits to zero.
    _REPARSE_DATA_BUFFER* reparseStructData = (_REPARSE_DATA_BUFFER*)calloc(1,
        MAXIMUM_REPARSE_DATA_BUFFER_SIZE);
    const size_t destMountPointBytes = lstrlen(szDestDir) * sizeof(WCHAR);

    reparseStructData->ReparseTag = IO_REPARSE_TAG_MOUNT_POINT;

    // Reserved(USHORT) + SubstituteNameOffset(USHORT) + SubstituteNameLength(USHORT)
    //      + PrintNameOffset(USHORT) + PrintNameLength(USHORT) + PathBuffer[1](WCHAR)
    uint spaceAfterGeneralData = sizeof(USHORT) * 5 + sizeof(WCHAR); //should be 12
    reparseStructData->ReparseDataLength = destMountPointBytes + spaceAfterGeneralData;
    reparseStructData->Reserved = 0;
    reparseStructData->MountPointReparseBuffer.SubstituteNameOffset = 0;
    reparseStructData->MountPointReparseBuffer.SubstituteNameLength = destMountPointBytes;
    // + sizeof(WCHAR) means \0 termination at the end of the string
    reparseStructData->MountPointReparseBuffer.PrintNameOffset = destMountPointBytes + sizeof(WCHAR);
    reparseStructData->MountPointReparseBuffer.PrintNameLength = 0;
    StringCchCopy(reparseStructData->MountPointReparseBuffer.PathBuffer, 1024, szDestDir);


    DWORD bytesReturned;
    if (!::DeviceIoControl(dirHandle.handle(), FSCTL_SET_REPARSE_POINT, reparseStructData,
        reparseStructData->ReparseDataLength + _REPARSE_DATA_BUFFER_HEADER_SIZE, 0, 0,
        &bytesReturned, 0)) {
            qWarning() << QString::fromLatin1("Could not set the reparse point: for '%1' to %2; error: %3"
                ).arg(linkPath, targetPath).arg(GetLastError());
    }
    return Link(linkPath);
}

bool removeJunction(const QString &path)
{
    // Allocates a block of memory for an array of num elements(1) and initializes all its bits to zero.
    _REPARSE_DATA_BUFFER* reparseStructData = (_REPARSE_DATA_BUFFER*)calloc(1,
        MAXIMUM_REPARSE_DATA_BUFFER_SIZE);

    reparseStructData->ReparseTag = IO_REPARSE_TAG_MOUNT_POINT;

    { // extra scope because we need to close the dirHandle before we can remove that directory
        FileHandleWrapper dirHandle(path);

        DWORD bytesReturned;
        if (!::DeviceIoControl(dirHandle.handle(), FSCTL_DELETE_REPARSE_POINT, reparseStructData,
            REPARSE_GUID_DATA_BUFFER_HEADER_SIZE, 0, 0,
            &bytesReturned, 0)) {

            qWarning() << QString::fromLatin1("Could not remove the reparse point: '%1'; error: %3"
                ).arg(path).arg(GetLastError());
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
        qWarning() << QString::fromLatin1("Could not create a symlink: from '%1' to %2; error: %3"
                    ).arg(linkPath, targetPath).arg(linkedError);
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
        qWarning() << QString::fromLatin1("Could not create the needed directories: %1").arg(
            link);
        return Link(link);
    }

#ifdef Q_OS_WIN
    if (QFileInfo(targetPath).isDir())
        return createJunction(link, targetPath);

    qWarning() << QString::fromLatin1("At the moment the %1 can not create anything else as "\
        "junctions for directories under windows").arg(QLatin1String(Q_FUNC_INFO));
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
    return QFileInfo(m_path).readLink();
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
