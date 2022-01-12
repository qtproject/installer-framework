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

#include "aboutapplicationdialog.h"

#include <packagemanagercore.h>
#include <settings.h>

#include <QGridLayout>
#include <QLabel>
#include <QDialogButtonBox>
#include <QPixmap>

#define QUOTE_(x) #x
#define QUOTE(x) QUOTE_(x)

#define ABOUT_IFW_VERSION "Built with Qt Installer Framework " QUOTE(IFW_VERSION_STR)
#define ABOUT_QT_VERSION "Based on Qt " QT_VERSION_STR
#define ABOUT_BUILD_DATE "Built on " __DATE__
#define ABOUT_BUILD_YEAR (__DATE__ + 7)
#define ABOUT_SHA "From revision " QUOTE(_GIT_SHA1_)

AboutApplicationDialog::AboutApplicationDialog(QInstaller::PackageManagerCore *core, QWidget *parent)
    : QDialog(parent)
{
    setObjectName(QLatin1String("AboutApplicationDialog"));

    const QString productName = core->value(QLatin1String("ProductName"));
    if (core->isInstaller())
        setWindowTitle(tr("About %1 installer").arg(productName));
    else
        setWindowTitle(tr("About %1 Maintenance Tool").arg(productName));

    QGridLayout *layout = new QGridLayout(this);
    layout->setSizeConstraint(QLayout::SetFixedSize);

    QLabel *logoLabel = new QLabel;
#ifdef Q_OS_MACOS
    QPixmap pixmap;
    if (pixmap.load(core->settings().installerApplicationIcon()))
        logoLabel->setPixmap(pixmap);
#else
    logoLabel->setPixmap(core->settings().installerWindowIcon());
#endif
    const QString description = QString::fromLatin1(
        "<h3>%1</h3>"
        "%2<br/>"
        "<br/>"
        "%3<br/>"
        "<br/>"
        "%4<br/>"
        "<br/>"
        "Copyright %5 The Qt Company Ltd. All rights reserved.<br/>"
        "<br/>"
        "The program is provided AS IS with NO WARRANTY OF ANY KIND, "
        "INCLUDING THE WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A "
        "PARTICULAR PURPOSE.<br/>")
        .arg(QLatin1String(ABOUT_IFW_VERSION),
             QLatin1String(ABOUT_QT_VERSION),
             QLatin1String(ABOUT_BUILD_DATE),
             QLatin1String(ABOUT_SHA),
             QLatin1String(ABOUT_BUILD_YEAR)
    );

    QLabel *aboutLabel = new QLabel(description);
    aboutLabel->setWordWrap(true);
    aboutLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    layout->addWidget(logoLabel , 0, 0, 1, 1);
    layout->addWidget(aboutLabel, 0, 1, 4, 4);
    layout->addWidget(buttonBox, 4, 0, 1, 5);
}
