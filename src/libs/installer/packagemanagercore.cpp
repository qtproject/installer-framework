/**************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
**
** $QT_END_LICENSE$
**
**************************************************************************/
#include "packagemanagercore.h"
#include "packagemanagercore_p.h"

#include "adminauthorization.h"
#include "binarycontent.h"
#include "component.h"
#include "componentmodel.h"
#include "downloadarchivesjob.h"
#include "errors.h"
#include "globals.h"
#include "messageboxhandler.h"
#include "packagemanagerproxyfactory.h"
#include "progresscoordinator.h"
#include "qprocesswrapper.h"
#include "qsettingswrapper.h"
#include "remoteclient.h"
#include "settings.h"
#include "utils.h"
#include "installercalculator.h"
#include "uninstallercalculator.h"

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

#include <QStandardPaths>

/*!
    \namespace QInstaller
    \inmodule QtInstallerFramework

    \keyword qinstaller-module

    \brief Contains classes to implement the core functionality of the Qt
    Installer Framework and the installer UI.
*/

/*!
    \qmltype installer
    \inqmlmodule scripting

    \brief Provides access to core functionality of the Qt Installer Framework.
*/

/*!
    \qmlproperty array installer::components
*/

/*!
    \qmlsignal installer::aboutCalculateComponentsToInstall()

    Emitted before the ordered list of components to install is calculated.
*/

/*!
    \qmlsignal installer::finishedCalculateComponentsToInstall()

    Emitted after the ordered list of components to install was calculated.
*/

/*!
    \qmlsignal installer::aboutCalculateComponentsToUninstall()

    Emitted before the ordered list of components to uninstall is calculated.
*/

/*!
    \qmlsignal installer::finishedCalculateComponentsToUninstall()

    Emitted after the ordered list of components to uninstall was calculated.
*/

/*!
    \qmlsignal installer::componentAdded(Component component)

    Emitted when a new root component has been added.

    \sa rootComponentsAdded, updaterComponentsAdded
*/

/*!
    \qmlsignal installer::rootComponentsAdded(list<Component> components)

    Emitted when a new list of root components has been added.

    \sa componentAdded, updaterComponentsAdded
*/

/*!
    \qmlsignal installer::updaterComponentsAdded(list<Component> components)

    Emitted when a new list of updater components has been added.
    \sa componentAdded, rootComponentsAdded
*/

/*!
    \qmlsignal installer::valueChanged(string key, string value)

    Emitted whenever a value changes.

    \sa setValue
*/

/*!
    \qmlsignal installer::statusChanged(Status status)

    Emitted whenever the installer status changes.
*/

/*!
    \qmlsignal installer::currentPageChanged(int page)

    Emitted whenever the current page changes.
*/

/*!
    \qmlsignal installer::finishButtonClicked()

    Emitted when the user clicks the \uicontrol Finish button of the installer.
*/

/*!
    \qmlsignal installer::metaJobProgress(int progress)

    Triggered with progress updates of the communication with a remote
    repository. Progress ranges from 0 to 100.
*/

/*!
    \qmlsignal installer::metaJobInfoMessage(string message)

    Triggered with informative updates of the communication with a remote repository.
*/

/*!
    \qmlsignal installer::startAllComponentsReset()

    Triggered when the list of components starts to get updated.

    \sa finishAllComponentsReset
*/

/*!
    \qmlsignal installer::finishAllComponentsReset(list<Component> rootComponents)

    Triggered when the list of new root components has been updated.

    \sa startAllComponentsReset
*/

/*!
    \qmlsignal installer::startUpdaterComponentsReset()

    Triggered when components start to get updated during a remote update.
*/

/*!
    \qmlsignal installer::finishUpdaterComponentsReset(list<Component> componentsWithUpdates)

    Triggered when the list of available remote updates has been updated.
*/

/*!
    \qmlsignal installer::installationStarted()

    Triggered when installation has started.

    \sa installationFinished installationInterrupted
*/

/*!
    \qmlsignal installer::installationInterrupted()

    Triggered when installation has been interrupted (cancelled).

    \sa interrupt installationStarted installationFinished
*/

/*!
    \qmlsignal installer::installationFinished()

    Triggered when installation has been finished.

    \sa installationStarted installationInterrupted
*/

/*!
    \qmlsignal installer::updateFinished()

    Triggered when an update has been finished.
*/

/*!
    \qmlsignal installer::uninstallationStarted()

    Triggered when uninstallation has started.

    \sa uninstallationFinished
*/

/*!
    \qmlsignal installer::uninstallationFinished()

    Triggered when uninstallation has been finished.

    \sa uninstallationStarted
*/

/*!
    \qmlsignal installer::titleMessageChanged(string title)

    Emitted when the text of the installer status (on the PerformInstallation page) changes to
    \a title.
*/

/*!
    \qmlsignal installer::wizardPageInsertionRequested(Widget widget, WizardPage page)

    Emitted when a custom \a widget is about to be inserted into \a page by addWizardPage.
*/

/*!
    \qmlsignal installer::wizardPageRemovalRequested(Widget widget)

    Emitted when a \a widget is removed by removeWizardPage.
*/

/*!
    \qmlsignal installer::wizardWidgetInsertionRequested(Widget widget, WizardPage page)

    Emitted when a \a widget is inserted into \a page by addWizardPageItem.
*/

/*!
    \qmlsignal installer::wizardWidgetRemovalRequested(Widget widget)

    Emitted when a \a widget is removed by removeWizardPageItem.
*/

/*!
    \qmlsignal installer::wizardPageVisibilityChangeRequested(bool visible, int page)

    Emitted when the visibility of the page with id \a page changes to \a visible.

    \sa setDefaultPageVisible
*/

