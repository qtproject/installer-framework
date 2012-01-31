/**************************************************************************
**
** This file is part of Qt SDK**
**
** Copyright (c) 2012 Nokia Corporation and/or its subsidiary(-ies).*
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
#include "packagemanagercore.h"

#include "adminauthorization.h"
#include "common/binaryformat.h"
#include "common/errors.h"
#include "common/utils.h"
#include "component.h"
#include "downloadarchivesjob.h"
#include "fsengineclient.h"
#include "getrepositoriesmetainfojob.h"
#include "messageboxhandler.h"
#include "packagemanagercore_p.h"
#include "packagemanagerproxyfactory.h"
#include "progresscoordinator.h"
#include "qinstallerglobal.h"
#include "qprocesswrapper.h"
#include "qsettingswrapper.h"
#include "settings.h"

#include <QtCore/QTemporaryFile>

#include <QtGui/QDesktopServices>

#include <QtScript/QScriptEngine>
#include <QtScript/QScriptContext>

#include <kdsysinfo.h>
#include <kdupdaterupdateoperationfactory.h>

#ifdef Q_OS_WIN
#include "qt_windows.h"
#endif

using namespace QInstaller;

static QFont sVirtualComponentsFont;

static bool sNoForceInstallation = false;
static bool sVirtualComponentsVisible = false;

static QScriptValue checkArguments(QScriptContext *context, int amin, int amax)
{
    if (context->argumentCount() < amin || context->argumentCount() > amax) {
        if (amin != amax) {
            return context->throwError(QObject::tr("Invalid arguments: %1 arguments given, %2 to "
                "%3 expected.").arg(QString::number(context->argumentCount()),
                QString::number(amin), QString::number(amax)));
        }
        return context->throwError(QObject::tr("Invalid arguments: %1 arguments given, %2 expected.")
            .arg(QString::number(context->argumentCount()), QString::number(amin)));
    }
    return QScriptValue();
}

static bool componentMatches(const Component *component, const QString &name,
    const QString &version = QString())
{
    if (name.isEmpty() || component->name() != name)
        return false;

    if (version.isEmpty())
        return true;

    // can be remote or local version
    return PackageManagerCore::versionMatches(component->value(scVersion), version);
}

Component *PackageManagerCore::subComponentByName(const QInstaller::PackageManagerCore *installer,
    const QString &name, const QString &version, Component *check)
{
    if (name.isEmpty())
        return 0;

    if (check != 0 && componentMatches(check, name, version))
        return check;

    if (installer->runMode() == AllMode) {
        QList<Component*> rootComponents;
        if (check == 0)
            rootComponents = installer->rootComponents();
        else
            rootComponents = check->childComponents(false, AllMode);

        foreach (QInstaller::Component *component, rootComponents) {
            Component *const result = subComponentByName(installer, name, version, component);
            if (result != 0)
                return result;
        }
    } else {
        const QList<Component*> updaterComponents = installer->updaterComponents()
            + installer->d->m_updaterComponentsDeps;
        foreach (QInstaller::Component *component, updaterComponents) {
            if (componentMatches(component, name, version))
                return component;
        }
    }
    return 0;
}

/*!
    Scriptable version of PackageManagerCore::componentByName(QString).
    \sa PackageManagerCore::componentByName
 */
QScriptValue QInstaller::qInstallerComponentByName(QScriptContext *context, QScriptEngine *engine)
{
    const QScriptValue check = checkArguments(context, 1, 1);
    if (check.isError())
        return check;

    // well... this is our "this" pointer
    PackageManagerCore *const core = dynamic_cast<PackageManagerCore*>(engine->globalObject()
        .property(QLatin1String("installer")).toQObject());

    const QString name = context->argument(0).toString();
    return engine->newQObject(core->componentByName(name));
}

QScriptValue QInstaller::qDesktopServicesOpenUrl(QScriptContext *context, QScriptEngine *engine)
{
    Q_UNUSED(engine);
    const QScriptValue check = checkArguments(context, 1, 1);
    if (check.isError())
        return check;
    QString url = context->argument(0).toString();
    url.replace(QLatin1String("\\\\"), QLatin1String("/"));
    url.replace(QLatin1String("\\"), QLatin1String("/"));
    return QDesktopServices::openUrl(QUrl::fromUserInput(url));
}

QScriptValue QInstaller::qDesktopServicesDisplayName(QScriptContext *context, QScriptEngine *engine)
{
    Q_UNUSED(engine);
    const QScriptValue check = checkArguments(context, 1, 1);
    if (check.isError())
        return check;
    const QDesktopServices::StandardLocation location =
        static_cast< QDesktopServices::StandardLocation >(context->argument(0).toInt32());
    return QDesktopServices::displayName(location);
}

QScriptValue QInstaller::qDesktopServicesStorageLocation(QScriptContext *context, QScriptEngine *engine)
{
    Q_UNUSED(engine);
    const QScriptValue check = checkArguments(context, 1, 1);
    if (check.isError())
        return check;
    const QDesktopServices::StandardLocation location =
        static_cast< QDesktopServices::StandardLocation >(context->argument(0).toInt32());
    return QDesktopServices::storageLocation(location);
}

QString QInstaller::uncaughtExceptionString(QScriptEngine *scriptEngine, const QString &context)
{
    QString error(QLatin1String("\n\n%1\n\nBacktrace:\n\t%2"));
    if (!context.isEmpty())
        error.prepend(context);

    return error.arg(scriptEngine->uncaughtException().toString(), scriptEngine->uncaughtExceptionBacktrace()
        .join(QLatin1String("\n\t")));
}


/*!
    \class QInstaller::PackageManagerCore
    PackageManagerCore forms the core of the installation, update, maintenance and un-installation system.
 */

/*!
    \enum QInstaller::PackageManagerCore::WizardPage
    WizardPage is used to number the different pages known to the Installer GUI.
 */

/*!
    \var QInstaller::PackageManagerCore::Introduction
  I ntroduction page.
 */

/*!
    \var QInstaller::PackageManagerCore::LicenseCheck
    License check page
 */
/*!
    \var QInstaller::PackageManagerCore::TargetDirectory
    Target directory selection page
 */
/*!
    \var QInstaller::PackageManagerCore::ComponentSelection
    %Component selection page
 */
/*!
    \var QInstaller::PackageManagerCore::StartMenuSelection
    Start menu directory selection page - Microsoft Windows only
 */
/*!
    \var QInstaller::PackageManagerCore::ReadyForInstallation
    "Ready for Installation" page
 */
/*!
    \var QInstaller::PackageManagerCore::PerformInstallation
    Page shown while performing the installation
 */
/*!
    \var QInstaller::PackageManagerCore::InstallationFinished
    Page shown when the installation was finished
 */
/*!
    \var QInstaller::PackageManagerCore::End
    Non-existing page - this value has to be used if you want to insert a page after \a InstallationFinished
 */

