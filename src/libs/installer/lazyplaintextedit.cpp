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

#include "lazyplaintextedit.h"

#include <QScrollBar>

const int INTERVAL = 20;

LazyPlainTextEdit::LazyPlainTextEdit(QWidget *parent)
    : QPlainTextEdit(parent)
    , m_timerId(0)
{
}

void LazyPlainTextEdit::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == m_timerId) {
        killTimer(m_timerId);
        m_timerId = 0;
        m_cachedOutput.chop(1); //removes the last \n
        if (!m_cachedOutput.isEmpty()) {
            appendPlainText(m_cachedOutput);
            updateCursor(TextCursorPosition::Keep);
            horizontalScrollBar()->setValue(0);
            m_cachedOutput.clear();
        }
    }
}

void LazyPlainTextEdit::append(const QString &text)
{
    m_cachedOutput.append(text + QLatin1String("\n"));
    if (isVisible() && m_timerId == 0)
        m_timerId = startTimer(INTERVAL);
}

void LazyPlainTextEdit::clear()
{
    if (m_timerId) {
        killTimer(m_timerId);
        m_timerId = 0;
        m_cachedOutput.clear();
    }
    QPlainTextEdit::clear();
}

void LazyPlainTextEdit::setVisible(bool visible)
{
    if (m_timerId) {
        killTimer(m_timerId);
        m_timerId = 0;
    }

    if (visible)
        m_timerId = startTimer(INTERVAL);

    QPlainTextEdit::setVisible(visible);
    updateCursor(TextCursorPosition::Keep);
}

void LazyPlainTextEdit::updateCursor(TextCursorPosition position)
{
    QTextCursor cursor = textCursor();
    if ((position == TextCursorPosition::ForceEnd) || cursor.atEnd()) {
        // Workaround for height calculation issue if scrollbar is set to Qt::ScrollBarAsNeeded.
        Qt::ScrollBarPolicy policy = horizontalScrollBarPolicy();
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn); // enforce always on

        cursor.movePosition(QTextCursor::End);
        setTextCursor(cursor);
        ensureCursorVisible();

        setHorizontalScrollBarPolicy(policy); // but reset once we updated the cursor position
    }
}
