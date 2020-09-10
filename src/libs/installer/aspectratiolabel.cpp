/**************************************************************************
**
** Copyright (C) 2020 The Qt Company Ltd.
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

#include "aspectratiolabel.h"

using namespace QInstaller;

/*!
    \class QInstaller::AspectRatioLabel
    \inmodule QtInstallerFramework
    \brief The AspectRatioLabel class provides a label for displaying
        a pixmap that maintains its original aspect ratio when resized.
*/

/*!
    Constructs the label with \a parent as parent.
*/
AspectRatioLabel::AspectRatioLabel(QWidget *parent)
    : QLabel(parent)
{
    setMinimumSize(1, 1);
    setScaledContents(false);
}

/*!
    Sets the \a pixmap shown on the label. Setting a new pixmap clears the previous content.
*/
void AspectRatioLabel::setPixmap(const QPixmap &pixmap)
{
    m_pixmap = pixmap;
    QLabel::setPixmap(scaledPixmap());
}

/*!
    \reimp
*/
int AspectRatioLabel::heightForWidth(int w) const
{
    return m_pixmap.isNull()
        ? height()
        : m_pixmap.height() * w / m_pixmap.width();
}

/*!
    \reimp
*/
QSize AspectRatioLabel::sizeHint() const
{
    const int w = width();
    return QSize(w, heightForWidth(w));
}

/*!
    Returns the currently set pixmap scaled to the label size,
    while preserving the original aspect ratio.
*/
QPixmap AspectRatioLabel::scaledPixmap() const
{
    return m_pixmap.isNull()
        ? QPixmap()
        : m_pixmap.scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

/*!
    \reimp
*/
void AspectRatioLabel::resizeEvent(QResizeEvent *event)
{
    Q_UNUSED(event);

    if (!m_pixmap.isNull())
        QLabel::setPixmap(scaledPixmap());
}
