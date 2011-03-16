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
#include "qinstaller.h"

#include "adminauthorization.h"
#include "common/binaryformat.h"
#include "common/errors.h"
#include "common/installersettings.h"
#include "common/utils.h"
#include "downloadarchivesjob.h"
#include "fsengineclient.h"
#include "getrepositoriesmetainfojob.h"
#include "messageboxhandler.h"
#include "progresscoordinator.h"
#include "qinstaller_p.h"
#include "qinstallercomponent.h"
#include "qinstallerglobal.h"

#include <QtCore/QTemporaryFile>

#include <QtGui/QDesktopServices>

#include <QtScript/QScriptEngine>
#include <QtScript/QScriptContext>

#include <KDToolsCore/KDSysInfo>

#include <KDUpdater/KDUpdater>

#ifdef Q_OS_WIN
#include "qt_windows.h"
#endif

using namespace QInstaller;

static QScriptValue checkArguments(QScriptContext* context, int amin, int amax)
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

/*!
    Appends \a comp preceded by its dependencies to \a components. Makes sure components contains
    every component only once.
    \internal
*/
static void appendComponentAndMissingDependencies(QList<Component*>& components, Component* comp)
{
    if (comp == 0)
        return;

    const QList<Component*> deps = comp->installer()->missingDependencies(comp);
    for (QList<Component*>::const_iterator it = deps.begin(); it != deps.end(); ++it)
        appendComponentAndMissingDependencies(components, *it);
    if (!components.contains(comp))
        components.push_back(comp);
}

/*!
 Scriptable version of Installer::componentByName(QString).
 \sa Installer::componentByName
 */
QScriptValue QInstaller::qInstallerComponentByName(QScriptContext* context, QScriptEngine* engine)
{
    const QScriptValue check = checkArguments(context, 1, 1);
    if (check.isError())
        return check;

    // well... this is our "this" pointer
    Installer* const installer = dynamic_cast< Installer* >(engine->globalObject()
        .property(QLatin1String("installer")).toQObject());

    const QString name = context->argument(0).toString();
    Component* const c = installer->componentByName(name);
    return engine->newQObject(c);
}

QScriptValue QInstaller::qDesktopServicesOpenUrl(QScriptContext* context, QScriptEngine* engine)
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

QScriptValue QInstaller::qDesktopServicesDisplayName(QScriptContext* context, QScriptEngine* engine)
{
    Q_UNUSED(engine);
    const QScriptValue check = checkArguments(context, 1, 1);
    if (check.isError())
        return check;
    const QDesktopServices::StandardLocation location =
        static_cast< QDesktopServices::StandardLocation >(context->argument(0).toInt32());
    return QDesktopServices::displayName(location);
}

QScriptValue QInstaller::qDesktopServicesStorageLocation(QScriptContext* context, QScriptEngine* engine)
{
    Q_UNUSED(engine);
    const QScriptValue check = checkArguments(context, 1, 1);
    if (check.isError())
        return check;
    const QDesktopServices::StandardLocation location =
        static_cast< QDesktopServices::StandardLocation >(context->argument(0).toInt32());
    return QDesktopServices::storageLocation(location);
}

QString QInstaller::uncaughtExceptionString(QScriptEngine *scriptEngine/*, const QString &context*/)
{
    //QString errorString(QLatin1String("%1 %2\n%3"));
    QString errorString(QLatin1String("\t\t%1\n%2"));
    //if (!context.isEmpty())
    //    errorString.prepend(context + QLatin1String(": "));

    //usually the linenumber is in the backtrace
    errorString = errorString.arg(/*QString::number(scriptEngine->uncaughtExceptionLineNumber()),*/
                    scriptEngine->uncaughtException().toString(),
                    scriptEngine->uncaughtExceptionBacktrace().join(QLatin1String("\n")));
    return errorString;
}


/*!
 \class QInstaller::Installer
 Installer forms the core of the installation and uninstallation system.
 */

/*!
  \enum QInstaller::Installer::WizardPage
  WizardPage is used to number the different pages known to the Installer GUI.
 */

/*!
  \var QInstaller::Installer::Introduction
  Introduction page.
 */

/*!
  \var QInstaller::Installer::LicenseCheck
  License check page
 */
/*!
  \var QInstaller::Installer::TargetDirectory
  Target directory selection page
 */
/*!
  \var QInstaller::Installer::ComponentSelection
  %Component selection page
 */
/*!
  \var QInstaller::Installer::StartMenuSelection
  Start menu directory selection page - Microsoft Windows only
 */
/*!
  \var QInstaller::Installer::ReadyForInstallation
  "Ready for Installation" page
 */
/*!
  \var QInstaller::Installer::PerformInstallation
  Page shown while performing the installation
 */
/*!
  \var QInstaller::Installer::InstallationFinished
  Page shown when the installation was finished
 */
/*!
  \var QInstaller::Installer::End
  Non-existing page - this value has to be used if you want to insert a page after \a InstallationFinished
 */


KDUpdater::Application& Installer::updaterApplication() const
{
    return *d->m_app;
}

void Installer::setUpdaterApplication(KDUpdater::Application *app)
{
    d->m_app = app;
}

void Installer::writeUninstaller()
{
    if (d->m_needToWriteUninstaller) {
        bool error = false;
        QString errorMsg;
        try {
            d->writeUninstaller(d->m_performedOperationsOld  + d->m_performedOperationsCurrentSession);

            bool gainedAdminRights = false;
            QTemporaryFile tempAdminFile(d->targetDir()
                + QLatin1String("/testjsfdjlkdsjflkdsjfldsjlfds") + QString::number(qrand() % 1000));
            if (!tempAdminFile.open() || !tempAdminFile.isWritable()) {
                gainAdminRights();
                gainedAdminRights = true;
            }
            d->m_app->packagesInfo()->writeToDisk();
            if (gainedAdminRights)
                dropAdminRights();
            d->m_needToWriteUninstaller = false;
        } catch (const Error& e) {
            error = true;
            errorMsg = e.message();
        }

        if (error) {
            MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(),
                QLatin1String("WriteError"), tr("Error writing Uninstaller"), errorMsg,
                QMessageBox::Ok, QMessageBox::Ok);
        }
    }
}

void Installer::reset(const QHash<QString, QString> &params)
{
    d->m_completeUninstall = false;
    d->m_forceRestart = false;
    d->m_status = Installer::Unfinished;
    d->m_installerBaseBinaryUnreplaced.clear();
    d->m_vars.clear();
    d->m_vars = params;
    d->initialize();
}

/*!
 * Sets the uninstallation to be \a complete. If \a complete is false, only components deselected
 * by the user will be uninstalled.
 * This option applies only on uninstallation.
 */
void Installer::setCompleteUninstallation(bool complete)
{
    d->m_completeUninstall = complete;
    d->m_packageManagingMode = !d->m_completeUninstall;
}

void Installer::autoAcceptMessageBoxes()
{
    MessageBoxHandler::instance()->setDefaultAction(MessageBoxHandler::Accept);
}

void Installer::autoRejectMessageBoxes()
{
    MessageBoxHandler::instance()->setDefaultAction(MessageBoxHandler::Reject);
}

void Installer::setMessageBoxAutomaticAnswer(const QString &identifier, int button)
{
    MessageBoxHandler::instance()->setAutomaticAnswer(identifier,
        static_cast<QMessageBox::Button>(button));
}

void Installer::installSelectedComponents()
{
    d->setStatus(Installer::Running);
    // download

    double downloadPartProgressSize = double(1)/3;
    double componentsInstallPartProgressSize = double(2)/3;
    // get the list of packages we need to install in proper order and do it for the updater
    downloadNeededArchives(UpdaterMode, downloadPartProgressSize);
    // get the list of packages we need to install in proper order
    const QList<Component*> components = calculateComponentOrder(UpdaterMode);

    if (!isInstaller() && !QFileInfo(installerBinaryPath()).isWritable())
        gainAdminRights();

    d->stopProcessesForUpdates(components);
    int progressOperationCount = d->countProgressOperations(components);
    double progressOperationSize = componentsInstallPartProgressSize / progressOperationCount;

    //TODO: devide this in undo steps and install steps (2 "for" loops) for better progress calculation
    for (QList<Component*>::const_iterator it = components.begin(); it != components.end(); ++it) {
        if (d->statusCanceledOrFailed())
            throw Error(tr("Installation canceled by user"));
        Component* const currentComponent = *it;
        ProgressCoordninator::instance()->emitLabelAndDetailTextChanged(tr("\nRemoving the old "
            "version of: %1").arg(currentComponent->name()));
        if ((isUpdater() || isPackageManager()) && currentComponent->removeBeforeUpdate()) {
            // undo all operations done by this component upon installation
            for (int i = d->m_performedOperationsOld.count() - 1; i >= 0; --i) {
                KDUpdater::UpdateOperation* const op = d->m_performedOperationsOld[i];
                if (op->value(QLatin1String("component")) != currentComponent->name())
                    continue;
                const bool becameAdmin = !d->m_FSEngineClientHandler->isActive()
                    && op->value(QLatin1String("admin")).toBool() && gainAdminRights();
                InstallerPrivate::performOperationThreaded(op, InstallerPrivate::Undo);
                if (becameAdmin)
                    dropAdminRights();
                d->m_performedOperationsOld.remove(i);
                delete op;
            }
            d->m_app->packagesInfo()->removePackage(currentComponent->name());
            d->m_app->packagesInfo()->writeToDisk();
        }
        ProgressCoordninator::instance()->emitLabelAndDetailTextChanged(
            tr("\nInstalling the new version of: %1").arg(currentComponent->name()));
        installComponent(currentComponent, progressOperationSize);
        //commit all operations for this allready updated/installed component
        //so an undo during the installComponent function only undos the uncomplete installed one
        d->commitSessionOperations();
        d->m_needToWriteUninstaller = true;
    }

    d->setStatus(Installer::Success);
    ProgressCoordninator::instance()->emitLabelAndDetailTextChanged(tr("\nUpdate finished!"));
    emit updateFinished();
}

