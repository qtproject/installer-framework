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

#include <KDUpdater/Application>
#include <KDUpdater/UpdateFinder>
#include <KDUpdater/UpdateSourcesInfo>
#include <KDUpdater/PackagesInfo>

#include <common/installersettings.h>
#include <common/utils.h>
#include <common/errors.h>

#include <componentselectiondialog.h>
#include <getrepositoriesmetainfojob.h>
#include <qinstaller.h>
#include <qinstallercomponent.h>
#include <updater.h>
#include <qinstallergui.h>

#include <QtCore/QDebug>
#include <QtCore/QHash>
#include <QtCore/QPointer>

#include <QtGui/QDialog>
#include <QtGui/QProgressDialog>

#include <QtScript/QScriptEngine>

#include <QtXml/QDomDocument>
#include <QtXml/QDomElement>

class TabController::Private
{
public:
    Private();
    ~Private();
    void preselectInstalledPackages();

    QHash<QString, QString> m_params;
    bool m_updaterInitialized;
    bool m_packageManagerInitialized;
    bool m_init;
    QPointer<QInstaller::Gui> m_gui;
    QScopedPointer <QInstaller::Updater> m_updater;
    QInstaller::Installer *m_installer;
    KDUpdater::Application *m_app;
    QString m_controlScript;
    int m_Tab_Pos_Updater;
    int m_Tab_Pos_PackageManager;
    QInstaller::Installer::Status m_state;
    bool m_repoReached;
    bool m_repoUpdateNeeded;
};

TabController::Private::Private()
    :
    m_updaterInitialized(false),
    m_packageManagerInitialized(false),
    m_init(false),
    m_gui(0),
    m_updater(0),
    m_installer(0),
    m_app(0),
    m_Tab_Pos_Updater(0),
    m_Tab_Pos_PackageManager(0),
    m_state(QInstaller::Installer::Success),
    m_repoReached(true),
    m_repoUpdateNeeded(true)
{
}

TabController::Private::~Private()
{
    delete m_gui;
}

// the package manager should preselect the currently installed packages
void TabController::Private::preselectInstalledPackages()
{
    Q_ASSERT(m_installer->isPackageManager());

    QList<QInstaller::Component*>::const_iterator it;
    const QList<QInstaller::Component*> components = m_installer->components(true);
    for (it = components.begin(); it != components.end(); ++it) {
        QInstaller::Component* const comp = *it;
        const bool selected = m_app->packagesInfo()->findPackageInfo(comp->name()) > -1;
        comp->setSelected(selected, QInstaller::AllMode,
            QInstaller::Component::InitializeComponentTreeSelectMode);
        comp->setEnabled(m_repoReached || selected);
    }
}

// -- TabController

TabController::TabController(QObject *parent)
    : QObject(parent)
    , d(new Private)
{
}

TabController::~TabController()
{
    delete d;
}