/*!
    \qmlsignal installer::setValidatorForCustomPageRequested(Component component, string name,
                                        string callbackName)

    Triggered when setValidatorForCustomPage is called.
*/

/*!
    \qmlsignal installer::setAutomatedPageSwitchEnabled(bool request)

    Triggered when the automatic switching from PerformInstallation to InstallationFinished page
    is enabled (\a request = \c true) or disabled (\a request = \c false).

    The automatic switching is disabled automatically when for example the user expands or unexpands
    the \gui Details section of the PerformInstallation page.
*/

/*!
    \qmlsignal installer::coreNetworkSettingsChanged()

    Emitted when the network settings are changed.
*/


using namespace QInstaller;

/*!
    \class QInstaller::PackageManagerCore
    \inmodule QtInstallerFramework
    \brief The PackageManagerCore class provides the core functionality of the Qt Installer
        Framework.
*/

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

void PackageManagerCore::writeMaintenanceTool()
{
    if (d->m_needToWriteMaintenanceTool) {
        try {
            d->writeMaintenanceTool(d->m_performedOperationsOld + d->m_performedOperationsCurrentSession);

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
            d->m_needToWriteMaintenanceTool = false;
        } catch (const Error &error) {
            MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(),
                QLatin1String("WriteError"), tr("Error writing Maintenance Tool"), error.message(),
                QMessageBox::Ok, QMessageBox::Ok);
        }
    }
}

void PackageManagerCore::writeMaintenanceConfigFiles()
{
    d->writeMaintenanceConfigFiles();
}

void PackageManagerCore::reset(const QHash<QString, QString> &params)
{
    d->m_completeUninstall = false;
    d->m_needsHardRestart = false;
    d->m_status = PackageManagerCore::Unfinished;
    d->m_installerBaseBinaryUnreplaced.clear();

    d->initialize(params);
}

void PackageManagerCore::setGuiObject(QObject *gui)
{
    if (gui == d->m_guiObject)
        return;
    d->m_guiObject = gui;
    emit guiObjectChanged(gui);
}

QObject *PackageManagerCore::guiObject() const
{
    return d->m_guiObject;
}

/*!
    \qmlmethod void installer::setCompleteUninstallation(bool complete)

    Sets the uninstallation to be \a complete. If \a complete is false, only components deselected
    by the user will be uninstalled. This option applies only on uninstallation.
 */
void PackageManagerCore::setCompleteUninstallation(bool complete)
{
    d->m_completeUninstall = complete;
}

/*!
    \qmlmethod void installer::cancelMetaInfoJob()

    Cancels the retrieval of meta information from a remote repository.
 */
void PackageManagerCore::cancelMetaInfoJob()
{
    d->m_metadataJob.cancel();
}

/*!
    \qmlmethod void installer::componentsToInstallNeedsRecalculation()

    Ensures that component dependencies are re-calculated.
 */
void PackageManagerCore::componentsToInstallNeedsRecalculation()
{
    d->clearInstallerCalculator();
    d->clearUninstallerCalculator();
    QList<Component*> selectedComponentsToInstall = componentsMarkedForInstallation();

    d->m_componentsToInstallCalculated =
            d->installerCalculator()->appendComponentsToInstall(selectedComponentsToInstall);

    QList<Component *> componentsToInstall = d->installerCalculator()->orderedComponentsToInstall();

    QList<Component *> selectedComponentsToUninstall;
    foreach (Component *component, components(ComponentType::All)) {
        if (component->uninstallationRequested() && !selectedComponentsToInstall.contains(component))
            selectedComponentsToUninstall.append(component);
    }

    d->uninstallerCalculator()->appendComponentsToUninstall(selectedComponentsToUninstall);

    QSet<Component *> componentsToUninstall = d->uninstallerCalculator()->componentsToUninstall();

    foreach (Component *component, components(ComponentType::Root | ComponentType::Descendants))
        component->setInstallAction(component->isInstalled()
                           ? ComponentModelHelper::KeepInstalled
                           : ComponentModelHelper::KeepUninstalled);
    foreach (Component *component, componentsToUninstall)
        component->setInstallAction(ComponentModelHelper::Uninstall);
    foreach (Component *component, componentsToInstall)
        component->setInstallAction(ComponentModelHelper::Install);

    // update all nodes uncompressed size
    foreach (Component *const component, components(ComponentType::Root))
        component->updateUncompressedSize(); // this is a recursive call
}

/*!
   \qmlmethod void installer::autoAcceptMessageBoxes()

   Automatically accept all user message boxes.

   \sa autoRejectMessageBoxes, setMessageBoxAutomaticAnswer
 */
void PackageManagerCore::autoAcceptMessageBoxes()
{
    MessageBoxHandler::instance()->setDefaultAction(MessageBoxHandler::Accept);
}

/*!
   \qmlmethod void installer::autoRejectMessageBoxes()

   Automatically reject all user message boxes.

   \sa autoAcceptMessageBoxes, setMessageBoxAutomaticAnswer
 */
void PackageManagerCore::autoRejectMessageBoxes()
{
    MessageBoxHandler::instance()->setDefaultAction(MessageBoxHandler::Reject);
}

/*!
   \qmlmethod void installer::setMessageBoxAutomaticAnswer(string identifier, int button)

   Automatically close the message box with ID \a identifier as if the user had pressed \a button.

   This can be used for unattended (automatic) installations.

   \sa QMessageBox, autoAcceptMessageBoxes, autoRejectMessageBoxes
 */
void PackageManagerCore::setMessageBoxAutomaticAnswer(const QString &identifier, int button)
{
    MessageBoxHandler::instance()->setAutomaticAnswer(identifier,
        static_cast<QMessageBox::Button>(button));
}