quint64 Installer::requiredDiskSpace() const
{
    quint64 result = 0;
    QList<Component*>::const_iterator it;
    const QList<Component*> availableComponents = components(true);
    for (it = availableComponents.begin(); it != availableComponents.end(); ++it) {
        Component* const comp = *it;
        if (!comp->isSelected())
            continue;
        if (comp->value(QLatin1String("PreviousState")) == QLatin1String("Installed"))
            continue;
        result += comp->value(QLatin1String("UncompressedSize")).toLongLong();
    }
    return result;
}

quint64 Installer::requiredTemporaryDiskSpace() const
{
    quint64 result = 0;
    QList<Component*>::const_iterator it;
    const QList<Component*> availableComponents = components(true);
    for (it = availableComponents.begin(); it != availableComponents.end(); ++it) {
        Component* const comp = *it;
        if (!comp->isSelected())
            continue;
        if (comp->value(QLatin1String("PreviousState")) == QLatin1String("Installed"))
            continue;
        result += comp->value(QLatin1String("CompressedSize")).toLongLong();
    }
    return result;
}

/*!
    Returns the will be downloaded archives count
*/
int Installer::downloadNeededArchives(RunModes runMode, double partProgressSize)
{
    Q_ASSERT(partProgressSize >= 0 && partProgressSize <= 1);

    QList<Component*> neededComponents;
    QList<QPair<QString, QString> > archivesToDownload;

    QList<Component*>::const_iterator it;
    const QList<Component*> availableComponents = components(true, runMode);
    for (it = availableComponents.begin(); it != availableComponents.end(); ++it) {
        Component* const comp = *it;
        if (!comp->isSelected(runMode))
            continue;
        if (comp->value(QLatin1String("PreviousState")) == QLatin1String("Installed")
            && runMode == AllMode) {
                continue;
        }
        appendComponentAndMissingDependencies(neededComponents, comp);
    }

    for (it = neededComponents.begin(); it != neededComponents.end(); ++it) {
        Component* const comp = *it;

        // collect all archives to be downloaded
        const QStringList toDownload = comp->downloadableArchives();
        for (QStringList::const_iterator it2 = toDownload.begin(); it2 != toDownload.end(); ++it2) {
            QString versionFreeString(*it2);
            archivesToDownload.push_back(qMakePair(QString::fromLatin1("installer://%1/%2")
                .arg(comp->name(), versionFreeString), QString::fromLatin1("%1/%2/%3")
                .arg(comp->repositoryUrl().toString(), comp->name(), versionFreeString)));
        }
    }

    if (archivesToDownload.isEmpty())
        return 0;

    ProgressCoordninator::instance()->emitLabelAndDetailTextChanged(tr("\nDownloading packages..."));

    // don't have it on the stack, since it keeps the temporary files
    DownloadArchivesJob* const archivesJob =
        new DownloadArchivesJob(d->m_installerSettings->publicKey(), this);
    archivesJob->setArchivesToDownload(archivesToDownload);
    archivesJob->setAutoDelete(false);
    connect(archivesJob, SIGNAL(outputTextChanged(QString)), ProgressCoordninator::instance(),
        SLOT(emitLabelAndDetailTextChanged(QString)));
    ProgressCoordninator::instance()->registerPartProgress(archivesJob,
        SIGNAL(progressChanged(double)), partProgressSize);
    connect(this, SIGNAL(installationInterrupted()), archivesJob, SLOT(cancel()));
    archivesJob->start();
    archivesJob->waitForFinished();

    if (archivesJob->error() == KDJob::Canceled)
        interrupt();
    else if (archivesJob->error() != DownloadArchivesJob::NoError)
        throw Error(archivesJob->errorString());
    if (d->statusCanceledOrFailed())
        throw Error(tr("Installation canceled by user"));

    return archivesToDownload.count();
}

QList<Component*> Installer::calculateComponentOrder(RunModes runMode) const
{
    return componentsToInstall(true, true, runMode);
}

void Installer::installComponent(Component* comp, double progressOperationSize)
{
    Q_ASSERT(progressOperationSize);

    d->setStatus(Installer::Running);
    const QList<KDUpdater::UpdateOperation*> operations = comp->operations();

    // show only component which are doing something, MinimumProgress is only for progress
    // calculation safeness
    if (operations.count() > 1
        || (operations.count() == 1 && operations.at(0)->name() != QLatin1String("MinimumProgress"))) {
        ProgressCoordninator::instance()->emitLabelAndDetailTextChanged(tr("\nInstalling component %1")
            .arg(comp->displayName()));
    }

    if (!comp->operationsCreatedSuccessfully())
        setCanceled();

    QList<KDUpdater::UpdateOperation*>::const_iterator op;
    for (op = operations.begin(); op != operations.end(); ++op) {
        if (d->statusCanceledOrFailed())
            throw Error(tr("Installation canceled by user"));

        KDUpdater::UpdateOperation* const operation = *op;
        d->connectOperationToInstaller(operation, progressOperationSize);

        // maybe this operations wants us to be admin...
        const bool becameAdmin = !d->m_FSEngineClientHandler->isActive()
            && operation->value(QLatin1String("admin")).toBool() && gainAdminRights();
        // perform the operation
        if (becameAdmin)
            verbose() << operation->name() << " as admin: " << becameAdmin << std::endl;

        // allow the operation to backup stuff before performing the operation
        InstallerPrivate::performOperationThreaded(operation, InstallerPrivate::Backup);

        bool ignoreError = false;
        bool ok = InstallerPrivate::performOperationThreaded(operation);
        while (!ok && !ignoreError && status() != Installer::Canceled) {
            verbose() << QString(QLatin1String("operation '%1' with arguments: '%2' failed: %3"))
                .arg(operation->name(), operation->arguments().join(QLatin1String("; ")),
                operation->errorString()) << std::endl;;
            const QMessageBox::StandardButton button =
                MessageBoxHandler::warning(MessageBoxHandler::currentBestSuitParent(),
                QLatin1String("installationErrorWithRetry"), tr("Installer Error"),
                tr("Error during installation process:\n%1").arg(operation->errorString()),
                QMessageBox::Retry | QMessageBox::Ignore | QMessageBox::Cancel, QMessageBox::Retry);

            if (button == QMessageBox::Retry)
                ok = InstallerPrivate::performOperationThreaded(operation);
            else if (button == QMessageBox::Ignore)
                ignoreError = true;
            else if (button == QMessageBox::Cancel)
                interrupt();
        }

        if (ok || operation->error() > KDUpdater::UpdateOperation::InvalidArguments) {
            // remember that the operation was performed what allows us to undo it if a
            // following operaton fails or if this operation failed but still needs
            // an undo call to cleanup.
            d->addPerformed(operation);
            operation->setValue(QLatin1String("component"), comp->name());
        }

        if (becameAdmin)
            dropAdminRights();

        if (!ok && !ignoreError)
            throw Error(operation->errorString());

        if (comp->value(QLatin1String("Important"), QLatin1String("false")) == QLatin1String("true"))
            d->m_forceRestart = true;
    }

    d->registerPathesForUninstallation(comp->pathesForUninstallation(), comp->name());

    if (!comp->stopProcessForUpdateRequests().isEmpty()) {
        KDUpdater::UpdateOperation *stopProcessForUpdatesOp =
            KDUpdater::UpdateOperationFactory::instance().create(QLatin1String("FakeStopProcessForUpdate"));
        const QStringList arguments(comp->stopProcessForUpdateRequests().join(QLatin1String(",")));
        stopProcessForUpdatesOp->setArguments(arguments);
        d->addPerformed(stopProcessForUpdatesOp);
        stopProcessForUpdatesOp->setValue(QLatin1String("component"), comp->name());
    }

    // now mark the component as installed
    KDUpdater::PackagesInfo* const packages = d->m_app->packagesInfo();
    const bool forcedInstall =
        comp->value(QLatin1String("ForcedInstallation")).toLower() == QLatin1String("true")
        ? true : false;
    const bool virtualComponent =
        comp->value(QLatin1String ("Virtual")).toLower() == QLatin1String("true") ? true : false;
    packages->installPackage(comp->value(QLatin1String("Name")),
        comp->value(QLatin1String("Version")), comp->value(QLatin1String("DisplayName")),
        comp->value(QLatin1String("Description")), comp->dependencies(), forcedInstall,
        virtualComponent, comp->value(QLatin1String ("UncompressedSize")).toULongLong());

    comp->setValue(QLatin1String("CurrentState"), QLatin1String("Installed"));
    comp->markAsPerformedInstallation();
}