int TabController::init()
{
    if (!d->m_installer->isInstaller()) {
        connect(d->m_gui, SIGNAL(accepted()), this, SLOT(finished()));
        connect(d->m_gui, SIGNAL(rejected()), this, SLOT(finished()));
        d->m_gui->setWindowTitle(d->m_installer->value(QLatin1String("MaintenanceTitle")));
    }

    IntroductionPageImpl *introPage =
        qobject_cast<IntroductionPageImpl*>(d->m_gui->page(QInstaller::Installer::Introduction));
    connect(introPage, SIGNAL(initUpdater()), this, SLOT(initUpdater()));
    connect(introPage, SIGNAL(initPackageManager()), this, SLOT(initPackageManager()));

    //if (!d->m_installer->isInstaller() && !d->m_init) {
        //d->m_updater.reset(new Updater);
        //d->m_updater->setInstaller(d->m_installer);

        //QInstaller::ComponentSelectionDialog* w =
        //    new QInstaller::ComponentSelectionDialog(d->m_installer);
        //d->m_updater->setUpdaterGui(w);
        //d->m_updater->init();

        //connect(d->m_updater.data(), SIGNAL(updateFinished(bool)), this, SLOT(updaterFinished(bool)));
        //connect(d->m_updater.data(), SIGNAL(updateFinished(bool)), w, SLOT(refreshDialog()));
        //connect(w, SIGNAL(rejected()), this, SLOT(updaterFinishedWithError()));
        //connect(w, SIGNAL(finished(int)), this, SLOT(updaterFinished(int)));
        //connect(this, SIGNAL(refresh()), w, SLOT(refreshDialog()));
        //connect(d->m_updaterGuiWidget, SIGNAL(currentChanged(int)), this, SLOT(changeCurrentTab(int)));
        //connect(d->m_updaterGuiWidget, SIGNAL(closeRequested()), this, SLOT(close()),
        //    Qt::QueuedConnection);
        //connect(d->m_installer, SIGNAL(installationStarted()), this, SLOT(disableUpdaterTab()));
        //connect(d->m_installer, SIGNAL(uninstallationStarted()), this, SLOT(disableUpdaterTab()));
        //d->m_init = true;
        //d->m_gui->setWindowTitle(d->m_installer->value(QLatin1String("MaintenanceTitle")));
    //}

    if (d->m_installer->isUpdater())
        return initUpdater();

    if (d->m_installer->isUninstaller())
        return initUninstaller();

    return initPackageManager();
}

void TabController::setCurrentTab(int tab)
{
    //if (!d->m_updaterGuiWidget)
    //    return;

    //d->m_updaterGuiWidget->setCurrentIndex((tab == PACKAGE_MANAGER_TAB)
    //    ? d->m_Tab_Pos_PackageManager : d->m_Tab_Pos_Updater);
    //d->m_gui->callControlScriptMethod(QLatin1String((tab == PACKAGE_MANAGER_TAB)
    //    ? "PackageManagerSelectedCallback" : "UpdaterSelectedCallback"));
}

void TabController::setInstallerParams(const QHash<QString, QString> &params)
{
    d->m_params = params;
}

void TabController::setInstallerGui(QInstaller::Gui *gui)
{
    d->m_gui = gui;
    connect(d->m_gui, SIGNAL(gotRestarted()), this, SLOT(restartWizard()));
}

void TabController::setInstaller(QInstaller::Installer *installer)
{
    d->m_installer = installer;
}

void TabController::setTabWidget(MainTabWidget *widget)
{
    //d->m_updaterGuiWidget = widget;
}

void TabController::setApplication(KDUpdater::Application *app)
{
    d->m_app = app;
}

void TabController::setControlScript (const QString &script)
{
    d->m_controlScript = script;
}

void TabController::close()
{
    if (!d->m_installer->isInstaller() && !d->m_gui.isNull())
        d->m_gui->cancelButtonClicked();
}

void TabController::disableUpdaterTab()
{
    //d->m_updaterGuiWidget->setTabEnabled(d->m_Tab_Pos_Updater, false);
}

void TabController::enableUpdaterTab()
{
    //d->m_updaterGuiWidget->setTabEnabled(d->m_Tab_Pos_Updater, true);
}

void TabController::restartWizard()
{
    // TODO: How useful is this, should we do this for the updater as well?
    //enableUpdaterTab();
    d->m_installer->reset(d->m_params);
    checkRepositories();

    if (d->m_installer->isPackageManager())
        d->preselectInstalledPackages();
}

void TabController::finished()
{
    d->m_installer->writeUninstaller();
    //d->m_updaterGuiWidget->setCloseWithoutWarning(true);
    //bool res = d->m_updaterGuiWidget->close();
    //bool res2 = d->m_updaterGuiWidget->close();
    //QInstaller::verbose() << " widget was closed ? : " << res << " " << res2 << std::endl;
}

