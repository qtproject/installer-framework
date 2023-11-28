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

#include "sysinfo.h"
#include "link.h"

#ifdef Q_CC_MINGW
# ifndef _WIN32_WINNT
#  define _WIN32_WINNT 0x0501
# endif
#endif

#include <qt_windows.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <winbase.h>
#include <winnetwk.h>

#ifndef Q_CC_MINGW
#pragma comment(lib, "mpr.lib")
#endif

#include <QDebug>
#include <QDir>
#include <QLibrary>

namespace KDUpdater {

VolumeInfo updateVolumeSizeInformation(const VolumeInfo &info)
{
    ULARGE_INTEGER bytesTotal;
    ULARGE_INTEGER freeBytesPerUser;

    VolumeInfo update = info;
    if (GetDiskFreeSpaceExA(qPrintable(info.volumeDescriptor()), &freeBytesPerUser, &bytesTotal, nullptr)) {
        update.setSize(bytesTotal.QuadPart);
        update.setAvailableSize(freeBytesPerUser.QuadPart);
    }
    return update;
}

/*
    Returns a list of volume info objects that are mounted as network drive shares.
*/
QList<VolumeInfo> networkVolumeInfosFromMountPoints()
{
    QList<VolumeInfo> volumes;
    QFileInfoList drives = QDir::drives();
    foreach (const QFileInfo &drive, drives) {
        const QString driveLetter = QDir::toNativeSeparators(drive.canonicalPath());
        const uint driveType = GetDriveTypeA(qPrintable(driveLetter));
        switch (driveType) {
            case DRIVE_REMOTE: {
                char buffer[1024] = "";
                DWORD bufferLength = 1024;
                UNIVERSAL_NAME_INFOA *universalNameInfo = (UNIVERSAL_NAME_INFOA*) &buffer;
                if (WNetGetUniversalNameA(qPrintable(driveLetter), UNIVERSAL_NAME_INFO_LEVEL,
                    LPVOID(universalNameInfo), &bufferLength) == NO_ERROR) {
                        VolumeInfo info;
                        info.setMountPath(driveLetter);
                        info.setVolumeDescriptor(QLatin1String(universalNameInfo->lpUniversalName));
                        volumes.append(info);
                }
            }   break;

            default:
                break;
        }
    }
    return volumes;
}

/*
    Returns a list of volume info objects based on the given \a volumeGUID. The function also solves mounted
    volume folder paths. It does not return any network drive shares.
*/
QList<VolumeInfo> localVolumeInfosFromMountPoints(PTCHAR volumeGUID)
{
#ifndef UNICODE
#define fromWCharArray fromLatin1
#endif
    QList<VolumeInfo> volumes;
    DWORD bufferSize;
    TCHAR volumeNames[MAX_PATH + 1] = { 0 };
    if (GetVolumePathNamesForVolumeName(volumeGUID, volumeNames, MAX_PATH, &bufferSize)) {
        QStringList mountedPaths = QString::fromWCharArray(volumeNames, bufferSize).split(QLatin1Char(char(0)),
            Qt::SkipEmptyParts);
        foreach (const QString &mountedPath, mountedPaths) {
            VolumeInfo info;
            info.setMountPath(mountedPath);
            info.setVolumeDescriptor(QString::fromWCharArray(volumeGUID));
            volumes.append(info);
        }
    }
    return volumes;
#ifndef UNICODE
#undef fromWCharArray
#endif
}

QList<VolumeInfo> mountedVolumes()
{
    // suppress message box shown while accessing possible unmounted devices
    const UINT old = SetErrorMode(SEM_FAILCRITICALERRORS);

    QList<VolumeInfo> tmp;
    TCHAR volumeGUID[MAX_PATH + 1] = { 0 };
    HANDLE handle = FindFirstVolume(volumeGUID, MAX_PATH);
    if (handle != INVALID_HANDLE_VALUE) {
        tmp += localVolumeInfosFromMountPoints(volumeGUID);
        while (FindNextVolume(handle, volumeGUID, MAX_PATH)) {
            tmp += localVolumeInfosFromMountPoints(volumeGUID);
        }
        FindVolumeClose(handle);
    }
    tmp += networkVolumeInfosFromMountPoints();

    QList<VolumeInfo> volumes;
    while (!tmp.isEmpty()) // update volume size information
        volumes.append(updateVolumeSizeInformation(tmp.takeFirst()));

    SetErrorMode(old);  // reset error mode
    return volumes;
}

bool pathIsOnLocalDevice(const QString &path)
{
    if (!QFileInfo::exists(path))
        return false;

    if (path.startsWith(QLatin1String("//")))
        return false;

    QDir dir(path);
    do {
        if (QFileInfo(dir, QString()).isSymLink()) {
            QString currentPath = QFileInfo(dir, QString()).absoluteFilePath();
            return pathIsOnLocalDevice(Link(currentPath).targetPath());
        }
    } while (dir.cdUp());

    const UINT DRIVE_REMOTE_TYPE = 4;
    if (path.contains(QLatin1Char(':'))) {
        const QLatin1Char nullTermination('\0');
        // for example "c:\"
        const QString driveSearchString = path.left(3) + nullTermination;
        WCHAR wCharDriveSearchArray[4];
        driveSearchString.toWCharArray(wCharDriveSearchArray);
        UINT type = GetDriveType(wCharDriveSearchArray);
        if (type == DRIVE_REMOTE_TYPE)
            return false;
    }

    return true;
}

bool CALLBACK TerminateAppEnum(HWND hwnd, LPARAM lParam)
{
   DWORD dwID;
   GetWindowThreadProcessId(hwnd, &dwID);

   if (dwID == (DWORD)lParam)
      PostMessage(hwnd, WM_CLOSE, 0, 0);
   return true;
}

bool killProcess(const ProcessInfo &process, int msecs)
{
    DWORD dwTimeout = msecs;
    if (msecs == -1)
        dwTimeout = INFINITE;

    // If we can't open the process with PROCESS_TERMINATE rights, then we give up immediately.
    HANDLE hProc = OpenProcess(SYNCHRONIZE | PROCESS_TERMINATE, false, process.id);
    if (hProc == nullptr)
        return false;

    // TerminateAppEnum() posts WM_CLOSE to all windows whose PID matches your process's.
    EnumWindows((WNDENUMPROC)TerminateAppEnum, (LPARAM)process.id);

    // Wait on the handle. If it signals, great. If it times out, then kill it.
    bool returnValue = false;
    if (WaitForSingleObject(hProc, dwTimeout) != WAIT_OBJECT_0)
        returnValue = TerminateProcess(hProc, 0);

    CloseHandle(hProc);
    return returnValue;
}

}
