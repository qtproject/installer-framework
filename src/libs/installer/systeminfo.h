/**************************************************************************
**
** Copyright (C) 2023 The Qt Company Ltd.
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

#ifndef SYSTEMINFO_H
#define SYSTEMINFO_H

#include <QObject>

namespace QInstaller {

class SystemInfo : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(SystemInfo)

    Q_PROPERTY(QString currentCpuArchitecture READ currentCpuArchitecture CONSTANT)
    Q_PROPERTY(QString buildCpuArchitecture READ buildCpuArchitecture CONSTANT)
    Q_PROPERTY(QString kernelType READ kernelType CONSTANT)
    Q_PROPERTY(QString kernelVersion READ kernelVersion CONSTANT)
    Q_PROPERTY(QString productType READ productType CONSTANT)
    Q_PROPERTY(QString productVersion READ productVersion CONSTANT)
    Q_PROPERTY(QString prettyProductName READ prettyProductName CONSTANT)

public:
    explicit SystemInfo(QObject *parent = 0);

    QString currentCpuArchitecture() const;
    QString buildCpuArchitecture() const;
    QString kernelType() const;
    QString kernelVersion() const;
    QString productType() const;
    QString productVersion() const;
    QString prettyProductName() const;
};

} // namespace QInstaller

#endif // SYSTEMINFO_H
