/****************************************************************************
**
** Copyright (C) 2013 Klaralvdalens Datakonsult AB (KDAB)
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
****************************************************************************/

#include "sysinfo.h"

#include <Carbon/Carbon.h>

#include <sys/mount.h>
#include <sys/sysctl.h>
#include <sys/types.h>

#include <QtCore/QList>

namespace KDUpdater {

quint64 installedMemory()
{
    SInt32 mb = 0;
    Gestalt(gestaltPhysicalRAMSizeInMegabytes, &mb);
    return quint64(static_cast<quint64>(mb) * 1024LL * 1024LL);
}

QList<VolumeInfo> mountedVolumes()
{
    QList<VolumeInfo> result;
    FSVolumeRefNum volume;
    FSVolumeInfo info;
    HFSUniStr255 volName;
    FSRef ref;
    int i = 0;

    while (FSGetVolumeInfo(kFSInvalidVolumeRefNum, ++i, &volume, kFSVolInfoFSInfo, &info, &volName, &ref) == 0) {
        UInt8 path[PATH_MAX + 1];
        if (FSRefMakePath(&ref, path, PATH_MAX) == 0) {
            FSGetVolumeInfo(volume, 0, 0, kFSVolInfoSizes, &info, 0, 0);

            VolumeInfo v;
            v.setSize(quint64(info.totalBytes));
            v.setAvailableSize(quint64(info.freeBytes));
            v.setMountPath(QString::fromLocal8Bit(reinterpret_cast< char* >(path)));

            struct statfs data;
            if (statfs(qPrintable(v.mountPath() + QLatin1String("/.")), &data) == 0) {
                v.setFileSystemType(QLatin1String(data.f_fstypename));
                v.setVolumeDescriptor(QLatin1String(data.f_mntfromname));
            }
            result.append(v);
        }
    }
    return result;
}

QList<ProcessInfo> runningProcesses()
{
    int mib[4] = {
        CTL_KERN,
        KERN_ARGMAX,
        0,
        0
    };

    int argMax = 0;
    size_t argMaxSize = sizeof(argMax);
    // fetch the maximum process arguments size
    sysctl(mib, 2, &argMax, &argMaxSize, NULL, 0);
    char *processArguments = (char*) malloc(argMax);

    mib[1] = KERN_PROC;
    mib[2] = KERN_PROC_ALL;
    size_t processTableSize = 0;
    // fetch the kernel process table size
    sysctl(mib, 4, NULL, &processTableSize, NULL, 0);
    struct kinfo_proc *processTable = (kinfo_proc*) malloc(processTableSize);

    // fetch the process table
    sysctl(mib, 4, processTable, &processTableSize, NULL, 0);

    QList<ProcessInfo> processes;
    for (size_t i = 0; i < (processTableSize / sizeof(struct kinfo_proc)); ++i) {
        struct kinfo_proc *process = processTable + i;

        ProcessInfo processInfo;
        processInfo.id = process->kp_proc.p_pid;

        mib[1] = KERN_PROCARGS2;
        mib[2] = process->kp_proc.p_pid;
        mib[3] = 0;

        size_t size = argMax;
        // fetch the process arguments
        if (sysctl(mib, 3, processArguments, &size, NULL, 0) != -1) {
            /*
             * |-----------------| <-- data returned by sysctl()
             * |      argc       |
             * |-----------------|
             * | executable path |
             * |-----------------|
             * |    arguments    |
             * ~~~~~~~~~~~~~~~~~~~
             * |-----------------|
            */
            processInfo.name = QString::fromLocal8Bit(processArguments + sizeof(int));
        } else {
            // if we fail, use the name from the process table
            processInfo.name = QString::fromLocal8Bit(process->kp_proc.p_comm);
        }
        processes.append(processInfo);
    }
    free(processTable);
    free(processArguments);

    return processes;
}

bool pathIsOnLocalDevice(const QString &path)
{
    Q_UNUSED(path);

    return true;
}

bool killProcess(const ProcessInfo &process, int msecs)
{
    Q_UNUSED(process);
    Q_UNUSED(msecs);

    return true;
}

} // namespace KDUpdater
