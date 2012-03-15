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

#include <sys/utsname.h>
#include <sys/statvfs.h>

#include <QtCore/QFile>
#include <QtCore/QTextStream>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>

namespace KDUpdater {

quint64 installedMemory()
{
#ifdef Q_OS_LINUX
    QFile f(QLatin1String("/proc/meminfo"));
    f.open(QIODevice::ReadOnly);
    QTextStream stream(&f);
    while (true) {
        const QString s = stream.readLine();
        if( !s.startsWith(QLatin1String("MemTotal:" )))
            continue;
        else if (s.isEmpty())
            return quint64();

        const QStringList parts = s.split(QLatin1Char(' '), QString::SkipEmptyParts);
        return quint64(parts.at(1).toInt() * 1024LL);
    }
#else
    quint64 physmem;
    size_t len = sizeof physmem;
    static int mib[2] = { CTL_HW, HW_MEMSIZE };
    sysctl(mib, 2, &physmem, &len, 0, 0);
    return quint64(physmem);
#endif
    return 0;
}

QList<VolumeInfo> mountedVolumes()
{
    QList<VolumeInfo> result;

    QFile f(QLatin1String("/etc/mtab"));
    if (!f.open(QIODevice::ReadOnly)) {
        qCritical("%s: Could not open %s: %s", Q_FUNC_INFO, qPrintable(f.fileName()), qPrintable(f.errorString()));
        return result; //better error-handling?
    }
    
    QTextStream stream(&f);
    while (true) {
        const QString s = stream.readLine();
        if (s.isNull())
            return result;

        if (!s.startsWith(QLatin1Char('/')))
            continue;

        const QStringList parts = s.split(QLatin1Char(' '), QString::SkipEmptyParts);

        VolumeInfo v;
        v.setMountPath(parts.at(1));
        v.setVolumeDescriptor(parts.at(0));
        v.setFileSystemType(parts.value(2));

        struct statvfs data;
        if (statvfs(qPrintable(v.mountPath() + QLatin1String("/.")), &data) == 0) {
            v.setSize(quint64(static_cast<quint64>(data.f_blocks) * data.f_bsize));
            v.setAvailableSize(quint64(static_cast<quint64>(data.f_bavail) * data.f_bsize));
        }
        result.append(v);
    }
    return result;
}

QList<ProcessInfo> runningProcesses()
{
    QList<ProcessInfo> processes;
    QDir procDir(QLatin1String("/proc"));
    const QFileInfoList procCont = procDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Readable);
    QRegExp validator(QLatin1String("[0-9]+"));
    Q_FOREACH (const QFileInfo &info, procCont) {
        if (validator.exactMatch(info.fileName())) {
            const QString linkPath = QDir(info.absoluteFilePath()).absoluteFilePath(QLatin1String("exe"));
            const QFileInfo linkInfo(linkPath);
            if (linkInfo.exists()) {
                ProcessInfo processInfo;
                processInfo.name = linkInfo.symLinkTarget();
                processInfo.id = info.fileName().toInt();
                processes.append(processInfo);
            }
        }
    }
    return processes;
}

} // namespace KDUpdater
