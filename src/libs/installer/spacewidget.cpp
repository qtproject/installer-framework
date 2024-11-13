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

#include "spacewidget.h"

#include <QStyleOption>
#include <QPainter>
#include <QHBoxLayout>

#include "sysinfo.h"
#include "fileutils.h"

const QLatin1String SPACE_ITEM("|");
const QLatin1String SPACE_REQUIRED(QT_TR_NOOP("Space required: %1"));
const QLatin1String SPACE_AVAILABLE(QT_TR_NOOP("Space available: %1"));

using namespace QInstaller;

SpaceWidget::SpaceWidget(PackageManagerCore *core, QWidget *parent)
    : QWidget(parent)
    , m_core(core)
    , m_spaceRequiredLabel(nullptr)
    , m_spaceAvailableLabel(nullptr)
{
    setObjectName(QLatin1String("SpaceItem"));

    QHBoxLayout *spaceLabelLayout = new QHBoxLayout(this);
    spaceLabelLayout->setContentsMargins(-1, 0, -1, 0);

    m_spaceRequiredLabel = new QLabel();
    spaceLabelLayout->addWidget(m_spaceRequiredLabel);
    spaceLabelLayout->addWidget(new QLabel(SPACE_ITEM));

    m_spaceAvailableLabel = new QLabel();
    spaceLabelLayout->addWidget(m_spaceAvailableLabel);
    spaceLabelLayout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum));

    updateSpaceRequiredText();
    installDirectoryChanged(m_core->value(scTargetDir));
    connect(m_core, &PackageManagerCore::installDirectoryChanged, this, &SpaceWidget::installDirectoryChanged);
}

void SpaceWidget::updateSpaceRequiredText()
{
    if (m_spaceRequiredLabel)
        m_spaceRequiredLabel->setText(SPACE_REQUIRED.arg(humanReadableSize(m_core->requiredDiskSpace())));
}

void SpaceWidget::installDirectoryChanged(const QString &newDirectory)
{
    if (m_spaceAvailableLabel) {
        const KDUpdater::VolumeInfo targetVolume = KDUpdater::VolumeInfo::fromPath(newDirectory);
        m_spaceAvailableLabel->setText(SPACE_AVAILABLE.arg(humanReadableSize(targetVolume.availableSize())));
    }
}

void SpaceWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}
