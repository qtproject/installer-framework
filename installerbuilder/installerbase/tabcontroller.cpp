/**************************************************************************
**
** This file is part of Qt SDK**
**
** Copyright (c) 2011 Nokia Corporation and/or its subsidiary(-ies).*
**
** Contact:  Nokia Corporation qt-info@nokia.com**
**
** No Commercial Usage
**
** This file contains pre-release code and may not be distributed.
** You may use this file in accordance with the terms and conditions
** contained in the Technology Preview License Agreement accompanying
** this package.
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception version
** 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** If you are unsure which license is appropriate for your use, please contact
** (qt-info@nokia.com).
**
**************************************************************************/
#include "tabcontroller.h"
#include "installerbasecommons.h"

#include <common/utils.h>
#include <component.h>
#include <qinstaller.h>

#include <QtCore/QPointer>
#include <QtCore/QTimer>

#include <QtScript/QScriptEngine>

using namespace QInstaller;


// -- TabController::Private

class TabController::Private
{
public:
    Private();
    ~Private();

    bool m_init;
    bool m_updatesFetched;
    bool m_allPackagesFetched;
    bool m_introPageConnected;

    QPointer<QInstaller::Gui> m_gui;
    QInstaller::Installer *m_installer;

    QString m_controlScript;
    QHash<QString, QString> m_params;
};

TabController::Private::Private()
    : m_init(false)
    , m_updatesFetched(false)
    , m_allPackagesFetched(false)
    , m_introPageConnected(false)
    , m_gui(0)
    , m_installer(0)
{
}

TabController::Private::~Private()
{
    delete m_gui;
}


// -- TabController

TabController::TabController(QObject *parent)
    : QObject(parent)
    , d(new Private)
{
}

TabController::~TabController()
{
    d->m_installer->writeUninstaller();
    delete d;
}

void TabController::setInstallerGui(QInstaller::Gui *gui)
{
    d->m_gui = gui;
    connect(d->m_gui, SIGNAL(gotRestarted()), this, SLOT(restartWizard()));
}

void TabController::setControlScript (const QString &script)
{
    d->m_controlScript = script;
}

void TabController::setInstaller(QInstaller::Installer *installer)
{
    d->m_installer = installer;
}

void TabController::setInstallerParams(const QHash<QString, QString> &params)
{
    d->m_params = params;
}

// -- public slots

int TabController::init()
{
    if (!d->m_init) {
        d->m_init = true;
        // this should called as early as possible, to handle error messageboxes for example
        if (!d->m_controlScript.isEmpty()) {
            QInstaller::verbose() << "Non-interactive installation using script: "
                << qPrintable(d->m_controlScript) << std::endl;

            d->m_gui->loadControlScript(d->m_controlScript);
            QScriptEngine *engine = d->m_gui->controlScriptEngine();
            engine->globalObject().setProperty(QLatin1String("tabController"),
                engine->newQObject(this));
        }

        if (!d->m_installer->isInstaller())
            d->m_gui->setWindowTitle(d->m_installer->value(QLatin1String("MaintenanceTitle")));

        IntroductionPageImpl *introPage =
            qobject_cast<IntroductionPageImpl*>(d->m_gui->page(Installer::Introduction));
        connect(introPage, SIGNAL(initUpdater()), this, SLOT(initUpdater()));
        connect(introPage, SIGNAL(initPackageManager()), this, SLOT(initPackageManager()));
    }

    d->m_updatesFetched = false;
    d->m_allPackagesFetched = false;

    if (d->m_installer->isUpdater())
        return initUpdater();

    if (d->m_installer->isUninstaller())
        return initUninstaller();

    return initPackageManager();
}

int TabController::initUpdater()
{
    IntroductionPageImpl *introPage = introductionPage();

    introPage->showAll();
    introPage->setComplete(false);
    introPage->setMaintenanceToolsEnabled(false);

    if (!d->m_introPageConnected) {
        d->m_introPageConnected = true;
        connect(d->m_installer, SIGNAL(metaJobInfoMessage(KDJob*,QString)), introPage,
            SLOT(message(KDJob*, QString)));
    }

    d->m_gui->setWindowModality(Qt::WindowModal);
    d->m_gui->show();

    if (!d->m_updatesFetched)
        d->m_updatesFetched = d->m_installer->fetchUpdaterPackages();

    // Initialize the gui. Needs to be done after check repositories as only then the ui can handle
    // hide of pages depenging on the components.
    d->m_gui->init();
    d->m_gui->callControlScriptMethod(QLatin1String("UpdaterSelectedCallback"));
    d->m_gui->triggerControlScriptForCurrentPage();

    introPage->showMaintenanceTools();
    introPage->setMaintenanceToolsEnabled(true);
    introPage->setComplete(true);

    if (d->m_installer->status() == Installer::Canceled)
        return Installer::Canceled;
    return Installer::Success;
}

int TabController::initUninstaller()
{
    IntroductionPageImpl *introPage = introductionPage();

    introPage->setComplete(true);
    introPage->showMaintenanceTools();

    d->m_gui->setWindowModality(Qt::WindowModal);
    d->m_gui->show();

    return Installer::Success;
}

int TabController::initPackageManager()
{
    IntroductionPageImpl *introPage = introductionPage();

    introPage->setComplete(false);
    introPage->showMetaInfoUdate();
    if (d->m_installer->isPackageManager()) {
        introPage->showAll();
        introPage->setMaintenanceToolsEnabled(false);
    }

    if (!d->m_introPageConnected) {
        d->m_introPageConnected = true;
        connect(d->m_installer, SIGNAL(metaJobInfoMessage(KDJob*,QString)), introPage,
            SLOT(message(KDJob*, QString)));
    }

    d->m_gui->setWindowModality(Qt::WindowModal);
    d->m_gui->show();

    if (!d->m_allPackagesFetched)
        d->m_allPackagesFetched = d->m_installer->fetchAllPackages();

    // Initialize the gui. Needs to be done after check repositories as only then the ui can handle
    // hide of pages depenging on the components.
    d->m_gui->init();
    d->m_gui->callControlScriptMethod(QLatin1String("PackageManagerSelectedCallback"));
    d->m_gui->triggerControlScriptForCurrentPage();

    if (d->m_installer->isPackageManager()) {
        introPage->showMaintenanceTools();
        introPage->setMaintenanceToolsEnabled(true);
    } else {
        introPage->hideAll();
    }
    introPage->setComplete(true);

    if (d->m_installer->status() == Installer::Canceled)
        return Installer::Canceled;
    return Installer::Success;
}

// -- private slots

void TabController::restartWizard()
{
    d->m_installer->reset(d->m_params);

    // restart and switch back to intro page
    QTimer::singleShot(0, this, SLOT(init()));
}

// -- private

IntroductionPageImpl *TabController::introductionPage() const
{
    return qobject_cast<IntroductionPageImpl*> (d->m_gui->page(Installer::Introduction));
}
