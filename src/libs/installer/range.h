/**************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
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
