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

#include <QLibrary>
#include <QStringList>

#ifdef Q_CC_MINGW
# ifndef _WIN32_WINNT
#  define _WIN32_WINNT 0x0501
# endif
#endif

#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>

const int KDSYSINFO_PROCESS_QUERY_LIMITED_INFORMATION = 0x1000;

namespace KDUpdater {

quint64 installedMemory()
{
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    GlobalMemoryStatusEx(&status);
    return quint64(status.ullTotalPhys);
}

struct EnumWindowsProcParam
{
    QList<ProcessInfo> processes;
    QList<quint32> seenIDs;
};

typedef BOOL (WINAPI *QueryFullProcessImageNamePtr)(HANDLE, DWORD, char *, PDWORD);
typedef DWORD (WINAPI *GetProcessImageFileNamePtr)(HANDLE, char *, DWORD);

QList<ProcessInfo> runningProcesses()
{
    EnumWindowsProcParam param;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (!snapshot)
        return param.processes;

    QStringList deviceList;
    const DWORD bufferSize = 1024;
    char buffer[bufferSize + 1] = { 0 };
    if (QSysInfo::windowsVersion() <= QSysInfo::WV_5_2) {
        const DWORD size = GetLogicalDriveStringsA(bufferSize, buffer);
        deviceList = QString::fromLatin1(buffer, size).split(QLatin1Char(char(0)), QString::SkipEmptyParts);
    }

    QLibrary kernel32(QLatin1String("Kernel32.dll"));
    kernel32.load();
    QueryFullProcessImageNamePtr pQueryFullProcessImageNamePtr = (QueryFullProcessImageNamePtr) kernel32
        .resolve("QueryFullProcessImageNameA");

    QLibrary psapi(QLatin1String("Psapi.dll"));
    psapi.load();
    GetProcessImageFileNamePtr pGetProcessImageFileNamePtr = (GetProcessImageFileNamePtr) psapi
        .resolve("GetProcessImageFileNameA");

    PROCESSENTRY32 processStruct;
    processStruct.dwSize = sizeof(PROCESSENTRY32);
    bool foundProcess = Process32First(snapshot, &processStruct);
    while (foundProcess) {
        HANDLE procHandle = OpenProcess(QSysInfo::windowsVersion() > QSysInfo::WV_5_2
            ? KDSYSINFO_PROCESS_QUERY_LIMITED_INFORMATION : PROCESS_QUERY_INFORMATION, false, processStruct
                .th32ProcessID);

        bool succ = false;
        QString executablePath;
        DWORD bufferSize = 1024;

        if (QSysInfo::windowsVersion() > QSysInfo::WV_5_2) {
            succ = pQueryFullProcessImageNamePtr(procHandle, 0, buffer, &bufferSize);
            executablePath = QString::fromLatin1(buffer);
        } else if (pGetProcessImageFileNamePtr) {
            succ = pGetProcessImageFileNamePtr(procHandle, buffer, bufferSize);
            executablePath = QString::fromLatin1(buffer);
            for (int i = 0; i < deviceList.count(); ++i) {
                executablePath.replace(QString::fromLatin1( "\\Device\\HarddiskVolume%1\\" ).arg(i + 1),
                    deviceList.at(i));
            }
        }

        if (succ) {
            const quint32 pid = processStruct.th32ProcessID;
            param.seenIDs.append(pid);
            ProcessInfo info;
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
