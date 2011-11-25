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
#include "settingsdialog.h"

#include <common/utils.h>
#include <component.h>
#include <packagemanagercore.h>

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

    QInstaller::PackageManagerGui *m_gui;
    QInstaller::PackageManagerCore *m_core;

    QString m_controlScript;
    QHash<QString, QString> m_params;

    Settings m_settings;
    bool m_networkSettingsChanged;
};

TabController::Private::Private()
    : m_init(false)
    , m_updatesFetched(false)
    , m_allPackagesFetched(false)
    , m_introPageConnected(false)
    , m_gui(0)
    , m_core(0)
    , m_networkSettingsChanged(false)
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
    d->m_core->writeUninstaller();
    delete d;
}

void TabController::setGui(QInstaller::PackageManagerGui *gui)
{
    d->m_gui = gui;
    connect(d->m_gui, SIGNAL(gotRestarted()), this, SLOT(restartWizard()));
}

void TabController::setControlScript (const QString &script)
{
    d->m_controlScript = script;
}

void TabController::setManager(QInstaller::PackageManagerCore *core)
{
    d->m_core = core;
}

void TabController::setManagerParams(const QHash<QString, QString> &params)
{
    d->m_params = params;
}

// -- public slots

int TabController::init()
{
    if (!d->m_init) {
        d->m_init = true;
        // this should called as early as possible, to handle error message boxes for example
        if (!d->m_controlScript.isEmpty()) {
            QInstaller::verbose() << "Non-interactive installation using script: "
                << qPrintable(d->m_controlScript) << std::endl;

            d->m_gui->loadControlScript(d->m_controlScript);
            QScriptEngine *engine = d->m_gui->controlScriptEngine();
            engine->globalObject().setProperty(QLatin1String("tabController"),
                engine->newQObject(this));
        }

        IntroductionPageImpl *introPage =
            qobject_cast<IntroductionPageImpl*>(d->m_gui->page(PackageManagerCore::Introduction));
        connect(introPage, SIGNAL(initUpdater()), this, SLOT(initUpdater()));
        connect(introPage, SIGNAL(initUninstaller()), this, SLOT(initUninstaller()));
        connect(introPage, SIGNAL(initPackageManager()), this, SLOT(initPackageManager()));

        connect(d->m_gui, SIGNAL(currentIdChanged(int)), this, SLOT(onCurrentIdChanged(int)));
        connect(d->m_gui, SIGNAL(settingsButtonClicked()), this, SLOT(onSettingsButtonClicked()));
    }

    d->m_updatesFetched = false;
    d->m_allPackagesFetched = false;

    if (d->m_core->isUpdater())
        return initUpdater();

    if (d->m_core->isUninstaller())
        return initUninstaller();

    return initPackageManager();
}

int TabController::initUpdater()
{
    onCurrentIdChanged(d->m_gui->currentId());
    IntroductionPageImpl *introPage = introductionPage();

    introPage->setMessage(QString());
    introPage->setErrorMessage(QString());
    introPage->showAll();
    introPage->setComplete(false);
    introPage->setMaintenanceToolsEnabled(false);

    if (!d->m_introPageConnected) {
        d->m_introPageConnected = true;
        connect(d->m_core, SIGNAL(metaJobInfoMessage(QString)), introPage, SLOT(setMessage(QString)));
    }

    d->m_gui->setWindowModality(Qt::WindowModal);
    d->m_gui->init();   // Initialize/ reset the ui.
    d->m_gui->show();
    d->m_gui->setSettingsButtonEnabled(false);

    if (!d->m_updatesFetched) {
        d->m_updatesFetched = d->m_core->fetchRemotePackagesTree();
        if (!d->m_updatesFetched)
            introPage->setErrorMessage(d->m_core->error());
    }

    // Needs to be done after check repositories as only then the ui can handle hide of pages depending on
    // the components.
    d->m_gui->callControlScriptMethod(QLatin1String("UpdaterSelectedCallback"));
    d->m_gui->triggerControlScriptForCurrentPage();

    introPage->showMaintenanceTools();
    introPage->setMaintenanceToolsEnabled(true);

    if (d->m_updatesFetched) {
        if (d->m_core->updaterComponents().count() <= 0)
            introPage->setErrorMessage(tr("<b>No updates available.</b>"));
        else
            introPage->setComplete(true);
    }
    d->m_gui->setSettingsButtonEnabled(true);

    return d->m_core->status();
}

