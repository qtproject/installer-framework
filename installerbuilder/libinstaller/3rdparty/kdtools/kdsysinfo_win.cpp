/****************************************************************************
** Copyright (C) 2001-2010 Klaralvdalens Datakonsult AB.  All rights reserved.
**
** This file is part of the KD Tools library.
**
** Licensees holding valid commercial KD Tools licenses may use this file in
** accordance with the KD Tools Commercial License Agreement provided with
** the Software.
**
**
** This file may be distributed and/or modified under the terms of the
** GNU Lesser General Public License version 2 and version 3 as published by the
** Free Software Foundation and appearing in the file LICENSE.LGPL included.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
** Contact info@kdab.com if any conditions of this licensing are not
** clear to you.
**
**********************************************************************/

#include "kdsysinfo.h"

#include <windows.h>
#include <Tlhelp32.h>
#include <Psapi.h>

#include <QDir>
#include <QPair>

#include <QtCore/QLibrary>

const int KDSYSINFO_PROCESS_QUERY_LIMITED_INFORMATION = 0x1000;

namespace KDUpdater {

quint64 installedMemory()
{
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    GlobalMemoryStatusEx(&status);
    return quint64(status.ullTotalPhys);
}

QPair<quint64, quint64> volumeSpace(const QString &volume)
{
    QPair<quint64, quint64> result;
    ULARGE_INTEGER bytes;
    ULARGE_INTEGER freebytes;
    if (GetDiskFreeSpaceExA(qPrintable(volume), 0, &bytes, &freebytes) != 0) {
        result.first = quint64(bytes.QuadPart);
        result.second = quint64(freebytes.QuadPart);
    }
    return result;
}

QString volumeName(const QString &volume)
{
    char name[MAX_PATH + 1] = "";
    DWORD dummy;
    char dummy2[MAX_PATH + 1] = "";
    GetVolumeInformationA(qPrintable(volume), name, MAX_PATH + 1, &dummy, &dummy, &dummy, dummy2, MAX_PATH + 1);
    QString vName = QString::fromLatin1(name);
    if (vName.isEmpty()) {
        const uint driveType = GetDriveTypeA(qPrintable(volume));
        switch (driveType) {
        case DRIVE_REMOVABLE:
            vName = QObject::tr("Removable Disk");
            break;
        case DRIVE_CDROM:
            vName = QObject::tr("CD Drive");
            break;
        case DRIVE_FIXED:
            vName = QObject::tr("Local Disk");
            break;
        default:
            return volume.left(2);
        }
    }
    return QString::fromLatin1("%2 (%1)").arg(volume.left(2), vName);
}

QString fileSystemType(const QString &path)
{
    char name[MAX_PATH + 1] = "";
    DWORD dummy;
    char fileSystem[MAX_PATH + 1] = "";
    BOOL result = GetVolumeInformationA(qPrintable(path), name, MAX_PATH + 1, &dummy, &dummy, &dummy,
        fileSystem, MAX_PATH + 1);
    if (result)
        return QLatin1String(fileSystem);
    return QLatin1String("unknown");
}

QList<VolumeInfo> mountedVolumes()
{
    QList<VolumeInfo> result;
    const QFileInfoList drives = QDir::drives();
    for (QFileInfoList::const_iterator it = drives.constBegin(); it != drives.constEnd(); ++it) {
        VolumeInfo volume;
        const QString path = QDir::toNativeSeparators(it->path());
        volume.setPath(path);
        volume.setName(volumeName(path));
        volume.setFileSystemType(fileSystemType(path));
        const QPair<quint64, quint64> sizes = volumeSpace(path);
        volume.setSize(sizes.first);
        volume.setAvailableSpace(sizes.second);
        result.push_back(volume);
    }
    return result;
}

struct EnumWindowsProcParam
{
    QList<ProcessInfo> processes;
    QList<quint32> seenIDs;
};

//BOOL CALLBACK EnumWindowsProc(HWND hWnd, LPARAM lParam)
//{
//    EnumWindowsProcParam* const list = reinterpret_cast< EnumWindowsProcParam* >( lParam );
//    ProcessInfo info;

//    // process id
//    DWORD procID = 0;
//    GetWindowThreadProcessId( hWnd, &procID );
//    info.id = procID;

//    if( list->seenIDs.contains( info.id ) )
//        return TRUE;
//    list->seenIDs.push_back( info.id );

//    HINSTANCE inst = (HINSTANCE)GetWindowLongA( hWnd, GWL_HINSTANCE );
//    // process name
//    HANDLE process = OpenProcess( PROCESS_ALL_ACCESS, FALSE, procID );

//    static char buffer[ 1024 ];
//    if( GetModuleFileNameExA( process, inst, buffer, sizeof( buffer ) ) )
//        info.name = QString::fromLocal8Bit( buffer );
//    CloseHandle( process );

//    list->processes.push_back( info );
//    return TRUE;
//}

//QList<ProcessInfo> runningProcesses()
//{
//    EnumWindowsProcParam param;
//    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
//    if (!snapshot)
//        return param.processes;
//    PROCESSENTRY32 processStruct;
//    processStruct.dwSize = sizeof(PROCESSENTRY32);
//    bool foundProcess = Process32First(snapshot, &processStruct);
//    while (foundProcess) {
//        //const QString executableName = QString::fromWCharArray( processStruct.szExeFile );
//        ProcessInfo info;
//        HANDLE procHandle = OpenProcess( PROCESS_QUERY_LIMITED_INFORMATION, false, processStruct.th32ProcessID );
//        char buffer[ 1024 ];
//        DWORD bufferSize = 1024;
//        const bool succ = QueryFullProcessImageNameA( procHandle, 0, buffer, &bufferSize );
//        if (succ) {
//            const QString executablepath = QString::fromLatin1( buffer );
//            const quint32 pid = processStruct.th32ProcessID;
//            param.seenIDs.append( pid );

//            info.id = pid;
//            info.name = executablepath;
//            param.processes.append( info );
//        }
//        CloseHandle( procHandle );

//        foundProcess =  Process32Next( snapshot, &processStruct );
//    }
//    if (snapshot)
//        CloseHandle( snapshot );
////    EnumDesktopWindows( 0, &EnumWindowsProc, reinterpret_cast< LPARAM >( &param ) );
//    return param.processes;
//}

typedef BOOL (WINAPI *QueryFullProcessImageNamePtr)(HANDLE, DWORD, char *, PDWORD);
typedef DWORD (WINAPI *GetProcessImageFileNamePtr)(HANDLE, char *, DWORD);

QList<ProcessInfo> runningProcesses()
{
    EnumWindowsProcParam param;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (!snapshot)
        return param.processes;
    PROCESSENTRY32 processStruct;
    processStruct.dwSize = sizeof(PROCESSENTRY32);
    bool foundProcess = Process32First(snapshot, &processStruct);
    const DWORD bufferSize = 1024;
    char driveBuffer[bufferSize];
    QStringList deviceList;
    if (QSysInfo::windowsVersion() <= QSysInfo::WV_5_2) {
        DWORD size = GetLogicalDriveStringsA(bufferSize, driveBuffer);
        deviceList = QString::fromLatin1(driveBuffer, size).split(QLatin1Char(char(0)), QString::SkipEmptyParts);
    }

    QLibrary kernel32(QLatin1String("Kernel32.dll"));
    kernel32.load();
    void *pQueryFullProcessImageNameA = kernel32.resolve("QueryFullProcessImageNameA");

    QLibrary psapi(QLatin1String("Psapi.dll"));
    psapi.load();
    void *pGetProcessImageFileNamePtr = psapi.resolve("GetProcessImageFileNameA");
    QueryFullProcessImageNamePtr callPtr = (QueryFullProcessImageNamePtr) pQueryFullProcessImageNameA;
    GetProcessImageFileNamePtr callPtrXp = (GetProcessImageFileNamePtr) pGetProcessImageFileNamePtr;

    while (foundProcess) {
        HANDLE procHandle = OpenProcess(QSysInfo::windowsVersion() > QSysInfo::WV_5_2
                                            ? KDSYSINFO_PROCESS_QUERY_LIMITED_INFORMATION
                                            : PROCESS_QUERY_INFORMATION,
                                         false,
                                         processStruct.th32ProcessID);
        char buffer[1024];
        DWORD bufferSize = 1024;
        bool succ = false;
        QString executablePath;
        ProcessInfo info;

        if (QSysInfo::windowsVersion() > QSysInfo::WV_5_2) {
            succ = callPtr(procHandle, 0, buffer, &bufferSize);
            executablePath = QString::fromLatin1(buffer);
        } else if (pGetProcessImageFileNamePtr) {
            succ = callPtrXp(procHandle, buffer, bufferSize);
            executablePath = QString::fromLatin1(buffer);
            for (int i = 0; i < deviceList.count(); ++i)
                executablePath.replace(QString::fromLatin1( "\\Device\\HarddiskVolume%1\\" ).arg(i + 1), deviceList.at(i));
        }

        if (succ) {
            const quint32 pid = processStruct.th32ProcessID;
            param.seenIDs.append(pid);
            info.id = pid;
            info.name = executablePath;
            param.processes.append(info);
        }

        CloseHandle(procHandle);
        foundProcess = Process32Next(snapshot, &processStruct);

    }
    if (snapshot)
        CloseHandle(snapshot);

    kernel32.unload();
    return param.processes;
}

} // namespace KDUpdater
