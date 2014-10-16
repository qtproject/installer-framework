/**************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
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
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
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
