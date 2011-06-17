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

#include <KDToolsCore/kdtoolsglobal.h>

#include <QtCore/QString>
#include <QtCore/QSysInfo>
#include <QtCore/QSharedDataPointer>

#include "kdbytesize.h"

class KDVersion;

class KDTOOLSCORE_EXPORT KDSysInfo : public QSysInfo
{
private:
    KDSysInfo();

public:
    ~KDSysInfo();

    enum OperatingSystemType
    {
        UnknownOperatingSystem = -1,
        Linux,
        MacOSX,
        Windows
    };

    enum ArchitectureType
    {
        UnknownArchitecture = -1,
        ARM,
        Intel,
        AMD64,
        IA64,
        PowerPC,
        Motorola68k
    };

    class KDTOOLSCORE_EXPORT Volume
    {
        friend class ::KDSysInfo;
    public:
        static Volume fromPath( const QString& path );

        Volume();
        Volume( const Volume& other );
        ~Volume();

        QString name() const;
        QString path() const;
        KDByteSize size() const;
        QString fileSystemType() const;
        KDByteSize availableSpace() const;

        void swap( Volume& other );
        Volume& operator=( const Volume& other );
        bool operator == ( const Volume& other ) const;

    private:
        void setPath( const QString& path );
        void setName( const QString& name );
        void setSize( const KDByteSize& size );
        void setFileSystemType(const QString &type);
        void setAvailableSpace( const KDByteSize& available );

    private:
        class Private;
        QSharedDataPointer<Private> d;
    };

    struct ProcessInfo
    {
        quint32 id;
        QString name;
    };

    static OperatingSystemType osType();
    static KDVersion osVersion();
    static QString osDescription();
    static ArchitectureType architecture();
    
    static KDByteSize installedMemory();
    static QList< Volume > mountedVolumes();
    static QList< ProcessInfo > runningProcesses();
};

QT_BEGIN_NAMESPACE
class QDebug;
QT_END_NAMESPACE

QDebug operator<<( QDebug dbg, KDSysInfo::OperatingSystemType type );
QDebug operator<<( QDebug dbg, KDSysInfo::ArchitectureType type );
QDebug operator<<( QDebug dbg, KDSysInfo::Volume volume );
QDebug operator<<( QDebug dbg, KDSysInfo::ProcessInfo process );

#endif
