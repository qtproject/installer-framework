/**************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
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
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
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
