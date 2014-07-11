/**************************************************************************
**
** Copyright (C) 2012-2014 Digia Plc and/or its subsidiary(-ies).
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

#include "updatechecker.h"
#include "sdkapp.h"

#include <binarycontent.h>
#include <component.h>
#include <constants.h>
#include <errors.h>
#include <init.h>
#include <kdrunoncechecker.h>
#include <packagemanagercore.h>
#include <productkeycheck.h>

#include <QDomDocument>

#include <iostream>

int UpdateChecker::check(int argc, char *argv[])
{
    try {
        SDKApp<QCoreApplication> app(argc, argv);

        KDRunOnceChecker runCheck((QLatin1String("lockmyApp15021976.lock")));
        if (runCheck.isRunning(KDRunOnceChecker::Lockfile))
            throw QInstaller::Error(QLatin1String("An instance is already checking for updates."));

        QInstaller::init(); // register custom operations

        const QInstaller::BinaryContent content =
            QInstaller::BinaryContent::readAndRegisterFromBinary(app.binaryFile());
        if (content.magicMarker() != QInstaller::BinaryContent::MagicInstallerMarker)
            throw QInstaller::Error(QLatin1String("Installers cannot check for updates."));

        QInstaller::PackageManagerCore core(QInstaller::BinaryContent::MagicUpdaterMarker, content
            .performedOperations());
        ProductKeyCheck::instance()->init(&core);
        QInstaller::PackageManagerCore::setVirtualComponentsVisible(true);

        if (!core.fetchRemotePackagesTree())
            throw QInstaller::Error(core.error());

        const QList<QInstaller::Component *> components = core.updaterComponents();
        if (components.isEmpty())
            throw QInstaller::Error(QLatin1String("There are currently no updates available."));

        QDomDocument doc;
        QDomElement root = doc.createElement(QLatin1String("updates"));
        doc.appendChild(root);

        foreach (QInstaller::Component *component, components) {
            QDomElement update = doc.createElement(QLatin1String("update"));
            update.setAttribute(QLatin1String("name"),
                component->value(QInstaller::scDisplayName));
            update.setAttribute(QLatin1String("version"),
                component->value(QInstaller::scRemoteVersion));
            update.setAttribute(QLatin1String("size"),
                component->value(QInstaller::scUncompressedSize));
            root.appendChild(update);
        }

        std::cout << qPrintable(doc.toString(4)) << std::endl;
        return EXIT_SUCCESS;
    } catch (const QInstaller::Error &e) {
        std::cerr << qPrintable(e.message()) << std::endl;
    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
    } catch (...) {
        std::cerr <<  "Unknown exception caught." << std::endl;
    }
    return EXIT_FAILURE;
}