/*!
    If a component marked as important was installed during update
    process true is returned.
*/
bool Installer::needsRestart() const
{
    return d->m_forceRestart;
}

void Installer::rollBackInstallation()
{
    emit titleMessageChanged(tr("Cancelling the Installer"));
    // rolling back

    //this unregisters all operation progressChanged connects
    ProgressCoordninator::instance()->setUndoMode();
    int progressOperationCount =
        d->countProgressOperations(d->m_performedOperationsCurrentSession.toList());
    double progressOperationSize = double(1) / progressOperationCount;

    //reregister all the undooperations with the new size to the ProgressCoordninator
    foreach (KDUpdater::UpdateOperation* const operation, d->m_performedOperationsCurrentSession) {
        QObject* const operationObject = dynamic_cast<QObject*>(operation);
        if (operationObject != 0) {
            const QMetaObject* const mo = operationObject->metaObject();
            if (mo->indexOfSignal(QMetaObject::normalizedSignature("progressChanged(double)")) > -1) {
                ProgressCoordninator::instance()->registerPartProgress(operationObject,
                    SIGNAL(progressChanged(double)), progressOperationSize);
            }
        }
    }

    while (!d->m_performedOperationsCurrentSession.isEmpty()) {
        try {
            KDUpdater::UpdateOperation* const operation = d->m_performedOperationsCurrentSession.last();
            d->m_performedOperationsCurrentSession.pop_back();

            const bool becameAdmin = !d->m_FSEngineClientHandler->isActive()
                && operation->value(QLatin1String("admin")).toBool() && gainAdminRights();
            InstallerPrivate::performOperationThreaded(operation, InstallerPrivate::Undo);
            if (becameAdmin)
                dropAdminRights();
        } catch(const Error &e) {
            MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(),
                QLatin1String("ElevationError"), tr("Authentication Error"), tr("Some components "
                "could not be removed completely because admin rights could not be acquired: %1.")
                .arg(e.message()));
        }
    }
}

bool Installer::isFileExtensionRegistered(const QString& extension) const
{
    QSettings settings(QLatin1String("HKEY_CLASSES_ROOT"), QSettings::NativeFormat);
    return settings.value(QString::fromLatin1(".%1/Default").arg(extension)).isValid();
}


// -- QInstaller

Installer::Installer(qint64 magicmaker,
        const QVector<KDUpdater::UpdateOperation*>& performedOperations)
    : d(new InstallerPrivate(this, magicmaker, performedOperations))
{
    qRegisterMetaType< QInstaller::Installer::Status >("QInstaller::Installer::Status");
    qRegisterMetaType< QInstaller::Installer::WizardPage >("QInstaller::Installer::WizardPage");

    d->initialize();
}

Installer::~Installer()
{
    if (!isUninstaller() && !(isInstaller() && status() == Installer::Canceled)) {
        QDir targetDir(value(QLatin1String("TargetDir")));
        QString logFileName = targetDir.absoluteFilePath(value(QLatin1String("LogFileName"),
            QLatin1String("InstallationLog.txt")));
        QInstaller::VerboseWriter::instance()->setOutputStream(logFileName);
    }

    d->m_FSEngineClientHandler->setActive(false);
    delete d;
}

bool Installer::fetchAllPackages()
{
    if (isUninstaller() || isUpdater())
        return false;

    const QInstaller::InstallerSettings &settings = *d->m_installerSettings;
    GetRepositoriesMetaInfoJob metaInfoJob(settings.publicKey(), true);
    if (!isOfflineOnly())
        metaInfoJob.setRepositories(settings.repositories());

    connect(&metaInfoJob, SIGNAL(infoMessage(KDJob*, QString)), this,
        SIGNAL(allComponentsInfoMessage(KDJob*,QString)));
    connect(this, SIGNAL(cancelAllComponentsInfoJob()), &metaInfoJob, SLOT(doCancel()),
        Qt::QueuedConnection);

    try {
        metaInfoJob.setAutoDelete(false);
        metaInfoJob.start();
        metaInfoJob.waitForFinished();
    } catch (Error &error) {
        verbose() << tr("Could not retrieve components: %1").arg(error.message()) << std::endl;
        return false;
    }

    if (metaInfoJob.isCanceled() || metaInfoJob.error() != KDJob::NoError) {
        verbose() << tr("Could not retrieve components: %1").arg(metaInfoJob.errorString()) << std::endl;
        return false;
    }

    KDUpdater::Application &updaterApp = *d->m_app;
    const QString &appName = settings.applicationName();
    const QStringList tempDirs = metaInfoJob.temporaryDirectories();
    foreach (const QString &tmpDir, tempDirs) {
        if (tmpDir.isEmpty())
            continue;

        const QString updatesXmlPath = tmpDir + QLatin1String("/Updates.xml");
        QFile updatesFile(updatesXmlPath);
        try {
            openForRead(&updatesFile, updatesFile.fileName());
        } catch(const Error &e) {
            verbose() << tr("Error opening Updates.xml: ") << e.message() << std::endl;
            return false;
        }

        int line = 0;
        int column = 0;
        QString error;
        QDomDocument doc;
        if (!doc.setContent(&updatesFile, &error, &line, &column)) {
            verbose() << tr("Parse error in File %4 : %1 at line %2 col %3").arg(error,
                QString::number(line), QString::number(column), updatesFile.fileName()) << std::endl;
            return false;
        }

        const QDomNode checksum = doc.documentElement().firstChildElement(QLatin1String("Checksum"));
        if (!checksum.isNull()) {
            const QDomElement checksumElem = checksum.toElement();
            setTestChecksum(checksumElem.text().toLower() == QLatin1String("true"));
        }

        updaterApp.addUpdateSource(appName, appName, QString(), QUrl::fromLocalFile(tmpDir), 1);
    }

    if (updaterApp.updateSourcesInfo()->updateSourceInfoCount() == 0) {
        verbose() << tr("Could not find any component source infos.") << std::endl;
        return false;
    }
    updaterApp.updateSourcesInfo()->setModified(false);

    KDUpdater::UpdateFinder updateFinder(&updaterApp);
    updateFinder.setUpdateType(KDUpdater::PackageUpdate | KDUpdater::NewPackage);
    updateFinder.run();

    const QList<KDUpdater::Update*> &packages = updateFinder.updates();
    if (packages.isEmpty()) {
        verbose() << tr("Could not retrieve components: %1").arg(updateFinder.errorString());
        return false;
    }

    emit startAllComponentsReset();

    d->m_componentHash.clear();
    qDeleteAll(d->m_rootComponents);
    d->m_rootComponents.clear();

    QMap<QInstaller::Component*, QString> scripts;
    QMap<QString, QInstaller::Component*> components;

    KDUpdater::PackagesInfo &packagesInfo = *updaterApp.packagesInfo();
    if (isPackageManager()) {
        if (!setAndParseLocalComponentsFile(packagesInfo)) {
            verbose() << tr("Could not parse local components xml file: %1")
                .arg(d->localComponentsXmlPath());
            return false;
        }
        packagesInfo.setApplicationName(appName);
        packagesInfo.setApplicationVersion(settings.applicationVersion());
    }

    foreach (KDUpdater::Update *package, packages) {
        const QString name = package->data(QLatin1String("Name")).toString();
        if (components.contains(name)) {
            qCritical("Could not register component! Component with identifier %s already registered.",
                qPrintable(name));
            continue;
        }

        QScopedPointer<QInstaller::Component> component(new QInstaller::Component(package, this));

        QString state = QLatin1String("Uninstalled");
        const int indexOfPackage = packagesInfo.findPackageInfo(name);
        if (indexOfPackage > -1) {
            state = QLatin1String("Installed");
            component->setValue(QLatin1String("InstalledVersion"),
                packagesInfo.packageInfo(indexOfPackage).version);
            component->setSelected(true, AllMode, Component::InitializeComponentTreeSelectMode);
        }
        component->setValue(QLatin1String("CurrentState"), state);
        component->setValue(QLatin1String("PreviousState"), state);

        const QString localPath = QInstaller::pathFromUrl(package->sourceInfo().url);
        const Repository repo = metaInfoJob.repositoryForTemporaryDirectory(localPath);
        component->setRepositoryUrl(repo.url());

        static QLatin1String newComponent("NewComponent");
        component->setValue(newComponent, package->data(newComponent).toString());

        static const QLatin1String important("Important");
        component->setValue(important, package->data(important).toString());

        static const QLatin1String forcedInstallation("ForcedInstallation");
        component->setValue(forcedInstallation, package->data(forcedInstallation).toString());

        static const QLatin1String updateText("UpdateText");
        component->setValue(updateText, package->data(updateText).toString());

        static const QLatin1String requiresAdminRights("RequiresAdminRights");
        component->setValue(requiresAdminRights, package->data(requiresAdminRights).toString());

        const QStringList uis = package->data(QLatin1String("UserInterfaces")).toString()
            .split(QString::fromLatin1(","), QString::SkipEmptyParts);
        if (!uis.isEmpty()) {
            if (uis.contains(QLatin1String("installationkind.ui"))) {
                int index = uis.indexOf(QLatin1String("installationkind.ui"));
                index = index;
            }
            component->loadUserInterfaces(QDir(QString::fromLatin1("%1/%2").arg(localPath, name)), uis);
        }
        const QStringList qms = package->data(QLatin1String("Translations")).toString()
            .split(QString::fromLatin1(","), QString::SkipEmptyParts);
        if (!qms.isEmpty())
            component->loadTranslations(QDir(QString::fromLatin1("%1/%2").arg(localPath, name)), qms);

        QHash<QString, QVariant> licenseHash = package->data(QLatin1String("Licenses")).toHash();
        if (!licenseHash.isEmpty())
            component->loadLicenses(QString::fromLatin1("%1/%2/").arg(localPath, name), licenseHash);

        const QString script = package->data(QLatin1String("Script")).toString();
        if (!script.isEmpty())
            scripts.insert(component.data(), QString::fromLatin1("%1/%2/%3").arg(localPath, name, script));

        components.insert(name, component.take());
    }

    // now append all components to their respective parents
    QMap<QString, QInstaller::Component*>::const_iterator it;
    for (it = components.begin(); it != components.end(); ++it) {
        QString id = it.key();
        QInstaller::Component *component = it.value();
        while (!id.isEmpty() && component->parentComponent() == 0) {
            id = id.section(QLatin1Char('.'), 0, -2);
            if (components.contains(id))
                components[id]->appendComponent(component);
        }
    }

    // append all components w/o parent to the direct list
    foreach (QInstaller::Component *component, components) {
        if (component->parentComponent() == 0)
            appendComponent(component);
    }

    // after everything is set up, load the scripts
    QList<QInstaller::Component*> keys = scripts.keys();
    foreach (QInstaller::Component *component, keys) {
        const QString &script = scripts.value(component);
        verbose() << "Loading script for component " << component->name() << " (" << script << ")"
            << std::endl;
        component->loadComponentScript(script);
    }

    emit componentsAdded(d->m_rootComponents);
    emit finishAllComponentsReset();

    return true;
}

