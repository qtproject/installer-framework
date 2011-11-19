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

#include <QDir>
#include <QDebug>

#include <algorithm>

using namespace KDUpdater;

QDebug operator<<(QDebug dbg, VolumeInfo volume)
{
    return dbg << "KDUpdater::Volume(" << volume.path() << ")";
}

//QPair<quint64, quint64> volumeSpace(const QString &volume);

//QString volumeName(const QString &volume);

VolumeInfo::VolumeInfo()
{
    m_size = 0;
    m_availableSpace = 0;
}

void VolumeInfo::setPath(const QString &path)
{
    m_path = path;
}

bool VolumeInfo::operator==(const VolumeInfo &other) const
{
    return m_name == other.m_name && m_path == other.m_path;
}

void VolumeInfo::setName(const QString &name)
{
    m_name = name;
}

QString VolumeInfo::name() const
{
    return m_name;
}

QString VolumeInfo::path() const
{
    return m_path;
}

quint64 VolumeInfo::size() const
{
    return m_size;
}

void VolumeInfo::setSize(const quint64 &size)
{
    m_size = size;
}

QString VolumeInfo::fileSystemType() const
{
    return m_fileSystemType;
}

void VolumeInfo::setFileSystemType(const QString &type)
{
    m_fileSystemType = type;
}

quint64 VolumeInfo::availableSpace() const
{
    return m_availableSpace;
}

void VolumeInfo::setAvailableSpace(const quint64 &available)
{
    m_availableSpace = available;
}

struct PathLongerThan
{
    bool operator()(const VolumeInfo &lhs, const VolumeInfo &rhs) const
    {
        return lhs.path().length() > rhs.path().length();
    }
};

VolumeInfo VolumeInfo::fromPath(const QString &path)
{
    QList<VolumeInfo> volumes = mountedVolumes();
    // sort by length to get the longest mount point (not just "/") first
    std::sort(volumes.begin(), volumes.end(), PathLongerThan());
    for (QList< VolumeInfo >::const_iterator it = volumes.constBegin(); it != volumes.constEnd(); ++it) {
#ifdef Q_WS_WIN
        if (QDir::toNativeSeparators(path).toLower().startsWith(it->path().toLower()))
#else
        if (QDir(path).canonicalPath().startsWith(it->path()))
#endif
            return *it;
    }
    return VolumeInfo();
}

QDebug operator<<(QDebug dbg, ProcessInfo process)
{
    return dbg << "KDUpdater::ProcessInfo(" << process.id << ", " << process.name << ")";
}