void PackageManagerCore::writeUninstaller()
{
    if (d->m_needToWriteUninstaller) {
        try {
            d->writeUninstaller(d->m_performedOperationsOld + d->m_performedOperationsCurrentSession);

            bool gainedAdminRights = false;
            QTemporaryFile tempAdminFile(d->targetDir()
                + QLatin1String("/testjsfdjlkdsjflkdsjfldsjlfds") + QString::number(qrand() % 1000));
            if (!tempAdminFile.open() || !tempAdminFile.isWritable()) {
                gainAdminRights();
                gainedAdminRights = true;
            }
            d->m_updaterApplication.packagesInfo()->writeToDisk();
            if (gainedAdminRights)
                dropAdminRights();
            d->m_needToWriteUninstaller = false;
        } catch (const Error &error) {
            MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(),
                QLatin1String("WriteError"), tr("Error writing Uninstaller"), error.message(),
                QMessageBox::Ok, QMessageBox::Ok);
        }
    }
}

void PackageManagerCore::reset(const QHash<QString, QString> &params)
{
    d->m_completeUninstall = false;
    d->m_forceRestart = false;
    d->m_status = PackageManagerCore::Unfinished;
    d->m_installerBaseBinaryUnreplaced.clear();
    d->m_vars.clear();
    d->m_vars = params;
    d->initialize();
}

/*!
    Sets the uninstallation to be \a complete. If \a complete is false, only components deselected
    by the user will be uninstalled. This option applies only on uninstallation.
 */
void PackageManagerCore::setCompleteUninstallation(bool complete)
{
    d->m_completeUninstall = complete;
}

void PackageManagerCore::cancelMetaInfoJob()
{
    if (d->m_repoMetaInfoJob)
        d->m_repoMetaInfoJob->cancel();
}

void PackageManagerCore::componentsToInstallNeedsRecalculation()
{
    d->m_componentsToInstallCalculated = false;
}

void PackageManagerCore::autoAcceptMessageBoxes()
{
    MessageBoxHandler::instance()->setDefaultAction(MessageBoxHandler::Accept);
}

void PackageManagerCore::autoRejectMessageBoxes()
{
    MessageBoxHandler::instance()->setDefaultAction(MessageBoxHandler::Reject);
}

void PackageManagerCore::setMessageBoxAutomaticAnswer(const QString &identifier, int button)
{
    MessageBoxHandler::instance()->setAutomaticAnswer(identifier,
        static_cast<QMessageBox::Button>(button));
}

quint64 size(QInstaller::Component *component, const QString &value)
{
    if (!component->isSelected() || component->isInstalled())
        return quint64(0);
    return component->value(value).toLongLong();
}

quint64 PackageManagerCore::requiredDiskSpace() const
{
    quint64 result = 0;

    foreach (QInstaller::Component *component, rootComponents())
        result += component->updateUncompressedSize();

    return result;
}

quint64 PackageManagerCore::requiredTemporaryDiskSpace() const
{
    quint64 result = 0;

    foreach (QInstaller::Component *component, orderedComponentsToInstall())
        result += size(component, scCompressedSize);

    return result;
}

/*!
    Returns the count of archives that will be downloaded.
*/
int PackageManagerCore::downloadNeededArchives(double partProgressSize)
{
    Q_ASSERT(partProgressSize >= 0 && partProgressSize <= 1);

    QList<QPair<QString, QString> > archivesToDownload;
    QList<Component*> neededComponents = orderedComponentsToInstall();
    foreach (Component *component, neededComponents) {
        // collect all archives to be downloaded
        const QStringList toDownload = component->downloadableArchives();
        foreach (const QString &versionFreeString, toDownload) {
            archivesToDownload.push_back(qMakePair(QString::fromLatin1("installer://%1/%2")
                .arg(component->name(), versionFreeString), QString::fromLatin1("%1/%2/%3")
                .arg(component->repositoryUrl().toString(), component->name(), versionFreeString)));
        }
    }

    if (archivesToDownload.isEmpty())
        return 0;

    ProgressCoordinator::instance()->emitLabelAndDetailTextChanged(tr("\nDownloading packages..."));

    // don't have it on the stack, since it keeps the temporary files
    DownloadArchivesJob *const archivesJob =
        new DownloadArchivesJob(d->m_settings.publicKey(), this);
    archivesJob->setAutoDelete(false);
    archivesJob->setArchivesToDownload(archivesToDownload);
    connect(this, SIGNAL(installationInterrupted()), archivesJob, SLOT(cancel()));
    connect(archivesJob, SIGNAL(outputTextChanged(QString)), ProgressCoordinator::instance(),
        SLOT(emitLabelAndDetailTextChanged(QString)));
    connect(archivesJob, SIGNAL(downloadStatusChanged(QString)), ProgressCoordinator::instance(),
        SIGNAL(downloadStatusChanged(QString)));

    ProgressCoordinator::instance()->registerPartProgress(archivesJob, SIGNAL(progressChanged(double)),
        partProgressSize);

    archivesJob->start();
    archivesJob->waitForFinished();

    if (archivesJob->error() == KDJob::Canceled)
        interrupt();
    else if (archivesJob->error() != KDJob::NoError)
        throw Error(archivesJob->errorString());

    if (d->statusCanceledOrFailed())
        throw Error(tr("Installation canceled by user"));
    ProgressCoordinator::instance()->emitDownloadStatus(tr("All downloads finished."));

    return archivesToDownload.count();
}

void PackageManagerCore::installComponent(Component *component, double progressOperationSize)
{
    Q_ASSERT(progressOperationSize);

    d->setStatus(PackageManagerCore::Running);
    try {
        d->installComponent(component, progressOperationSize);
        d->setStatus(PackageManagerCore::Success);
    } catch (const Error &error) {
        if (status() != PackageManagerCore::Canceled) {
            d->setStatus(PackageManagerCore::Failure);
            MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(),
                QLatin1String("installationError"), tr("Error"), error.message());
        }
    }
}

/*!
    If a component marked as important was installed during update
    process true is returned.
*/
bool PackageManagerCore::needsRestart() const
{
    return d->m_forceRestart;
}

