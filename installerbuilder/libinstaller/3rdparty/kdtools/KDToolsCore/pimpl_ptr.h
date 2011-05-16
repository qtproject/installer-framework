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

#ifndef __KDTOOLSCORE__PIMPL_PTR_H__
#define __KDTOOLSCORE__PIMPL_PTR_H__

#include <KDToolsCore/kdtoolsglobal.h>

#ifndef DOXYGEN_RUN
namespace kdtools {
#endif

    template <typename T>
    class MAKEINCLUDES_EXPORT pimpl_ptr {
        KDAB_DISABLE_COPY( pimpl_ptr );
        T * d;
    public:
        pimpl_ptr() : d( new T ) {}
        explicit pimpl_ptr( T * t ) : d( t ) {}
        ~pimpl_ptr() { delete d; d = 0; }

        T * get() { return d; }
        const T * get() const { return d; }

        T * operator->() { return get(); }
        const T * operator->() const { return get(); }

        T & operator*() { return *get(); }
        const T & operator*() const { return *get(); }

        KDAB_IMPLEMENT_SAFE_BOOL_OPERATOR( get() )
    };

    // these are not implemented, so's we can catch their use at
    // link-time. Leaving them undeclared would open up a comparison
    // via operator unspecified-bool-type().
    template <typename T, typename S>
    void operator==( const pimpl_ptr<T> &, const pimpl_ptr<S> & );
    template <typename T, typename S>
    void operator!=( const pimpl_ptr<T> &, const pimpl_ptr<S> & );

#ifndef DOXYGEN_RUN
} // namespace kdtools
#endif

#endif /* __KDTOOLSCORE__PIMPL_PTR_H__ */

