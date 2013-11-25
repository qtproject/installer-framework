/**************************************************************************
**
** Copyright (C) 2012-2013 Digia Plc and/or its subsidiary(-ies).
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
#include "tabcontroller.h"

#include "installerbasecommons.h"
#include "settingsdialog.h"

#include <packagemanagercore.h>

#include <productkeycheck.h>

#include <QtCore/QTimer>

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
    , m_gui(0)
    , m_core(0)
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

void TabController::setControlScript(const QString &script)
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
            qDebug() << "Non-interactive installation using script:" << d->m_controlScript;

            d->m_gui->loadControlScript(d->m_controlScript);
        }

        connect(d->m_gui, SIGNAL(currentIdChanged(int)), this, SLOT(onCurrentIdChanged(int)));
        connect(d->m_gui, SIGNAL(settingsButtonClicked()), this, SLOT(onSettingsButtonClicked()));
    }

    IntroductionPageImpl *page =
        qobject_cast<IntroductionPageImpl*> (d->m_gui->page(PackageManagerCore::Introduction));
    if (page) {
        page->setMessage(QString());
        page->setErrorMessage(QString());
        page->onCoreNetworkSettingsChanged();
    }

    d->m_gui->restart();
    d->m_gui->show();

    onCurrentIdChanged(d->m_gui->currentId());
    return PackageManagerCore::Success;
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
        IntroductionPageImpl *page =
            qobject_cast<IntroductionPageImpl*> (d->m_gui->page(PackageManagerCore::Introduction));
        if (page) {
            page->setMessage(QString());
            page->setErrorMessage(QString());
        }
        restartWizard();
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