void PackageManagerCore::rollBackInstallation()
{
    emit titleMessageChanged(tr("Cancelling the Installer"));

    //this unregisters all operation progressChanged connects
    ProgressCoordinator::instance()->setUndoMode();
    const int progressOperationCount = d->countProgressOperations(d->m_performedOperationsCurrentSession);
    const double progressOperationSize = double(1) / progressOperationCount;

    //re register all the undo operations with the new size to the ProgressCoordninator
    foreach (Operation *const operation, d->m_performedOperationsCurrentSession) {
        QObject *const operationObject = dynamic_cast<QObject*> (operation);
        if (operationObject != 0) {
            const QMetaObject* const mo = operationObject->metaObject();
            if (mo->indexOfSignal(QMetaObject::normalizedSignature("progressChanged(double)")) > -1) {
                ProgressCoordinator::instance()->registerPartProgress(operationObject,
                    SIGNAL(progressChanged(double)), progressOperationSize);
            }
        }
    }

    KDUpdater::PackagesInfo &packages = *d->m_updaterApplication.packagesInfo();
    while (!d->m_performedOperationsCurrentSession.isEmpty()) {
        try {
            Operation *const operation = d->m_performedOperationsCurrentSession.takeLast();
            const bool becameAdmin = !d->m_FSEngineClientHandler->isActive()
                && operation->value(QLatin1String("admin")).toBool() && gainAdminRights();

            PackageManagerCorePrivate::performOperationThreaded(operation, PackageManagerCorePrivate::Undo);

            const QString componentName = operation->value(QLatin1String("component")).toString();
            if (!componentName.isEmpty()) {
                Component *component = componentByName(componentName);
                if (!component)
                    component = d->componentsToReplace(runMode()).value(componentName).second;
                if (component) {
                    component->setUninstalled();
                    packages.removePackage(component->name());
                }
            }

            if (becameAdmin)
                dropAdminRights();
        } catch (const Error &e) {
            MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(),
                QLatin1String("ElevationError"), tr("Authentication Error"), tr("Some components "
                "could not be removed completely because admin rights could not be acquired: %1.")
                .arg(e.message()));
        } catch (...) {
            MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(), QLatin1String("unknown"),
                tr("Unknown error."), tr("Some components could not be removed completely because an unknown "
                "error happened."));
        }
    }
    packages.writeToDisk();
}

bool PackageManagerCore::isFileExtensionRegistered(const QString &extension) const
{
    QSettingsWrapper settings(QLatin1String("HKEY_CLASSES_ROOT"), QSettingsWrapper::NativeFormat);
    return settings.value(QString::fromLatin1(".%1/Default").arg(extension)).isValid();
}


// -- QInstaller

/*!
    Used by operation runner to get a fake installer, can be removed if installerbase can do what operation
    runner does.
*/
PackageManagerCore::PackageManagerCore()
    : d(new PackageManagerCorePrivate(this))
{
}

PackageManagerCore::PackageManagerCore(qint64 magicmaker, const OperationList &performedOperations)
    : d(new PackageManagerCorePrivate(this, magicmaker, performedOperations))
{
    qRegisterMetaType<QInstaller::PackageManagerCore::Status>("QInstaller::PackageManagerCore::Status");
    qRegisterMetaType<QInstaller::PackageManagerCore::WizardPage>("QInstaller::PackageManagerCore::WizardPage");

    d->initialize();
}

PackageManagerCore::~PackageManagerCore()
{
    if (!isUninstaller() && !(isInstaller() && status() == PackageManagerCore::Canceled)) {
        QDir targetDir(value(scTargetDir));
        QString logFileName = targetDir.absoluteFilePath(value(QLatin1String("LogFileName"),
            QLatin1String("InstallationLog.txt")));
        QInstaller::VerboseWriter::instance()->setOutputStream(logFileName);
    }
    delete d;
}

/* static */
QFont PackageManagerCore::virtualComponentsFont()
{
    return sVirtualComponentsFont;
}

/* static */
void PackageManagerCore::setVirtualComponentsFont(const QFont &font)
{
    sVirtualComponentsFont = font;
}

/* static */
bool PackageManagerCore::virtualComponentsVisible()
{
    return sVirtualComponentsVisible;
}

/* static */
void PackageManagerCore::setVirtualComponentsVisible(bool visible)
{
    sVirtualComponentsVisible = visible;
}

/* static */
bool PackageManagerCore::noForceInstallation()
{
    return sNoForceInstallation;
}

/* static */
void PackageManagerCore::setNoForceInstallation(bool value)
{
    sNoForceInstallation = value;
}

RunMode PackageManagerCore::runMode() const
{
    return isUpdater() ? UpdaterMode : AllMode;
}

bool PackageManagerCore::fetchLocalPackagesTree()
{
    d->setStatus(Running);

    if (!isPackageManager()) {
        d->setStatus(Failure, tr("Application not running in Package Manager mode!"));
        return false;
    }

    LocalPackagesHash installedPackages = d->localInstalledPackages();
    if (installedPackages.isEmpty()) {
        if (status() != Failure)
            d->setStatus(Failure, tr("No installed packages found."));
        return false;
    }

    emit startAllComponentsReset();

    d->clearAllComponentLists();
    QHash<QString, QInstaller::Component*> components;

    const QStringList &keys = installedPackages.keys();
    foreach (const QString &key, keys) {
        QScopedPointer<QInstaller::Component> component(new QInstaller::Component(this));
        component->loadDataFromPackage(installedPackages.value(key));
        const QString &name = component->name();
        if (components.contains(name)) {
            qCritical("Could not register component! Component with identifier %s already registered.",
                qPrintable(name));
            continue;
        }
        components.insert(name, component.take());
    }

    if (!d->buildComponentTree(components, false))
        return false;

    updateDisplayVersions(scDisplayVersion);

    emit finishAllComponentsReset();
    d->setStatus(Success);

    return true;
}

LocalPackagesHash PackageManagerCore::localInstalledPackages()
{
    return d->localInstalledPackages();
}

void PackageManagerCore::networkSettingsChanged()
{
    cancelMetaInfoJob();

    d->m_updates = false;
    d->m_repoFetched = false;
    d->m_updateSourcesAdded = false;

    if (d->isUpdater() || d->isPackageManager())
        d->writeMaintenanceConfigFiles();
    KDUpdater::FileDownloaderFactory::instance().setProxyFactory(proxyFactory());

    emit coreNetworkSettingsChanged();
}

KDUpdater::FileDownloaderProxyFactory *PackageManagerCore::proxyFactory() const
{
    if (d->m_proxyFactory)
        return d->m_proxyFactory->clone();
    return new PackageManagerProxyFactory(this);
}

void PackageManagerCore::setProxyFactory(KDUpdater::FileDownloaderProxyFactory *factory)
{
    delete d->m_proxyFactory;
    d->m_proxyFactory = factory;
    KDUpdater::FileDownloaderFactory::instance().setProxyFactory(proxyFactory());
}

PackagesList PackageManagerCore::remotePackages()
{
    return d->remotePackages();
}

bool PackageManagerCore::fetchRemotePackagesTree()
{
    d->setStatus(Running);

    if (isUninstaller()) {
        d->setStatus(Failure, tr("Application running in Uninstaller mode!"));
        return false;
    }

    const LocalPackagesHash installedPackages = d->localInstalledPackages();
    if (!isInstaller() && status() == Failure)
        return false;

    if (!d->fetchMetaInformationFromRepositories())
        return false;

    if (!d->addUpdateResourcesFromRepositories(true))
        return false;

    const PackagesList &packages = d->remotePackages();
    if (packages.isEmpty())
        return false;

    bool success = false;
    if (runMode() == AllMode)
        success = fetchAllPackages(packages, installedPackages);
    else {
        success = fetchUpdaterPackages(packages, installedPackages);
    }

    updateDisplayVersions(scRemoteDisplayVersion);

    if (success && !d->statusCanceledOrFailed())
        d->setStatus(Success);
    return success;
}

