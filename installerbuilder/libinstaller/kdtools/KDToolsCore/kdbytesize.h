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
#ifndef KDBYTESIZE_H
#define KDBYTESIZE_H

#include <KDToolsCore/kdtoolsglobal.h>

class KDTOOLSCORE_EXPORT KDByteSize
{
public:
    explicit KDByteSize( quint64 size = 0 );
    virtual ~KDByteSize();

    operator quint64() const;
    quint64 size() const;

    QString toString() const;

private:
    quint64 m_size;
};

KDTOOLSCORE_EXPORT bool operator==( const KDByteSize& lhs, const KDByteSize& rhs );
KDTOOLSCORE_EXPORT bool operator<( const KDByteSize& lhs, const KDByteSize& rhs );
KDTOOLSCORE_EXPORT KDByteSize operator*( const KDByteSize& lhs, int rhs );

KDTOOLS_MAKE_RELATION_OPERATORS( KDByteSize, static inline )

class QDebug;
KDTOOLSCORE_EXPORT QDebug operator<<( QDebug dbg, const KDByteSize& size );

#endif
