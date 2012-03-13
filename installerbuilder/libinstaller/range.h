/**************************************************************************
**
** This file is part of Installer Framework**
**
** Copyright (c) 2010 Nokia Corporation and/or its subsidiary(-ies).*
**
** Contact:  Nokia Corporation qt-info@nokia.com**
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception version
** 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** If you are unsure which license is appropriate for your use, please contact
** (qt-info@nokia.com).
**
**************************************************************************/
#ifndef RANGE_H
#define RANGE_H

#include <algorithm>

template <typename T>
class Range {
public:
    static Range<T> fromStartAndEnd( const T& start, const T& end ) {
        Range<T> r;
        r.m_start = start;
        r.m_end = end;
        return r;
    }

    static Range<T> fromStartAndLength( const T& start, const T& length ) {
        Range<T> r;
        r.m_start = start;
        r.m_end = start + length;
        return r;
    }

    Range() : m_start( 0 ), m_end( 0 ) {}

    T start() const { return m_start; }

    T end() const { return m_end; }

    void move( const T& by ) {
        m_start += by;
        m_end += by;
    }

    Range<T> moved( const T& by ) const {
        Range<T> b = *this;
        b.move( by );
        return b;
    }

    T length() const { return m_end - m_start; }

    Range<T> normalized() const {
        Range<T> r2( *this );
        if ( r2.m_start > r2.m_end )
            std::swap( r2.m_start, r2.m_end );
        return r2;
    }

    bool operator==( const Range<T>& other ) const {
        return m_start == other.m_start && m_end && other.m_end;
    }
    bool operator<( const Range<T>& other ) const {
        if ( m_start != other.m_start )
            return m_start < other.m_start;
        return m_end < other.m_end;
    }

private:
    T m_start;
    T m_end;
};

#endif /* RANGE_H_ */