/*!
    Adds the widget with objectName() \a name registered by \a component as a new page
    into the installer's GUI wizard. The widget is added before \a page.
    \a page has to be a value of \ref QInstaller::PackageManagerCore::WizardPage "WizardPage".
*/
bool PackageManagerCore::addWizardPage(Component *component, const QString &name, int page)
{
    if (QWidget* const widget = component->userInterface(name)) {
        emit wizardPageInsertionRequested(widget, static_cast<WizardPage>(page));
        return true;
    }
    return false;
}

/*!
    Removes the widget with objectName() \a name previously added to the installer's wizard
    by \a component.
*/
bool PackageManagerCore::removeWizardPage(Component *component, const QString &name)
{
    if (QWidget* const widget = component->userInterface(name)) {
        emit wizardPageRemovalRequested(widget);
        return true;
    }
    return false;
}

/*!
    Sets the visibility of the default page with id \a page to \a visible, i.e.
    removes or adds it from/to the wizard. This works only for pages which have been
    in the installer when it was started.
 */
bool PackageManagerCore::setDefaultPageVisible(int page, bool visible)
{
    emit wizardPageVisibilityChangeRequested(visible, page);
    return true;
}

/*!
    Adds the widget with objectName() \a name registered by \a component as an GUI element
    into the installer's GUI wizard. The widget is added on \a page.
    \a page has to be a value of \ref QInstaller::PackageManagerCore::WizardPage "WizardPage".
*/
bool PackageManagerCore::addWizardPageItem(Component *component, const QString &name, int page)
{
    if (QWidget* const widget = component->userInterface(name)) {
        emit wizardWidgetInsertionRequested(widget, static_cast<WizardPage>(page));
        return true;
    }
    return false;
}

/*!
    Removes the widget with objectName() \a name previously added to the installer's wizard
    by \a component.
*/
bool PackageManagerCore::removeWizardPageItem(Component *component, const QString &name)
{
    if (QWidget* const widget = component->userInterface(name)) {
        emit wizardWidgetRemovalRequested(widget);
        return true;
    }
    return false;
}

void PackageManagerCore::addUserRepositories(const QSet<Repository> &repositories)
{
    d->m_settings.addUserRepositories(repositories);
}

/*!
    Sets additional repository for this instance of the installer or updater.
    Will be removed after invoking it again.
*/
void PackageManagerCore::setTemporaryRepositories(const QSet<Repository> &repositories, bool replace)
{
    d->m_settings.setTemporaryRepositories(repositories, replace);
}

/*!
    Checks if the downloader should try to download sha1 checksums for archives.
*/
bool PackageManagerCore::testChecksum() const
{
    return d->m_testChecksum;
}

/*!
    Defines if the downloader should try to download sha1 checksums for archives.
*/
void PackageManagerCore::setTestChecksum(bool test)
{
    d->m_testChecksum = test;
}

/*!
    Returns the number of components in the list for installer and package manager mode. Might return 0 in
    case the engine has only been run in updater mode or no components have been fetched.
*/
int PackageManagerCore::rootComponentCount() const
{
    return d->m_rootComponents.size();
}

/*!
    Returns the component at index position \a i in the components list. \a i must be a valid index
    position in the list (i.e., 0 <= i < rootComponentCount()).
*/
Component *PackageManagerCore::rootComponent(int i) const
{
    return d->m_rootComponents.value(i, 0);
}

/*!
    Returns a list of root components if run in installer or package manager mode. Might return an empty list
    in case the engine has only been run in updater mode or no components have been fetched.
*/
QList<Component*> PackageManagerCore::rootComponents() const
{
    return d->m_rootComponents;
}

/*!
    Appends a component as root component to the internal storage for installer or package manager components.
    To append a component as a child to an already existing component, use Component::appendComponent(). Emits
    the componentAdded() signal.
*/
void PackageManagerCore::appendRootComponent(Component *component)
{
    d->m_rootComponents.append(component);
    emit componentAdded(component);
}

/*!
    Returns the number of components in the list for updater mode. Might return 0 in case the engine has only
    been run in installer or package manager mode or no components have been fetched.
*/
int PackageManagerCore::updaterComponentCount() const
{
    return d->m_updaterComponents.size();
}

/*!
    Returns the component at index position \a i in the updates component list. \a i must be a valid index
    position in the list (i.e., 0 <= i < updaterComponentCount()).
*/
Component *PackageManagerCore::updaterComponent(int i) const
{
    return d->m_updaterComponents.value(i, 0);
}

/*!
    Returns a list of components if run in updater mode. Might return an empty list in case the engine has only
    been run in installer or package manager mode or no components have been fetched.
*/
QList<Component*> PackageManagerCore::updaterComponents() const
{
    return d->m_updaterComponents;
}

/*!
    Appends a component to the internal storage for updater components. Emits the componentAdded() signal.
*/
void PackageManagerCore::appendUpdaterComponent(Component *component)
{
    component->setUpdateAvailable(true);
    d->m_updaterComponents.append(component);
    emit componentAdded(component);
}

/*!
    Returns a list of all available components found during a fetch. Note that depending on the run mode the
    returned list might have different values. In case of updater mode, components scheduled for an
    update as well as all possible dependencies are returned.
*/
QList<Component*> PackageManagerCore::availableComponents() const
{
    if (isUpdater())
        return d->m_updaterComponents + d->m_updaterComponentsDeps + d->m_updaterDependencyReplacements;

    QList<Component*> result = d->m_rootComponents;
    foreach (QInstaller::Component *component, d->m_rootComponents)
        result += component->childComponents(true, AllMode);
    return result + d->m_rootDependencyReplacements;
}

/*!
    Returns a component matching \a name. \a name can also contains a version requirement.
    E.g. "com.nokia.sdk.qt" returns any component with that name, "com.nokia.sdk.qt->=4.5" requires
    the returned component to have at least version 4.5.
    If no component matches the requirement, 0 is returned.
*/
Component *PackageManagerCore::componentByName(const QString &name) const
{
    if (name.isEmpty())
        return 0;

    if (name.contains(QChar::fromLatin1('-'))) {
        // the last part is considered to be the version, then
        const QString version = name.section(QLatin1Char('-'), 1);
        return subComponentByName(this, name.section(QLatin1Char('-'), 0, 0), version);
    }

    return subComponentByName(this, name);
}

/*!
    Calculates an ordered list of components to install based on the current run mode. Also auto installed
    dependencies are resolved.
*/
bool PackageManagerCore::calculateComponentsToInstall() const
{
    if (!d->m_componentsToInstallCalculated) {
        d->clearComponentsToInstall();
        QList<Component*> components;
        if (runMode() == UpdaterMode) {
            foreach (Component *component, updaterComponents()) {
                if (component->updateRequested())
                    components.append(component);
            }
        } else if (runMode() == AllMode) {
            // relevant means all components which are not replaced
            QList<Component*> relevantComponents = rootComponents();
            foreach (QInstaller::Component *component, rootComponents())
                relevantComponents += component->childComponents(true, AllMode);
            foreach (Component *component, relevantComponents) {
                // ask for all components which will be installed to get all dependencies
                // even dependencies wich are changed without an increased version
                if (component->installationRequested() || (component->isInstalled() && !component->uninstallationRequested()))
                    components.append(component);
            }
        }

        d->m_componentsToInstallCalculated = d->appendComponentsToInstall(components);
    }
    return d->m_componentsToInstallCalculated;
}

