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

#ifndef KDVERSION_H
#define KDVERSION_H

#include <KDToolsCore/kdtoolsglobal.h>

#include <QtCore/QStringList>

class KDTOOLSCORE_EXPORT KDVersion
{
    friend bool operator<( const KDVersion& lhs, const KDVersion& rhs ); 
    friend bool operator==( const KDVersion& lhs, const KDVersion& rhs ); 
public:
    KDVersion();
    virtual ~KDVersion();

    bool isNull() const;

    QString toString() const;
    static KDVersion fromString( const QString& string );

private:
    QStringList parts;
};

bool operator<( const KDVersion& lhs, const KDVersion& rhs );
bool operator==( const KDVersion& lhs, const KDVersion& rhs );

QDebug operator<<( QDebug debug, const KDVersion& version );

#endif
