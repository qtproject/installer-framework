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

#ifndef PACKAGESOURCE_H
#define PACKAGESOURCE_H

#include "installer_global.h"

#include <QUrl>

namespace QInstaller {

struct INSTALLER_EXPORT PackageSource
{
    PackageSource()
        : priority(-1)
    {}
    PackageSource(const QUrl &u, int p, bool pl = false)
        : url(u)
        , priority(p)
        , postLoadComponentScript(pl)
    {}

    QUrl url;
    int priority;
    bool postLoadComponentScript;
};

INSTALLER_EXPORT hashValue qHash(const PackageSource &key, hashValue seed);
INSTALLER_EXPORT bool operator==(const PackageSource &lhs, const PackageSource &rhs);

} // namespace QInstaller

#endif // PACKAGESOURCE_H
