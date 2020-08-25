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

#include "installerbase.h"

#include "installerbasecommons.h"
#include "tabcontroller.h"

#include <init.h>
#include <messageboxhandler.h>
#include <packagemanagercore.h>
#include <globals.h>
#include <runoncechecker.h>

#include <QDir>
#include <QDirIterator>
#include <QFontDatabase>
#include <QTranslator>

#ifdef ENABLE_SQUISH
#include <qtbuiltinhook.h>
#endif

InstallerBase::InstallerBase(int &argc, char *argv[])
    : SDKApp<QApplication>(argc, argv)
{
    QInstaller::init(); // register custom operations
}

InstallerBase::~InstallerBase()
{
    delete m_core;
}

int InstallerBase::run()
{
    QString errorMessage;
    if (!init(errorMessage)) {
        QInstaller::MessageBoxHandler::information(nullptr, QLatin1String("UnableToStart"),
            tr("Unable to start installer"), errorMessage);
        return EXIT_FAILURE;
    }

    if (m_parser.isSet(CommandLineOptions::scShowVirtualComponentsLong)) {
        QFont f;
        f.setItalic(true);
        QInstaller::PackageManagerCore::setVirtualComponentsFont(f);
    }
    QString controlScript;
    if (m_parser.isSet(CommandLineOptions::scScriptLong)) {
        controlScript = m_parser.value(CommandLineOptions::scScriptLong);
        if (!QFileInfo(controlScript).exists()) {
            qCDebug(QInstaller::lcInstallerInstallLog) << "Script file does not exist.";
            return false;
        }

    } else if (!m_core->settings().controlScript().isEmpty()) {
        controlScript = QLatin1String(":/metadata/installer-config/")
            + m_core->settings().controlScript();
    }
    qCDebug(QInstaller::lcInstallerInstallLog) << "Language:" << QLocale().uiLanguages()
        .value(0, QLatin1String("No UI language set")).toUtf8().constData();
#ifndef IFW_DISABLE_TRANSLATIONS
    const QString directory = QLatin1String(":/translations");
    // Check if there is a modified translation first to enable people
    // to easily provide corrected translations to Qt/IFW for their installers
    const QString newDirectory = QLatin1String(":/translations_new");
    const QStringList translations = m_core->settings().translations();

    if (translations.isEmpty()) {
        foreach (const QLocale locale, QLocale().uiLanguages()) {
            QScopedPointer<QTranslator> qtTranslator(new QTranslator(QCoreApplication::instance()));
            bool qtLoaded = qtTranslator->load(locale, QLatin1String("qt"),
                                              QLatin1String("_"), newDirectory);
            if (!qtLoaded)
                qtLoaded = qtTranslator->load(locale, QLatin1String("qt"),
                                              QLatin1String("_"), directory);

            if (qtLoaded || locale.language() == QLocale::English) {
                if (qtLoaded)
                    QCoreApplication::instance()->installTranslator(qtTranslator.take());

                QScopedPointer<QTranslator> ifwTranslator(new QTranslator(QCoreApplication::instance()));
                bool ifwLoaded = ifwTranslator->load(locale, QLatin1String("ifw"), QLatin1String("_"), newDirectory);
                if (!ifwLoaded)
                    ifwLoaded = ifwTranslator->load(locale, QLatin1String("ifw"), QLatin1String("_"), directory);
                if (ifwLoaded)
                    QCoreApplication::instance()->installTranslator(ifwTranslator.take());

                // To stop loading other translations it's sufficient that
                // qt was loaded successfully or we hit English as system language
                break;
            }
        }
    } else {
        foreach (const QString &translation, translations) {
            QScopedPointer<QTranslator> translator(new QTranslator(QCoreApplication::instance()));
            if (translator->load(translation, QLatin1String(":/translations")))
                QCoreApplication::instance()->installTranslator(translator.take());
        }
    }
#endif
    {
        QDirIterator fontIt(QStringLiteral(":/fonts"));
        while (fontIt.hasNext()) {
            const QString path = fontIt.next();
            qCDebug(QInstaller::lcDeveloperBuild) << "Registering custom font" << path;
            if (QFontDatabase::addApplicationFont(path) == -1)
                qCWarning(QInstaller::lcDeveloperBuild) << "Failed to register font!";
        }
    }
    //create the wizard GUI
    TabController controller(nullptr);
    controller.setManager(m_core);
    controller.setControlScript(controlScript);
    if (m_core->isInstaller())
        controller.setGui(new InstallerGui(m_core));
    else
        controller.setGui(new MaintenanceGui(m_core));

    QInstaller::PackageManagerCore::Status status =
        QInstaller::PackageManagerCore::Status(controller.init());
    if (status != QInstaller::PackageManagerCore::Success)
        return status;

#ifdef ENABLE_SQUISH
    int squishPort = 11233;
    if (m_parser.isSet(CommandLineOptions::scSquishPortLong)) {
        squishPort = m_parser.value(CommandLineOptions::scSquishPortLong).toInt();
    }
    if (squishPort != 0) {
        if (Squish::allowAttaching(squishPort))
            qCDebug(QInstaller::lcDeveloperBuild)  << "Attaching to squish port " << squishPort << " succeeded";
        else
            qCDebug(QInstaller::lcDeveloperBuild)  << "Attaching to squish failed.";
    } else {
        qCWarning(QInstaller::lcDeveloperBuild)  << "Invalid squish port number: " << squishPort;
    }
#endif
    const int result = QCoreApplication::instance()->exec();
    if (result != 0)
        return result;

    if (m_core->finishedWithSuccess())
        return QInstaller::PackageManagerCore::Success;

    status = m_core->status();
    switch (status) {
        case QInstaller::PackageManagerCore::Success:
            return status;

        case QInstaller::PackageManagerCore::Canceled:
            return status;

        default:
            break;
    }
    return QInstaller::PackageManagerCore::Failure;

}
