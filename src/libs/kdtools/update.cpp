/****************************************************************************
**
** Copyright (C) 2013 Klaralvdalens Datakonsult AB (KDAB)
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
****************************************************************************/

#include "update.h"

using namespace KDUpdater;

/*!
   \inmodule kdupdater
   \class KDUpdater::Update
   \brief Represents a single update.

   The KDUpdater::Update class contains information about an update. It is created by KDUpdater::UpdateFinder
   corresponding to the update.

   The constructor of the KDUpdater::Update class is made protected, because it can be instantiated only by
   KDUpdater::UpdateFinder (which is a friend class). The destructor however is public.
*/

/*!
    \fn KDUpdater::Update::packageSource() const

    Returns the package source.
*/

/*!
   \internal
*/
Update::Update(const QInstaller::PackageSource &packageSource, const UpdateInfo &updateInfo)
    : m_packageSource(packageSource)
    , m_updateInfo(updateInfo)
{
}

/*!
   Returns the data specified by \a name, or an invalid \a defaultValue if the
   data does not exist.
*/
QVariant Update::data(const QString &name, const QVariant &defaultValue) const
{
    return m_updateInfo.data.value(name, defaultValue);
}