int TabController::initUninstaller()
{
    onCurrentIdChanged(d->m_gui->currentId());
    IntroductionPageImpl *introPage = introductionPage();

    introPage->setMessage(QString());
    introPage->setErrorMessage(QString());
    introPage->setComplete(true);
    introPage->showMaintenanceTools();

    d->m_gui->setWindowModality(Qt::WindowModal);
    d->m_gui->show();

    return PackageManagerCore::Success;
}

int TabController::initPackageManager()
{
    onCurrentIdChanged(d->m_gui->currentId());
    IntroductionPageImpl *introPage = introductionPage();

    introPage->setMessage(QString());
    introPage->setErrorMessage(QString());
    introPage->setComplete(false);
    introPage->showMetaInfoUdate();

    if (d->m_core->isPackageManager()) {
        introPage->showAll();
        introPage->setMaintenanceToolsEnabled(false);
    }

    if (!d->m_introPageConnected) {
        d->m_introPageConnected = true;
        connect(d->m_core, SIGNAL(metaJobInfoMessage(QString)), introPage, SLOT(setMessage(QString)));
    }

    d->m_gui->setWindowModality(Qt::WindowModal);
    d->m_gui->init();   // Initialize/ reset the ui.
    d->m_gui->show();
    d->m_gui->setSettingsButtonEnabled(false);

    bool localPackagesTreeFetched = false;
    if (!d->m_allPackagesFetched) {
        // first try to fetch the server side packages tree
        d->m_allPackagesFetched = d->m_core->fetchRemotePackagesTree();
        if (!d->m_allPackagesFetched) {
            QString error = d->m_core->error();
            if (d->m_core->isPackageManager()) {
                // if that fails and we're in maintenance mode, try to fetch local installed tree
                localPackagesTreeFetched = d->m_core->fetchLocalPackagesTree();
                if (localPackagesTreeFetched) {
                    // if that succeeded, adjust error message
                    error = QLatin1String("<font color=\"red\">") + error + tr(" Only local package "
                        "management available.") + QLatin1String("</font>");
                }
            }
            introPage->setErrorMessage(error);
        }
    }

    // Needs to be done after check repositories as only then the ui can handle hide of pages depending on
    // the components.
    d->m_gui->callControlScriptMethod(QLatin1String("PackageManagerSelectedCallback"));
    d->m_gui->triggerControlScriptForCurrentPage();

    if (d->m_core->isPackageManager()) {
        introPage->showMaintenanceTools();
        introPage->setMaintenanceToolsEnabled(true);
    } else {
        introPage->hideAll();
    }

    if (d->m_allPackagesFetched | localPackagesTreeFetched)
        introPage->setComplete(true);
    d->m_gui->setSettingsButtonEnabled(true);

    return d->m_core->status();
}

// -- private slots

void TabController::restartWizard()
{
    d->m_core->reset(d->m_params);
    if (d->m_networkSettingsChanged) {
        d->m_networkSettingsChanged = false;

        d->m_core->settings().setFtpProxy(d->m_settings.ftpProxy());
        d->m_core->settings().setHttpProxy(d->m_settings.httpProxy());
        d->m_core->settings().setProxyType(d->m_settings.proxyType());

        d->m_core->settings().setUserRepositories(d->m_settings.userRepositories());
        d->m_core->settings().setDefaultRepositories(d->m_settings.defaultRepositories());
        d->m_core->settings().setTemporaryRepositories(d->m_settings.temporaryRepositories(),
            d->m_settings.hasReplacementRepos());
        d->m_core->networkSettingsChanged();
    }

    // restart and switch back to intro page
    QTimer::singleShot(0, this, SLOT(init()));
}

void TabController::onSettingsButtonClicked()
{
    SettingsDialog dialog(d->m_core);
    connect (&dialog, SIGNAL(networkSettingsChanged(QInstaller::Settings)), this,
        SLOT(onNetworkSettingsChanged(QInstaller::Settings)));
    dialog.exec();

    if (d->m_networkSettingsChanged) {
        d->m_core->setCanceled();
        QTimer::singleShot(0, d->m_gui, SLOT(restart()));
        QTimer::singleShot(100, this, SLOT(restartWizard()));
    }
}

void TabController::onCurrentIdChanged(int newId)
{
    if (d->m_gui && d->m_core) {
        d->m_gui->showSettingsButton((newId == PackageManagerCore::Introduction) &
            (!d->m_core->isOfflineOnly()) & (!d->m_core->isUninstaller()));
    }
}

void TabController::onNetworkSettingsChanged(const QInstaller::Settings &settings)
{
    d->m_settings = settings;
    d->m_networkSettingsChanged = true;
}

// -- private

IntroductionPageImpl *TabController::introductionPage() const
{
    return qobject_cast<IntroductionPageImpl*> (d->m_gui->page(PackageManagerCore::Introduction));
}
