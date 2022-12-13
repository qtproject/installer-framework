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

#include "qinstallerglobal.h"

/*!
    \enum QInstaller::JobError

    \value InvalidUrl
    \value Timeout
    \value DownloadError
    \value InvalidUpdatesXml
    \value InvalidMetaInfo
    \value ExtractionError
    \value UserIgnoreError
    \value RepositoryUpdatesReceived
    \value CacheError
*/

/*!
    \typedef QInstaller::Operation

    Synonym for KDUpdater::UpdateOperation.
*/

/*!
    \typedef QInstaller::OperationList

    Synonym for QList<QInstaller::Operation*>.
*/

/*!
    \typedef QInstaller::Package

    Synonym for KDUpdater::Update.
*/

/*!
    \typedef QInstaller::PackagesList

    Synonym for QList<QInstaller::Package*>.
*/

/*!
    \typedef QInstaller::LocalPackagesMap

    Synonym for QMap<QString, KDUpdater::LocalPackage>. The map key is component name,
    and value contains information about the local package.
*/

/*!
    \typedef QInstaller::AutoDependencyHash

    Synonym for QHash<QString, QStringList>. The hash key is component name,
    that other components automatically depend on. The value can contain
    several component names, which are installed as an automatic dependency.
    For example:
    \badcode
    <Name>A</Name> //Hash value
    <AutoDependOn>B</AutoDependOn> //Hash key
    \endcode
*/

/*!
    \typedef QInstaller::LocalDependencyHash

    Synonym for QHash<QString, QStringList>. The hash key is component name,
    which other components depend on. The value can contain several component
    names, which depend on the key. For example:
    \badcode
    <Name>A</Name> //Hash value
    <Dependencies>B</Dependencies> //Hash key
    \endcode
*/
