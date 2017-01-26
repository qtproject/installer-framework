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

#include <QDebug>
#include <QDateTime>
#include "windows.h"

void FileTimeToDateTime(const FILETIME *source, QDateTime *target)
{
    ULARGE_INTEGER store;
    QDateTime tempDateTime(QDate(1601, 1, 1));

    store.QuadPart  = source->dwHighDateTime;
    store.QuadPart  = store.QuadPart << 32;
    store.QuadPart += source->dwLowDateTime;

    *target = tempDateTime.addMSecs(store.QuadPart / 10000);
}

void DateTimeToSystemTime(const QDateTime *source, SYSTEMTIME *target)
{
    target->wYear = source->date().year();
    target->wMonth = source->date().month();
    target->wDayOfWeek = source->date().dayOfWeek();
    target->wDay = source->date().day();
    target->wHour = source->time().hour();
    target->wMinute = source->time().minute();
    target->wSecond = source->time().second();
    target->wMilliseconds = source->time().msec();
}


BOOL WINAPI FileTimeToSystemTime(CONST FILETIME *source,SYSTEMTIME *target)
{
    QDateTime tempDateTime;
    FileTimeToDateTime(source, &tempDateTime);
    DateTimeToSystemTime(&tempDateTime, target);

    return TRUE;
}

BOOL WINAPI SystemTimeToFileTime(const SYSTEMTIME *source,FILETIME *target)
{
    // TODO: Implementation!
    // This doesn't seem to be called at all

    qDebug() << "SystemTimeToFileTime";

    target->dwHighDateTime = 0;
    target->dwLowDateTime = 0;

    qWarning() << Q_FUNC_INFO;

    return TRUE;
}

BOOL WINAPI FileTimeToLocalFileTime(CONST FILETIME *source,FILETIME *target)
{
    target->dwHighDateTime = source->dwHighDateTime;
    target->dwLowDateTime  = source->dwLowDateTime;

    QDateTime tempDateTime;
    FileTimeToDateTime(source, &tempDateTime);

    tempDateTime = tempDateTime.toLocalTime();

    return TRUE;
}

BOOLEAN WINAPI RtlTimeToSecondsSince1970(const LARGE_INTEGER *Time, DWORD *Seconds)
{
    SYSTEMTIME tempSystemTime;
    FILETIME fileTime;

    fileTime.dwLowDateTime = Time->QuadPart;
    fileTime.dwHighDateTime = Time->QuadPart >> 32;

    FileTimeToSystemTime(&fileTime, &tempSystemTime);

    QDate targetDate(tempSystemTime.wYear, tempSystemTime.wMonth, tempSystemTime.wDay);
    QTime targetTime(tempSystemTime.wHour, tempSystemTime.wMinute, tempSystemTime.wSecond, tempSystemTime.wMilliseconds);
    QDateTime targetDateTime(targetDate, targetTime, Qt::UTC);

    quint64 secsSince1970 = targetDateTime.toMSecsSinceEpoch() / 1000;

    *Seconds = secsSince1970;

    return TRUE;
}

void WINAPI RtlSecondsSince1970ToFileTime(DWORD Seconds, FILETIME *ft)
{
    QDateTime fileTimeStartDate(QDate(1601, 1, 1));
    quint64 hnseconds = Seconds;
    QDateTime sourceDateTime = QDateTime::fromMSecsSinceEpoch(hnseconds * 1000);

    hnseconds = fileTimeStartDate.msecsTo(sourceDateTime);
    hnseconds *= 10000;

    ft->dwLowDateTime = hnseconds;
    ft->dwHighDateTime = hnseconds >> 32;
}

VOID WINAPI GetSystemTime(SYSTEMTIME *st)
{
    QDateTime nowDateTime = QDateTime::currentDateTimeUtc();
    DateTimeToSystemTime(&nowDateTime, st);
}
