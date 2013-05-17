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
#include "packagemanagercore.h"
#include "packagemanagercore_p.h"

#include "adminauthorization.h"
#include "binaryformat.h"
#include "component.h"
#include "componentmodel.h"
#include "downloadarchivesjob.h"
#include "errors.h"
#include "fsengineclient.h"
#include "globals.h"
#include "getrepositoriesmetainfojob.h"
#include "messageboxhandler.h"
#include "packagemanagerproxyfactory.h"
#include "progresscoordinator.h"
#include "qprocesswrapper.h"
#include "qsettingswrapper.h"
#include "settings.h"
#include "utils.h"

#include <productkeycheck.h>

#include <QFuture>
#include <QFutureWatcher>
#include <QtConcurrentRun>

#include <QtCore/QMutex>
#include <QtCore/QSettings>
#include <QtCore/QTemporaryFile>

#include <QDesktopServices>
#include <QFileDialog>

#include "kdsysinfo.h"
#include "kdupdaterupdateoperationfactory.h"

#ifdef Q_OS_WIN
#   include "qt_windows.h"
#endif

#if QT_VERSION >= 0x050000
#   include <QStandardPaths>
#endif

/*!
    \qmltype QInstaller
    \inqmlmodule scripting

    \brief Provides access to the installer from Qt Script.

    Use the \c installer object in the global namespace to access functionality of the installer.

    \section2 Wizard Pages

    The installer has various pre-defined pages that can be used to for example insert pages
    in a certain place:
    \list
    \li QInstaller.Introduction
    \li QInstaller.TargetDirectory
    \li QInstaller.ComponentSelection
    \li QInstaller.LicenseCheck
    \li QInstaller.StartMenuSelection
    \li QInstaller.ReadyForInstallation
    \li QInstaller.PerformInstallation
    \li QInstaller.InstallationFinished
    \li QInstaller.End
    \endlist
*/


/*!
   \qmlproperty enumeration QInstaller::status

   Status of the installer.

   Possible values are:
   \list
   \li QInstaller.Success (deprecated: QInstaller.InstallerSucceeded)
   \li QInstaller.Failure (deprecated: QInstaller.InstallerFailed)
   \li QInstaller.Running (deprecated: QInstaller.InstallerFailed)
   \li QInstaller.Canceled (deprecated: QInstaller.CanceledByUser)
   \li deprecated: QInstaller.InstallerUnfinished
   \endlist
*/

/*!
    \qmlsignal QInstaller::aboutCalculateComponentsToInstall()

    Emitted before the ordered list of components to install is calculated.
*/

/*!
    \qmlsignal QInstaller::componentAdded(Component component)

    Emitted when a new root component has been added.

    \sa rootComponentsAdded, updaterComponentsAdded
*/

/*!
    \qmlsignal QInstaller::rootComponentsAdded(list<Component> components)

    Emitted when a new list of root components has been added.

    \sa componentAdded, updaterComponentsAdded
*/

/*!
    \qmlsignal QInstaller::updaterComponentsAdded(list<Component> components)

    Emitted when a new list of updater components has been added.
    \sa componentAdded, rootComponentsAdded
*/

/*!
    \qmlsignal QInstaller::componentsAboutToBeCleared()

    Deprecated, and not emitted any more.
*/

/*!
    \qmlsignal QInstaller::valueChanged(string key, string value)

    Emitted whenever a value changes.

    \sa setValue
*/

/*!
    \qmlsignal QInstaller::statusChanged(Status status)

    Emitted whenever the installer status changes.
*/

/*!
    \qmlsignal QInstaller::currentPageChanged(int page)

    Emitted whenever the current page changes.
*/

/*!
    \qmlsignal QInstaller::finishButtonClicked()

    Emitted when the user clicks the \uicontrol Finish button of the installer.
*/

/*!
    \qmlsignal QInstaller::metaJobInfoMessage(string message)

    Triggered with informative updates of the communication with a remote repository.
    This is only useful for debugging purposes.
*/

/*!
    \qmlsignal QInstaller::setRootComponents(list<Component> components)

    Triggered with the list of new root components (for example after an online update).
*/

/*!
    \qmlsignal QInstaller::startAllComponentsReset()

    Triggered when the list of components starts to get updated.

    \sa finishAllComponentsReset
*/

/*!
    \qmlsignal QInstaller::finishAllComponentsReset()

    Triggered when the list of components has been updated.

    \sa startAllComponentsReset
*/

/*!
    \qmlsignal QInstaller::startUpdaterComponentsReset()

    Triggered when components start to get updated during a remote update.
*/