quint64 PackageManagerCore::size(QInstaller::Component *component, const QString &value) const
{
    if (component->installAction() == ComponentModelHelper::Install)
        return component->value(value).toLongLong();
    return quint64(0);
}

/*!
   \qmlmethod float installer::requiredDiskSpace()

   Returns the additional estimated amount of disk space in bytes required after installation.

   \sa requiredTemporaryDiskSpace
 */
quint64 PackageManagerCore::requiredDiskSpace() const
{
    quint64 result = 0;

    foreach (QInstaller::Component *component, orderedComponentsToInstall())
        result += size(component, scUncompressedSize);

    return result;
}

/*!
   \qmlmethod float installer::requiredTemporaryDiskSpace()

   Returns the estimated required disk space during installation in bytes.

   \sa requiredDiskSpace
 */
quint64 PackageManagerCore::requiredTemporaryDiskSpace() const
{
    if (isOfflineOnly())
        return 0;

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

    DownloadArchivesJob archivesJob(this);
    archivesJob.setAutoDelete(false);
    archivesJob.setArchivesToDownload(archivesToDownload);
    connect(this, SIGNAL(installationInterrupted()), &archivesJob, SLOT(cancel()));
    connect(&archivesJob, SIGNAL(outputTextChanged(QString)), ProgressCoordinator::instance(),
        SLOT(emitLabelAndDetailTextChanged(QString)));
    connect(&archivesJob, SIGNAL(downloadStatusChanged(QString)), ProgressCoordinator::instance(),
        SIGNAL(downloadStatusChanged(QString)));

    ProgressCoordinator::instance()->registerPartProgress(&archivesJob,
        SIGNAL(progressChanged(double)), partProgressSize);

    archivesJob.start();
    archivesJob.waitForFinished();

    if (archivesJob.error() == KDJob::Canceled)
        interrupt();
    else if (archivesJob.error() != KDJob::NoError)
        throw Error(archivesJob.errorString());

    if (d->statusCanceledOrFailed())
        throw Error(tr("Installation canceled by user"));

    ProgressCoordinator::instance()->emitDownloadStatus(tr("All downloads finished."));

    return archivesJob.numberOfDownloads();
}

/*!
    Returns \c true if a component marked as essential was installed during the
    update process.
*/
bool PackageManagerCore::needsHardRestart() const
{
    return d->m_needsHardRestart;
}

void PackageManagerCore::setNeedsHardRestart(bool needsHardRestart)
{
    d->m_needsHardRestart = needsHardRestart;
}


