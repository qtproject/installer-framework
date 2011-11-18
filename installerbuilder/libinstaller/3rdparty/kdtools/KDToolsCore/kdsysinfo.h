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

class KDTOOLSCORE_EXPORT KDSysInfo : public QSysInfo
{
private:
    KDSysInfo();

public:
    ~KDSysInfo();

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
        quint64 size() const;
        QString fileSystemType() const;
        quint64 availableSpace() const;

        void swap( Volume& other );
        Volume& operator=( const Volume& other );
        bool operator == ( const Volume& other ) const;

    private:
        void setPath( const QString& path );
        void setName( const QString& name );
        void setSize( const quint64& size );
        void setFileSystemType(const QString &type);
        void setAvailableSpace( const quint64& available );

    private:
        class Private;
        QSharedDataPointer<Private> d;
    };

    struct ProcessInfo
    {
        quint32 id;
        QString name;
    };

    static quint64 installedMemory();
    static QList< Volume > mountedVolumes();
    static QList< ProcessInfo > runningProcesses();
};

QT_BEGIN_NAMESPACE
class QDebug;
QT_END_NAMESPACE

QDebug operator<<( QDebug dbg, KDSysInfo::Volume volume );
QDebug operator<<( QDebug dbg, KDSysInfo::ProcessInfo process );

#endif
