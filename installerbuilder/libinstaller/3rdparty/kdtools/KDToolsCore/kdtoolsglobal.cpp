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

#include "kdtoolsglobal.h"

#include <QByteArray>

#include <algorithm>

namespace {
    struct Version {
	unsigned char v[3];
    };

    static inline bool operator<( const Version & lhs, const Version & rhs ) {
	return std::lexicographical_compare( lhs.v, lhs.v + 3, rhs.v, rhs.v + 3 );
    }
    static inline bool operator==( const Version & lhs, const Version & rhs ) {
	return std::equal( lhs.v, lhs.v + 3, rhs.v );
    }
    KDTOOLS_MAKE_RELATION_OPERATORS( Version, static inline )
}

static Version kdParseQtVersion( const char * const version ) {
    if ( !version || qstrlen( version ) < 5 || version[1] != '.' || version[3] != '.' || ( version[5] != 0 && version[5] != '.' && version[5] != '-' ) )
	return Version(); // parse error
    const Version result = { { version[0] - '0', version[2] - '0', version[4] - '0' } };
    return result;
}

bool _kdCheckQtVersion_impl( int major, int minor, int patchlevel ) {
    static const Version actual = kdParseQtVersion( qVersion() ); // do this only once each run...
    const Version requested = { { major, minor, patchlevel } };
    return actual >= requested;
}