void TabController::updaterFinishedWithError()
{
    d->m_installer->writeUninstaller();
    d->m_state = QInstaller::Installer::Canceled;

    if (!d->m_installer->isInstaller() && !d->m_gui.isNull())
        d->m_gui->rejectWithoutPrompt();

    finished();
}

int TabController::status() const
{
    return d->m_state;
}

int TabController::checkRepositories()
{
    using namespace QInstaller;

    const bool isInstaller = d->m_installer->isInstaller();
    const bool needsRepoCheck = d->m_installer->isPackageManager() || d->m_installer->isUpdater();

    GetRepositoriesMetaInfoJob metaInfoJob(d->m_installer->settings().publicKey(), needsRepoCheck);
    if ((isInstaller && !d->m_installer->isOfflineOnly()) || needsRepoCheck)
        metaInfoJob.setRepositories(d->m_installer->settings().repositories());

    IntroductionPageImpl *introPage =
        qobject_cast<IntroductionPageImpl*>(d->m_gui->page(Installer::Introduction));
    introPage->setComplete(false);
    introPage->showMetaInfoUdate();
    if (needsRepoCheck)
        introPage->showAll();

    metaInfoJob.connect(&metaInfoJob, SIGNAL(infoMessage(KDJob*, QString)), introPage,
        SLOT(message(KDJob*, QString)));

    // show the wizard-gui as early as possible
    d->m_gui->connect(d->m_gui, SIGNAL(rejected()), &metaInfoJob, SLOT(doCancel()),
        Qt::QueuedConnection);
    d->m_gui->setWindowModality(Qt::WindowModal);
    d->m_gui->show();

    // start...
    try {
        metaInfoJob.setAutoDelete(false);
        metaInfoJob.start();
        metaInfoJob.waitForFinished();
        d->m_repoReached = true;
    } catch (const Error &e) {
        MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(),
            QLatin1String("Unknown error"), tr("Unknown error"), e.message(), QMessageBox::Ok,
            QMessageBox::Ok);
    }

    if (metaInfoJob.isCanceled() || metaInfoJob.error() == KDJob::Canceled)
        return QInstaller::Installer::Canceled;

    if (metaInfoJob.error() == GetRepositoriesMetaInfoJob::UserIgnoreError) {
        d->m_repoReached = false;
    } else if (metaInfoJob.error() != KDJob::NoError) {
        MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(),
            QLatin1String("metaInfoJobError"), QObject::tr("Error"), metaInfoJob.errorString());
        return QInstaller::Installer::Failure;
    }

    const QInstaller::TempDirDeleter tempDirDeleter(metaInfoJob.releaseTemporaryDirectories());
    const QStringList tempDirs = tempDirDeleter.paths();

    foreach (const QString &i, tempDirs) {
        if (i.isEmpty())
            continue;
        verbose() << "addUpdateSource()" << QUrl::fromLocalFile(i) << std::endl;
        const QString updatesXmlPath = i + QLatin1String("/Updates.xml");

        QFile updatesFile(updatesXmlPath);
        try {
            openForRead(&updatesFile, updatesFile.fileName());
        } catch(const Error &e) {
            MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(),
                QLatin1String("Open error"), tr("Error parsing Updates.xml"), e.message());
            return QInstaller::Installer::Failure;
        }

        verbose() << "Path to Update.xml " << qPrintable(updatesXmlPath) << std::endl;

        QString err;
        int line = 0;
        int col = 0;
        QDomDocument doc;
        if (!doc.setContent(&updatesFile, &err, &line, &col)) {
            MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(),
                QLatin1String("parseError"), tr("Error parsing Updates.xml"),
                tr("Parse error in File %4 : %1 at line %2 col %3").arg(err, QString::number(line),
                QString::number(col), updatesFile.fileName()));
            return QInstaller::Installer::Failure;
        }

        const QDomNode checksum = doc.documentElement().firstChildElement(QLatin1String("Checksum"));
        if (!checksum.isNull()) {
            const QDomElement checksumElem = checksum.toElement();
            d->m_installer->setTestChecksum(checksumElem.text() == QLatin1String("true"));
        }

        verbose() << "addUpdateSource()" << QUrl::fromLocalFile(i) << std::endl;
        const QString productName = d->m_installer->value(QLatin1String("ProductName"));
        d->m_app->addUpdateSource(productName, productName, QString(), QUrl::fromLocalFile(i), 1);
    }

    // The changes done above by adding update source don't count as modification that
    // need to be saved cause they will be re-set on the next start anyway. This does
    // prevent creating of xml files not needed atm. We can still set the modified
    // state later once real things changed that we like to restore at the  next startup.
    d->m_app->updateSourcesInfo()->setModified(false);

    // create the packages info
    KDUpdater::UpdateFinder* const updateFinder = new KDUpdater::UpdateFinder(d->m_app);
    QObject::connect(updateFinder, SIGNAL(error(int, QString)), d->m_app, SLOT(printError(int, QString)));
    updateFinder->setUpdateType(KDUpdater::PackageUpdate | KDUpdater::NewPackage);
    updateFinder->run();

    try {
        // now create installable components
        d->m_installer->createComponents(updateFinder->updates(), metaInfoJob);
    } catch (const Error &e) {
        MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(),
            QLatin1String("createComponentsError"), QObject::tr("Error"), e.message());
    }

    if (d->m_installer->status() == QInstaller::Installer::Failure)
        return QInstaller::Installer::Failure;

    // we need at least one component else the installer makes no
    // sense cause nothing can be installed.
    if (isInstaller)
        Q_ASSERT(!d->m_installer->components().isEmpty());
    d->m_repoUpdateNeeded = false;

    if (!d->m_installer->isInstaller())
        introPage->showMaintenanceTools();
    else
        introPage->hideAll();

    return QInstaller::Installer::Success;
}

