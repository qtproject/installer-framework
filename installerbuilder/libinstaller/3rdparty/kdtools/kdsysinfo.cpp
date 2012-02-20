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

#include <QtCore/QDebug>
#include <QtCore/QDir>

using namespace KDUpdater;

struct PathLongerThan
{
    bool operator()(const VolumeInfo &lhs, const VolumeInfo &rhs) const
    {
        return lhs.mountPath().length() > rhs.mountPath().length();
    }
};

VolumeInfo::VolumeInfo()
    : m_size(0)
    , m_availableSize(0)
{
}

VolumeInfo VolumeInfo::fromPath(const QString &path)
{
    QDir targetPath(QDir::cleanPath(path));
    QList<VolumeInfo> volumes = mountedVolumes();

    // sort by length to get the longest mount point (not just "/") first
    qSort(volumes.begin(), volumes.end(), PathLongerThan());
    foreach (const VolumeInfo &volume, volumes) {
        const QDir volumePath(volume.mountPath());
        if (targetPath == volumePath)
            return volume;
#ifdef Q_OS_WIN
        if (QDir::toNativeSeparators(path).toLower().startsWith(volume.mountPath().toLower()))
#else
        // we need to take some care here, as canonical path might return an empty string if the target
        // does not exist yet
        if (targetPath.exists()) {
            // the target exist, we can solve the path and if it fits return
            if (targetPath.canonicalPath().startsWith(volume.mountPath()))
                return volume;
            continue;
        }

        // the target directory does not exist yet, we need to cd up till we find the first existing dir
        QStringList parts = targetPath.absolutePath().split(QDir::separator(),QString::SkipEmptyParts);
        while (targetPath.absolutePath() != QDir::rootPath()) {
            if (targetPath.exists())
                break;
            parts.pop_back();
            if (parts.isEmpty())
                targetPath = QDir(QDir::rootPath());
            else
                targetPath = QDir(parts.join(QDir::separator()));
        }

        if (targetPath.canonicalPath().startsWith(volume.mountPath()))
#endif
            return volume;
    }
    return VolumeInfo();
}

QString VolumeInfo::mountPath() const
{
    return m_mountPath;
}

void VolumeInfo::setMountPath(const QString &path)
{
    m_mountPath = path;
}

QString VolumeInfo::fileSystemType() const
{
    return m_fileSystemType;
}

void VolumeInfo::setFileSystemType(const QString &type)
{
    m_fileSystemType = type;
}

QString VolumeInfo::volumeDescriptor() const
{
    return m_volumeDescriptor;
}

void VolumeInfo::setVolumeDescriptor(const QString &descriptor)
{
    m_volumeDescriptor = descriptor;
}

quint64 VolumeInfo::size() const
{
    return m_size;
}

void VolumeInfo::setSize(const quint64 &size)
{
    m_size = size;
}

quint64 VolumeInfo::availableSize() const
{
    return m_availableSize;
}

void VolumeInfo::setAvailableSize(const quint64 &available)
{
    m_availableSize = available;
}

bool VolumeInfo::operator==(const VolumeInfo &other) const
{
    return m_volumeDescriptor == other.m_volumeDescriptor;
}

QDebug operator<<(QDebug dbg, VolumeInfo volume)
{
    return dbg << "KDUpdater::Volume(" << volume.mountPath() << ")";
}

QDebug operator<<(QDebug dbg, ProcessInfo process)
{
    return dbg << "KDUpdater::ProcessInfo(" << process.id << ", " << process.name << ")";
}