/*!
    Returns a list of ordered components to install. The list can be empty.
*/
QList<Component*> PackageManagerCore::orderedComponentsToInstall() const
{
    return d->m_orderedComponentsToInstall;
}

/*!
    Calculates a list of components to uninstall based on the current run mode. Auto installed dependencies
    are resolved as well.
*/
bool PackageManagerCore::calculateComponentsToUninstall() const
{
    if (runMode() == UpdaterMode)
        return true;

    // hack to avoid removeing needed dependencies
    QSet<Component*>  componentsToInstall = d->m_orderedComponentsToInstall.toSet();

    QList<Component*> components;
    foreach (Component *component, availableComponents()) {
        if (component->uninstallationRequested() && !componentsToInstall.contains(component))
            components.append(component);
    }


    d->m_componentsToUninstall.clear();
    return d->appendComponentsToUninstall(components);
}

/*!
    Returns a list of components to uninstall. The list can be empty.
*/
QList<Component *> PackageManagerCore::componentsToUninstall() const
{
    return d->m_componentsToUninstall.toList();
}

QString PackageManagerCore::componentsToInstallError() const
{
    return d->m_componentsToInstallError;
}

/*!
    Returns the reason why the component needs to be installed. Reasons can be: The component was scheduled
    for installation, the component was added as a dependency for an other component or added as an automatic
    dependency.
*/
QString PackageManagerCore::installReason(Component *component) const
{
    return d->installReason(component);
}

/*!
    Returns a list of components that dependend on \a component. The list can be empty. Note: Auto
    installed dependencies are not resolved.
*/
QList<Component*> PackageManagerCore::dependees(const Component *_component) const
{
    QList<Component*> dependees;
    const QList<Component*> components = availableComponents();
    if (!_component || components.isEmpty())
        return dependees;

    const QLatin1Char dash('-');
    foreach (Component *component, components) {
        const QStringList &dependencies = component->dependencies();
        foreach (const QString &dependency, dependencies) {
            // the last part is considered to be the version then
            const QString name = dependency.contains(dash) ? dependency.section(dash, 0, 0) : dependency;
            const QString version = dependency.contains(dash) ? dependency.section(dash, 1) : QString();
            if (componentMatches(_component, name, version))
                dependees.append(component);
        }
    }
    return dependees;
}

/*!
    Returns a list of dependencies for \a component. If there's a dependency which cannot be fulfilled,
    \a missingComponents will contain the missing components. Note: Auto installed dependencies are not
    resolved.
*/
QList<Component*> PackageManagerCore::dependencies(const Component *component, QStringList &missingComponents) const
{
    QList<Component*> result;
    foreach (const QString &dependency, component->dependencies()) {
        Component *component = componentByName(dependency);
        if (component)
            result.append(component);
        else
            missingComponents.append(dependency);
    }
    return result;
}

Settings &PackageManagerCore::settings() const
{
    return d->m_settings;
}

/*!
    This method tries to gain admin rights. On success, it returns true.
*/
bool PackageManagerCore::gainAdminRights()
{
    if (AdminAuthorization::hasAdminRights())
        return true;

    d->m_FSEngineClientHandler->setActive(true);
    if (!d->m_FSEngineClientHandler->isActive())
        throw Error(QObject::tr("Error while elevating access rights."));
    return true;
}

/*!
    This method drops gained admin rights.
*/
void PackageManagerCore::dropAdminRights()
{
    d->m_FSEngineClientHandler->setActive(false);
}

/*!
    Return true, if a process with \a name is running. On Windows, the comparison
    is case-insensitive.
*/
bool PackageManagerCore::isProcessRunning(const QString &name) const
{
    return PackageManagerCorePrivate::isProcessRunning(name, runningProcesses());
}

/*!
    Executes a program.

    \param program The program that should be executed.
    \param arguments Optional list of arguments.
    \param stdIn Optional stdin the program reads.
    \return If the command could not be executed, an empty QList, otherwise the output of the
            command as first item, the return code as second item.
    \note On Unix, the output is just the output to stdout, not to stderr.
*/

QList<QVariant> PackageManagerCore::execute(const QString &program, const QStringList &arguments,
    const QString &stdIn) const
{
    QEventLoop loop;
    QProcessWrapper process;

    QString adjustedProgram = replaceVariables(program);
    QStringList adjustedArguments;
    foreach (const QString &argument, arguments)
        adjustedArguments.append(replaceVariables(argument));
    QString adjustedStdIn = replaceVariables(stdIn);

    connect(&process, SIGNAL(finished(int, QProcess::ExitStatus)), &loop, SLOT(quit()));
    process.start(adjustedProgram, adjustedArguments,
        adjustedStdIn.isNull() ? QIODevice::ReadOnly : QIODevice::ReadWrite);

    if (!process.waitForStarted())
        return QList< QVariant >();

    if (!adjustedStdIn.isNull()) {
        process.write(adjustedStdIn.toLatin1());
        process.closeWriteChannel();
    }

    if (process.state() != QProcessWrapper::NotRunning)
        loop.exec();

    return QList<QVariant>() << QString::fromLatin1(process.readAllStandardOutput()) << process.exitCode();
}

/*!
    Executes a program.

    \param program The program that should be executed.
    \param arguments Optional list of arguments.
    \return If the command could not be executed, an false will be returned
*/

bool PackageManagerCore::executeDetached(const QString &program, const QStringList &arguments) const
{
    QString adjustedProgram = replaceVariables(program);
    QStringList adjustedArguments;
    foreach (const QString &argument, arguments)
        adjustedArguments.append(replaceVariables(argument));
    return QProcess::startDetached(adjustedProgram, adjustedArguments);
}


/*!
    Returns an environment variable.
*/
QString PackageManagerCore::environmentVariable(const QString &name) const
{
#ifdef Q_WS_WIN
    const LPCWSTR n =  (LPCWSTR) name.utf16();
    LPTSTR buff = (LPTSTR) malloc(4096 * sizeof(TCHAR));
    DWORD getenvret = GetEnvironmentVariable(n, buff, 4096);
    const QString actualValue = getenvret != 0
        ? QString::fromUtf16((const unsigned short *) buff) : QString();
    free(buff);
    return actualValue;
#else
    const char *pPath = name.isEmpty() ? 0 : getenv(name.toLatin1());
    return pPath ? QLatin1String(pPath) : QString();
#endif
}

