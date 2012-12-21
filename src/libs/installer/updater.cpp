/**************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
**************************************************************************/
#include "updater.h"

#include "binaryformat.h"
#include "component.h"
#include "init.h"
#include "packagemanagercore.h"
#include "utils.h"

#include <QtCore/QDebug>

#include <QtXml/QDomDocument>

#include <iostream>

using namespace QInstaller;
using namespace QInstallerCreator;


Updater::Updater()
{
    QInstaller::init();
}

void Updater::setVerbose(bool verbose)
{
    QInstaller::setVerbose(verbose);
}

bool Updater::checkForUpdates()
{
    BinaryContent content = BinaryContent::readAndRegisterFromApplicationFile();
    if (content.magicMarker() == MagicInstallerMarker) {
        qDebug() << "Impossible to use an installer to check for updates!";
        return false;
    }

    PackageManagerCore core(content.magicMarker(), content.performedOperations());
    core.setUpdater();
    PackageManagerCore::setVirtualComponentsVisible(true);

    if (!core.fetchRemotePackagesTree())
        return false;

    const QList<QInstaller::Component *> components = core.updaterComponents();

    if (components.isEmpty()) {
        qDebug() << "There are currently no updates available.";
        return false;
    }

    QDomDocument doc;
    QDomElement root = doc.createElement(QLatin1String("updates"));
    doc.appendChild(root);

    QList<QInstaller::Component *>::const_iterator it;
    for (it = components.begin(); it != components.end(); ++it) {
        QDomElement update = doc.createElement(QLatin1String("update"));
        update.setAttribute(QLatin1String("name"), (*it)->value(scDisplayName));
        update.setAttribute(QLatin1String("version"), (*it)->value(scRemoteVersion));
        update.setAttribute(QLatin1String("size"), (*it)->value(scUncompressedSize));
        root.appendChild(update);
    }

    std::cout << doc.toString(4) << std::endl;
    return true;
}
