/**************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
**************************************************************************/

#include "windows.h"

#include <QDateTime>

#include <chrono>

/*
    MSDN description about FILETIME structure:

    The FILETIME structure is a 64-bit value representing the number of 100-nanosecond intervals
    since January 1, 1601 (UTC).

    The DeltaToEpochInMsec can be calculated like this:

    \code
        quint64 delta = quint64(QDateTime(QDate(1601, 1, 1)).msecsTo(QDateTime(QDate(1970, 1, 1))))
            * 10000ULL;
    \endcode

    But since the value is static, we use a precalculated number here.
*/
static const ULONGLONG DeltaToEpochInMsec = 116444736000000000ULL;

inline void LocalDateTimeToFileTime(const QDateTime &dt, ULONGLONG utcOffsetInMsec, FILETIME *ft)
{
    const ULONGLONG msec = dt.toMSecsSinceEpoch() + utcOffsetInMsec;
    const ULONGLONG nano100 = (msec * 10000ULL) + DeltaToEpochInMsec;

    ft->dwLowDateTime = nano100;
    ft->dwHighDateTime = nano100 >> 32;

}

inline void UTCDateTimeToSystemTime(const QDateTime &dt, SYSTEMTIME *st)
{
    const QDate date = dt.date();
    const QTime time = dt.time();

    st->wYear = date.year();
    st->wMonth = date.month();
    st->wDayOfWeek = date.dayOfWeek();
    st->wDay = date.day();
    st->wHour = time.hour();
    st->wMinute = time.minute();
    st->wSecond = time.second();
    st->wMilliseconds = time.msec();
}


// -- WINAPI

BOOL WINAPI FileTimeToSystemTime(CONST FILETIME *source,SYSTEMTIME *target)
{
    const QDateTime dt = QDateTime(QDate(1601, 1, 1), QTime(0, 0, 0, 1), Qt::UTC).addMSecs
        ((ULONGLONG(source->dwLowDateTime) + (ULONGLONG(source->dwHighDateTime) << 32)) / 10000ULL);
    UTCDateTimeToSystemTime(dt.toUTC(), target);
    return TRUE;
}

BOOL WINAPI FileTimeToLocalFileTime(CONST FILETIME *source,FILETIME *target)
{
    const QDateTime dt = QDateTime(QDate(1601, 1, 1), QTime(0, 0, 0, 1), Qt::UTC).addMSecs
        ((ULONGLONG(source->dwLowDateTime) + (ULONGLONG(source->dwHighDateTime) << 32)) / 10000ULL);

    LocalDateTimeToFileTime(dt.toLocalTime(), ULONGLONG(QDateTime::currentDateTime()
        .offsetFromUtc()) * 1000ULL, target);
    return TRUE;
}

BOOLEAN WINAPI RtlTimeToSecondsSince1970(const LARGE_INTEGER *Time, DWORD *Seconds)
{
    /*
        MSDN suggests to implement the function like this:

        1. Call SystemTimeToFileTime to copy the system time to a FILETIME structure. Call
           GetSystemTime to get the current system time to pass to SystemTimeToFileTime.
        2. Copy the contents of the FILETIME structure to a ULARGE_INTEGER structure.
        3. Initialize a SYSTEMTIME structure with the date and time of the first second of
           January 1, 1970.
        4. Call SystemTimeToFileTime, passing the SYSTEMTIME structure initialized in Step 3
           to the call.
        5. Copy the contents of the FILETIME structure returned by SystemTimeToFileTime in Step 4
           to a second ULARGE_INTEGER. The copied value should be less than or equal to the value
           copied in Step 2.
        6. Subtract the 64-bit value in the ULARGE_INTEGER structure initialized in Step 5
           (January 1, 1970) from the 64-bit value of the ULARGE_INTEGER structure initialized
           in Step 2 (the current system time). This produces a value in 100-nanosecond intervals
           since January 1, 1970. To convert this value to seconds, divide by 10,000,000.

            We can omit step 1 and 2, cause we get the LARGE_INTEGER passed as function argument.
    */
    SYSTEMTIME stFirstSecondOf1979;
    stFirstSecondOf1979.wSecond = 1;
    stFirstSecondOf1979.wMinute = 0;
    stFirstSecondOf1979.wHour = 0;
    stFirstSecondOf1979.wDay = 1;
    stFirstSecondOf1979.wMonth = 1;
    stFirstSecondOf1979.wYear = 1970;
    stFirstSecondOf1979.wDayOfWeek = 4;

    FILETIME ftFirstSecondOf1979;
    SystemTimeToFileTime(&stFirstSecondOf1979, &ftFirstSecondOf1979);

    LARGE_INTEGER liFirstSecondOf1979;
    liFirstSecondOf1979.LowPart = ftFirstSecondOf1979.dwLowDateTime;
    liFirstSecondOf1979.HighPart = ftFirstSecondOf1979.dwHighDateTime;

    const ULONGLONG diffNano100 = Time->QuadPart - liFirstSecondOf1979.QuadPart;
    *Seconds = diffNano100 / 10000000ULL;

    return TRUE;
}

void WINAPI RtlSecondsSince1970ToFileTime(DWORD Seconds, FILETIME *ft)
{
    const ULONGLONG nano100 = (ULONGLONG(Seconds) * 10000000ULL) + DeltaToEpochInMsec;
    ft->dwLowDateTime = nano100;
    ft->dwHighDateTime = nano100 >> 32;
}

VOID WINAPI GetSystemTime(SYSTEMTIME *st)
{
    UTCDateTimeToSystemTime(QDateTime::currentDateTimeUtc(), st);
}

VOID WINAPI GetSystemTimeAsFileTime(FILETIME *time)
{
    SYSTEMTIME st;
    GetSystemTime(&st);
    SystemTimeToFileTime(&st, time);
}

DWORD WINAPI GetTickCount()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

BOOL WINAPI SystemTimeToFileTime(const SYSTEMTIME *lpSystemTime, FILETIME *lpFileTime)
{
    const QDateTime dt(QDate(lpSystemTime->wYear, lpSystemTime->wMonth, lpSystemTime->wDay),
        QTime(lpSystemTime->wHour, lpSystemTime->wMinute, lpSystemTime->wSecond,
            lpSystemTime->wMilliseconds), Qt::UTC);

    LocalDateTimeToFileTime(dt.toLocalTime(), 0ULL, lpFileTime);
    return TRUE;
}