/*!
    Instantly performs an operation \a name with \a arguments.
    \sa Component::addOperation
*/
bool PackageManagerCore::performOperation(const QString &name, const QStringList &arguments)
{
    QScopedPointer<Operation> op(KDUpdater::UpdateOperationFactory::instance().create(name));
    if (!op.data())
        return false;

    op->setArguments(replaceVariables(arguments));
    op->backup();
    if (!PackageManagerCorePrivate::performOperationThreaded(op.data())) {
        PackageManagerCorePrivate::performOperationThreaded(op.data(), PackageManagerCorePrivate::Undo);
        return false;
    }
    return true;
}

/*!
    Returns true when \a version matches the \a requirement.
    \a requirement can be a fixed version number or it can be prefix by the comparators '>', '>=',
    '<', '<=' and '='.
*/
bool PackageManagerCore::versionMatches(const QString &version, const QString &requirement)
{
    QRegExp compEx(QLatin1String("([<=>]+)(.*)"));
    const QString comparator = compEx.exactMatch(requirement) ? compEx.cap(1) : QLatin1String("=");
    const QString ver = compEx.exactMatch(requirement) ? compEx.cap(2) : requirement;

    const bool allowEqual = comparator.contains(QLatin1Char('='));
    const bool allowLess = comparator.contains(QLatin1Char('<'));
    const bool allowMore = comparator.contains(QLatin1Char('>'));

    if (allowEqual && version == ver)
        return true;

    if (allowLess && KDUpdater::compareVersion(ver, version) > 0)
        return true;

    if (allowMore && KDUpdater::compareVersion(ver, version) < 0)
        return true;

    return false;
}

/*!
    Finds a library named \a name in \a paths.
    If \a paths is empty, it gets filled with platform dependent default paths.
    The resulting path is stored in \a library.
    This method can be used by scripts to check external dependencies.
*/
QString PackageManagerCore::findLibrary(const QString &name, const QStringList &paths)
{
    QStringList findPaths = paths;
#if defined(Q_WS_WIN)
    return findPath(QString::fromLatin1("%1.lib").arg(name), findPaths);
#else
    if (findPaths.isEmpty()) {
        findPaths.push_back(QLatin1String("/lib"));
        findPaths.push_back(QLatin1String("/usr/lib"));
        findPaths.push_back(QLatin1String("/usr/local/lib"));
        findPaths.push_back(QLatin1String("/opt/local/lib"));
    }
#if defined(Q_WS_MAC)
    const QString dynamic = findPath(QString::fromLatin1("lib%1.dylib").arg(name), findPaths);
#else
    const QString dynamic = findPath(QString::fromLatin1("lib%1.so*").arg(name), findPaths);
#endif
    if (!dynamic.isEmpty())
        return dynamic;
    return findPath(QString::fromLatin1("lib%1.a").arg(name), findPaths);
#endif
}

/*!
    Tries to find a file name \a name in one of \a paths.
    The resulting path is stored in \a path.
    This method can be used by scripts to check external dependencies.
*/
QString PackageManagerCore::findPath(const QString &name, const QStringList &paths)
{
    foreach (const QString &path, paths) {
        const QDir dir(path);
        const QStringList entries = dir.entryList(QStringList() << name, QDir::Files | QDir::Hidden);
        if (entries.isEmpty())
            continue;

        return dir.absoluteFilePath(entries.first());
    }
    return QString();
}

/*!
    Sets the "installerbase" binary to use when writing the package manager/uninstaller.
    Set this if an update to installerbase is available.
    If not set, the executable segment of the running un/installer will be used.
*/
void PackageManagerCore::setInstallerBaseBinary(const QString &path)
{
    d->m_installerBaseBinaryUnreplaced = path;
}

/*!
    Returns the installer value for \a key. If \a key is not known to the system, \a defaultValue is
    returned. Additionally, on Windows, \a key can be a registry key.
*/
QString PackageManagerCore::value(const QString &key, const QString &defaultValue) const
{
#ifdef Q_WS_WIN
    if (!d->m_vars.contains(key)) {
        static const QRegExp regex(QLatin1String("\\\\|/"));
        const QString filename = key.section(regex, 0, -2);
        const QString regKey = key.section(regex, -1);
        const QSettingsWrapper registry(filename, QSettingsWrapper::NativeFormat);
        if (!filename.isEmpty() && !regKey.isEmpty() && registry.contains(regKey))
            return registry.value(regKey).toString();
    }
#else
    if (key == scTargetDir) {
        const QString dir = d->m_vars.value(key, defaultValue);
        if (dir.startsWith(QLatin1String("~/")))
            return QDir::home().absoluteFilePath(dir.mid(2));
        return dir;
    }
#endif
    return d->m_vars.value(key, defaultValue);
}

/*!
    Sets the installer value for \a key to \a value.
*/
void PackageManagerCore::setValue(const QString &key, const QString &value)
{
    if (d->m_vars.value(key) == value)
        return;

    d->m_vars.insert(key, value);
    emit valueChanged(key, value);
}

/*!
    Returns true, when the installer contains a value for \a key.
*/
bool PackageManagerCore::containsValue(const QString &key) const
{
    return d->m_vars.contains(key);
}

void PackageManagerCore::setSharedFlag(const QString &key, bool value)
{
    d->m_sharedFlags.insert(key, value);
}

bool PackageManagerCore::sharedFlag(const QString &key) const
{
    return d->m_sharedFlags.value(key, false);
}

bool PackageManagerCore::isVerbose() const
{
    return QInstaller::isVerbose();
}

void PackageManagerCore::setVerbose(bool on)
{
    QInstaller::setVerbose(on);
}

PackageManagerCore::Status PackageManagerCore::status() const
{
    return PackageManagerCore::Status(d->m_status);
}

QString PackageManagerCore::error() const
{
    return d->m_error;
}

/*!
    Returns true if at least one complete installation/update was successful, even if the user cancelled the
    newest installation process.
*/
bool PackageManagerCore::finishedWithSuccess() const
{
    return d->m_status == PackageManagerCore::Success || d->m_needToWriteUninstaller;
}

void PackageManagerCore::interrupt()
{
    setCanceled();
    emit installationInterrupted();
}

void PackageManagerCore::setCanceled()
{
    cancelMetaInfoJob();
    d->setStatus(PackageManagerCore::Canceled);
}

/*!
    Replaces all variables within \a str by their respective values and returns the result.
*/
QString PackageManagerCore::replaceVariables(const QString &str) const
{
    return d->replaceVariables(str);
}

/*!
    \overload
    Replaces all variables in any of \a str by their respective values and returns the results.
*/
QStringList PackageManagerCore::replaceVariables(const QStringList &str) const
{
    QStringList result;
    foreach (const QString &s, str)
        result.push_back(d->replaceVariables(s));

    return result;
}

/*!
    \overload
    Replaces all variables within \a ba by their respective values and returns the result.
*/
QByteArray PackageManagerCore::replaceVariables(const QByteArray &ba) const
{
    return d->replaceVariables(ba);
}

/*!
    Returns the path to the installer binary.
*/
QString PackageManagerCore::installerBinaryPath() const
{
    return d->installerBinaryPath();
}

/*!
    Returns true when this is the installer running.
*/
bool PackageManagerCore::isInstaller() const
{
    return d->isInstaller();
}

