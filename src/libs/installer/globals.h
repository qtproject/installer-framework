/**************************************************************************
**
** Copyright (C) 2020 The Qt Company Ltd.
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
#ifndef GLOBALS_H
#define GLOBALS_H

#include "installer_global.h"

#include <QRegExp>
#include <QLoggingCategory>

namespace QInstaller {

INSTALLER_EXPORT Q_DECLARE_LOGGING_CATEGORY(lcComponentChecker)
INSTALLER_EXPORT Q_DECLARE_LOGGING_CATEGORY(lcResources)
INSTALLER_EXPORT Q_DECLARE_LOGGING_CATEGORY(lcTranslations)
INSTALLER_EXPORT Q_DECLARE_LOGGING_CATEGORY(lcNetwork)
INSTALLER_EXPORT Q_DECLARE_LOGGING_CATEGORY(lcServer)
INSTALLER_EXPORT Q_DECLARE_LOGGING_CATEGORY(lcGeneral)

INSTALLER_EXPORT Q_DECLARE_LOGGING_CATEGORY(lcPackageDisplayname)
INSTALLER_EXPORT Q_DECLARE_LOGGING_CATEGORY(lcPackageDescription)
INSTALLER_EXPORT Q_DECLARE_LOGGING_CATEGORY(lcPackageVersion)
INSTALLER_EXPORT Q_DECLARE_LOGGING_CATEGORY(lcPackageInstalledVersion)
INSTALLER_EXPORT Q_DECLARE_LOGGING_CATEGORY(lcPackageReleasedate)
INSTALLER_EXPORT Q_DECLARE_LOGGING_CATEGORY(lcPackageInstallDate)
INSTALLER_EXPORT Q_DECLARE_LOGGING_CATEGORY(lcPackageUpdateDate)
INSTALLER_EXPORT Q_DECLARE_LOGGING_CATEGORY(lcPackageName)
INSTALLER_EXPORT Q_DECLARE_LOGGING_CATEGORY(lcPackageDependencies)
INSTALLER_EXPORT Q_DECLARE_LOGGING_CATEGORY(lcPackageAutodependon)
INSTALLER_EXPORT Q_DECLARE_LOGGING_CATEGORY(lcPackageVirtual)
INSTALLER_EXPORT Q_DECLARE_LOGGING_CATEGORY(lcPackageSortingpriority)
INSTALLER_EXPORT Q_DECLARE_LOGGING_CATEGORY(lcPackageScript)
INSTALLER_EXPORT Q_DECLARE_LOGGING_CATEGORY(lcPackageDefault)
INSTALLER_EXPORT Q_DECLARE_LOGGING_CATEGORY(lcPackageEssential)
INSTALLER_EXPORT Q_DECLARE_LOGGING_CATEGORY(lcPackageForcedinstallation)
INSTALLER_EXPORT Q_DECLARE_LOGGING_CATEGORY(lcPackageReplaces)
INSTALLER_EXPORT Q_DECLARE_LOGGING_CATEGORY(lcPackageDownloadableArchives)
INSTALLER_EXPORT Q_DECLARE_LOGGING_CATEGORY(lcPackageRequiresAdminRights)
INSTALLER_EXPORT Q_DECLARE_LOGGING_CATEGORY(lcPackageCheckable)
INSTALLER_EXPORT Q_DECLARE_LOGGING_CATEGORY(lcPackageLicenses)
INSTALLER_EXPORT Q_DECLARE_LOGGING_CATEGORY(lcPackageUncompressedSize)
INSTALLER_EXPORT Q_DECLARE_LOGGING_CATEGORY(lcPackageCompressedSize)
INSTALLER_EXPORT Q_DECLARE_LOGGING_CATEGORY(lcInstallerInstallLog)
INSTALLER_EXPORT Q_DECLARE_LOGGING_CATEGORY(lcInstallerUninstallLog)

QStringList INSTALLER_EXPORT loggingCategories();

QRegExp INSTALLER_EXPORT commaRegExp();

QString htmlToString(const QString &html);
QString enumToString(const QMetaObject& metaObject, const char *enumerator, int key);

}   // QInstaller

#endif  // GLOBALS_H
