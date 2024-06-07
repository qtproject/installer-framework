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
#include "tabcontroller.h"

#include "installerbasecommons.h"
#include "settingsdialog.h"
#include "aboutapplicationdialog.h"
#include "globals.h"

#include <packagemanagercore.h>

#include <productkeycheck.h>

#include <QtCore/QTimer>
#include <QtWidgets/QMessageBox>
#include <QtConcurrent>

using namespace QInstaller;


// -- TabController::Private

class TabController::Private
{
public:
    Private();
    ~Private();

    bool m_init;
    QString m_controlScript;
    QHash<QString, QString> m_params;

    Settings m_settings;
    bool m_networkSettingsChanged;

    QInstaller::PackageManagerGui *m_gui;
    QInstaller::PackageManagerCore *m_core;
};

TabController::Private::Private()
    : m_init(false)
    , m_networkSettingsChanged(false)
    , m_gui(nullptr)
    , m_core(nullptr)
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
    d->m_core->writeMaintenanceTool();
    delete d;
}

void TabController::setGui(QInstaller::PackageManagerGui *gui)
{
    d->m_gui = gui;
    connect(d->m_gui, &PackageManagerGui::gotRestarted, this, &TabController::restartWizard);
}

void TabController::setControlScript(const QString &script)
{
    d->m_controlScript = script;
}

void TabController::setManager(QInstaller::PackageManagerCore *core)
{
    d->m_core = core;
}

// -- public slots

int TabController::init()
{
    if (!d->m_init) {
        d->m_init = true;
        // this should called as early as possible, to handle error message boxes for example
        if (!d->m_controlScript.isEmpty()) {
            d->m_gui->loadControlScript(d->m_controlScript);
            qCDebug(QInstaller::lcInstallerInstallLog) << "Using control script:" << d->m_controlScript;
        }

        connect(d->m_gui, &QWizard::currentIdChanged, this, &TabController::onCurrentIdChanged);
        connect(d->m_gui, &PackageManagerGui::settingsButtonClicked,
                this, &TabController::onSettingsButtonClicked);
        connect(d->m_gui, &PackageManagerGui::aboutApplicationClicked,
                this, &TabController::onAboutApplicationClicked);
    }

    IntroductionPage *page =
        qobject_cast<IntroductionPage*> (d->m_gui->page(PackageManagerCore::Introduction));
    if (page) {
        page->setMessage(QString());
        page->setErrorMessage(QString());
        page->resetFetchedState();
    }

    d->m_gui->restart();
    d->m_gui->setVisible(!d->m_gui->isSilent());

    onCurrentIdChanged(d->m_gui->currentId());
    return PackageManagerCore::Success;
}

// -- private slots

void TabController::restartWizard()
{
    if (d->m_networkSettingsChanged) {
        d->m_networkSettingsChanged = false;

        d->m_core->settings().setFtpProxy(d->m_settings.ftpProxy());
        d->m_core->settings().setHttpProxy(d->m_settings.httpProxy());
        d->m_core->settings().setProxyType(d->m_settings.proxyType());

        d->m_core->settings().setUserRepositories(d->m_settings.userRepositories());
        d->m_core->settings().setDefaultRepositories(d->m_settings.defaultRepositories());
        d->m_core->settings().setTemporaryRepositories(d->m_settings.temporaryRepositories(),
            d->m_settings.hasReplacementRepos());
        d->m_core->settings().setLocalCachePath(d->m_settings.localCachePath());

        d->m_core->networkSettingsChanged();
    }

    // Make sure we are writing the .dat file with the list of uninstall operations already now.
    // Otherwise we will write at the end of the next updater run, with a potentially
    // empty component list (if no updates are found).
    d->m_core->writeMaintenanceTool();

    d->m_core->reset();
    // restart and switch back to intro page
    QTimer::singleShot(0, this, &TabController::init);
}

void TabController::onSettingsButtonClicked()
{
    SettingsDialog dialog(d->m_core);
    dialog.adjustSize();
    connect(&dialog, &SettingsDialog::networkSettingsChanged,
            this, &TabController::onNetworkSettingsChanged);
    connect(&dialog, &SettingsDialog::clearLocalCacheClicked,
            this, &TabController::onClearCacheClicked);
    dialog.exec();

    if (d->m_networkSettingsChanged) {
        d->m_core->setCanceled();
        IntroductionPage *page =
            qobject_cast<IntroductionPage*> (d->m_gui->page(PackageManagerCore::Introduction));
        if (page) {
            page->setMessage(QString());
            page->setErrorMessage(QString());
        }
        restartWizard();
    }
}

void TabController::onAboutApplicationClicked()
{
    AboutApplicationDialog dialog(d->m_core);
    dialog.exec();
}

void TabController::onClearCacheClicked()
{
    SettingsDialog *settingsDialog = static_cast<SettingsDialog *>(sender());
    settingsDialog->setEnabled(false);
    settingsDialog->showClearCacheProgress(true);

    QString errorMessage;
    bool success = true;

    // Clearing might take some time, run in a separate thread
    QEventLoop loop;
    QFutureWatcher<bool> futureWatcher;

    connect(&futureWatcher, &QFutureWatcher<bool>::finished, this, [&]() {
        success = futureWatcher.future().result();
        if (loop.isRunning())
            loop.quit();
    });

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    futureWatcher.setFuture(QtConcurrent::run(&PackageManagerCore::clearLocalCache,
        d->m_core, &errorMessage));
#else
    futureWatcher.setFuture(QtConcurrent::run(d->m_core,
        &PackageManagerCore::clearLocalCache, &errorMessage));
#endif

    if (!futureWatcher.isFinished())
        loop.exec();

    settingsDialog->setEnabled(true);
    settingsDialog->showClearCacheProgress(false);

    QMessageBox msgBox(settingsDialog);
    msgBox.setWindowModality(Qt::WindowModal);
    msgBox.setStandardButtons(QMessageBox::Close);

    msgBox.setIcon(success
        ? QMessageBox::Information
        : QMessageBox::Critical);

    msgBox.setText(success
        ? tr("Cache cleared successfully!")
        : errorMessage);

    msgBox.exec();
}

void TabController::onCurrentIdChanged(int newId)
{
    if (d->m_gui) {
        if (PackageManagerPage *page = qobject_cast<PackageManagerPage *>(d->m_gui->page(newId)))
            d->m_gui->requestSettingsButtonByInstaller(page->settingsButtonRequested());
    }
}

void TabController::onNetworkSettingsChanged(const QInstaller::Settings &settings)
{
    d->m_settings = settings;
    d->m_networkSettingsChanged = true;
}
