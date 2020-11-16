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

#include "printoutput.h"

#include "component.h"
#include "localpackagehub.h"
#include "globals.h"

#include <iostream>
#include <QDomDocument>

namespace QInstaller
{

void printComponentInfo(const QList<Component *> components)
{
    QDomDocument doc;
    QDomElement root = doc.createElement(QLatin1String("updates"));
    doc.appendChild(root);

    foreach (Component *component, components) {
        QDomElement update = doc.createElement(QLatin1String("update"));
        update.setAttribute(QLatin1String("name"), component->value(scDisplayName));
        update.setAttribute(QLatin1String("version"), component->value(scVersion));
        update.setAttribute(QLatin1String("size"), component->value(scUncompressedSize));
        update.setAttribute(QLatin1String("id"), component->value(scName));
        root.appendChild(update);
    }
    qCDebug(lcPackageInfo) << qPrintable(doc.toString(4));
}

void printLocalPackageInformation(const QList<KDUpdater::LocalPackage> &packages)
{
    QDomDocument doc;
    QDomElement root = doc.createElement(QLatin1String("localpackages"));
    doc.appendChild(root);
    foreach (KDUpdater::LocalPackage package, packages) {
        QDomElement update = doc.createElement(QLatin1String("package"));
        update.setAttribute(QLatin1String("name"), package.name);
        update.setAttribute(QLatin1String("displayname"), package.title);
        update.setAttribute(QLatin1String("version"), package.version);
        if (verboseLevel() == VerbosityLevel::Detailed) {
            update.setAttribute(QLatin1String("description"), package.description);
            update.setAttribute(QLatin1String("dependencies"), package.dependencies.join(QLatin1Char(',')));
            update.setAttribute(QLatin1String("autoDependencies"), package.autoDependencies.join(QLatin1Char(',')));
            update.setAttribute(QLatin1String("virtual"), package.virtualComp);
            update.setAttribute(QLatin1String("forcedInstallation"), package.forcedInstallation);
            update.setAttribute(QLatin1String("checkable"), package.checkable);
            update.setAttribute(QLatin1String("uncompressedSize"), package.uncompressedSize);
            update.setAttribute(QLatin1String("installDate"), package.installDate.toString());
            update.setAttribute(QLatin1String("lastUpdateDate"), package.lastUpdateDate.toString());
        }
        root.appendChild(update);
    }
    qCDebug(lcPackageInfo) << qPrintable(doc.toString(4));
}

void printPackageInformation(const PackagesList &matchedPackages, const LocalPackagesHash &installedPackages)
{
    QDomDocument doc;
    QDomElement root = doc.createElement(QLatin1String("availablepackages"));
    doc.appendChild(root);
    foreach (Package *package, matchedPackages) {
        const QString name = package->data(scName).toString();
        QDomElement update = doc.createElement(QLatin1String("package"));
        update.setAttribute(QLatin1String("name"), name);
        update.setAttribute(QLatin1String("displayname"), package->data(scDisplayName).toString());
        update.setAttribute(QLatin1String("version"), package->data(scVersion).toString());
        //Check if package already installed
        if (installedPackages.contains(name))
            update.setAttribute(QLatin1String("installedVersion"), installedPackages.value(name).version);
        if (verboseLevel() == VerbosityLevel::Detailed) {
            update.setAttribute(QLatin1String("description"), package->data(scDescription).toString());
            update.setAttribute(QLatin1String("dependencies"), package->data(scDependencies).toString());
            update.setAttribute(QLatin1String("autoDependencies"), package->data(scAutoDependOn).toString());
            update.setAttribute(QLatin1String("virtual"), package->data(scVirtual).toString());
            update.setAttribute(QLatin1String("forcedInstallation"), package->data(QLatin1String("ForcedInstallation")).toString());
            update.setAttribute(QLatin1String("checkable"), package->data(scCheckable).toString());
            update.setAttribute(QLatin1String("default"), package->data(scDefault).toString());
            update.setAttribute(QLatin1String("essential"), package->data(scEssential).toString());
            update.setAttribute(QLatin1String("compressedsize"), package->data(QLatin1String("CompressedSize")).toString());
            update.setAttribute(QLatin1String("uncompressedsize"), package->data(QLatin1String("UncompressedSize")).toString());
            update.setAttribute(QLatin1String("releaseDate"), package->data(scReleaseDate).toString());
            update.setAttribute(QLatin1String("downloadableArchives"), package->data(scDownloadableArchives).toString());
            update.setAttribute(QLatin1String("licenses"), package->data(QLatin1String("Licenses")).toString());
            update.setAttribute(QLatin1String("script"), package->data(scScript).toString());
            update.setAttribute(QLatin1String("sortingPriority"), package->data(scSortingPriority).toString());
            update.setAttribute(QLatin1String("replaces"), package->data(scReplaces).toString());
            update.setAttribute(QLatin1String("requiresAdminRights"), package->data(scRequiresAdminRights).toString());
        }
        root.appendChild(update);
    }
    qCDebug(lcPackageInfo) << qPrintable(doc.toString(4));
}
} // namespace QInstaller