/*!
    \qmlsignal QInstaller::finishUpdaterComponentsReset()

    Triggered when components have been updated during a remote update.
*/

/*!
    \qmlsignal QInstaller::installationStarted()

    Triggered when installation has started.

    \sa installationFinished installationInterrupted
*/

/*!
    \qmlsignal QInstaller::installationInterrupted()

    Triggered when installation has been interrupted (cancelled).

    \sa interrupt installationStarted installationFinished
*/

/*!
    \qmlsignal QInstaller::installationFinished()

    Triggered when installation has been finished.

    \sa installationStarted installationInterrupted
*/

/*!
    \qmlsignal QInstaller::updateFinished()

    Triggered when an update has been finished.
*/

/*!
    \qmlsignal QInstaller::uninstallationStarted()

    Triggered when uninstallation has started.

    \sa uninstallationFinished
*/

/*!
    \qmlsignal QInstaller::uninstallationFinished()

    Triggered when uninstallation has been finished.

    \sa uninstallationStarted
*/

/*!
    \qmlsignal QInstaller::titleMessageChanged(string title)

    Emitted when the text of the installer status (on the PerformInstallation page) changes to
    \a title.
*/

/*!
    \qmlsignal QInstaller::wizardPageInsertionRequested(Widget widget, WizardPage page)

    Emitted when a custom \a widget is about to be inserted into \a page by addWizardPage.
*/

/*!
    \qmlsignal QInstaller::wizardPageRemovalRequested(Widget widget)

    Emitted when a \a widget is removed by removeWizardPage.
*/

/*!
    \qmlsignal QInstaller::wizardWidgetInsertionRequested(Widget widget, WizardPage page)

    Emitted when a \a widget is inserted into \a page by addWizardPageItem.
*/

/*!
    \qmlsignal QInstaller::wizardWidgetRemovalRequested(Widget widget)

    Emitted when a \a widget is removed by removeWizardPageItem.
*/

/*!
    \qmlsignal QInstaller::wizardPageVisibilityChangeRequested(bool visible, int page)

    Emitted when the visibility of the page with id \a page changes to \a visible.

    \sa setDefaultPageVisible
*/

/*!
    \qmlsignal QInstaller::setValidatorForCustomPageRequested(Componentcomponent, string name,
                                        string callbackName)

    Triggered when setValidatorForCustomPage is called.
*/

/*!
    \qmlsignal QInstaller::setAutomatedPageSwitchEnabled(bool request)

    Triggered when the automatic switching from PerformInstallation to InstallationFinished page
    is enabled (\a request = \c true) or disabled (\a request = \c false).

    The automatic switching is disabled automatically when for example the user expands or unexpands
    the \gui Details section of the PerformInstallation page.
*/

/*!
    \qmlsignal QInstaller::coreNetworkSettingsChanged()

    Emitted when the network settings are changed.
*/

/*!
    \qmlmethod list<Component> QInstaller::components()

    Returns the list of all components.
*/

using namespace QInstaller;

Q_GLOBAL_STATIC(QMutex, globalModelMutex);
static QFont *sVirtualComponentsFont = 0;
Q_GLOBAL_STATIC(QMutex, globalVirtualComponentsFontMutex);

static bool sNoForceInstallation = false;
static bool sVirtualComponentsVisible = false;
static bool sCreateLocalRepositoryFromBinary = false;

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

    if (!installer->isUpdater()) {
        QList<Component*> rootComponents;
        if (check == 0)
            rootComponents = installer->rootComponents();
        else
            rootComponents = check->childComponents(Component::DirectChildrenOnly);

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

    d->initialize(params);
}

/*!
    \qmlmethod void QInstaller::setCompleteUninstallation(bool complete)

    Sets the uninstallation to be \a complete. If \a complete is false, only components deselected
    by the user will be uninstalled. This option applies only on uninstallation.
 */
void PackageManagerCore::setCompleteUninstallation(bool complete)
{
    d->m_completeUninstall = complete;
}

/*!
    \qmlmethod void QInstaller::cancelMetaInfoJob()

    Cancels the retrieval of meta information from a remote repository.
 */
void PackageManagerCore::cancelMetaInfoJob()
{
    if (d->m_repoMetaInfoJob)
        d->m_repoMetaInfoJob->cancel();
}

/*!
    \qmlmethod void QInstaller::componentsToInstallNeedsRecalculation()

    Ensures that component dependencies are re-calculated.
 */
void PackageManagerCore::componentsToInstallNeedsRecalculation()
{
    d->m_componentsToInstallCalculated = false;
}