void TabController::updaterFinished(bool error)
{
    if (d->m_installer->needsRestart())
        updaterFinishedWithError();
    else if (!error)
        checkRepositories();
}

void TabController::updaterFinished(int val)
{
    if (val != QDialog::Accepted) {
        if (!d->m_installer->isInstaller() && !d->m_gui.isNull())
            d->m_gui->rejectWithoutPrompt();
        finished();
    }
}

void TabController::canceled()
{
    d->m_installer->writeUninstaller();
    //d->m_updaterGuiWidget->setCloseWithoutWarning(true);
}

int TabController::initUpdater()
{
    using namespace QInstaller;

    if (d->m_updaterInitialized)
        return QInstaller::Installer::Success;

    d->m_updaterInitialized = true;

    IntroductionPageImpl *introPage =
        qobject_cast<IntroductionPageImpl*>(d->m_gui->page(Installer::Introduction));
    introPage->showAll();
    introPage->setComplete(false);

    connect(d->m_installer, SIGNAL(updaterInfoMessage(KDJob*, QString)), introPage,
        SLOT(message(KDJob*, QString)));
    d->m_gui->connect(d->m_gui, SIGNAL(rejected()), d->m_installer, SIGNAL(cancelUpdaterInfoJob()),
        Qt::QueuedConnection);

    d->m_gui->setWindowModality(Qt::WindowModal);
    d->m_gui->show();

    d->m_installer->setLinearComponentList(true);
    if (!d->m_installer->fetchUpdaterPackages())
        return QInstaller::Installer::Failure;

    // Initialize the gui. Needs to be done after check repositories as only then the ui can handle
    // hide of pages depenging on the components.
    d->m_gui->init();
    d->m_gui->callControlScriptMethod(QLatin1String("UpdaterSelectedCallback"));
    d->m_gui->triggerControlScriptForCurrentPage();

    introPage->setComplete(true);
    introPage->showMaintenanceTools();

    return QInstaller::Installer::Success;
}