bool Installer::fetchUpdaterPackages()
{
    if (!isUpdater())
        return false;

    const QInstaller::InstallerSettings &settings = *d->m_installerSettings;
    GetRepositoriesMetaInfoJob metaInfoJob(settings.publicKey(), true);
    metaInfoJob.setRepositories(settings.repositories());

    connect(&metaInfoJob, SIGNAL(infoMessage(KDJob*, QString)), this,
        SIGNAL(updaterInfoMessage(KDJob*, QString)));
    connect(this, SIGNAL(cancelUpdaterInfoJob()), &metaInfoJob, SLOT(doCancel()),
        Qt::QueuedConnection);

    try {
        metaInfoJob.setAutoDelete(false);
        metaInfoJob.start();
        metaInfoJob.waitForFinished();
    } catch (Error &error) {
        verbose() << tr("Could not retrieve updates: %1").arg(error.message()) << std::endl;
        return false;
    }

    if (metaInfoJob.isCanceled() || metaInfoJob.error() != KDJob::NoError) {
        verbose() << tr("Could not retrieve updates: %1").arg(metaInfoJob.errorString()) << std::endl;
        return false;
    }

    KDUpdater::Application &updaterApp = *d->m_app;
    const QString &appName = settings.applicationName();
    const QStringList tempDirs = metaInfoJob.temporaryDirectories();
    foreach (const QString &tmpDir, tempDirs) {
        if (tmpDir.isEmpty())
            continue;

        const QString updatesXmlPath = tmpDir + QLatin1String("/Updates.xml");
        QFile updatesFile(updatesXmlPath);
        try {
            openForRead(&updatesFile, updatesFile.fileName());
        } catch(const Error &e) {
            verbose() << tr("Error opening Updates.xml: ") << e.message() << std::endl;
            return false;
        }

        int line = 0;
        int column = 0;
        QString error;
        QDomDocument doc;
        if (!doc.setContent(&updatesFile, &error, &line, &column)) {
            verbose() << tr("Parse error in File %4 : %1 at line %2 col %3").arg(error,
                QString::number(line), QString::number(column), updatesFile.fileName()) << std::endl;
            return false;
        }

        const QDomNode checksum = doc.documentElement().firstChildElement(QLatin1String("Checksum"));
        if (!checksum.isNull()) {
            const QDomElement checksumElem = checksum.toElement();
            setTestChecksum(checksumElem.text().toLower() == QLatin1String("true"));
        }

        updaterApp.addUpdateSource(appName, appName, QString(), QUrl::fromLocalFile(tmpDir), 1);
    }

    if (updaterApp.updateSourcesInfo()->updateSourceInfoCount() == 0) {
        verbose() << tr("Could not find any update source infos.") << std::endl;
        return false;
    }
    updaterApp.updateSourcesInfo()->setModified(false);

    KDUpdater::UpdateFinder updateFinder(&updaterApp);
    updateFinder.setUpdateType(KDUpdater::PackageUpdate | KDUpdater::NewPackage);
    updateFinder.run();

    const QList<KDUpdater::Update*> &updates = updateFinder.updates();
    if (updates.isEmpty()) {
        verbose() << tr("Could not retrieve updates: %1").arg(updateFinder.errorString());
        return false;
    }

    emit startUpdaterComponentsReset();

    qDeleteAll(d->m_updaterComponents);
    d->m_updaterComponents.clear();

    bool importantUpdates = false;
    QMap<QInstaller::Component*, QString> scripts;
    QMap<QString, QInstaller::Component*> components;

    KDUpdater::PackagesInfo &packagesInfo = *updaterApp.packagesInfo();
    if (!setAndParseLocalComponentsFile(packagesInfo)) {
        verbose() << tr("Could not parse local components xml file: %1")
            .arg(d->localComponentsXmlPath());
        return false;
    }
    packagesInfo.setApplicationName(appName);
    packagesInfo.setApplicationVersion(settings.applicationVersion());

    foreach (KDUpdater::Update * const update, updates) {
        const QString isNew = update->data(QLatin1String("NewComponent")).toString();
        if (isNew.toLower() != QLatin1String("true"))
            continue;

        const QString name = update->data(QLatin1String("Name")).toString();
        if (components.contains(name)) {
            qCritical("Could not register component! Component with identifier %s already registered.",
                qPrintable(name));
            continue;
        }

        QString state = QLatin1String("Uninstalled");
        const int indexOfPackage = packagesInfo.findPackageInfo(name);
        if (indexOfPackage > -1) {
            state = QLatin1String("Installed");

            const QString updateVersion = update->data(QLatin1String("Version")).toString();
            const QString pkgVersion = packagesInfo.packageInfo(indexOfPackage).version;
            if (KDUpdater::compareVersion(updateVersion, pkgVersion) <= 0)
                continue;

            // It is quite possible that we may have already installed the update. Lets check the last
            // update date of the package and the release date of the update. This way we can compare and
            // figure out if the update has been installed or not.
            const QDate updateDate = update->data(QLatin1String("ReleaseDate")).toDate();
            const QDate pkgDate = packagesInfo.packageInfo(indexOfPackage).lastUpdateDate;
            if (pkgDate > updateDate)
                continue;
        }

        QScopedPointer<QInstaller::Component> component(new QInstaller::Component(update, this));
        component->setValue(QLatin1String("NewComponent"), isNew);
        component->setValue(QLatin1String("CurrentState"), state);
        component->setValue(QLatin1String("PreviousState"), state);

        if (indexOfPackage > -1) {
            component->setValue(QLatin1String("InstalledVersion"),
                packagesInfo.packageInfo(indexOfPackage).version);
        }

        const QString localPath = QInstaller::pathFromUrl(update->sourceInfo().url);
        const Repository repo = metaInfoJob.repositoryForTemporaryDirectory(localPath);
        component->setRepositoryUrl(repo.url());

        static const QLatin1String important("Important");
        component->setValue(important, update->data(important).toString());
        importantUpdates |= update->data(important).toString().toLower() == QLatin1String("true");

        static const QLatin1String forcedInstallation("ForcedInstallation");
        component->setValue(forcedInstallation, update->data(forcedInstallation).toString());

        static const QLatin1String updateText("UpdateText");
        component->setValue(updateText, update->data(updateText).toString());

        static const QLatin1String requiresAdminRights("RequiresAdminRights");
        component->setValue(requiresAdminRights, update->data(requiresAdminRights).toString());

        const QStringList uis = update->data(QLatin1String("UserInterfaces")).toString()
            .split(QString::fromLatin1(","), QString::SkipEmptyParts);
        if (!uis.isEmpty())
            component->loadUserInterfaces(QDir(QString::fromLatin1("%1/%2").arg(localPath, name)), uis);

        const QStringList qms = update->data(QLatin1String("Translations")).toString()
            .split(QString::fromLatin1(","), QString::SkipEmptyParts);
        if (!qms.isEmpty())
            component->loadTranslations(QDir(QString::fromLatin1("%1/%2").arg(localPath, name)), qms);

        QHash<QString, QVariant> licenseHash = update->data(QLatin1String("Licenses")).toHash();
        if (!licenseHash.isEmpty())
            component->loadLicenses(QString::fromLatin1("%1/%2/").arg(localPath, name), licenseHash);

        const QString script = update->data(QLatin1String("Script")).toString();
        if (!script.isEmpty())
            scripts.insert(component.data(), QString::fromLatin1("%1/%2/%3").arg(localPath, name, script));

        components.insert(name, component.take());
    }

    // remove all unimportant components
    QList<QInstaller::Component*> updaterComponents = components.values();
    if (importantUpdates) {
        for (int i = updaterComponents.count() - 1; i >= 0; --i) {
            const QString important = updaterComponents.at(i)->value(QLatin1String("Important"));
            if (important.toLower() == QLatin1String ("false") || important.isEmpty()) {
                delete updaterComponents[i];
                updaterComponents.removeAt(i);
            }
        }
    }

    // append all components w/o parent to the direct list
    foreach (QInstaller::Component *component, updaterComponents) {
        d->m_updaterComponents.append(component);
        emit componentAdded(component);
    }

    // after everything is set up, load the scripts
    foreach (QInstaller::Component *component, d->m_updaterComponents) {
        const QString &script = scripts.value(component);
        if (script.isEmpty()) {
            verbose() << "Loading script for component " << component->name() << " (" << script << ")"
                << std::endl;
            component->loadComponentScript(script);
        }
    }

    emit updaterComponentsAdded(d->m_updaterComponents);
    emit finishUpdaterComponentsReset();

    return true;
}

