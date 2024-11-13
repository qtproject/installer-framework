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

#ifndef SPACEWIDGET_H
#define SPACEWIDGET_H

#include <QWidget>
#include <QLabel>

#include "packagemanagercore.h"

namespace QInstaller {

class INSTALLER_EXPORT SpaceWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SpaceWidget(PackageManagerCore *core, QWidget *parent = nullptr);

public:
    Q_INVOKABLE void updateSpaceRequiredText();

public Q_SLOTS:
    void installDirectoryChanged(const QString &newDirectory);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    PackageManagerCore *m_core;
    QLabel *m_spaceRequiredLabel;
    QLabel *m_spaceAvailableLabel;
};

}

#endif // SPACEWIDGET_H
