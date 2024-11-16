/**************************************************************************
**
** Copyright (C) 2024 The Qt Company Ltd.
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

#include "horizontalruler.h"

#include <QStylePainter>
#include <QStyleOption>

using namespace QInstaller;

HorizontalRuler::HorizontalRuler(QWidget *parent)
    : QWidget{parent}
{
    setFixedHeight(4);
    setObjectName(QLatin1String("HorizontalRuler"));
}

void HorizontalRuler::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QStylePainter painter(this);
    int x = width() - 2;
    int y = height() - 2;
    const QPalette &pal = palette();
    painter.setPen(pal.mid().color());
    painter.drawLine(0, y, x, y);
    painter.setPen(pal.base().color());
    painter.drawPoint(x + 1, y);
    painter.drawLine(0, y + 1, x + 1, y + 1);
}