/*!
    Adds the widget with objectName() \a name registered by \a component as a new page
    into the installer's GUI wizard. The widget is added before \a page.
    \a page has to be a value of \ref QInstaller::Installer::WizardPage "WizardPage".
*/
bool Installer::addWizardPage(Component* component, const QString &name, int page)
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
bool Installer::removeWizardPage(Component *component, const QString &name)
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
bool Installer::setDefaultPageVisible(int page, bool visible)
{
    emit wizardPageVisibilityChangeRequested(visible, page);
    return true;
}

/*!
    Adds the widget with objectName() \a name registered by \a component as an GUI element
    into the installer's GUI wizard. The widget is added on \a page.
    \a page has to be a value of \ref QInstaller::Installer::WizardPage "WizardPage".
*/
bool Installer::addWizardPageItem(Component *component, const QString &name, int page)
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
bool Installer::removeWizardPageItem(Component *component, const QString &name)
{
    if (QWidget* const widget = component->userInterface(name)) {
        emit wizardWidgetRemovalRequested(widget);
        return true;
    }
    return false;
}

void Installer::setRemoteRepositories(const QList<Repository> &repositories)
{
    GetRepositoriesMetaInfoJob metaInfoJob(d->m_installerSettings->publicKey(),
        (isPackageManager() || isUpdater()));
    metaInfoJob.setRepositories(repositories);

    // start...
    metaInfoJob.setAutoDelete(false);
    metaInfoJob.start();
    metaInfoJob.waitForFinished();

    if (metaInfoJob.isCanceled())
        return;

    if (metaInfoJob.error() != KDJob::NoError)
        throw Error(tr("Could not retrieve updates: %1").arg(metaInfoJob.errorString()));

    KDUpdater::Application &updaterApp = *d->m_app;

    const QStringList tempDirs = metaInfoJob.temporaryDirectories();
    d->m_tempDirDeleter->add(tempDirs);
    foreach (const QString &tmpDir, tempDirs) {
        if (tmpDir.isEmpty())
            continue;
        const QString &applicationName = d->m_installerSettings->applicationName();
        updaterApp.addUpdateSource(applicationName, applicationName, QString(),
            QUrl::fromLocalFile(tmpDir), 1);
    }

    if (updaterApp.updateSourcesInfo()->updateSourceInfoCount() == 0)
        throw Error(tr("Could not reach any update sources."));

    if (!setAndParseLocalComponentsFile(*updaterApp.packagesInfo()))
        return;

    // The changes done above by adding update source don't count as modification that
    // need to be saved cause they will be re-set on the next start anyway. This does
    // prevent creating of xml files not needed atm. We can still set the modified
    // state later once real things changed that we like to restore at the next startup.
    updaterApp.updateSourcesInfo()->setModified(false);

    // create the packages info
    KDUpdater::UpdateFinder* const updateFinder = new KDUpdater::UpdateFinder(&updaterApp);
    connect(updateFinder, SIGNAL(error(int, QString)), &updaterApp, SLOT(printError(int, QString)));
    updateFinder->setUpdateType(KDUpdater::PackageUpdate | KDUpdater::NewPackage);
    updateFinder->run();

    // now create installable componets
    createComponentsV2(updateFinder->updates(), metaInfoJob);
}

/*!
    Sets additional repository for this instance of the installer or updater
    Will be removed after invoking it again
*/
void Installer::setTemporaryRepositories(const QList<Repository> &repositories, bool replace)
{
    d->m_installerSettings->setTemporaryRepositories(repositories, replace);
}

/*!
    checks if the downloader should try to download sha1 checksums for archives
*/
bool Installer::testChecksum() const
{
    return d->m_testChecksum;
}

/*!
    Defines if the downloader should try to download sha1 checksums for archives
*/
void Installer::setTestChecksum(bool test)
{
    d->m_testChecksum = test;
}

/*!
    Creates components from the \a updates found by KDUpdater.
*/
void Installer::createComponentsV2(const QList<KDUpdater::Update*> &updates,
    const GetRepositoriesMetaInfoJob &metaInfoJob)
{
    verbose() << "entered create components V2 in installer" << std::endl;

    emit componentsAboutToBeCleared();

    qDeleteAll(d->m_componentHash);
    d->m_rootComponents.clear();
    d->m_updaterComponents.clear();
    d->m_componentHash.clear();
    QStringList importantUpdates;

    KDUpdater::Application &updaterApp = *d->m_app;
    KDUpdater::PackagesInfo &packagesInfo = *updaterApp.packagesInfo();

    if (isUninstaller() || isPackageManager()) {
        //reads all installed components from components.xml
        if (!setAndParseLocalComponentsFile(packagesInfo))
            return;
        packagesInfo.setApplicationName(d->m_installerSettings->applicationName());
        packagesInfo.setApplicationVersion(d->m_installerSettings->applicationVersion());
    }

    QHash<QString, KDUpdater::PackageInfo> alreadyInstalledPackagesHash;
    foreach (const KDUpdater::PackageInfo &info, packagesInfo.packageInfos()) {
        alreadyInstalledPackagesHash.insert(info.name, info);
    }

    QStringList globalUnNeededList;
    QList<Component*> componentsToSelectInPackagemanager;
    foreach (KDUpdater::Update * const update, updates) {
        const QString name = update->data(QLatin1String("Name")).toString();
        Q_ASSERT(!name.isEmpty());
        QInstaller::Component* component(new QInstaller::Component(this));
        component->loadDataFromUpdate(update);

        QString installedVersion;

        //to find the installed version number we need to check for all possible names
        QStringList possibleInstalledNameList;
        if (!update->data(QLatin1String("Replaces")).toString().isEmpty()) {
            QString replacesAsString = update->data(QLatin1String("Replaces")).toString();
            QStringList replaceList(replacesAsString.split(QLatin1String(","),
                QString::SkipEmptyParts));
            possibleInstalledNameList.append(replaceList);
            globalUnNeededList.append(replaceList);
        }
        if (alreadyInstalledPackagesHash.contains(name)) {
            possibleInstalledNameList.append(name);
        }
        foreach(const QString &possibleName, possibleInstalledNameList) {
            if (alreadyInstalledPackagesHash.contains(possibleName)) {
                if (!installedVersion.isEmpty()) {
                    qCritical("If installed version is not empty there are more then one package " \
                              "which would be replaced and this is completly wrong");
                    Q_ASSERT(false);
                }
                installedVersion = alreadyInstalledPackagesHash.value(possibleName).version;
            }
            //if we have newer data in the repository we don't need the data from harddisk
            alreadyInstalledPackagesHash.remove(name);
        }

        const Repository currentUsedRepository = metaInfoJob.repositoryForTemporaryDirectory(
                    QInstaller::pathFromUrl(update->sourceInfo().url));
        component->setRepositoryUrl(currentUsedRepository.url());

        // the package manager should preselect the currently installed packages
        if (isPackageManager()) {

            //reset PreviousState and CurrentState to have a chance to get a diff, after the user
            //changed something
            if (installedVersion.isEmpty()) {
                setValue(QLatin1String("PreviousState"), QLatin1String("Uninstalled"));
                setValue(QLatin1String("CurrentState"), QLatin1String("Uninstalled"));
                if (component->value(QLatin1String("NewComponent")) == QLatin1String("true")) {
                    d->m_updaterComponents.append(component);
                }
            } else {
                setValue(QLatin1String("PreviousState"), QLatin1String("Installed"));
                setValue(QLatin1String("CurrentState"), QLatin1String("Installed"));
                const QString updateVersion = update->data(QLatin1String("Version")).toString();
                //check if it is an update
                if (KDUpdater::compareVersion(installedVersion, updateVersion) >= 0) {
                    d->m_updaterComponents.append(component);
                }
                if (update->data(QLatin1String("Important"), false).toBool())
                    importantUpdates.append(name);
                componentsToSelectInPackagemanager.append(component);
            }
        }

        d->m_componentHash[name] = component;
    } //foreach (KDUpdater::Update * const update, updates)

    if (!importantUpdates.isEmpty()) {
        //ups, there was an important update - then we only want to show these
        d->m_updaterComponents.clear();
        //maybe in future we have more then one
        foreach (const QString &importantUpdate, importantUpdates) {
            d->m_updaterComponents.append(d->m_componentHash[importantUpdate]);
        }
    }
    //globalUnNeededList

    // now append all components to their respective parents
    QHash<QString, QInstaller::Component*>::iterator it;
    for (it = d->m_componentHash.begin(); it != d->m_componentHash.end(); ++it) {
        QInstaller::Component* const currentComponent = *it;
        QString id = it.key();
        while (!id.isEmpty() && currentComponent->parentComponent() == 0) {
            id = id.section(QChar::fromLatin1('.'), 0, -2);
            //is there a component which is called like a parent
            if (d->m_componentHash.contains(id))
                d->m_componentHash[id]->appendComponent(currentComponent);
        }
    }

//    //foreach component instead of every update or so
//    const QString script = update->data(QLatin1String("Script")).toString();
//    if (!script.isEmpty()) {
//        scripts.insert(component.data(), QString::fromLatin1("%1/%2/%3").arg(
//                QInstaller::pathFromUrl(update->sourceInfo().url), newComponentName, script));
//    }

    // select all components in the updater model
    foreach (QInstaller::Component* const i, d->m_updaterComponents)
        i->setSelected(true, UpdaterMode, Component::InitializeComponentTreeSelectMode);

    // select all components in the package manager model
    foreach (QInstaller::Component* const i, componentsToSelectInPackagemanager)
        i->setSelected(true, AllMode, Component::InitializeComponentTreeSelectMode);

    d->m_rootComponents = d->m_componentHash.values();
    //signals for the qinstallermodel
    emit componentsAdded(d->m_rootComponents);
    emit updaterComponentsAdded(d->m_updaterComponents);
}

