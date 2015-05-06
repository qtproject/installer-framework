/**************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
**
** $QT_END_LICENSE$
**
**************************************************************************/

#include "packagesource.h"

namespace QInstaller {

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::PackageSource
    \brief The PackageSource class specifies a single package source.

    An package source represents a link to an repository that contains packages applicable by
    the installer or package maintenance application. This structure describes a single package
    source in terms of url and priority. While different repositories can host the same packages,
    packages coming from a higher priority source take precedence over lower priority packages
    during applicable package computation.
*/

/*!
    \fn PackageSource::PackageSource()

    Constructs an empty package source info object. The object's priority is set to -1. The url is
    initialized using a \l{default-constructed value}.
*/

/*!
    \fn PackageSource::PackageSource(const QUrl &u, int p)

    Constructs an package source info object. The object's url is set to \a u, while the priority
    is set to \a p.
*/

/*!
    \variable PackageSource::url
    \brief The URL of the package source.
*/

/*!
    \variable PackageSource::priority
    \brief The priority of the package source.
*/

/*!
    Returns the hash value for the \a key, using \a seed to seed the calculation.
*/
uint qHash(const PackageSource &key, uint seed)
{
    return qHash(key.url, seed) ^ key.priority;
}

/*!
    Returns \c true if \a lhs and \a rhs are equal; otherwise returns \c false.
*/
bool operator==(const PackageSource &lhs, const PackageSource &rhs)
{
    return lhs.url == rhs.url && lhs.priority == rhs.priority;
}

} // namespace QInstaller
