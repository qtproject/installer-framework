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

#ifndef KDSYSINFO_H
#define KDSYSINFO_H

#include <kdtoolsglobal.h>

#include <QtCore/QString>

namespace KDUpdater {

class KDTOOLS_EXPORT VolumeInfo
{
public:
    VolumeInfo();
    static VolumeInfo fromPath(const QString &path);

    QString mountPath() const;
    void setMountPath(const QString &path);

    QString fileSystemType() const;
    void setFileSystemType(const QString &type);

    QString volumeDescriptor() const;
    void setVolumeDescriptor(const QString &descriptor);

    quint64 size() const;
    void setSize(const quint64 &size);

    quint64 availableSize() const;
    void setAvailableSize(const quint64 &available);

    bool operator==(const VolumeInfo &other) const;

private:
    QString m_mountPath;
    QString m_fileSystemType;
    QString m_volumeDescriptor;

    quint64 m_size;
    quint64 m_availableSize;
};

struct ProcessInfo
{
    quint32 id;
    QString name;
};

quint64 installedMemory();
QList<VolumeInfo> mountedVolumes();
QList<ProcessInfo> runningProcesses();

} // namespace KDUpdater

QT_BEGIN_NAMESPACE
class QDebug;
QT_END_NAMESPACE

QDebug operator<<(QDebug dbg, KDUpdater::VolumeInfo volume);
QDebug operator<<(QDebug dbg, KDUpdater::ProcessInfo process);

#endif // KDSYSINFO_H