int TabController::initUninstaller()
{
    IntroductionPageImpl *introPage =
        qobject_cast<IntroductionPageImpl*>(d->m_gui->page(QInstaller::Installer::Introduction));
    introPage->setComplete(true);
    introPage->showMaintenanceTools();

    d->m_gui->setWindowModality(Qt::WindowModal);
    d->m_gui->show();

    return QInstaller::Installer::Success;
}

int TabController::initPackageManager()
{
    using namespace QInstaller;

    if (d->m_packageManagerInitialized)
        return QInstaller::Installer::Success;

    d->m_packageManagerInitialized = true;

    // this should called as early as possible, to handle checkRepositories error messageboxes for
    // example
    if (!d->m_controlScript.isEmpty()) {
        // TODO: Check if we really need from this, or what parts of it.
        QInstaller::verbose() << "Non-interactive installation using script: "
            << qPrintable(d->m_controlScript) << std::endl;
        d->m_gui->loadControlScript(d->m_controlScript);
        QScriptEngine* engine = d->m_gui->controlScriptEngine();
        QScriptValue tabControllerValues = engine->newArray();

        tabControllerValues.setProperty(QLatin1String("UPDATER"),
            engine->newVariant(static_cast<int>(TabController::UPDATER_TAB)));
        tabControllerValues.setProperty(QLatin1String("PACKAGE_MANAGER"),
            engine->newVariant(static_cast<int>(TabController::PACKAGE_MANAGER_TAB)));

        tabControllerValues.setProperty(QLatin1String("UPDATER_TAB"),
            engine->newVariant(static_cast<int>(TabController::UPDATER_TAB)));
        tabControllerValues.setProperty(QLatin1String("PACKAGE_MANAGER_TAB"),
            engine->newVariant(static_cast<int>(TabController::PACKAGE_MANAGER_TAB)));

        engine->globalObject().setProperty(QLatin1String("TabController"), tabControllerValues);
        engine->globalObject().setProperty(QLatin1String("tabController"), engine->newQObject(this));
        if (d->m_updater && d->m_updater->updaterGui()) {
            Q_ASSERT(d->m_updater->updaterGui());
            engine->globalObject().setProperty(QLatin1String("updaterGui"),
                engine->newQObject(d->m_updater->updaterGui() ));
        }
    }

    IntroductionPageImpl *introPage =
        qobject_cast<IntroductionPageImpl*>(d->m_gui->page(Installer::Introduction));
    introPage->showMetaInfoUdate();
    if (d->m_installer->isPackageManager())
        introPage->showAll();
    introPage->setComplete(false);

    connect(d->m_installer, SIGNAL(allComponentsInfoMessage(KDJob*,QString)), introPage,
        SLOT(message(KDJob*, QString)));
    d->m_gui->connect(d->m_gui, SIGNAL(rejected()), d->m_installer, SIGNAL(cancelAllComponentsInfoJob()),
        Qt::QueuedConnection);

    d->m_gui->setWindowModality(Qt::WindowModal);
    d->m_gui->show();

    if (!d->m_installer->fetchAllPackages())
        return QInstaller::Installer::Failure;

    // Initialize the gui. Needs to be done after check repositories as only then the ui can handle
    // hide of pages depenging on the components.
    d->m_gui->init();
    d->m_gui->callControlScriptMethod(QLatin1String("PackageManagerSelectedCallback"));
    d->m_gui->triggerControlScriptForCurrentPage();

    introPage->setComplete(true);

    if (d->m_installer->isPackageManager())
        introPage->showMaintenanceTools();
    else
        introPage->hideAll();

    return QInstaller::Installer::Success;
}

void TabController::changeCurrentTab(int index)
{
    if (index == d->m_Tab_Pos_PackageManager) {
        if (!d->m_packageManagerInitialized)
            initPackageManager();
    } else {
        if (!d->m_updaterInitialized)
            initUpdater();
    }
}
