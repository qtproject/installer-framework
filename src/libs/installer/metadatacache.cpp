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

#include "metadatacache.h"

#define QUOTE_(x) #x
#define QUOTE(x) QUOTE_(x)

namespace QInstaller {

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::MetadataCache
    \brief The MetadataCache is a class for a checksum based storage of \c Metadata objects on disk.

    MetadataCache manages a cache storage for a set \l{path()}, which contains
    a subdirectory for each registered \c Metadata item. The cache has a manifest file in
    its root directory, which lists the version and type of the cache, and all its items.
    The file is updated automatically when the metadata cache object is destructed, or
    it can be updated periodically by calling \l{sync()}.
*/

/*!
    Constructs a new empty cache. The cache is invalid until set with a
    path and initialized.
*/
MetadataCache::MetadataCache()
    : GenericDataCache<Metadata>()
{
    setType(QLatin1String("Metadata"));
    setVersion(QLatin1String(QUOTE(IFW_CACHE_FORMAT_VERSION)));
}

/*!
    Constructs a cache to \a path. The cache is initialized automatically.
*/
MetadataCache::MetadataCache(const QString &path)
    : GenericDataCache(path, QLatin1String("Metadata"), QLatin1String(QUOTE(IFW_CACHE_FORMAT_VERSION)))
{
}

} // namespace QInstaller