void PackageManagerCore::rollBackInstallation()
{
    emit titleMessageChanged(tr("Cancelling the Installer"));

    // this unregisters all operation progressChanged connected
    ProgressCoordinator::instance()->setUndoMode();
    const int progressOperationCount = d->countProgressOperations(d->m_performedOperationsCurrentSession);
    const double progressOperationSize = double(1) / progressOperationCount;

    // reregister all the undo operations with the new size to the ProgressCoordinator
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
            const bool becameAdmin = !RemoteClient::instance().isActive()
                && operation->value(QLatin1String("admin")).toBool() && gainAdminRights();

            if (operation->value(QLatin1String("uninstall-only")).toBool() &&
                QVariant(value(scRemoveTargetDir)).toBool() &&
                (operation->name() == QLatin1String("Mkdir"))) {
                // We know the mkdir operation which is creating the target path. If we do a
                // full uninstall, prevent a forced remove of the full install path including the
                // target, instead try to remove the target only and only if it is empty,
                // otherwise fail silently. Note: this can only happen if RemoveTargetDir is set,
                // otherwise the operation does not exist at all.
                operation->setValue(QLatin1String("forceremoval"), false);
            }

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

            packages.writeToDisk();
            if (isInstaller()) {
                if (packages.packageInfoCount() == 0) {
                    QFile file(packages.fileName());
                    file.remove();
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
    \qmlmethod boolean installer::fileExists(string filePath)

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
    Repository::registerMetaType(); // register, cause we stream the type as QVariant
    qRegisterMetaType<QInstaller::PackageManagerCore::Status>("QInstaller::PackageManagerCore::Status");
    qRegisterMetaType<QInstaller::PackageManagerCore::WizardPage>("QInstaller::PackageManagerCore::WizardPage");
}

PackageManagerCore::PackageManagerCore(qint64 magicmaker, const QList<OperationBlob> &operations,
        const QString &socketName, const QString &key, Protocol::Mode mode)
    : d(new PackageManagerCorePrivate(this, magicmaker, operations))
{
    Repository::registerMetaType(); // register, cause we stream the type as QVariant
    qRegisterMetaType<QInstaller::PackageManagerCore::Status>("QInstaller::PackageManagerCore::Status");
    qRegisterMetaType<QInstaller::PackageManagerCore::WizardPage>("QInstaller::PackageManagerCore::WizardPage");

    // Creates and initializes a remote client, makes us get admin rights for QFile, QSettings
    // and QProcess operations. Init needs to called to set the server side authorization key.
    RemoteClient::instance().init(socketName, key, mode, Protocol::StartAs::SuperUser);

    d->initialize(QHash<QString, QString>());

    //
    // Sanity check to detect a broken installations with missing operations.
    // Every installed package should have at least one MinimalProgress operation.
    //
    QSet<QString> installedPackages = d->m_core->localInstalledPackages().keys().toSet();
    QSet<QString> operationPackages;
    foreach (QInstaller::Operation *operation, d->m_performedOperationsOld) {
        if (operation->hasValue(QLatin1String("component")))
            operationPackages.insert(operation->value(QLatin1String("component")).toString());
    }

    QSet<QString> packagesWithoutOperation = installedPackages - operationPackages;
    QSet<QString> orphanedOperations = operationPackages - installedPackages;
    if (!packagesWithoutOperation.isEmpty() || !orphanedOperations.isEmpty())  {
        qCritical() << "Operations missing for installed packages" << packagesWithoutOperation.toList();
        qCritical() << "Orphaned operations" << orphanedOperations.toList();
        MessageBoxHandler::critical(
                    MessageBoxHandler::currentBestSuitParent(),
                    QLatin1String("Corrupt_Installation_Error"),
                    QCoreApplication::translate("QInstaller", "Corrupt installation"),
                    QCoreApplication::translate("QInstaller",
                                                "Your installation seems to be corrupted. "
                                                "Please consider re-installing from scratch."
                                                ));
    } else {
        qDebug() << "Operations sanity check succeeded.";
    }
}

PackageManagerCore::~PackageManagerCore()
{
    if (!isUninstaller() && !(isInstaller() && status() == PackageManagerCore::Canceled)) {
        QDir targetDir(value(scTargetDir));
        QString logFileName = targetDir.absoluteFilePath(value(QLatin1String("LogFileName"),
            QLatin1String("InstallationLog.txt")));
        QInstaller::VerboseWriter::instance()->setFileName(logFileName);
    }
    delete d;

    RemoteClient::instance().setActive(false);
    RemoteClient::instance().shutdown();

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

    emit finishAllComponentsReset(d->m_rootComponents);
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

PackageManagerProxyFactory *PackageManagerCore::proxyFactory() const
{
    if (d->m_proxyFactory)
        return d->m_proxyFactory->clone();
    return new PackageManagerProxyFactory(this);
}

void PackageManagerCore::setProxyFactory(PackageManagerProxyFactory *factory)
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

    if (!ProductKeyCheck::instance()->hasValidKey()) {
        d->setStatus(Failure, ProductKeyCheck::instance()->lastErrorString());
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
    if (!isUpdater()) {
        success = fetchAllPackages(packages, installedPackages);
        if (success && !d->statusCanceledOrFailed() && isPackageManager()) {
            foreach (Package *const update, packages) {
                if (update->data(scEssential, scFalse).toString().toLower() == scTrue) {
                    const QString name = update->data(scName).toString();
                    if (!installedPackages.contains(name)) {
                        success = false;
                        break;  // unusual, the maintenance tool should always be available
                    }

                    const LocalPackage localPackage = installedPackages.value(name);
                    const QString updateVersion = update->data(scRemoteVersion).toString();
                    if (KDUpdater::compareVersion(updateVersion, localPackage.version) <= 0)
                        break;  // remote version equals or is less than the installed maintenance tool

                    const QDate updateDate = update->data(scReleaseDate).toDate();
                    if (localPackage.lastUpdateDate >= updateDate)
                        break;  // remote release date equals or is less than the installed maintenance tool

                    success = false;
                    break;  // we found a newer version of the maintenance tool
                }
            }

            if (!success && !d->statusCanceledOrFailed()) {
                updateDisplayVersions(scRemoteDisplayVersion);
                d->setStatus(ForceUpdate, tr("There is an important update available, please run the "
                    "updater first."));
                return false;
            }
        }
    } else {
        success = fetchUpdaterPackages(packages, installedPackages);
    }

    updateDisplayVersions(scRemoteDisplayVersion);

    if (success && !d->statusCanceledOrFailed())
        d->setStatus(Success);
    return success;
}

/*!
    \qmlmethod boolean installer::addWizardPage(Component component, string name, int page)

    Adds the widget with objectName() \a name registered by \a component as a new page
    into the installer's GUI wizard. The widget is added before \a page.

    See \l{Controller Scripting} for the possible values of \a page.

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
    \qmlmethod boolean installer::removeWizardPage(Component component, string name)

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
    \qmlmethod boolean installer::setDefaultPageVisible(int page, boolean visible)

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
    \qmlmethod void installer::setValidatorForCustomPage(Component component, string name,
                                                         string callbackName)

    \sa setValidatorForCustomPageRequested
 */
void PackageManagerCore::setValidatorForCustomPage(Component *component, const QString &name,
                                                   const QString &callbackName)
{
    emit setValidatorForCustomPageRequested(component, name, callbackName);
}

/*!
    \qmlmethod boolean installer::addWizardPageItem(Component component, string name, int page)

    Adds the widget with objectName() \a name registered by \a component as a GUI element
    into the installer's GUI wizard. The widget is added on \a page.

    See \l{Controller Scripting} for the possible values of \a page.

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
    \qmlmethod boolean installer::removeWizardPageItem(Component component, string name)

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
    \qmlmethod void installer::addUserRepositories(stringlist repositories)

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
    \qmlmethod void installer::setTemporaryRepositories(stringlist repositories, boolean replace)

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
    Checks whether the downloader should try to download SHA-1 checksums for
    archives.
*/
bool PackageManagerCore::testChecksum() const
{
    return d->m_testChecksum;
}

/*!
    The \a test argument determines whether the downloader should try to
    download SHA-1 checksums for archives.
*/
void PackageManagerCore::setTestChecksum(bool test)
{
    d->m_testChecksum = test;
}

ScriptEngine *PackageManagerCore::componentScriptEngine() const
{
    return d->componentScriptEngine();
}

ScriptEngine *PackageManagerCore::controlScriptEngine() const
{
    return d->controlScriptEngine();
}

/*!
    Appends \a component as the root component to the internal storage for
    installer or package manager components. To append a component as a child to
    an already existing component, use Component::appendComponent(). Emits
    the componentAdded() signal.
*/
void PackageManagerCore::appendRootComponent(Component *component)
{
    d->m_rootComponents.append(component);
    emit componentAdded(component);
}

/*!
    \class PackageManagerCore::ComponentType
    \inmodule QtInstallerFramework
    \brief The ComponentType class is used with components() to describe what type of \c Component
        list this function should return.

    \value Root
        Return a list of root components.

    \value Descendants
        Return a list of all descendant components. \b Note: In updater mode the list is empty,
        because component updates cannot have children.

    \value Dependencies
        Return a list of all available dependencies when run as updater. \b Note: In Installer,
        package manager and uninstaller mode, this will always result in an empty list.

    \value Replacements
        Return a list of all available replacement components relevant to the run mode.

    \value AllNoReplacements
        Return a list of available components, including root, descendant and dependency
        components relevant to the run mode.

    \value All
        Return a list of all available components, including root, descendant, dependency and
        replacement components relevant to the run mode.
*/

/*!
    Returns a list of components depending on the component types passed in \a mask.
*/
QList<Component *> PackageManagerCore::components(ComponentTypes mask) const
{
    QList<Component *> components;

    const bool updater = isUpdater();
    if (mask.testFlag(ComponentType::Root))
        components += updater ? d->m_updaterComponents : d->m_rootComponents;
    if (mask.testFlag(ComponentType::Replacements))
        components += updater ? d->m_updaterDependencyReplacements : d->m_rootDependencyReplacements;

    if (!updater) {
        if (mask.testFlag(ComponentType::Descendants)) {
            foreach (QInstaller::Component *component, d->m_rootComponents)
                components += component->descendantComponents();
        }
    } else {
        if (mask.testFlag(ComponentType::Dependencies))
            components.append(d->m_updaterComponentsDeps);
        // No descendants here, updates are always a flat list and cannot have children!
    }

    return components;
}

/*!
    Appends \a component to the internal storage for updater components. Emits
    the componentAdded() signal.
*/
void PackageManagerCore::appendUpdaterComponent(Component *component)
{
    component->setUpdateAvailable(true);
    d->m_updaterComponents.append(component);
    emit componentAdded(component);
}

/*!
    \qmlmethod component installer::componentByName(string name)

    Returns a component matching \a name. \a name can also contain a version requirement.
    For example "org.qt-project.sdk.qt" returns any component with that name,
    "org.qt-project.sdk.qt->=4.5" requires the returned component to have at least version 4.5.
    If no component matches the requirement, 0 is returned.
*/
Component *PackageManagerCore::componentByName(const QString &name) const
{
    return componentByName(name, components(ComponentType::AllNoReplacements));
}

Component *PackageManagerCore::componentByName(const QString &name, const QList<Component *> &components)
{
    if (name.isEmpty())
        return 0;

    QString fixedVersion;
    QString fixedName = name;
    if (name.contains(QChar::fromLatin1('-'))) {
        // the last part is considered to be the version, then
        fixedVersion = name.section(QLatin1Char('-'), 1);
        fixedName = name.section(QLatin1Char('-'), 0, 0);
    }

    foreach (Component *component, components) {
        if (componentMatches(component, fixedName, fixedVersion))
            return component;
    }

    return 0;
}

QList<Component *> PackageManagerCore::componentsMarkedForInstallation() const
{
    QList<Component*> markedForInstallation;
    const QList<Component*> relevant = components(ComponentType::Root | ComponentType::Descendants);
    if (isUpdater()) {
        foreach (Component *component, relevant) {
            if (component->updateRequested())
                markedForInstallation.append(component);
        }
    } else {
        // relevant means all components which are not replaced
        foreach (Component *component, relevant) {
            // ask for all components which will be installed to get all dependencies
            // even dependencies which are changed without an increased version
            if (component->isSelectedForInstallation()
                    || (component->isInstalled()
                    && !component->uninstallationRequested())) {
                markedForInstallation.append(component);
            }
        }
    }
    return markedForInstallation;
}

/*!
    \qmlmethod boolean installer::calculateComponentsToInstall()

    Calculates an ordered list of components to install based on the current run mode. Also auto
    installed dependencies are resolved. The aboutCalculateComponentsToInstall() signal is emitted
    before the calculation starts, the finishedCalculateComponentsToInstall() signal once all
    calculations are done.
*/
bool PackageManagerCore::calculateComponentsToInstall() const
{
    emit aboutCalculateComponentsToInstall();
    if (!d->m_componentsToInstallCalculated) {
        d->clearInstallerCalculator();
        QList<Component*> selectedComponentsToInstall = componentsMarkedForInstallation();

        d->storeCheckState();
        d->m_componentsToInstallCalculated =
            d->installerCalculator()->appendComponentsToInstall(selectedComponentsToInstall);
    }
    emit finishedCalculateComponentsToInstall();
    return d->m_componentsToInstallCalculated;
}

/*!
    Returns a list of ordered components to install. The list can be empty.
*/
QList<Component*> PackageManagerCore::orderedComponentsToInstall() const
{
    return d->installerCalculator()->orderedComponentsToInstall();
}

/*!
    \qmlmethod boolean installer::calculateComponentsToUninstall()

    Calculates a list of components to uninstall based on the current run mode. Auto installed
    dependencies are not yet resolved.  The aboutCalculateComponentsToUninstall() signal is emitted
    before the calculation starts, the finishedCalculateComponentsToUninstall() signal once all
    calculations are done. Always returns \c true.
*/
bool PackageManagerCore::calculateComponentsToUninstall() const
{
    emit aboutCalculateComponentsToUninstall();
    if (!isUpdater()) {
        // hack to avoid removing needed dependencies
        QSet<Component*>  componentsToInstall = d->installerCalculator()->orderedComponentsToInstall().toSet();

        QList<Component*> componentsToUninstall;
        foreach (Component *component, components(ComponentType::All)) {
            if (component->uninstallationRequested() && !componentsToInstall.contains(component))
                componentsToUninstall.append(component);
        }

        d->clearUninstallerCalculator();
        d->storeCheckState();
        d->uninstallerCalculator()->appendComponentsToUninstall(componentsToUninstall);
    }
    emit finishedCalculateComponentsToUninstall();
    return true;
}

/*!
    Returns a list of components to uninstall. The list can be empty.
*/
QList<Component *> PackageManagerCore::componentsToUninstall() const
{
    return d->uninstallerCalculator()->componentsToUninstall().toList();
}

QString PackageManagerCore::componentsToInstallError() const
{
    return d->installerCalculator()->componentsToInstallError();
}

/*!
    Returns the reason why \a component needs to be installed:

    \list
        \li The component was scheduled for installation.
        \li The component was added as a dependency for another component.
        \li The component was added as an automatic dependency.
    \endlist
*/
QString PackageManagerCore::installReason(Component *component) const
{
    return d->installerCalculator()->installReason(component);
}

/*!
    Returns a list of components that depend on \a _component. The list can be
    empty.

    \note Automatic dependencies are not resolved.
*/
QList<Component*> PackageManagerCore::dependees(const Component *_component) const
{
    if (!_component)
        return QList<Component *>();

    const QList<QInstaller::Component *> availableComponents = components(ComponentType::All);
    if (availableComponents.isEmpty())
        return QList<Component *>();

    const QLatin1Char dash('-');
    QList<Component *> dependees;
    foreach (Component *component, availableComponents) {
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

ComponentModel *PackageManagerCore::defaultComponentModel() const
{
    QMutexLocker _(globalModelMutex());
    if (!d->m_defaultModel) {
        d->m_defaultModel = componentModel(const_cast<PackageManagerCore*> (this),
            QLatin1String("AllComponentsModel"));
    }
    connect(this, SIGNAL(finishAllComponentsReset(QList<QInstaller::Component*>)), d->m_defaultModel,
        SLOT(setRootComponents(QList<QInstaller::Component*>)));
    return d->m_defaultModel;
}

ComponentModel *PackageManagerCore::updaterComponentModel() const
{
    QMutexLocker _(globalModelMutex());
    if (!d->m_updaterModel) {
        d->m_updaterModel = componentModel(const_cast<PackageManagerCore*> (this),
            QLatin1String("UpdaterComponentsModel"));
    }
    connect(this, SIGNAL(finishUpdaterComponentsReset(QList<QInstaller::Component*>)), d->m_updaterModel,
        SLOT(setRootComponents(QList<QInstaller::Component*>)));
    return d->m_updaterModel;
}

Settings &PackageManagerCore::settings() const
{
    return d->m_data.settings();
}

/*!
    \qmlmethod boolean installer::gainAdminRights()

    Tries to gain admin rights. On success, it returns \c true.

    \sa dropAdminRights
*/
bool PackageManagerCore::gainAdminRights()
{
    if (AdminAuthorization::hasAdminRights())
        return true;

    RemoteClient::instance().setActive(true);
    if (!RemoteClient::instance().isActive())
        throw Error(tr("Error while elevating access rights."));
    return true;
}

/*!
    \qmlmethod void installer::dropAdminRights()

    Drops admin rights gained by gainAdminRights.

    \sa gainAdminRights
*/
void PackageManagerCore::dropAdminRights()
{
    RemoteClient::instance().setActive(false);
}

/*!
    \qmlmethod boolean installer::isProcessRunning(string name)

    Returns \c true if a process with \a name is running. On Windows, the comparison
    is case-insensitive.
*/
bool PackageManagerCore::isProcessRunning(const QString &name) const
{
    return PackageManagerCorePrivate::isProcessRunning(name, runningProcesses());
}

/*!
    \qmlmethod boolean installer::killProcess(string absoluteFilePath)

    Returns \c true if a process with \a absoluteFilePath could be killed or is
    not running.

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
    \qmlmethod void installer::setDependsOnLocalInstallerBinary()

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
    \qmlmethod boolean installer::localInstallerBinaryUsed()

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
    \qmlmethod array installer::execute(string program, stringlist arguments = undefined,
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
    QProcessWrapper process;

    QString adjustedProgram = replaceVariables(program);
    QStringList adjustedArguments;
    foreach (const QString &argument, arguments)
        adjustedArguments.append(replaceVariables(argument));
    QString adjustedStdIn = replaceVariables(stdIn);

    process.start(adjustedProgram, adjustedArguments,
        adjustedStdIn.isNull() ? QIODevice::ReadOnly : QIODevice::ReadWrite);

    if (!process.waitForStarted())
        return QList< QVariant >();

    if (!adjustedStdIn.isNull()) {
        process.write(adjustedStdIn.toLatin1());
        process.closeWriteChannel();
    }

    process.waitForFinished(-1);

    return QList<QVariant>() << QString::fromLatin1(process.readAllStandardOutput()) << process.exitCode();
}

/*!
    \qmlmethod boolean installer::executeDetached(string program, stringlist arguments = undefined,
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
    \qmlmethod string installer::environmentVariable(string name)

    Returns the content of the environment variable \a name. An empty string is returned if the
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

bool PackageManagerCore::operationExists(const QString &name)
{
    static QSet<QString> existingOperations;
    if (existingOperations.contains(name))
        return true;
    QScopedPointer<Operation> op(KDUpdater::UpdateOperationFactory::instance().create(name));
    if (!op.data())
        return false;
    existingOperations.insert(name);
    return true;
}

/*!
    \qmlmethod boolean installer::performOperation(string name, stringlist arguments)

    Instantly performs the operation \a name with \a arguments.
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
    \qmlmethod boolean installer::versionMatches(string version, string requirement)

    Returns \c true when \a version matches the \a requirement.
    \a requirement can be a fixed version number or it can be prefixed by the comparators '>', '>=',
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
    \qmlmethod string installer::findLibrary(string name, stringlist paths = [])

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
#if defined(Q_OS_OSX)
    if (findPaths.isEmpty()) {
        findPaths.push_back(QLatin1String("/lib"));
        findPaths.push_back(QLatin1String("/usr/lib"));
        findPaths.push_back(QLatin1String("/usr/local/lib"));
        findPaths.push_back(QLatin1String("/opt/local/lib"));
    }
    const QString dynamic = findPath(QString::fromLatin1("lib%1.dylib").arg(name), findPaths);
#else
    if (findPaths.isEmpty()) {
        findPaths.push_back(QLatin1String("/lib"));
        findPaths.push_back(QLatin1String("/usr/lib"));
        findPaths.push_back(QLatin1String("/usr/local/lib"));
        findPaths.push_back(QLatin1String("/lib64"));
        findPaths.push_back(QLatin1String("/usr/lib64"));
        findPaths.push_back(QLatin1String("/usr/local/lib64"));
    }
    const QString dynamic = findPath(QString::fromLatin1("lib%1.so*").arg(name), findPaths);
#endif
    if (!dynamic.isEmpty())
        return dynamic;
    return findPath(QString::fromLatin1("lib%1.a").arg(name), findPaths);
#endif
}

/*!
    \qmlmethod string installer::findPath(string name, stringlist paths = [])

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
    \qmlmethod void installer::setInstallerBaseBinary(string path)

    Sets the "installerbase" binary to use when writing the maintenance tool.
    Set this if an update to installerbase is available.

    If not set, the executable segment of the running installer or uninstaller
    will be used.
*/
void PackageManagerCore::setInstallerBaseBinary(const QString &path)
{
    d->m_installerBaseBinaryUnreplaced = path;
}

/*!
    \qmlmethod string installer::value(string key, string defaultValue = "")

    Returns the installer value for \a key. If \a key is not known to the system, \a defaultValue is
    returned. Additionally, on Windows, \a key can be a registry key.

    \sa setValue, containsValue, valueChanged
*/
QString PackageManagerCore::value(const QString &key, const QString &defaultValue) const
{
    return d->m_data.value(key, defaultValue).toString();
}

/*!
    \qmlmethod stringlist installer::values(string key, stringlist defaultValue = [])

    Returns the installer value for \a key. If \a key is not known to the system, \a defaultValue is
    returned. Additionally, on Windows, \a key can be a registry key.

    \sa value
*/
QStringList PackageManagerCore::values(const QString &key, const QStringList &defaultValue) const
{
    return d->m_data.value(key, defaultValue).toStringList();
}

/*!
    \qmlmethod void installer::setValue(string key, string value)

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
    \qmlmethod boolean installer::containsValue(string key)

    Returns \c true if the installer contains a value for \a key.

    \sa value, setValue, valueChanged
*/
bool PackageManagerCore::containsValue(const QString &key) const
{
    return d->m_data.contains(key);
}

/*!
    \qmlmethod void installer::setSharedFlag(string key, boolean value)

    Sets a shared flag with name \a key to \a value. This is one option
    to share information between scripts.

    Deprecated since 2.0.0

    \sa sharedFlag
*/
void PackageManagerCore::setSharedFlag(const QString &key, bool value)
{
    qDebug() << "sharedFlag is deprecated";
    d->m_sharedFlags.insert(key, value);
}

/*!
    \qmlmethod boolean installer::sharedFlag(string key)

    Returns shared flag with name \a key. This is one option
    to share information between scripts.

    Deprecated since 2.0.0

    \sa setSharedFlag
*/
bool PackageManagerCore::sharedFlag(const QString &key) const
{
    qDebug() << "sharedFlag is deprecated";
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
    Returns \c true if at least one complete installation or update was
    successful, even if the user cancelled the latest installation process.
*/
bool PackageManagerCore::finishedWithSuccess() const
{
    return d->m_status == PackageManagerCore::Success || d->m_needToWriteMaintenanceTool;
}

/*!
    \qmlmethod void installer::interrupt()

   Cancels an ongoing installation.

    \sa installationInterrupted
 */
void PackageManagerCore::interrupt()
{
    setCanceled();
    emit installationInterrupted();
}

/*!
    \qmlmethod void installer::setCanceled()

    Cancels the installation.
 */
void PackageManagerCore::setCanceled()
{
    if (!d->m_repoFetched)
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
    Replaces all variables in any instance of \a str by their respective values
    and returns the results.
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
    \qmlmethod boolean installer::isInstaller()

    Returns \c true if executed in an install step.

    \sa isUninstaller, isUpdater, isPackageManager
*/
bool PackageManagerCore::isInstaller() const
{
    return d->isInstaller();
}

/*!
    \qmlmethod boolean installer::isOfflineOnly()

    Returns \c true if this is an offline-only installer.
*/
bool PackageManagerCore::isOfflineOnly() const
{
    return d->isOfflineOnly();
}

/*!
    \qmlmethod void installer::setUninstaller()

    Forces an uninstaller context.

    \sa isUninstaller, setUpdater, setPackageManager
*/
void PackageManagerCore::setUninstaller()
{
    d->m_magicBinaryMarker = BinaryContent::MagicUninstallerMarker;
}

/*!
    \qmlmethod boolean installer::isUninstaller()

    Returns \c true if the script is executed in an uninstall context.

    \sa setUninstaller, isInstaller, isUpdater, isPackageManager
*/
bool PackageManagerCore::isUninstaller() const
{
    return d->isUninstaller();
}

/*!
    \qmlmethod void installer::setUpdater()

    Forces an updater context.

    \sa isUpdater, setUninstaller, setPackageManager
*/
void PackageManagerCore::setUpdater()
{
    d->m_magicBinaryMarker = BinaryContent::MagicUpdaterMarker;
}

/*!
    \qmlmethod boolean installer::isUpdater()

    Returns \c true if the script is executed in an updater context.

    \sa setUpdater, isInstaller, isUninstaller, isPackageManager
*/
bool PackageManagerCore::isUpdater() const
{
    return d->isUpdater();
}

/*!
    \qmlmethod void installer::setPackageManager()

    Forces a package manager context.
*/
void PackageManagerCore::setPackageManager()
{
    d->m_magicBinaryMarker = BinaryContent::MagicPackageManagerMarker;
}


/*!
    \qmlmethod boolean installer::isPackageManager()

    Returns \c true if the script is executed in a package manager context.
    \sa setPackageManager, isInstaller, isUninstaller, isUpdater
*/
bool PackageManagerCore::isPackageManager() const
{
    return d->isPackageManager();
}

/*!
    \qmlmethod boolean installer::runInstaller()

    Runs the installer. Returns \c true on success, \c false otherwise.
*/
bool PackageManagerCore::runInstaller()
{
    return d->runInstaller();
}

/*!
    \qmlmethod boolean installer::runUninstaller()

    Runs the uninstaller. Returns \c true on success, \c false otherwise.
*/
bool PackageManagerCore::runUninstaller()
{
    return d->runUninstaller();
}

/*!
    \qmlmethod boolean installer::runPackageUpdater()

    Runs the package updater. Returns \c true on success, \c false otherwise.
*/
bool PackageManagerCore::runPackageUpdater()
{
    return d->runPackageUpdater();
}

/*!
    \qmlmethod void installer::languageChanged()

    Calls languangeChanged on all components.
*/
void PackageManagerCore::languageChanged()
{
    foreach (Component *component, components(ComponentType::All))
        component->languageChanged();
}

/*!
    Runs the installer, uninstaller, updater, or package manager, depending on
    the type of this binary.
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
    Returns the path name of the maintenance tool binary.
*/
QString PackageManagerCore::maintenanceToolName() const
{
    return d->maintenanceToolName();
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


        const Repository repo = d->m_metadataJob.repositoryForDirectory(localPath);
        if (repo.isValid()) {
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

        if (!ProductKeyCheck::instance()->isValidPackage(package->data(scName).toString()))
            continue;

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

    emit finishAllComponentsReset(d->m_rootComponents);
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

        if (!ProductKeyCheck::instance()->isValidPackage(update->data(scName).toString()))
            continue;

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

            // break if the update is not valid and if it's not the maintenance tool (we might get an update
            // for the maintenance tool even if it's not currently installed - possible offline installation)
            if (!isValidUpdate && (update->data(scEssential, scFalse).toString().toLower() == scFalse))
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
        emit finishUpdaterComponentsReset(QList<QInstaller::Component*>());
        d->setStatus(Failure, error.message());

        // TODO: make sure we remove all message boxes inside the library at some point.
        MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(), QLatin1String("Error"),
            tr("Error"), error.message());
        return false;
    }

    emit finishUpdaterComponentsReset(d->m_updaterComponents);
    return true;
}

void PackageManagerCore::restoreCheckState()
{
    d->restoreCheckState();
}

void PackageManagerCore::updateDisplayVersions(const QString &displayKey)
{
    QHash<QString, QInstaller::Component *> componentsHash;
    foreach (QInstaller::Component *component, components(ComponentType::All))
        componentsHash[component->name()] = component;

    // set display version for all components in list
    const QStringList &keys = componentsHash.keys();
    foreach (const QString &key, keys) {
        QHash<QString, bool> visited;
        if (componentsHash.value(key)->isInstalled()) {
            componentsHash.value(key)->setValue(scDisplayVersion,
                findDisplayVersion(key, componentsHash, scInstalledVersion, visited));
        }
        visited.clear();
        const QString displayVersionRemote = findDisplayVersion(key, componentsHash,
            scRemoteVersion, visited);
        if (displayVersionRemote.isEmpty())
            componentsHash.value(key)->setValue(displayKey, tr("invalid"));
        else
            componentsHash.value(key)->setValue(displayKey, displayVersionRemote);
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
    ComponentModel *model = new ComponentModel(ComponentModelHelper::LastColumn, core);

    model->setObjectName(objectName);
    model->setHeaderData(ComponentModelHelper::NameColumn, Qt::Horizontal,
        ComponentModel::tr("Component Name"));
    model->setHeaderData(ComponentModelHelper::ActionColumn, Qt::Horizontal,
        ComponentModel::tr("Action"));
    model->setHeaderData(ComponentModelHelper::InstalledVersionColumn, Qt::Horizontal,
        ComponentModel::tr("Installed Version"));
    model->setHeaderData(ComponentModelHelper::NewVersionColumn, Qt::Horizontal,
        ComponentModel::tr("New Version"));
    model->setHeaderData(ComponentModelHelper::ReleaseDateColumn, Qt::Horizontal,
        ComponentModel::tr("Release Date"));
    model->setHeaderData(ComponentModelHelper::UncompressedSizeColumn, Qt::Horizontal,
        ComponentModel::tr("Size"));
    connect(model, SIGNAL(checkStateChanged(QInstaller::ComponentModel::ModelState)), this,
        SLOT(componentsToInstallNeedsRecalculation()));

    return model;
}
