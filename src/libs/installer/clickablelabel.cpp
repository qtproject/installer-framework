/****************************************************************************
 **
 ** Copyright (C) 2024 The Qt Company Ltd.
 ** Contact: http://www.qt.io/licensing/
 **
 ** All Rights Reserved.
 **
 ** NOTICE: All information contained herein is, and remains
 ** the property of The Qt Company Ltd, if any.
 ** The intellectual and technical concepts contained
 ** herein are proprietary to The Qt Company Ltd
 ** and its suppliers and may be covered by Finnish and Foreign Patents,
 ** patents in process, and are protected by trade secret or copyright law.
 ** Dissemination of this information or reproduction of this material
 ** is strictly forbidden unless prior written permission is obtained
 ** from The Qt Company Ltd.
 ****************************************************************************/

#include "clickablelabel.h"

#include <QCheckBox>
#include <QMouseEvent>

using namespace QInstaller;

ClickableLabel::ClickableLabel(const QString &message, const QString &objectName, QWidget* parent)
    : QLabel(message, parent)
{
    setObjectName(objectName);
    setOpenExternalLinks(true);
    setWordWrap(true);
    setTextFormat(Qt::RichText);
    QFont clickableFont = font();
    clickableFont.setUnderline(true);
    setFont(clickableFont);
}

void ClickableLabel::mousePressEvent(QMouseEvent* event)
{
    Q_UNUSED(event)
    emit clicked();
}

bool ClickableLabel::event(QEvent *e)
{
    if (e->type() == QEvent::Enter)
        setCursor(Qt::PointingHandCursor);
    return QLabel::event(e);
}
