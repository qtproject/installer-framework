/**************************************************************************
**
** This file is part of Installer Framework
**
** Copyright (c) 2011-2012 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (qt-info@nokia.com)
**
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
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** Other Usage
**
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
** If you have questions regarding the use of this file, please contact
** Nokia at qt-info@nokia.com.
**
**************************************************************************/

#include "lazyplaintextedit.h"

#include <QScrollBar>

const int INTERVAL = 20;

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
        if (!m_chachedOutput.isEmpty())
            appendPlainText(m_chachedOutput);
        horizontalScrollBar()->setValue( 0 );
        m_chachedOutput.clear();
    }
}

void LazyPlainTextEdit::append(const QString &text)
{
    m_chachedOutput.append(text + QLatin1String("\n"));
    if (isVisible() && m_timerId == 0)
        m_timerId = startTimer(INTERVAL);
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


void LazyPlainTextEdit::setVisible(bool visible)
{
    if (m_timerId) {
        killTimer(m_timerId);
        m_timerId = 0;
    }
    if (visible) {
        m_timerId = startTimer(INTERVAL);
    }
    QPlainTextEdit::setVisible(visible);
}
