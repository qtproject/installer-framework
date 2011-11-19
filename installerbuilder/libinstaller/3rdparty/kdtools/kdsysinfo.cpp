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

QDebug operator<<(QDebug dbg, KDSysInfo::Volume volume)
{
    return dbg << "KDSysInfo::Volume(" << volume.path() << ")";
}

QPair<quint64, quint64> volumeSpace(const QString &volume);
QString volumeName(const QString &volume);

KDSysInfo::Volume::Volume() 
{
    m_size = 0;
    m_availableSpace = 0;
}

void KDSysInfo::Volume::setPath(const QString &path)
{
    m_path = path;
}

bool KDSysInfo::Volume::operator==(const Volume &other) const
{
    return m_name == other.m_name && m_path == other.m_path;
}

void KDSysInfo::Volume::setName(const QString &name)
{
    m_name = name;
}

QString KDSysInfo::Volume::name() const
{
    return m_name;
}

QString KDSysInfo::Volume::path() const
{
    return m_path;
}

quint64 KDSysInfo::Volume::size() const
{
    return m_size;
}

void KDSysInfo::Volume::setSize(const quint64 &size)
{
    m_size = size;
}

QString KDSysInfo::Volume::fileSystemType() const
{
    return m_fileSystemType;
}

void KDSysInfo::Volume::setFileSystemType(const QString &type)
{
    m_fileSystemType = type;
}

quint64 KDSysInfo::Volume::availableSpace() const
{
    return m_availableSpace;
}

void KDSysInfo::Volume::setAvailableSpace(const quint64 &available)
{
    m_availableSpace = available;
}

struct PathLongerThan
{
    bool operator()(const KDSysInfo::Volume &lhs, const KDSysInfo::Volume &rhs) const
    {
        return lhs.path().length() > rhs.path().length();
    }
};

KDSysInfo::Volume KDSysInfo::Volume::fromPath(const QString &path)
{
    QList<KDSysInfo::Volume> volumes = mountedVolumes();
    // sort by length to get the longest mount point (not just "/") first
    std::sort(volumes.begin(), volumes.end(), PathLongerThan());
    for (QList< KDSysInfo::Volume >::const_iterator it = volumes.constBegin(); it != volumes.constEnd(); ++it) {
#ifdef Q_WS_WIN
        if (QDir::toNativeSeparators(path).toLower().startsWith(it->path().toLower()))
#else
        if (QDir(path).canonicalPath().startsWith(it->path()))
#endif
            return *it;
    }
    return KDSysInfo::Volume();
}

QDebug operator<<(QDebug dbg, KDSysInfo::ProcessInfo process)
{
    return dbg << "KDSysInfo::ProcessInfo(" << process.id << ", " << process.name << ")";
}
