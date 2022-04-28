/**************************************************************************
**
** Copyright (C) 2022 The Qt Company Ltd.
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

#include <QDesktopServices>
#include <QTimer>

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
    , m_discardMousePress(false)
{
    setMinimumSize(1, 1);
    setScaledContents(false);
    setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
}

/*!
    Sets the \a pixmap shown on the label. Setting a new pixmap clears
    the previous content.

    \note This redefines the non-virtual slot of the same signature from the
    QLabel base class, which results in non polymorphic behavior when
    called via a base class pointer.
*/
void AspectRatioLabel::setPixmap(const QPixmap &pixmap)
{
    setPixmapAndUrl(pixmap, QString());
}

/*!
    Sets the \a pixmap shown on the label and an \a url. Setting a new
    pixmap clears the previous content. When clicking the \a pixmap, \a url
    is opened in a browser. If the \a url is a reference to a file, it will
    be opened with a suitable application instead of a Web browser.
*/
void AspectRatioLabel::setPixmapAndUrl(const QPixmap &pixmap, const QString &url)
{
    m_clickableUrl = url;
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
    if (m_pixmap.isNull())
        return QPixmap();

    return m_pixmap.scaled(size() * m_pixmap.devicePixelRatio(),
        Qt::KeepAspectRatio, Qt::SmoothTransformation);
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

/*!
    \reimp
*/
void AspectRatioLabel::mousePressEvent(QMouseEvent* event)
{
    Q_UNUSED(event)
    if (!m_clickableUrl.isEmpty() && !m_discardMousePress)
        QDesktopServices::openUrl(m_clickableUrl);
}

/*!
    \reimp
*/
bool AspectRatioLabel::event(QEvent *e)
{
    if (e->type() == QEvent::WindowActivate) {
        QTimer::singleShot(100, [&]() {
            m_discardMousePress = false;
        });
    } else if (e->type() == QEvent::WindowDeactivate) {
        m_discardMousePress = true;
    }
    return QLabel::event(e);
}
