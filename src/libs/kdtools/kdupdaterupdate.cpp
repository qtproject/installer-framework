/****************************************************************************
** Copyright (C) 2001-2010 Klaralvdalens Datakonsult AB.  All rights reserved.
**
** This file is part of the KD Tools library.
**
** Licensees holding valid commercial KD Tools licenses may use this file in
** accordance with the KD Tools Commercial License Agreement provided with
** the Software.
**
**
** This file may be distributed and/or modified under the terms of the
** GNU Lesser General Public License version 2 and version 3 as published by the
** Free Software Foundation and appearing in the file LICENSE.LGPL included.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
** Contact info@kdab.com if any conditions of this licensing are not
** clear to you.
**
**********************************************************************/

#include "kdupdaterupdate.h"

using namespace KDUpdater;

/*!
   \ingroup kdupdater
   \class KDUpdater::Update kdupdaterupdate.h KDUpdaterUpdate
   \brief Represents a single update

   The KDUpdater::Update class contains information about an update. It is created by KDUpdater::UpdateFinder
   corresponding to the update.

   The constructor of the KDUpdater::Update class is made protected, because it can be instantiated only by
   KDUpdater::UpdateFinder (which is a friend class). The destructor however is public.
*/


/*!
   \internal
*/
Update::Update(int priority, const QUrl &sourceInfoUrl, const QHash<QString, QVariant> &data)
    : m_priority(priority)
    , m_sourceInfoUrl(sourceInfoUrl)
    , m_data(data)
{
}

/*!
   Returns data whose name is given in parameter, or an invalid QVariant if the data doesn't exist.
*/
QVariant Update::data(const QString &name, const QVariant &defaultValue) const
{
    return m_data.value(name, defaultValue);
}

int Update::priority() const
{
    return m_priority;
}

QUrl Update::sourceInfoUrl() const
{
    return m_sourceInfoUrl;
}