/*!
    Creates components from the \a updates found by KDUpdater.
*/
void Installer::createComponents(const QList<KDUpdater::Update*> &updates,
    const GetRepositoriesMetaInfoJob &metaInfoJob)
{
    verbose() << "entered create components in installer" << std::endl;

    emit componentsAboutToBeCleared();

    qDeleteAll(d->m_rootComponents);
    qDeleteAll(d->m_updaterComponents);
    d->m_rootComponents.clear();
    d->m_updaterComponents.clear();
    d->m_packageManagerComponents.clear();
    d->m_componentHash.clear();

    KDUpdater::Application &updaterApp = *d->m_app;
    KDUpdater::PackagesInfo &packagesInfo = *updaterApp.packagesInfo();

    if (isUninstaller() || isPackageManager() || isUpdater()) {
        if (!setAndParseLocalComponentsFile(packagesInfo))
            return;
        packagesInfo.setApplicationName(d->m_installerSettings->applicationName());
        packagesInfo.setApplicationVersion(d->m_installerSettings->applicationVersion());
    }

    bool containsImportantUpdates = false;
    QMap<QInstaller::Component*, QString> scripts;
    QMap<QString, QInstaller::Component*> components;
    QList<Component*> componentsToSelectUpdater, componentsToSelectInstaller;

    //why do we need to add the components if there was a metaInfoJob.error()?
    //will this happen if there is no internet connection?
    //ok usually there should be the installed and the online tree
    if (metaInfoJob.error() == KDJob::UserDefinedError) {
        foreach (const KDUpdater::PackageInfo &info, packagesInfo.packageInfos()) {
            QScopedPointer<QInstaller::Component> component(new QInstaller::Component(this));

            if (components.contains(info.name)) {
                qCritical("Could not register component! Component with identifier %s already "
                    "registered", qPrintable(info.name));
            } else {
                component->loadDataFromPackageInfo(info);
                components.insert(info.name, component.take());
            }
        }
    } else {
        foreach (KDUpdater::Update * const update, updates) {
            const QString newComponentName = update->data(QLatin1String("Name")).toString();
            const int indexOfPackage = packagesInfo.findPackageInfo(newComponentName);

            QScopedPointer<QInstaller::Component> component(new QInstaller::Component(this));
            if (indexOfPackage > -1) {
                component->setValue(QLatin1String("InstalledVersion"),
                    packagesInfo.packageInfo(indexOfPackage).version);
            }

            component->loadDataFromUpdate(update);
            const Repository currentUsedRepository = metaInfoJob.repositoryForTemporaryDirectory(
                QInstaller::pathFromUrl(update->sourceInfo().url));
            component->setRepositoryUrl(currentUsedRepository.url());

            bool isUpdate = true;
            // the package manager should preselect the currently installed packages
            if (isPackageManager()) {
                const bool selected = indexOfPackage > -1;

                component->updateState(selected);

                if (selected)
                    componentsToSelectInstaller.append(component.data());

                const QString pkgVersion = packagesInfo.packageInfo(indexOfPackage).version;
                const QString updateVersion = update->data(QLatin1String("Version")).toString();
                if (KDUpdater::compareVersion(updateVersion, pkgVersion) <= 0)
                    isUpdate = false;

                // It is quite possible that we may have already installed the update.
                // Lets check the last update date of the package and the release date
                // of the update. This way we can compare and figure out if the update
                // has been installed or not.
                const QDate updateDate = update->data(QLatin1String("ReleaseDate")).toDate();
                const QDate pkgDate = packagesInfo.packageInfo(indexOfPackage).lastUpdateDate;
                if (pkgDate > updateDate)
                    isUpdate = false;
            }

            if (!components.contains(newComponentName)) {
                const QString script = update->data(QLatin1String("Script")).toString();
                if (!script.isEmpty()) {
                    scripts.insert(component.data(), QString::fromLatin1("%1/%2/%3").arg(
                        QInstaller::pathFromUrl(update->sourceInfo().url), newComponentName, script));
                }

                Component *tmpComponent = component.data();
                const bool isInstalled = tmpComponent->value(QLatin1String("PreviousState"))
                    == QLatin1String("Installed");
                const bool isNewComponent = tmpComponent->value(QLatin1String("NewComponent"))
                    == QLatin1String("true") ? true : false;
                const bool newPackageForUpdater = !isInstalled && isNewComponent;
                isUpdate = isUpdate && isInstalled;

                if (newPackageForUpdater) {
                    d->m_updaterComponents.push_back(component.take());
                    d->m_componentHash[newComponentName] = tmpComponent;
                } else {
                    components.insert(newComponentName, component.take());
                }

                if ((isPackageManager() || isUpdater()) && (isUpdate || newPackageForUpdater)) {
                    if (update->data(QLatin1String("Important")).toBool())
                        containsImportantUpdates = true;
                    componentsToSelectUpdater.append(tmpComponent);
                    d->m_packageManagerComponents.push_back(tmpComponent);
                }
            } else {
                qCritical("Could not register component! Component with identifier %s already "
                    "registered", qPrintable(newComponentName));
            }
        }
    }

    if (containsImportantUpdates) {
        foreach (QInstaller::Component *c, d->m_packageManagerComponents) {
            if (c->value(QLatin1String("Important")).toLower() == QLatin1String ("false")
                || c->value(QLatin1String("Important")).isEmpty()) {
                    if (isPackageManager())
                        d->m_packageManagerComponents.removeAll(c);
                    componentsToSelectUpdater.removeAll(c);
            }
        }
    }

    // now append all components to their respective parents
    QMap<QString, QInstaller::Component*>::const_iterator it;
    for (it = components.begin(); !d->m_linearComponentList && it != components.end(); ++it) {
        QInstaller::Component* const comp = *it;
        QString id = it.key();
        while (!id.isEmpty() && comp->parentComponent() == 0) {
            id = id.section(QChar::fromLatin1('.'), 0, -2);
            if (components.contains(id))
                components[id]->appendComponent(comp);
        }
    }

    // append all components w/o parent to the direct list
    for (it = components.begin(); it != components.end(); ++it) {
        if (d->m_linearComponentList || (*it)->parentComponent() == 0)
            appendComponent(*it);
    }

    // after everything is set up, load the scripts
    QMapIterator< QInstaller::Component*, QString > scriptIt(scripts);
    while (scriptIt.hasNext()) {
        scriptIt.next();
        QInstaller::Component* const component = scriptIt.key();
        const QString script = scriptIt.value();
        verbose() << "Loading script for component " << component->name() << " (" << script << ")"
            << std::endl;
        component->loadComponentScript(script);
    }

    // select all components in the updater model
    foreach (QInstaller::Component* const i, componentsToSelectUpdater)
        i->setSelected(true, UpdaterMode, Component::InitializeComponentTreeSelectMode);

    // select all components in the package manager model
    foreach (QInstaller::Component* const i, componentsToSelectInstaller)
        i->setSelected(true, AllMode, Component::InitializeComponentTreeSelectMode);

    emit updaterComponentsAdded(d->m_packageManagerComponents);
    emit componentsAdded(d->m_rootComponents);
}

//TODO: remove this unused function
void Installer::appendComponent(Component *component)
{
    d->m_rootComponents.append(component);
    d->m_componentHash[component->name()] = component;
    emit componentAdded(component);
}

int Installer::componentCount(RunModes runMode) const
{
    if (runMode == UpdaterMode)
        return d->m_updaterComponents.size();
    return d->m_rootComponents.size();
}

Component *Installer::component(int i, RunModes runMode) const
{
    if (runMode == UpdaterMode)
        return d->m_updaterComponents.at(i);
    return d->m_rootComponents.at(i);
}

Component *Installer::component(const QString &name) const
{
    return d->m_componentHash.contains(name) ? d->m_componentHash[name] : 0;
}