/*!
   \qmlmethod void QInstaller::autoAcceptMessageBoxes()

   Automatically accept all user message boxes.

   \sa autoRejectMessageBoxes, setMessageBoxAutomaticAnswer
 */
void PackageManagerCore::autoAcceptMessageBoxes()
{
    MessageBoxHandler::instance()->setDefaultAction(MessageBoxHandler::Accept);
}

/*!
   \qmlmethod void QInstaller::autoRejectMessageBoxes()

   Automatically reject all user message boxes.

   \sa autoAcceptMessageBoxes, setMessageBoxAutomaticAnswer
 */
void PackageManagerCore::autoRejectMessageBoxes()
{
    MessageBoxHandler::instance()->setDefaultAction(MessageBoxHandler::Reject);
}

/*!
   \qmlmethod void QInstaller::setMessageBoxAutomaticAnswer(string identifier, int button)

   Automatically close the message box with ID \a identifier as if the user had pressed \a button.

   This can be used for unattended (automatic) installations.

   \sa QMessageBox, autoAcceptMessageBoxes, autoRejectMessageBoxes
 */
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

/*!
   \qmlmethod float QInstaller::requiredDiskSpace()

   Returns the estimated amount of disk space in bytes required after installation.

   \sa requiredTemporaryDiskSpace
 */
quint64 PackageManagerCore::requiredDiskSpace() const
{
    quint64 result = 0;

    foreach (QInstaller::Component *component, rootComponents())
        result += component->updateUncompressedSize();

    return result;
}

/*!
   \qmlmethod float QInstaller::requiredTemporaryDiskSpace()

   Returns the estimated required disk space during installation in bytes.

   \sa requiredDiskSpace
 */
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
    DownloadArchivesJob *const archivesJob = new DownloadArchivesJob(this);
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
                    component = d->componentsToReplace().value(componentName).second;
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

/*!
    \qmlmethod boolean Installer::isFileExtensionRegistered(string extension)

    Returns whether a file extension is already registered in the Windows registry. Returns \c false
    on all other platforms.
 */
bool PackageManagerCore::isFileExtensionRegistered(const QString &extension) const
{
    QSettingsWrapper settings(QLatin1String("HKEY_CLASSES_ROOT"), QSettingsWrapper::NativeFormat);
    return settings.value(QString::fromLatin1(".%1/Default").arg(extension)).isValid();
}

/*!
    \qmlmethod boolean QInstaller::fileExists(string filePath)

    Returns \c true if the \a filePath exists; otherwise returns \c false.

    \note If the file is a symlink that points to a non existing
     file, \c false is returned.
 */
bool PackageManagerCore::fileExists(const QString &filePath) const
{
    return QFileInfo(filePath).exists();
}

// -- QInstaller

/*!
    Used by operation runner to get a fake installer, can be removed if installerbase can do what operation
    runner does.
*/
PackageManagerCore::PackageManagerCore()
    : d(new PackageManagerCorePrivate(this))
{
    qRegisterMetaType<QInstaller::PackageManagerCore::Status>("QInstaller::PackageManagerCore::Status");
    qRegisterMetaType<QInstaller::PackageManagerCore::WizardPage>("QInstaller::PackageManagerCore::WizardPage");
}

PackageManagerCore::PackageManagerCore(qint64 magicmaker, const OperationList &performedOperations)
    : d(new PackageManagerCorePrivate(this, magicmaker, performedOperations))
{
    qRegisterMetaType<QInstaller::PackageManagerCore::Status>("QInstaller::PackageManagerCore::Status");
    qRegisterMetaType<QInstaller::PackageManagerCore::WizardPage>("QInstaller::PackageManagerCore::WizardPage");

    d->initialize(QHash<QString, QString>());
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

    QMutexLocker _(globalVirtualComponentsFontMutex());
    delete sVirtualComponentsFont;
    sVirtualComponentsFont = 0;
}

/* static */
QFont PackageManagerCore::virtualComponentsFont()
{
    QMutexLocker _(globalVirtualComponentsFontMutex());
    if (!sVirtualComponentsFont)
        sVirtualComponentsFont = new QFont;
    return *sVirtualComponentsFont;
}

