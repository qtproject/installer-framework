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

#ifndef __KDTOOLS_CORE_KDAUTOPOINTER_H__
#define __KDTOOLS_CORE_KDAUTOPOINTER_H__

#include <KDToolsCore/kdtoolsglobal.h>

#include <QtCore/QPointer>
#include <QtCore/QObject>

class KDAutoPointerBase {
protected:
    QPointer<QObject> o;

    KDAutoPointerBase() : o() {}
    explicit KDAutoPointerBase( QObject * obj ) : o( obj ) {}
    KDAutoPointerBase( KDAutoPointerBase & other ) : o( other.release() ) {}
    ~KDAutoPointerBase() { delete o; }

    void swap( KDAutoPointerBase & other ) {
        const QPointer<QObject> copy( o );
        o = other.o;
        other.o = copy;
    }

    QObject * release() { QObject * copy = o; o = 0 ; return copy; }
    void reset( QObject * other ) { if ( o != other ) { delete o; o = other; } }

    KDAB_IMPLEMENT_SAFE_BOOL_OPERATOR( o );
};

template <typename T>
class MAKEINCLUDES_EXPORT KDAutoPointer : KDAutoPointerBase {
public:
    friend inline void swap( KDAutoPointer & lhs, KDAutoPointer & rhs ) { lhs.swap( rhs ); }

    typedef T element_type;
    typedef T value_type;
    typedef T * pointer;

    KDAutoPointer() : KDAutoPointerBase() {}
    explicit KDAutoPointer( T * obj ) : KDAutoPointerBase( obj ) {}
    KDAutoPointer( KDAutoPointer & other ) : KDAutoPointerBase( other.release() ) {}
    template <typename U>
    KDAutoPointer( KDAutoPointer<U> & other ) : KDAutoPointerBase( other.release() ) { T * t = other.get(); ( void )t; }

    KDAutoPointer & operator=( KDAutoPointer & other ) {
        this->reset( other.release() );
        return *this;
    }

    template <typename U>
    KDAutoPointer & operator=( KDAutoPointer<U> & other ) {
        this->reset( other.release() );
        return *this;
    }

    ~KDAutoPointer() {}

    void swap( KDAutoPointer & other ) {
        KDAutoPointerBase::swap( other );
    }

    T * get() const { QObject * obj = o; return static_cast<T*>( obj ); }
    T * release() { return static_cast<T*>( KDAutoPointerBase::release() ); }

    T * data() const { return get(); }
    T * take()       { return release(); }

    void reset( T * other=0 ) { KDAutoPointerBase::reset( other ); }

    T & operator*() const { return *get(); }
    T * operator->() const { return get(); }

    template <typename S>
    bool operator==( const KDAutoPointer<S> & other ) const {
        return get() == other.get();
    }
    template <typename S>
    bool operator!=( const KDAutoPointer<S> & other ) const {
        return get() != other.get();
    }

    KDAB_USING_SAFE_BOOL_OPERATOR( KDAutoPointerBase )
};

template <typename T>
inline void swap( KDAutoPointer<T> & lhs, KDAutoPointer<T> & rhs ) { lhs.swap( rhs ); }
template <typename T>
inline void qSwap( KDAutoPointer<T> & lhs, KDAutoPointer<T> & rhs ) { lhs.swap( rhs ); }

template <typename S, typename T>
inline bool operator==( const KDAutoPointer<S> & lhs, const T * rhs ) {
    return lhs.get() == rhs;
}
template <typename S, typename T>
inline bool operator==( const S * lhs, const KDAutoPointer<T> & rhs ) {
    return lhs == rhs.get();
}
template <typename S, typename T>
inline bool operator==( const KDAutoPointer<S> & lhs, const QPointer<T> & rhs ) {
    return lhs.get() == rhs;
}
template <typename S, typename T>
inline bool operator==( const QPointer<S> & lhs, const KDAutoPointer<T> & rhs ) {
    return lhs == rhs.get();
}

template <typename S, typename T>
inline bool operator!=( const KDAutoPointer<S> & lhs, const T * rhs ) {
    return !operator==( lhs, rhs );
}
template <typename S, typename T>
inline bool operator!=( const S * lhs, const KDAutoPointer<T> & rhs ) {
    return !operator==( lhs, rhs );
}
template <typename S, typename T>
inline bool operator!=( const KDAutoPointer<S> & lhs, const QPointer<T> & rhs ) {
    return !operator==( lhs, rhs );
}
template <typename S, typename T>
inline bool operator!=( const QPointer<S> & lhs, const KDAutoPointer<T> & rhs ) {
    return !operator==( lhs, rhs );
}


#endif /* __KDTOOLS_CORE_KDAUTOPOINTER_H__ */