QList<Component*> Installer::components(bool recursive, RunModes runMode) const
{
    if (runMode == UpdaterMode)
        return d->m_updaterComponents;

    if (!recursive)
        return d->m_rootComponents;

    QList<Component*> result;
    foreach (QInstaller::Component *component, d->m_rootComponents) {
        result.push_back(component);
        result += component->childComponents(true);
    }

    return result;
}

QList<Component*> Installer::componentsToInstall(bool recursive, bool sort, RunModes runMode) const
{
    QList<Component*> availableComponents = components(recursive, runMode);
    if (sort) {
        std::sort(availableComponents.begin(), availableComponents.end(),
            Component::InstallPriorityLessThan());
    }

    QList<Component*> componentsToInstall;
    foreach (QInstaller::Component *component, availableComponents) {
        if (!component->isSelected(runMode))
            continue;

        // it was already installed before, so don't add it
        if (component->value(QLatin1String("PreviousState")) == QLatin1String("Installed")
            && runMode == AllMode)    // TODO: is the last condition right ????
            continue;

        appendComponentAndMissingDependencies(componentsToInstall, component);
    }

    return componentsToInstall;
}

static bool componentMatches(const Component *component, const QString &name,
    const QString& version = QString())
{
    if (!name.isEmpty() && component->name() != name)
        return false;

    if (version.isEmpty())
        return true;

    return Installer::versionMatches(component->value(QLatin1String("Version")), version);
}

static Component* subComponentByName(const Installer *installer, const QString &name,
    const QString &version = QString(), Component *check = 0)
{
    if (check != 0 && componentMatches(check, name, version))
        return check;

    const QList<Component*> comps = check == 0 ? installer->components() : check->childComponents();
    for (QList<Component*>::const_iterator it = comps.begin(); it != comps.end(); ++it) {
        Component* const result = subComponentByName(installer, name, version, *it);
        if (result != 0)
            return result;
    }

    const QList<Component*> uocomps =
        check == 0 ? installer->components(false, UpdaterMode) : check->childComponents(false, UpdaterMode);
    for (QList<Component*>::const_iterator it = uocomps.begin(); it != uocomps.end(); ++it) {
        Component* const result = subComponentByName(installer, name, version, *it);
        if (result != 0)
            return result;
    }

    return 0;
}

bool Installer::hasLinearComponentList() const
{
    return d->m_linearComponentList;
}

void Installer::setLinearComponentList(bool showlinear)
{
    d->m_linearComponentList = showlinear;
}

/*!
    Returns a component matching \a name. \a name can also contains a version requirement.
    E.g. "com.nokia.sdk.qt" returns any component with that name, "com.nokia.sdk.qt->=4.5" requires
    the returned component to have at least version 4.5.
    If no component matches the requirement, 0 is returned.
*/
Component* Installer::componentByName(const QString &name) const
{
    if (name.contains(QChar::fromLatin1('-'))) {
        // the last part is considered to be the version, then
        const QString version = name.section(QLatin1Char('-'), 1);
        return subComponentByName(this, name.section(QLatin1Char('-'), 0, 0), version);
    }

    QHash< QString, QInstaller::Component* >::ConstIterator it = d->m_componentHash.constFind(name);
    Component * comp = 0;
    if (it != d->m_componentHash.constEnd())
        comp = *it;
    if (d->m_updaterComponents.contains(comp))
        return comp;

    return subComponentByName(this, name);
}

/*!
    Returns a list of packages depending on \a component.
*/
QList<Component*> Installer::dependees(const Component *component) const
{
    QList<Component*> result;

    const QList<Component*> allComponents = components(true, AllMode);
    for (QList<Component*>::const_iterator it = allComponents.begin(); it != allComponents.end(); ++it) {
        Component* const c = *it;

        const QStringList deps = c->value(QString::fromLatin1("Dependencies"))
            .split(QChar::fromLatin1(','), QString::SkipEmptyParts);

        const QLatin1Char dash('-');
        for (QStringList::const_iterator it2 = deps.begin(); it2 != deps.end(); ++it2) {
            // the last part is considered to be the version, then
            const QString id = it2->contains(dash) ? it2->section(dash, 0, 0) : *it2;
            const QString version = it2->contains(dash) ? it2->section(dash, 1) : QString();
            if (componentMatches(component, id, version))
                result.push_back(c);
        }
    }

    return result;
}

InstallerSettings Installer::settings() const
{
    return *d->m_installerSettings;
}

/*!
    Returns a list of dependencies for \a component.
    If there's a dependency which cannot be fullfilled, the list contains 0 values.
*/
QList<Component*> Installer::dependencies(const Component *component,
    QStringList *missingPackageNames) const
{
    QList<Component*> result;
    const QStringList deps = component->value(QString::fromLatin1("Dependencies"))
        .split(QChar::fromLatin1(','), QString::SkipEmptyParts);

    for (QStringList::const_iterator it = deps.begin(); it != deps.end(); ++it) {
        const QString name = *it;
        Component* comp = componentByName(*it);
        if (!comp && missingPackageNames)
            missingPackageNames->append(name);
        else
            result.push_back(comp);
    }
    return result;
}

/*!
    Returns the list of all missing (not installed) dependencies for \a component.
*/
QList<Component*> Installer::missingDependencies(const Component *component) const
{
    QList<Component*> result;
    const QStringList deps = component->value(QString::fromLatin1("Dependencies"))
        .split(QChar::fromLatin1(','), QString::SkipEmptyParts);

    const QLatin1Char dash('-');
    for (QStringList::const_iterator it = deps.begin(); it != deps.end(); ++it) {
        const bool containsVersionString = it->contains(dash);
        const QString version =  containsVersionString ? it->section(dash, 1) : QString();
        const QString name = containsVersionString ? it->section(dash, 0, 0) : *it;

        bool installed = false;
        const QList<Component*> compList = components(true);
        foreach (const Component* comp, compList) {
            if (!name.isEmpty() && comp->name() == name && !version.isEmpty()) {
                if (Installer::versionMatches(comp->value(QLatin1String("InstalledVersion")), version))
                    installed = true;
            } else if (comp->name() == name) {
                installed = true;
            }
        }

        foreach (const Component *comp,  d->m_updaterComponents) {
            if (!name.isEmpty() && comp->name() == name && !version.isEmpty()) {
                if (Installer::versionMatches(comp->value(QLatin1String("InstalledVersion")), version))
                    installed = true;
            } else if (comp->name() == name) {
                installed = true;
            }
        }

        if (!installed) {
            if (Component *comp = componentByName(name))
                result.push_back(comp);
        }
    }
    return result;
}

/*!
    This method tries to gain admin rights. On success, it returns true.
*/
bool Installer::gainAdminRights()
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
void Installer::dropAdminRights()
{
    d->m_FSEngineClientHandler->setActive(false);
}