/*!
    Returns true if this is an offline-only installer.
*/
bool PackageManagerCore::isOfflineOnly() const
{
    return d->isOfflineOnly();
}

void PackageManagerCore::setUninstaller()
{
    d->m_magicBinaryMarker = QInstaller::MagicUninstallerMarker;
}

/*!
    Returns true when this is the uninstaller running.
*/
bool PackageManagerCore::isUninstaller() const
{
    return d->isUninstaller();
}

void PackageManagerCore::setUpdater()
{
    d->m_magicBinaryMarker = QInstaller::MagicUpdaterMarker;
}

/*!
    Returns true when this is neither an installer nor an uninstaller running.
    Must be an updater, then.
*/
bool PackageManagerCore::isUpdater() const
{
    return d->isUpdater();
}

void PackageManagerCore::setPackageManager()
{
    d->m_magicBinaryMarker = QInstaller::MagicPackageManagerMarker;
}

/*!
    Returns true when this is the package manager running.
*/
bool PackageManagerCore::isPackageManager() const
{
    return d->isPackageManager();
}

/*!
    Runs the installer. Returns true on success, false otherwise.
*/
bool PackageManagerCore::runInstaller()
{
    try {
        d->runInstaller();
        return true;
    } catch (...) {
        return false;
    }
}

/*!
    Runs the uninstaller. Returns true on success, false otherwise.
*/
bool PackageManagerCore::runUninstaller()
{
    try {
        d->runUninstaller();
        return true;
    } catch (...) {
        return false;
    }
}

/*!
    Runs the package updater. Returns true on success, false otherwise.
*/
bool PackageManagerCore::runPackageUpdater()
{
    try {
        d->runPackageUpdater();
        return true;
    } catch (...) {
        return false;
    }
}

/*!
    \internal
    Calls languangeChanged on all components.
*/
void PackageManagerCore::languageChanged()
{
    foreach (Component *component, availableComponents())
        component->languageChanged();
}

/*!
    Runs the installer, un-installer, updater or package manager, depending on the type of this binary.
*/
bool PackageManagerCore::run()
{
    try {
        if (isInstaller())
            d->runInstaller();
        else if (isUninstaller())
            d->runUninstaller();
        else if (isPackageManager() || isUpdater())
            d->runPackageUpdater();
        return true;
    } catch (const Error &err) {
        qDebug() << "Caught Installer Error:" << err.message();
        return false;
    }
}

/*!
    Returns the path name of the uninstaller binary.
*/
QString PackageManagerCore::uninstallerName() const
{
    return d->uninstallerName();
}

bool PackageManagerCore::updateComponentData(struct Data &data, Component *component)
{
    try {
        // check if we already added the component to the available components list
        const QString name = data.package->data(scName).toString();
        if (data.components->contains(name)) {
            qCritical("Could not register component! Component with identifier %s already registered.",
                qPrintable(name));
            return false;
        }

        component->setUninstalled();
        const QString localPath = component->localTempPath();
        if (isVerbose()) {
            static QString lastLocalPath;
            if (lastLocalPath != localPath)
                qDebug() << "Url is:" << localPath;
            lastLocalPath = localPath;
        }

        if (d->m_repoMetaInfoJob) {
            const Repository &repo = d->m_repoMetaInfoJob->repositoryForTemporaryDirectory(localPath);
            component->setRepositoryUrl(repo.url());
            component->setValue(QLatin1String("username"), repo.username());
            component->setValue(QLatin1String("password"), repo.password());
        }

        // add downloadable archive from xml
        const QStringList downloadableArchives = data.package->data(scDownloadableArchives).toString()
            .split(QRegExp(QLatin1String("\\b(,|, )\\b")), QString::SkipEmptyParts);
        foreach (const QString downloadableArchive, downloadableArchives)
            component->addDownloadableArchive(downloadableArchive);

        const QStringList componentsToReplace = data.package->data(scReplaces).toString()
            .split(QRegExp(QLatin1String("\\b(,|, )\\b")), QString::SkipEmptyParts);

        if (!componentsToReplace.isEmpty()) {
            // Store the component (this is a component that replaces others) and all components that
            // this one will replace.
            data.replacementToExchangeables.insert(component, componentsToReplace);
        }

        if (isInstaller()) {
            // Running as installer means no component is installed, we do not need to check if the
            // replacement needs to be marked as installed, just return.
            return true;
        }

        if (data.installedPackages->contains(name)) {
            // The replacement is already installed, we can mark it as installed and skip the search for
            // a possible component to replace that might be installed (to mark the replacement as installed).
            component->setInstalled();
            component->setValue(scInstalledVersion, data.installedPackages->value(name).version);
            return true;
        }

        // The replacement is not yet installed, check all components to replace for there install state.
        foreach (const QString &componentName, componentsToReplace) {
            if (data.installedPackages->contains(componentName)) {
                // We found a replacement that is installed.
                if (isPackageManager()) {
                    // Mark the replacement component as installed as well. Only do this in package manager
                    // mode, otherwise it would not show up in the updaters component list.
                    component->setInstalled();
                    component->setValue(scInstalledVersion, data.installedPackages->value(componentName).version);
                    break;  // Break as soon as we know we found an installed component this one replaces.
                }
            }
        }
    } catch (...) {
        return false;
    }

    return true;
}

void PackageManagerCore::storeReplacedComponents(QHash<QString, Component *> &components, const struct Data &data)
{
    QHash<Component*, QStringList>::const_iterator it = data.replacementToExchangeables.constBegin();
    // remember all components that got a replacement, required for uninstall
    for (; it != data.replacementToExchangeables.constEnd(); ++it) {
        foreach (const QString &componentName, it.value()) {
            Component *component = components.take(componentName);
            // if one component has a replaces which is not existing in the current component list anymore
            // just ignore it
            if (!component) {
                // This case can happen when in installer mode, but should not occur when updating
                if (isUpdater())
                    qWarning() << componentName << "- Does not exist in the repositories anymore.";
                continue;
            }
            if (!component && !d->componentsToReplace(data.runMode).contains(componentName)) {
                component = new Component(this);
                component->setValue(scName, componentName);
            } else {
                component->loadComponentScript();
                d->replacementDependencyComponents(data.runMode).append(component);
            }
            d->componentsToReplace(data.runMode).insert(componentName, qMakePair(it.key(), component));
        }
    }
}

bool PackageManagerCore::fetchAllPackages(const PackagesList &remotes, const LocalPackagesHash &locals)
{
    emit startAllComponentsReset();

    d->clearAllComponentLists();
    QHash<QString, QInstaller::Component*> components;

    Data data;
    data.runMode = AllMode;
    data.components = &components;
    data.installedPackages = &locals;

    foreach (Package *const package, remotes) {
        if (d->statusCanceledOrFailed())
            return false;

        QScopedPointer<QInstaller::Component> component(new QInstaller::Component(this));
        data.package = package;
        component->loadDataFromPackage(*package);
        if (updateComponentData(data, component.data())) {
            const QString name = component->name();
            components.insert(name, component.take());
        }
    }

    foreach (const QString &key, locals.keys()) {
        QScopedPointer<QInstaller::Component> component(new QInstaller::Component(this));
        component->loadDataFromPackage(locals.value(key));
        const QString &name = component->name();
        if (!components.contains(name))
            components.insert(name, component.take());
    }

    // store all components that got a replacement
    storeReplacedComponents(components, data);

    if (!d->buildComponentTree(components, true))
        return false;

    emit finishAllComponentsReset();
    return true;
}

