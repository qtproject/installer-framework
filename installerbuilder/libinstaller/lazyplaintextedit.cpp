/**************************************************************************
**
** This file is part of Qt SDK**
**
** Copyright (c) 2011 Nokia Corporation and/or its subsidiary(-ies).*
**
** Contact:  Nokia Corporation qt-info@nokia.com**
**
** No Commercial Usage
**
** This file contains pre-release code and may not be distributed.
** You may use this file in accordance with the terms and conditions
** contained in the Technology Preview License Agreement accompanying
** this package.
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
#include "lazyplaintextedit.h"

#include <QScrollBar>

#define TIMER_TIME 20

LazyPlainTextEdit::LazyPlainTextEdit(QWidget *parent) :
    QPlainTextEdit(parent), m_timerId(0)
{
}

void LazyPlainTextEdit::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == m_timerId) {
        killTimer(m_timerId);
        m_timerId = 0;
        m_chachedOutput.chop(1); //removes the last \n
        appendPlainText(m_chachedOutput);
        horizontalScrollBar()->setValue( 0 );
        m_chachedOutput.clear();
    }
}

void LazyPlainTextEdit::append(const QString &text)
{
    //if (m_timerId) {
    //    killTimer(m_timerId);
    //    m_timerId = 0;
    //}
    m_chachedOutput.append(text + QLatin1String("\n"));
    if (isVisible() && m_timerId == 0) {
        m_timerId = startTimer(TIMER_TIME);
    }
}

void LazyPlainTextEdit::clear()
{
    if (m_timerId) {
        killTimer(m_timerId);
        m_timerId = 0;
        m_chachedOutput.clear();
    }
    QPlainTextEdit::clear();
}


void LazyPlainTextEdit::setVisible ( bool visible )
{
    if (m_timerId) {
        killTimer(m_timerId);
        m_timerId = 0;
    }
    if (visible) {
        m_timerId = startTimer(TIMER_TIME);
    }
    QPlainTextEdit::setVisible(visible);
}