/*!
    Return true, if a process with \a name is running. On Windows, the comparision
    is case-insensitive.
*/
bool Installer::isProcessRunning(const QString &name) const
{
    return InstallerPrivate::isProcessRunning(name, KDSysInfo::runningProcesses());
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
QList<QVariant> Installer::execute(const QString &program, const QStringList &arguments,
    const QString &stdIn) const
{
    QProcess p;
    p.start(program, arguments, stdIn.isNull() ? QIODevice::ReadOnly : QIODevice::ReadWrite);
    if (!p.waitForStarted())
        return QList< QVariant >();

    if (!stdIn.isNull()) {
        p.write(stdIn.toLatin1());
        p.closeWriteChannel();
    }

    QEventLoop loop;
    connect(&p, SIGNAL(finished(int, QProcess::ExitStatus)), &loop, SLOT(quit()));
    loop.exec();

    return QList< QVariant >() << QString::fromLatin1(p.readAllStandardOutput()) << p.exitCode();
}

/*!
    Returns an environment variable.
*/
QString Installer::environmentVariable(const QString &name) const
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
    Instantly performns an operation \a name with \a arguments.
    \sa Component::addOperation
*/
bool Installer::performOperation(const QString &name, const QStringList &arguments)
{
    QScopedPointer<KDUpdater::UpdateOperation> op(KDUpdater::UpdateOperationFactory::instance()
        .create(name));
    if (!op.data())
        return false;

    op->setArguments(arguments);
    op->backup();
    if (!InstallerPrivate::performOperationThreaded(op.data())) {
        InstallerPrivate::performOperationThreaded(op.data(), InstallerPrivate::Undo);
        return false;
    }
    return true;
}

/*!
    Returns true when \a version matches the \a requirement.
    \a requirement can be a fixed version number or it can be prefix by the comparaters '>', '>=',
    '<', '<=' and '='.
*/
bool Installer::versionMatches(const QString &version, const QString &requirement)
{
    QRegExp compEx(QLatin1String("([<=>]+)(.*)"));
    const QString comparator = compEx.exactMatch(requirement) ? compEx.cap(1) : QString::fromLatin1("=");
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
    Finds a library named \a name in \a pathes.
    If \a pathes is empty, it gets filled with platform dependent default pathes.
    The resulting path is stored in \a library.
    This method can be used by scripts to check external dependencies.
*/
QString Installer::findLibrary(const QString &name, const QStringList &pathes)
{
    QStringList findPathes = pathes;
#if defined(Q_WS_WIN)
    return findPath(QString::fromLatin1("%1.lib").arg(name), findPathes);
#else
    if (findPathes.isEmpty()) {
        findPathes.push_back(QLatin1String("/lib"));
        findPathes.push_back(QLatin1String("/usr/lib"));
        findPathes.push_back(QLatin1String("/usr/local/lib"));
        findPathes.push_back(QLatin1String("/opt/local/lib"));
    }
#if defined(Q_WS_MAC)
    const QString dynamic = findPath(QString::fromLatin1("lib%1.dylib").arg(name), findPathes);
#else
    const QString dynamic = findPath(QString::fromLatin1("lib%1.so*").arg(name), findPathes);
#endif
    if (!dynamic.isEmpty())
        return dynamic;
    return findPath(QString::fromLatin1("lib%1.a").arg(name), findPathes);
#endif
}

/*!
    Tries to find a file name \a name in one of \a pathes.
    The resulting path is stored in \a path.
    This method can be used by scripts to check external dependencies.
*/
QString Installer::findPath(const QString &name, const QStringList &pathes)
{
    for (QStringList::const_iterator it = pathes.begin(); it != pathes.end(); ++it) {
        const QDir dir(*it);
        const QStringList entries = dir.entryList(QStringList() << name, QDir::Files | QDir::Hidden);
        if (entries.isEmpty())
            continue;

        return dir.absoluteFilePath(entries.first());
    }
    return QString();
}

/*!
    sets the "installerbase" binary to use when writing the package manager/uninstaller.
    Set this if an update to installerbase is available.
    If not set, the executable segment of the running un/installer will be used.
*/
void Installer::setInstallerBaseBinary(const QString &path)
{
    d->m_forceRestart = true;
    d->m_installerBaseBinaryUnreplaced = path;
}

/*!
    Returns the installer value for \a key. If \a key is not known to the system, \a defaultValue is
    returned. Additionally, on Windows, \a key can be a registry key.
*/
QString Installer::value(const QString &key, const QString &defaultValue) const
{
#ifdef Q_WS_WIN
    if (!d->m_vars.contains(key)) {
        static const QRegExp regex(QLatin1String("\\\\|/"));
        const QString filename = key.section(regex, 0, -2);
        const QString regKey = key.section(regex, -1);
        const QSettings registry(filename, QSettings::NativeFormat);
        if (!filename.isEmpty() && !regKey.isEmpty() && registry.contains(regKey))
            return registry.value(regKey).toString();
    }
#else
    if (key == QLatin1String("TargetDir")) {
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
void Installer::setValue(const QString &key, const QString &value)
{
    if (d->m_vars.value(key) == value)
        return;

    d->m_vars.insert(key, value);
    emit valueChanged(key, value);
}

/*!
    Returns true, when the installer contains a value for \a key.
*/
bool Installer::containsValue(const QString &key) const
{
    return d->m_vars.contains(key);
}

void Installer::setSharedFlag(const QString &key, bool value)
{
    d->m_sharedFlags.insert(key, value);
}

bool Installer::sharedFlag(const QString &key) const
{
    return d->m_sharedFlags.value(key, false);
}

bool Installer::isVerbose() const
{
    return QInstaller::isVerbose();
}

void Installer::setVerbose(bool on)
{
    QInstaller::setVerbose(on);
}

Installer::Status Installer::status() const
{
    return Installer::Status(d->m_status);
}
/*!
    returns true if at least one complete installation/update
    was successfull, even if the user cancelled the newest
    installation process.
*/
bool Installer::finishedWithSuccess() const
{
    return (d->m_status == Installer::Success) || d->m_needToWriteUninstaller;
}

void Installer::interrupt()
{
    verbose() << "INTERRUPT INSTALLER" << std::endl;
    d->setStatus(Installer::Canceled);
    emit installationInterrupted();
}

void Installer::setCanceled()
{
    d->setStatus(Installer::Canceled);
}

/*!
    Replaces all variables within \a str by their respective values and
    returns the result.
*/
QString Installer::replaceVariables(const QString &str) const
{
    return d->replaceVariables(str);
}

/*!
    Replaces all variables in any of \a str by their respective values and
    returns the results.
    \overload
*/
QStringList Installer::replaceVariables(const QStringList &str) const
{
    QStringList result;
    for (QStringList::const_iterator it = str.begin(); it != str.end(); ++it)
        result.push_back(d->replaceVariables(*it));

    return result;
}

/*!
    Replaces all variables within \a ba by their respective values and
    returns the result.
    \overload
*/
QByteArray Installer::replaceVariables(const QByteArray &ba) const
{
    return d->replaceVariables(ba);
}

/*!
    Returns the path to the installer binary.
*/
QString Installer::installerBinaryPath() const
{
    return d->installerBinaryPath();
}

/*!
    Returns true when this is the installer running.
*/
bool Installer::isInstaller() const
{
    return d->isInstaller();
}

/*!
    Returns true if this is an offline-only installer.
*/
bool Installer::isOfflineOnly() const
{
    QSettings confInternal(QLatin1String(":/config/config-internal.ini"), QSettings::IniFormat);
    return confInternal.value(QLatin1String("offlineOnly")).toBool();
}

void Installer::setUninstaller()
{
    d->m_magicBinaryMarker = QInstaller::MagicUninstallerMarker;
}

/*!
    Returns true when this is the uninstaller running.
*/
bool Installer::isUninstaller() const
{
    return d->isUninstaller();
}

void Installer::setUpdater()
{
    d->m_magicBinaryMarker = QInstaller::MagicUpdaterMarker;
}

/*!
    Returns true when this is neither an installer nor an uninstaller running.
    Must be an updater, then.
*/
bool Installer::isUpdater() const
{
    return d->isUpdater();
}

void Installer::setPackageManager()
{
    d->m_magicBinaryMarker = QInstaller::MagicPackageManagerMarker;
}

/*!
    Returns true when this is the package manager running.
*/
bool Installer::isPackageManager() const
{
    return d->isPackageManager();
}

/*!
    Runs the installer. Returns true on success, false otherwise.
*/
bool Installer::runInstaller()
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
bool Installer::runUninstaller()
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
bool Installer::runPackageUpdater()
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
void Installer::languageChanged()
{
    const QList<Component*> comps = components(true);
    foreach (Component* component, comps)
        component->languageChanged();
}

/*!
    Runs the installer or uninstaller, depending on the type of this binary.
*/
bool Installer::run()
{
    try {
        if (isInstaller())
            d->runInstaller();
        else if (isUninstaller())
            d->runUninstaller();
        else if (isPackageManager())
            d->runPackageUpdater();
        return true;
    } catch (const Error &err) {
        verbose() << "Caught Installer Error: " << err.message() << std::endl;
        return false;
    }
}

/*!
    Returns the path name of the ininstaller binary.
*/
QString Installer::uninstallerName() const
{
    return d->uninstallerName();
}

bool Installer::setAndParseLocalComponentsFile(KDUpdater::PackagesInfo &packagesInfo)
{
    packagesInfo.setFileName(d->localComponentsXmlPath());
    const QString localComponentsXml = d->localComponentsXmlPath();

    // handle errors occured by loading components.xml
    QFileInfo componentFileInfo(localComponentsXml);
    int silentRetries = d->m_silentRetries;
    while (!componentFileInfo.exists()) {
        if (silentRetries > 0) {
            --silentRetries;
        } else {
            Status status = handleComponentsFileSetOrParseError(localComponentsXml);
            if (status == Installer::Canceled)
                return false;
        }
        packagesInfo.setFileName(localComponentsXml);
    }

    silentRetries = d->m_silentRetries;
    while (packagesInfo.error() != KDUpdater::PackagesInfo::NoError) {
        if (silentRetries > 0) {
            --silentRetries;
        } else {
            Status status = handleComponentsFileSetOrParseError(localComponentsXml);
            if (status == Installer::Canceled)
                return false;
        }
        packagesInfo.setFileName(localComponentsXml);
    }

    silentRetries = d->m_silentRetries;
    while (packagesInfo.error() != KDUpdater::PackagesInfo::NoError) {
        if (silentRetries > 0) {
            --silentRetries;
        } else {
            bool retry = false;
            if (packagesInfo.error() != KDUpdater::PackagesInfo::InvalidContentError
                && packagesInfo.error() != KDUpdater::PackagesInfo::InvalidXmlError) {
                    retry = true;
            }
            Status status = handleComponentsFileSetOrParseError(componentFileInfo.fileName(),
                packagesInfo.errorString(), retry);
            if (status == Installer::Canceled)
                return false;
        }
        packagesInfo.setFileName(localComponentsXml);
    }

    return true;
}

Installer::Status Installer::handleComponentsFileSetOrParseError(const QString &arg1,
    const QString &arg2, bool withRetry)
{
    QMessageBox::StandardButtons buttons = QMessageBox::Cancel;
    if (withRetry)
        buttons |= QMessageBox::Retry;

    const QMessageBox::StandardButton button =
        MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(),
        QLatin1String("Error loading component.xml"), tr("Loading error"),
        tr(arg2.isEmpty() ? "Could not load %1" : "Could not load %1 : %2").arg(arg1, arg2),
        buttons);

    if (button == QMessageBox::Cancel) {
        d->m_status = Installer::Failure;
        return Installer::Canceled;
    }
    return Installer::Unfinished;
}
