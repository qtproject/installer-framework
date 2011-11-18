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

static QString qt_mac_hfsunistr_to_qstring( const HFSUniStr255* hfs )
{
    const QChar* const charPointer = reinterpret_cast< const QChar* >( hfs->unicode );
    return QString( charPointer, hfs->length );
}

quint64 KDSysInfo::installedMemory()
{
    SInt32 mb = 0;
    Gestalt( gestaltPhysicalRAMSizeInMegabytes, &mb );
    return quint64( static_cast< quint64 >( mb ) * 1024LL * 1024LL );
}

QList< KDSysInfo::Volume > KDSysInfo::mountedVolumes()
{
    QList< KDSysInfo::Volume > result;
    FSVolumeRefNum volume;
    FSVolumeInfo info;
    HFSUniStr255 volName;
    FSRef ref;
    int i = 0;

    while (FSGetVolumeInfo(kFSInvalidVolumeRefNum, ++i, &volume, kFSVolInfoFSInfo, &info, &volName, &ref) == 0) {
        UInt8 path[PATH_MAX + 1];
        if (FSRefMakePath(&ref, path, PATH_MAX) == 0) {
            Volume v;
            v.setName(qt_mac_hfsunistr_to_qstring(&volName));
            v.setPath(QString::fromLocal8Bit(reinterpret_cast< char* >(path)));

            FSGetVolumeInfo(volume, 0, 0, kFSVolInfoSizes, &info, 0, 0);
            v.setSize(quint64(info.totalBytes));
            v.setAvailableSpace(quint64(info.freeBytes));

            struct statfs data;
            if (statfs(qPrintable(v.path()), &data) == 0)
                v.setFileSystemType(QLatin1String(data.f_fstypename));

            result.append(v);
        }
    }
    return result;
}

QList< KDSysInfo::ProcessInfo > KDSysInfo::runningProcesses()
{
    return QList< KDSysInfo::ProcessInfo >();
}
