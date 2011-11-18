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

#ifndef __KDTOOLS_KDTOOLSGLOBAL_H__
#define __KDTOOLS_KDTOOLSGLOBAL_H__

#include <QtCore/QtGlobal>

#ifdef KDTOOLS_SHARED
#  ifdef BUILD_SHARED_KDTOOLSCORE
#    define KDTOOLSCORE_EXPORT Q_DECL_EXPORT
#  else
#    define KDTOOLSCORE_EXPORT Q_DECL_IMPORT
#  endif
#  ifdef BUILD_SHARED_KDTOOLSGUI
#    define KDTOOLSGUI_EXPORT Q_DECL_EXPORT
#  else
#    define KDTOOLSGUI_EXPORT Q_DECL_IMPORT
#  endif
#  ifdef BUILD_SHARED_KDTOOLSXML
#    define KDTOOLSXML_EXPORT Q_DECL_EXPORT
#  else
#    define KDTOOLSXML_EXPORT Q_DECL_IMPORT
#  endif
#  ifdef BUILD_SHARED_KDUPDATER
#    define KDTOOLS_UPDATER_EXPORT    Q_DECL_EXPORT
#  else
#    define KDTOOLS_UPDATER_EXPORT    Q_DECL_IMPORT
#  endif
#else // KDTOOLS_SHARED
#  define KDTOOLSCORE_EXPORT
#  define KDTOOLSGUI_EXPORT
#  define KDTOOLSXML_EXPORT
#  define KDTOOLS_UPDATER_EXPORT
#endif // KDTOOLS_SHARED

#define KDTOOLS_MAKE_RELATION_OPERATORS( Class, linkage )             \
    linkage bool operator>( const Class & lhs, const Class & rhs ) {  \
        return operator<( rhs, lhs );                                 \
    }                                                                 \
    linkage bool operator!=( const Class & lhs, const Class & rhs ) { \
        return !operator==( lhs, rhs );                               \
    }                                                                 \
    linkage bool operator<=( const Class & lhs, const Class & rhs ) { \
        return !operator>( lhs, rhs );                                \
    }                                                                 \
    linkage bool operator>=( const Class & lhs, const Class & rhs ) { \
        return !operator<( lhs, rhs );                                \
    }



#endif /* __KDTOOLS_KDTOOLSGLOBAL_H__ */

