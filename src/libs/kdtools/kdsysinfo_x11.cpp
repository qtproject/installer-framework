/****************************************************************************
**
** Copyright (C) 2013 Klaralvdalens Datakonsult AB (KDAB)
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

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

        if (!s.startsWith(QLatin1Char('/')) && !s.startsWith(QLatin1String("tmpfs ") + QDir::tempPath()))
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
