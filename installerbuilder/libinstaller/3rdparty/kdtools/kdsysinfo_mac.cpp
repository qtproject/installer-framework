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

#include <Carbon/Carbon.h>

#include <sys/mount.h>

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
    return QList<ProcessInfo>();
}

} // namespace KDUpdater