/* static */
void PackageManagerCore::setVirtualComponentsFont(const QFont &font)
{
    QMutexLocker _(globalVirtualComponentsFontMutex());
    if (sVirtualComponentsFont)
        delete sVirtualComponentsFont;
    sVirtualComponentsFont = new QFont(font);
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

/* static */
bool PackageManagerCore::createLocalRepositoryFromBinary()
{
    return sCreateLocalRepositoryFromBinary;
}

/* static */
void PackageManagerCore::setCreateLocalRepositoryFromBinary(bool create)
{
    sCreateLocalRepositoryFromBinary = create;
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
    emit setRootComponents(d->m_rootComponents);

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

    if (!ProductKeyCheck::instance(this)->hasValidKey()) {
        d->setStatus(Failure, ProductKeyCheck::instance(this)->lastErrorString());
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
    if (!isUpdater())
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
    \qmlmethod boolean QInstaller::addWizardPage(Component component, string name, int page)

    Adds the widget with objectName() \a name registered by \a component as a new page
    into the installer's GUI wizard. The widget is added before \a page.

    See \l{Wizard Pages} for the possible values of \a page.

    Returns \c true if the operation succeeded.

    \sa removeWizardPage, setDefaultPageVisible
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
    \qmlmethod boolean QInstaller::removeWizardPage(Component component, string name)

    Removes the widget with objectName() \a name previously added to the installer's wizard
    by \a component.

    Returns \c true if the operation succeeded.

    \sa addWizardPage, setDefaultPageVisible, wizardPageRemovalRequested
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
    \qmlmethod boolean QInstaller::setDefaultPageVisible(int page, boolean visible)

    Sets the visibility of the default page with id \a page to \a visible, i.e.
    removes or adds it from/to the wizard. This works only for pages which have been
    in the installer when it was started.

    Returns \c true.

    \sa addWizardPage, removeWizardPage
 */
bool PackageManagerCore::setDefaultPageVisible(int page, bool visible)
{
    emit wizardPageVisibilityChangeRequested(visible, page);
    return true;
}

/*!
    \qmlmethod void QInstaller::setValidatorForCustomPage(Component component, string name,
                                                         string callbackName)

    \sa setValidatorForCustomPageRequested
 */
void PackageManagerCore::setValidatorForCustomPage(Component *component, const QString &name,
                                                   const QString &callbackName)
{
    emit setValidatorForCustomPageRequested(component, name, callbackName);
}

/*!
    \qmlmethod boolean QInstaller::addWizardPageItem(Component component, string name, int page)

    Adds the widget with objectName() \a name registered by \a component as an GUI element
    into the installer's GUI wizard. The widget is added on \a page.

    See \l{Wizard Pages} for the possible values of \a page.

    \sa removeWizardPageItem, wizardWidgetInsertionRequested
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
    \qmlmethod boolean QInstaller::removeWizardPageItem(Component component, string name)

    Removes the widget with objectName() \a name previously added to the installer's wizard
    by \a component.

    \sa addWizardPageItem
*/
bool PackageManagerCore::removeWizardPageItem(Component *component, const QString &name)
{
    if (QWidget* const widget = component->userInterface(name)) {
        emit wizardWidgetRemovalRequested(widget);
        return true;
    }
    return false;
}

/*!
    \qmlmethod void QInstaller::addUserRepositories(stringlist repositories)

    Registers additional \a repositories.

    \sa setTemporaryRepositories
 */
void PackageManagerCore::addUserRepositories(const QStringList &repositories)
{
    QSet<Repository> repositorySet;
    foreach (const QString &repository, repositories)
        repositorySet.insert(Repository::fromUserInput(repository));

    settings().addUserRepositories(repositorySet);
}

/*!
    \qmlmethod void QInstaller::setTemporaryRepositories(stringlist repositories, boolean replace)

    Sets additional \a repositories for this instance of the installer or updater.
    Will be removed after invoking it again.

    \sa addUserRepositories
*/
void PackageManagerCore::setTemporaryRepositories(const QStringList &repositories, bool replace)
{
    QSet<Repository> repositorySet;
    foreach (const QString &repository, repositories)
        repositorySet.insert(Repository::fromUserInput(repository));

    settings().setTemporaryRepositories(repositorySet, replace);
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

ScriptEngine *PackageManagerCore::scriptEngine()
{
    return d->scriptEngine();
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
    \qmlmethod int QInstaller::updaterComponentCount()

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
        result += component->childComponents(Component::Descendants);
    return result + d->m_rootDependencyReplacements;
}

/*!
    \qmlmethod Component QInstaller::componentByName(string name)

    Returns a component matching \a name. \a name can also contains a version requirement.
    E.g. "org.qt-project.sdk.qt" returns any component with that name, "org.qt-project.sdk.qt->=4.5" requires
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
    emit aboutCalculateComponentsToInstall();
    if (!d->m_componentsToInstallCalculated) {
        d->clearComponentsToInstall();
        QList<Component*> components;
        if (isUpdater()) {
            foreach (Component *component, updaterComponents()) {
                if (component->updateRequested())
                    components.append(component);
            }
        } else if (!isUpdater()) {
            // relevant means all components which are not replaced
            QList<Component*> relevantComponents = rootComponents();
            foreach (QInstaller::Component *component, rootComponents())
                relevantComponents += component->childComponents(Component::Descendants);
            foreach (Component *component, relevantComponents) {
                // ask for all components which will be installed to get all dependencies
                // even dependencies which are changed without an increased version
                if (component->installationRequested() || (component->isInstalled()
                    && !component->uninstallationRequested())) {
                        components.append(component);
                }
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
    if (isUpdater())
        return true;

    // hack to avoid removing needed dependencies
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
    Returns a list of components that depend on \a component. The list can be empty. Note: Auto
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

ComponentModel *PackageManagerCore::defaultComponentModel() const
{
    QMutexLocker _(globalModelMutex());
    if (!d->m_defaultModel) {
        d->m_defaultModel = componentModel(const_cast<PackageManagerCore*> (this),
            QLatin1String("AllComponentsModel"));
    }
    return d->m_defaultModel;
}

ComponentModel *PackageManagerCore::updaterComponentModel() const
{
    QMutexLocker _(globalModelMutex());
    if (!d->m_updaterModel) {
        d->m_updaterModel = componentModel(const_cast<PackageManagerCore*> (this),
            QLatin1String("UpdaterComponentsModel"));
    }
    return d->m_updaterModel;
}

Settings &PackageManagerCore::settings() const
{
    return d->m_data.settings();
}

/*!
    \qmlmethod boolean QInstaller::gainAdminRights()

    Tries to gain admin rights. On success, it returns \c true.

    \sa dropAdminRights
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
    \qmlmethod void QInstaller::dropAdminRights()

    Drops admin rights gained by gainAdminRights.

    \sa gainAdminRights
*/
void PackageManagerCore::dropAdminRights()
{
    d->m_FSEngineClientHandler->setActive(false);
}

/*!
    \qmlmethod boolean QInstaller::isProcessRunning(string name)

    Returns true, if a process with \a name is running. On Windows, the comparison
    is case-insensitive.
*/
bool PackageManagerCore::isProcessRunning(const QString &name) const
{
    return PackageManagerCorePrivate::isProcessRunning(name, runningProcesses());
}

/*!
    \qmlmethod boolean QInstaller::killProcess(string absoluteFilePath)

    Returns true, if a process with \a absoluteFilePath could be killed or isn't running

    \note This is implemented in a semi blocking way (to keep the main thread to paint the UI).
*/
bool PackageManagerCore::killProcess(const QString &absoluteFilePath) const
{
    QString normalizedPath = replaceVariables(absoluteFilePath);
    normalizedPath = QDir::cleanPath(normalizedPath.replace(QLatin1Char('\\'), QLatin1Char('/')));

    QList<ProcessInfo> allProcesses = runningProcesses();
    foreach (const ProcessInfo &process, allProcesses) {
        QString processPath = process.name;
        processPath =  QDir::cleanPath(processPath.replace(QLatin1Char('\\'), QLatin1Char('/')));

        if (processPath == normalizedPath) {
            qDebug() << QString::fromLatin1("try to kill process: %1(%2)").arg(process.name).arg(process.id);

            //to keep the ui responsible use QtConcurrent::run
            QFutureWatcher<bool> futureWatcher;
            const QFuture<bool> future = QtConcurrent::run(KDUpdater::killProcess, process, 30000);

            QEventLoop loop;
            loop.connect(&futureWatcher, SIGNAL(finished()), SLOT(quit()), Qt::QueuedConnection);
            futureWatcher.setFuture(future);

            if (!future.isFinished())
                loop.exec();

            qDebug() << QString::fromLatin1("\"%1\" killed!").arg(process.name);
            return future.result();
        }
    }
    return true;
}


/*!
    \qmlmethod void QInstaller::setDependsOnLocalInstallerBinary()

    Makes sure the installer runs from a local drive. Otherwise the user will get an
    appropriate error message.

    \note This only works on Windows.

    \sa localInstallerBinaryUsed
*/

void PackageManagerCore::setDependsOnLocalInstallerBinary()
{
    d->m_dependsOnLocalInstallerBinary = true;
}

/*!
    \qmlmethod boolean QInstaller::localInstallerBinaryUsed()

    Returns \c false if the installer is run on Windows, and the installer has been started from
    a remote file system drive. Otherwise returns \c true.

    \sa setDependsOnLocalInstallerBinary
*/
bool PackageManagerCore::localInstallerBinaryUsed()
{
#ifdef Q_OS_WIN
    return KDUpdater::pathIsOnLocalDevice(qApp->applicationFilePath());
#endif
    return true;
}

/*!
    \qmlmethod array QInstaller::execute(string program, stringlist arguments = undefined,
                                        string stdin = "")

    Starts the program \a program with the arguments \a arguments in a
    new process and waits for it to finish.

    \a stdin is sent as standard input to the application.

    Returns an empty array if the program could not be executed, otherwise
    the output of command as the first item, and the return code as the second.

    \note On Unix, the output is just the output to stdout, not to stderr.

    \sa executeDetached
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
    \qmlmethod boolean QInstaller::executeDetached(string program, stringlist arguments = undefined,
                                                  string workingDirectory = "")

    Starts the program \a program with the arguments \a arguments in a
    new process, and detaches from it. Returns \c true on success;
    otherwise returns \c false. If the installer exits, the
    detached process will continue to live.

    \note Arguments that contain spaces are not passed to the
    process as separate arguments.

    \b{Unix:} The started process will run in its own session and act
    like a daemon.

    \b{Windows:} Arguments that contain spaces are wrapped in quotes.
    The started process will run as a regular standalone process.

    The process will be started in the directory \a workingDirectory.
*/

bool PackageManagerCore::executeDetached(const QString &program, const QStringList &arguments,
    const QString &workingDirectory) const
{
    QString adjustedProgram = replaceVariables(program);
    QStringList adjustedArguments;
    QString adjustedWorkingDir = replaceVariables(workingDirectory);
    foreach (const QString &argument, arguments)
        adjustedArguments.append(replaceVariables(argument));
    qDebug() << "run application as detached process:" << adjustedProgram << adjustedArguments << adjustedWorkingDir;
    if (workingDirectory.isEmpty())
        return QProcess::startDetached(adjustedProgram, adjustedArguments);
    else
        return QProcess::startDetached(adjustedProgram, adjustedArguments, adjustedWorkingDir);
}


/*!
    \qmlmethod string QInstaller::environmentVariable(string name)

    Returns content of an environment variable \a name. An empty string is returned if the
    environment variable is not set.
*/
QString PackageManagerCore::environmentVariable(const QString &name) const
{
    if (name.isEmpty())
        return QString();

#ifdef Q_OS_WIN
    static TCHAR buffer[32767];
    DWORD size = GetEnvironmentVariable(LPCWSTR(name.utf16()), buffer, 32767);
    QString value = QString::fromUtf16((const unsigned short *) buffer, size);

    if (value.isEmpty()) {
        static QLatin1String userEnvironmentRegistryPath("HKEY_CURRENT_USER\\Environment");
        value = QSettings(userEnvironmentRegistryPath, QSettings::NativeFormat).value(name).toString();
        if (value.isEmpty()) {
            static QLatin1String systemEnvironmentRegistryPath("HKEY_LOCAL_MACHINE\\SYSTEM\\"
                "CurrentControlSet\\Control\\Session Manager\\Environment");
            value = QSettings(systemEnvironmentRegistryPath, QSettings::NativeFormat).value(name).toString();
        }
    }
    return value;
#else
    return QString::fromUtf8(qgetenv(name.toLatin1()));
#endif
}

/*!
    \qmlmethod boolean QInstaller::performOperation(string name, stringlist arguments)

    Instantly performs an operation \a name with \a arguments.
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
    \qmlmethod boolean QInstaller::versionMatches(string version, string requirement)

    Returns \c true when \a version matches the \a requirement.
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
    \qmlmethod string QInstaller::findLibrary(string name, stringlist paths = [])

    Finds a library named \a name in \a paths.
    If \a paths is empty, it gets filled with platform dependent default paths.
    The resulting path is returned.

    This method can be used by scripts to check external dependencies.

    \sa findPath
*/
QString PackageManagerCore::findLibrary(const QString &name, const QStringList &paths)
{
    QStringList findPaths = paths;
#if defined(Q_OS_WIN)
    return findPath(QString::fromLatin1("%1.lib").arg(name), findPaths);
#else
    if (findPaths.isEmpty()) {
        findPaths.push_back(QLatin1String("/lib"));
        findPaths.push_back(QLatin1String("/usr/lib"));
        findPaths.push_back(QLatin1String("/usr/local/lib"));
        findPaths.push_back(QLatin1String("/opt/local/lib"));
    }
#if defined(Q_OS_MAC)
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
    \qmlmethod string QInstaller::findPath(string name, stringlist paths = [])

    Tries to find a file name \a name in one of \a paths.
    The resulting path is returned.

    This method can be used by scripts to check external dependencies.

    \sa findLibrary
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
    \qmlmethod void QInstaller::setInstallerBaseBinary(string path)

    Sets the "installerbase" binary to use when writing the package manager/uninstaller.
    Set this if an update to installerbase is available.

    If not set, the executable segment of the running un/installer will be used.
*/
void PackageManagerCore::setInstallerBaseBinary(const QString &path)
{
    d->m_installerBaseBinaryUnreplaced = path;
}

/*!
    \qmlmethod string QInstaller::value(string key, string defaultValue = "")

    Returns the installer value for \a key. If \a key is not known to the system, \a defaultValue is
    returned. Additionally, on Windows, \a key can be a registry key.

    \sa setValue, containsValue, valueChanged
*/
QString PackageManagerCore::value(const QString &key, const QString &defaultValue) const
{
    return d->m_data.value(key, defaultValue).toString();
}

/*!
    \qmlmethod void QInstaller::setValue(string key, string value)

    Sets the installer value for \a key to \a value.

    \sa value, containsValue, valueChanged
*/
void PackageManagerCore::setValue(const QString &key, const QString &value)
{
    const QString normalizedValue = replaceVariables(value);
    if (d->m_data.setValue(key, normalizedValue))
        emit valueChanged(key, normalizedValue);
}

/*!
    \qmlmethod boolean QInstaller::containsValue(string key)

    Returns \c true if the installer contains a value for \a key.

    \sa value, setValue, valueChanged
*/
bool PackageManagerCore::containsValue(const QString &key) const
{
    return d->m_data.contains(key);
}

/*!
    \qmlmethod void QInstaller::setSharedFlag(string key, boolean value)

    Sets a shared flag with name \a key to \a value. This is one option
    to share information between scripts.

    \sa sharedFlag
*/
void PackageManagerCore::setSharedFlag(const QString &key, bool value)
{
    d->m_sharedFlags.insert(key, value);
}

/*!
    \qmlmethod boolean QInstaller::sharedFlag(string key)

    Returns shared flag with name \a key. This is one option
    to share information between scripts.

    \sa setSharedFlag
*/
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
    Returns \c true if at least one complete installation/update was successful, even if the user cancelled the
    newest installation process.
*/
bool PackageManagerCore::finishedWithSuccess() const
{
    return d->m_status == PackageManagerCore::Success || d->m_needToWriteUninstaller;
}

/*!
    \qmlmethod void QInstaller::interrupt()

   Cancels an ongoing installation.

    \sa installationInterrupted
 */
void PackageManagerCore::interrupt()
{
    setCanceled();
    emit installationInterrupted();
}

/*!
    \qmlmethod void QInstaller::setCanceled()

    Cancels the installation.
 */
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
        result.append(d->replaceVariables(s));

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
    \qmlmethod boolean QInstaller::isInstaller()

    Returns \c true if executed in an install step.

    \sa isUninstaller, isUpdater, isPackageManager
*/
bool PackageManagerCore::isInstaller() const
{
    return d->isInstaller();
}

/*!
    \qmlmethod boolean QInstaller::isOfflineOnly()

    Returns \c true if this is an offline-only installer.
*/
bool PackageManagerCore::isOfflineOnly() const
{
    return d->isOfflineOnly();
}

/*!
    \qmlmethod void QInstaller::setUninstaller()

    Forces an uninstaller context.

    \sa isUninstaller, setUpdater, setPackageManager
*/
void PackageManagerCore::setUninstaller()
{
    d->m_magicBinaryMarker = QInstaller::MagicUninstallerMarker;
}

/*!
    \qmlmethod boolean QInstaller::isUninstaller()

    Returns \c true if the script is executed in an uninstall context.

    \sa setUninstaller, isInstaller, isUpdater, isPackageManager
*/
bool PackageManagerCore::isUninstaller() const
{
    return d->isUninstaller();
}

/*!
    \qmlmethod void QInstaller::setUpdater()

    Forces an updater context.

    \sa isUpdater, setUninstaller, setPackageManager
*/
void PackageManagerCore::setUpdater()
{
    d->m_magicBinaryMarker = QInstaller::MagicUpdaterMarker;
}

/*!
    \qmlmethod boolean QInstaller::isUpdater()

    Returns \c true if the script is executed in an updater context.

    \sa setUpdater, isInstaller, isUninstaller, isPackageManager
*/
bool PackageManagerCore::isUpdater() const
{
    return d->isUpdater();
}

/*!
    \qmlmethod void QInstaller::setPackageManager()

    Forces a package manager context.
*/
void PackageManagerCore::setPackageManager()
{
    d->m_magicBinaryMarker = QInstaller::MagicPackageManagerMarker;
}


/*!
    \qmlmethod boolean QInstaller::isPackageManager()

    Returns \c true if the script is executed in a package manager context.
    \sa setPackageManager, isInstaller, isUninstaller, isUpdater
*/
bool PackageManagerCore::isPackageManager() const
{
    return d->isPackageManager();
}

/*!
    \qmlmethod boolean QInstaller::runInstaller()

    Runs the installer. Returns \c true on success, \c false otherwise.
*/
bool PackageManagerCore::runInstaller()
{
    return d->runInstaller();
}

/*!
    \qmlmethod boolean QInstaller::runUninstaller()

    Runs the uninstaller. Returns \c true on success, \c false otherwise.
*/
bool PackageManagerCore::runUninstaller()
{
    return d->runUninstaller();
}

/*!
    \qmlmethod boolean QInstaller::runPackageUpdater()

    Runs the package updater. Returns \c true on success, \c false otherwise.
*/
bool PackageManagerCore::runPackageUpdater()
{
    return d->runPackageUpdater();
}

/*!
    \qmlmethod void QInstaller::languageChanged()

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
    if (isInstaller())
        return d->runInstaller();
    else if (isUninstaller())
        return d->runUninstaller();
    else if (isPackageManager() || isUpdater())
        return d->runPackageUpdater();
    return false;
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
            .split(QInstaller::commaRegExp(), QString::SkipEmptyParts);

        if (component->isFromOnlineRepository()) {
            foreach (const QString downloadableArchive, downloadableArchives)
                component->addDownloadableArchive(downloadableArchive);
        }

        const QStringList componentsToReplace = data.package->data(scReplaces).toString()
            .split(QInstaller::commaRegExp(), QString::SkipEmptyParts);

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
            if (!component && !d->componentsToReplace().contains(componentName)) {
                component = new Component(this);
                component->setValue(scName, componentName);
            } else {
                component->loadComponentScript();
                d->replacementDependencyComponents().append(component);
            }
            d->componentsToReplace().insert(componentName, qMakePair(it.key(), component));
        }
    }
}

bool PackageManagerCore::fetchAllPackages(const PackagesList &remotes, const LocalPackagesHash &locals)
{
    emit startAllComponentsReset();

    d->clearAllComponentLists();
    QHash<QString, QInstaller::Component*> components;

    Data data;
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
    emit setRootComponents(d->m_rootComponents);
    return true;
}

bool PackageManagerCore::fetchUpdaterPackages(const PackagesList &remotes, const LocalPackagesHash &locals)
{
    emit startUpdaterComponentsReset();

    d->clearUpdaterComponentLists();
    QHash<QString, QInstaller::Component *> components;

    Data data;
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
                const QStringList possibleNames = replaces.split(QInstaller::commaRegExp(),
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
            // append all components w/o parent to the direct list
            foreach (QInstaller::Component *component, components) {
                appendUpdaterComponent(component);
            }

            // after everything is set up, load the scripts
            foreach (QInstaller::Component *component, components) {
                if (d->statusCanceledOrFailed())
                    return false;

                component->loadComponentScript();
                component->setCheckState(Qt::Checked);
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

            std::sort(d->m_updaterComponents.begin(), d->m_updaterComponents.end(),
                Component::SortingPriorityGreaterThan());
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
    emit setRootComponents(d->m_updaterComponents);
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

ComponentModel *PackageManagerCore::componentModel(PackageManagerCore *core, const QString &objectName) const
{
    ComponentModel *model = new ComponentModel(4, core);

    model->setObjectName(objectName);
    model->setHeaderData(ComponentModelHelper::NameColumn, Qt::Horizontal,
        ComponentModel::tr("Component Name"));
    model->setHeaderData(ComponentModelHelper::InstalledVersionColumn, Qt::Horizontal,
        ComponentModel::tr("Installed Version"));
    model->setHeaderData(ComponentModelHelper::NewVersionColumn, Qt::Horizontal,
        ComponentModel::tr("New Version"));
    model->setHeaderData(ComponentModelHelper::UncompressedSizeColumn, Qt::Horizontal,
        ComponentModel::tr("Size"));
    connect(this, SIGNAL(setRootComponents(QList<QInstaller::Component*>)), model,
        SLOT(setRootComponents(QList<QInstaller::Component*>)));
    connect(model, SIGNAL(checkStateChanged(QInstaller::ComponentModel::ModelState)), this,
        SLOT(componentsToInstallNeedsRecalculation()));

    return model;
}