bool PackageManagerCore::fetchUpdaterPackages(const PackagesList &remotes, const LocalPackagesHash &locals)
{
    emit startUpdaterComponentsReset();

    d->clearUpdaterComponentLists();
    QHash<QString, QInstaller::Component *> components;

    Data data;
    data.runMode = UpdaterMode;
    data.components = &components;
    data.installedPackages = &locals;

    bool foundEssentialUpdate = false;
    LocalPackagesHash installedPackages = locals;
    QStringList replaceMes;

    foreach (Package *const update, remotes) {
        if (d->statusCanceledOrFailed())
            return false;

        QScopedPointer<QInstaller::Component> component(new QInstaller::Component(this));
        data.package = update;
        component->loadDataFromPackage(*update);
        if (updateComponentData(data, component.data())) {
            // Keep a reference so we can resolve dependencies during update.
            d->m_updaterComponentsDeps.append(component.take());

//            const QString isNew = update->data(scNewComponent).toString();
//            if (isNew.toLower() != scTrue)
//                continue;

            const QString &name = d->m_updaterComponentsDeps.last()->name();
            const QString replaces = data.package->data(scReplaces).toString();
            installedPackages.take(name);   // remove from local installed packages

            bool isValidUpdate = locals.contains(name);
            if (!isValidUpdate && !replaces.isEmpty()) {
                const QStringList possibleNames = replaces.split(QRegExp(QLatin1String("\\b(,|, )\\b")),
                    QString::SkipEmptyParts);
                foreach (const QString &possibleName, possibleNames) {
                    if (locals.contains(possibleName)) {
                        isValidUpdate = true;
                        replaceMes << possibleName;
                    }
                }
            }

            if (!isValidUpdate)
                continue;   // Update for not installed package found, skip it.

            const LocalPackage &localPackage = locals.value(name);
            const QString updateVersion = update->data(scRemoteVersion).toString();
            if (KDUpdater::compareVersion(updateVersion, localPackage.version) <= 0)
                continue;

            // It is quite possible that we may have already installed the update. Lets check the last
            // update date of the package and the release date of the update. This way we can compare and
            // figure out if the update has been installed or not.
            const QDate updateDate = update->data(scReleaseDate).toDate();
            if (localPackage.lastUpdateDate > updateDate)
                continue;

            if (update->data(scEssential, scFalse).toString().toLower() == scTrue)
                foundEssentialUpdate = true;

            // this is not a dependency, it is a real update
            components.insert(name, d->m_updaterComponentsDeps.takeLast());
        }
    }

    QHash<QString, QInstaller::Component *> localReplaceMes;
    foreach (const QString &key, installedPackages.keys()) {
        QInstaller::Component *component = new QInstaller::Component(this);
        component->loadDataFromPackage(installedPackages.value(key));
        d->m_updaterComponentsDeps.append(component);
        // Keep a list of local components that should be replaced
        if (replaceMes.contains(component->name()))
            localReplaceMes.insert(component->name(), component);
    }

    // store all components that got a replacement, but do not modify the components list
    storeReplacedComponents(localReplaceMes.unite(components), data);

    try {
        if (!components.isEmpty()) {
            // load the scripts and append all components w/o parent to the direct list
            foreach (QInstaller::Component *component, components) {
                if (d->statusCanceledOrFailed())
                    return false;

                component->loadComponentScript();
                component->setCheckState(Qt::Checked);
                appendUpdaterComponent(component);
            }

            // after everything is set up, check installed components
            foreach (QInstaller::Component *component, d->m_updaterComponentsDeps) {
                if (d->statusCanceledOrFailed())
                    return false;
                // even for possible dependency we need to load the script for example to get archives
                component->loadComponentScript();
                if (component->isInstalled()) {
                    // since we do not put them into the model, which would force a update of e.g. tri state
                    // components, we have to check all installed components ourselves
                    component->setCheckState(Qt::Checked);
                }
            }

            if (foundEssentialUpdate) {
                foreach (QInstaller::Component *component, components) {
                    if (d->statusCanceledOrFailed())
                        return false;

                    component->setCheckable(false);
                    component->setSelectable(false);
                    if (component->value(scEssential, scFalse).toLower() == scFalse) {
                        // non essential updates are disabled, not checkable and unchecked
                        component->setEnabled(false);
                        component->setCheckState(Qt::Unchecked);
                    } else {
                        // essential updates are enabled, still not checkable but checked
                        component->setEnabled(true);
                    }
                }
            }
        } else {
            // we have no updates, no need to store possible dependencies
            d->clearUpdaterComponentLists();
        }
    } catch (const Error &error) {
        d->clearUpdaterComponentLists();
        emit finishUpdaterComponentsReset();
        d->setStatus(Failure, error.message());

        // TODO: make sure we remove all message boxes inside the library at some point.
        MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(), QLatin1String("Error"),
            tr("Error"), error.message());
        return false;
    }

    emit finishUpdaterComponentsReset();
    return true;
}

void PackageManagerCore::resetComponentsToUserCheckedState()
{
    d->resetComponentsToUserCheckedState();
}

void PackageManagerCore::updateDisplayVersions(const QString &displayKey)
{
    QHash<QString, QInstaller::Component *> components;
    const QList<QInstaller::Component *> componentList = availableComponents();
    foreach (QInstaller::Component *component, componentList)
        components[component->name()] = component;

    // set display version for all components in list
    const QStringList &keys = components.keys();
    foreach (const QString &key, keys) {
        QHash<QString, bool> visited;
        if (components.value(key)->isInstalled()) {
            components.value(key)->setValue(scDisplayVersion,
                findDisplayVersion(key, components, scInstalledVersion, visited));
        }
        visited.clear();
        const QString displayVersionRemote = findDisplayVersion(key, components, scRemoteVersion, visited);
        if (displayVersionRemote.isEmpty())
            components.value(key)->setValue(displayKey, tr("invalid"));
        else
            components.value(key)->setValue(displayKey, displayVersionRemote);
    }

}

QString PackageManagerCore::findDisplayVersion(const QString &componentName,
    const QHash<QString, Component *> &components, const QString &versionKey, QHash<QString, bool> &visited)
{
    if (!components.contains(componentName))
        return QString();
    const QString replaceWith = components.value(componentName)->value(scInheritVersion);
    visited[componentName] = true;

    if (replaceWith.isEmpty())
        return components.value(componentName)->value(versionKey);

    if (visited.contains(replaceWith))  // cycle
        return QString();

    return findDisplayVersion(replaceWith, components, versionKey, visited);
}
