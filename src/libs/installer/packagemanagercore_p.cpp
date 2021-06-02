/**************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
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
#include "packagemanagercore_p.h"

#include "adminauthorization.h"
#include "binarycontent.h"
#include "binaryformatenginehandler.h"
#include "binarylayout.h"
#include "component.h"
#include "scriptengine.h"
#include "componentmodel.h"
#include "errors.h"
#include "fileio.h"
#include "remotefileengine.h"
#include "graph.h"
#include "messageboxhandler.h"
#include "packagemanagercore.h"
#include "progresscoordinator.h"
#include "qprocesswrapper.h"
#include "protocol.h"
#include "qsettingswrapper.h"
#include "installercalculator.h"
#include "uninstallercalculator.h"
#include "componentchecker.h"
#include "globals.h"
#include "binarycreator.h"
#include "loggingutils.h"

#include "selfrestarter.h"
#include "filedownloaderfactory.h"
#include "updateoperationfactory.h"

#include <productkeycheck.h>

#include <QSettings>
#include <QtConcurrentRun>
#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QDirIterator>
#include <QtCore/QUuid>
#include <QtCore/QFuture>
#include <QtCore/QFutureWatcher>
#include <QtCore/QTemporaryFile>

#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include <errno.h>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

#define QUOTE_(x) #x
#define QUOTE(x) QUOTE_(x)

namespace QInstaller {

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::PackageManagerCorePrivate
    \internal
*/

class OperationTracer
{
public:
    OperationTracer(Operation *operation) : m_operation(nullptr)
    {
        // don't create output for that hacky pseudo operation
        if (operation->name() != QLatin1String("MinimumProgress"))
            m_operation = operation;
    }
    void trace(const QString &state)
    {
        if (!m_operation)
            return;
        qCDebug(QInstaller::lcInstallerInstallLog).noquote() << QString::fromLatin1("%1 %2 operation: %3")
            .arg(state, m_operation->value(QLatin1String("component")).toString(), m_operation->name());
        QStringList args = m_operation->arguments();
        if (m_operation->requiresUnreplacedVariables())
            args = m_operation->packageManager()->replaceVariables(m_operation->arguments());
        qCDebug(QInstaller::lcInstallerInstallLog).noquote() << QString::fromLatin1("\t- arguments: %1")
            .arg(args.join(QLatin1String(", ")));
    }
    ~OperationTracer() {
        if (!m_operation)
            return;
        qCDebug(QInstaller::lcInstallerInstallLog) << "Done";
    }
private:
    Operation *m_operation;
};

static bool runOperation(Operation *operation, Operation::OperationType type)
{
    OperationTracer tracer(operation);
    switch (type) {
        case Operation::Backup:
            tracer.trace(QLatin1String("backup"));
            operation->backup();
            return true;
        case Operation::Perform:
            tracer.trace(QLatin1String("perform"));
            return operation->performOperation();
        case Operation::Undo:
            tracer.trace(QLatin1String("undo"));
            return operation->undoOperation();
        default:
            Q_ASSERT(!"unexpected operation type");
    }
    return false;
}

static QStringList checkRunningProcessesFromList(const QStringList &processList)
{
    const QList<ProcessInfo> allProcesses = runningProcesses();
    QStringList stillRunningProcesses;
    foreach (const QString &process, processList) {
        if (!process.isEmpty() && PackageManagerCorePrivate::isProcessRunning(process, allProcesses))
            stillRunningProcesses.append(process);
    }
    return stillRunningProcesses;
}

static void deferredRename(const QString &oldName, const QString &newName, bool restart = false)
{
#ifdef Q_OS_WIN
    QStringList arguments;

    // Check if .vbs extension can be used for running renaming script. If not, create own extension
    QString extension = QLatin1String(".vbs");
    QSettingsWrapper settingRoot(QLatin1String("HKEY_CLASSES_ROOT\\.vbs"), QSettingsWrapper::NativeFormat);
    if (settingRoot.value(QLatin1String(".")).toString() != QLatin1String("VBSFile")) {
        extension = QLatin1String(".qtInstaller");
        QSettingsWrapper settingsUser(QLatin1String("HKEY_CURRENT_USER\\Software\\Classes"), QSettingsWrapper::NativeFormat);
        QString value = settingsUser.value(extension).toString();
        if (value != QLatin1String("VBSFile"))
            settingsUser.setValue(extension, QLatin1String("VBSFile"));
    }
    QTemporaryFile f(QDir::temp().absoluteFilePath(QLatin1String("deferredrenameXXXXXX%1")).arg(extension));

    QInstaller::openForWrite(&f);
    f.setAutoRemove(false);

    arguments << QDir::toNativeSeparators(f.fileName()) << QDir::toNativeSeparators(oldName)
        << QDir::toNativeSeparators(QFileInfo(oldName).dir().absoluteFilePath(QFileInfo(newName)
        .fileName()));

    QTextStream batch(&f);
    batch.setCodec("UTF-16");
    batch << "Set fso = WScript.CreateObject(\"Scripting.FileSystemObject\")\n";
    batch << "Set tmp = WScript.CreateObject(\"WScript.Shell\")\n";
    batch << QString::fromLatin1("file = \"%1\"\n").arg(arguments[2]);
    batch << "on error resume next\n";

    batch << "while fso.FileExists(file)\n";
    batch << "    fso.DeleteFile(file)\n";
    batch << "    WScript.Sleep(1000)\n";
    batch << "wend\n";
    batch << QString::fromLatin1("fso.MoveFile \"%1\", file\n").arg(arguments[1]);
    if (restart) {
        //Restart with same command line arguments as first executable
        QStringList commandLineArguments = QCoreApplication::arguments();
        batch <<  QString::fromLatin1("tmp.exec \"%1 --%2")
                  .arg(arguments[2]).arg(CommandLineOptions::scStartUpdaterLong);
        //Skip the first argument as that is executable itself
        for (int i = 1; i < commandLineArguments.count(); i++) {
            batch << QString::fromLatin1(" %1").arg(commandLineArguments.at(i));
        }
        batch << QString::fromLatin1("\"\n");
    }
    batch << "fso.DeleteFile(WScript.ScriptFullName)\n";

    QProcessWrapper::startDetached(QLatin1String("cscript"), QStringList() << QLatin1String("//Nologo")
        << arguments[0]);
#else
        QFile::remove(newName);
        QFile::rename(oldName, newName);
        SelfRestarter::setRestartOnQuit(restart);
#endif
}


// -- PackageManagerCorePrivate

PackageManagerCorePrivate::PackageManagerCorePrivate(PackageManagerCore *core)
    : m_updateFinder(nullptr)
    , m_compressedFinder(nullptr)
    , m_localPackageHub(std::make_shared<LocalPackageHub>())
    , m_status(PackageManagerCore::Unfinished)
    , m_needsHardRestart(false)
    , m_testChecksum(false)
    , m_launchedAsRoot(AdminAuthorization::hasAdminRights())
    , m_completeUninstall(false)
    , m_needToWriteMaintenanceTool(false)
    , m_dependsOnLocalInstallerBinary(false)
    , m_core(core)
    , m_updates(false)
    , m_repoFetched(false)
    , m_updateSourcesAdded(false)
    , m_magicBinaryMarker(0) // initialize with pseudo marker
    , m_magicMarkerSupplement(BinaryContent::Default)
    , m_componentsToInstallCalculated(false)
    , m_componentScriptEngine(nullptr)
    , m_controlScriptEngine(nullptr)
    , m_installerCalculator(nullptr)
    , m_uninstallerCalculator(nullptr)
    , m_proxyFactory(nullptr)
    , m_defaultModel(nullptr)
    , m_updaterModel(nullptr)
    , m_guiObject(nullptr)
    , m_remoteFileEngineHandler(nullptr)
    , m_foundEssentialUpdate(false)
    , m_commandLineInstance(false)
    , m_defaultInstall(false)
    , m_userSetBinaryMarker(false)
    , m_checkAvailableSpace(true)
    , m_autoAcceptLicenses(false)
    , m_disableWriteMaintenanceTool(false)
    , m_autoConfirmCommand(false)
{
}

PackageManagerCorePrivate::PackageManagerCorePrivate(PackageManagerCore *core, qint64 magicInstallerMaker,
        const QList<OperationBlob> &performedOperations)
    : m_updateFinder(nullptr)
    , m_compressedFinder(nullptr)
    , m_localPackageHub(std::make_shared<LocalPackageHub>())
    , m_status(PackageManagerCore::Unfinished)
    , m_needsHardRestart(false)
    , m_testChecksum(false)
    , m_launchedAsRoot(AdminAuthorization::hasAdminRights())
    , m_completeUninstall(false)
    , m_needToWriteMaintenanceTool(false)
    , m_dependsOnLocalInstallerBinary(false)
    , m_core(core)
    , m_updates(false)
    , m_repoFetched(false)
    , m_updateSourcesAdded(false)
    , m_magicBinaryMarker(magicInstallerMaker)
    , m_magicMarkerSupplement(BinaryContent::Default)
    , m_componentsToInstallCalculated(false)
    , m_componentScriptEngine(nullptr)
    , m_controlScriptEngine(nullptr)
    , m_installerCalculator(nullptr)
    , m_uninstallerCalculator(nullptr)
    , m_proxyFactory(nullptr)
    , m_defaultModel(nullptr)
    , m_updaterModel(nullptr)
    , m_guiObject(nullptr)
    , m_remoteFileEngineHandler(new RemoteFileEngineHandler)
    , m_foundEssentialUpdate(false)
    , m_commandLineInstance(false)
    , m_defaultInstall(false)
    , m_userSetBinaryMarker(false)
    , m_checkAvailableSpace(true)
    , m_autoAcceptLicenses(false)
    , m_disableWriteMaintenanceTool(false)
    , m_autoConfirmCommand(false)
{
    foreach (const OperationBlob &operation, performedOperations) {
        QScopedPointer<QInstaller::Operation> op(KDUpdater::UpdateOperationFactory::instance()
            .create(operation.name, core));
        if (op.isNull()) {
            qCWarning(QInstaller::lcInstallerInstallLog) << "Failed to load unknown operation"
                << operation.name;
            continue;
        }

        if (!op->fromXml(operation.xml)) {
            qCWarning(QInstaller::lcInstallerInstallLog) << "Failed to load XML for operation"
                << operation.name;
            continue;
        }
        m_performedOperationsOld.append(op.take());
    }

    connect(this, &PackageManagerCorePrivate::installationStarted,
            m_core, &PackageManagerCore::installationStarted);
    connect(this, &PackageManagerCorePrivate::installationFinished,
            m_core, &PackageManagerCore::installationFinished);
    connect(this, &PackageManagerCorePrivate::uninstallationStarted,
            m_core, &PackageManagerCore::uninstallationStarted);
    connect(this, &PackageManagerCorePrivate::uninstallationFinished,
            m_core, &PackageManagerCore::uninstallationFinished);
    connect(this, &PackageManagerCorePrivate::offlineGenerationStarted,
            m_core, &PackageManagerCore::offlineGenerationStarted);
    connect(this, &PackageManagerCorePrivate::offlineGenerationFinished,
            m_core, &PackageManagerCore::offlineGenerationFinished);
}

PackageManagerCorePrivate::~PackageManagerCorePrivate()
{
    clearAllComponentLists();
    clearUpdaterComponentLists();
    clearInstallerCalculator();
    clearUninstallerCalculator();

    qDeleteAll(m_ownedOperations);
    qDeleteAll(m_performedOperationsOld);
    qDeleteAll(m_performedOperationsCurrentSession);

    delete m_updateFinder;
    delete m_proxyFactory;

    delete m_defaultModel;
    delete m_updaterModel;

    // at the moment the tabcontroller deletes the m_gui, this needs to be changed in the future
    // delete m_gui;
}

/*
    Return true, if a process with \a name is running. On Windows, comparison is case-insensitive.
*/
/* static */
bool PackageManagerCorePrivate::isProcessRunning(const QString &name,
    const QList<ProcessInfo> &processes)
{
    QList<ProcessInfo>::const_iterator it;
    for (it = processes.constBegin(); it != processes.constEnd(); ++it) {
        if (it->name.isEmpty())
            continue;

#ifndef Q_OS_WIN
        if (it->name == name)
            return true;
        const QFileInfo fi(it->name);
        if (fi.fileName() == name || fi.baseName() == name)
            return true;
#else
        if (it->name.toLower() == name.toLower())
            return true;
        if (it->name.toLower() == QDir::toNativeSeparators(name.toLower()))
            return true;
        const QFileInfo fi(it->name);
        if (fi.fileName().toLower() == name.toLower() || fi.baseName().toLower() == name.toLower())
            return true;
#endif
    }
    return false;
}

/* static */
bool PackageManagerCorePrivate::performOperationThreaded(Operation *operation, Operation::OperationType type)
{
    QFutureWatcher<bool> futureWatcher;
    const QFuture<bool> future = QtConcurrent::run(runOperation, operation, type);

    QEventLoop loop;
    QObject::connect(&futureWatcher, &decltype(futureWatcher)::finished, &loop, &QEventLoop::quit,
                     Qt::QueuedConnection);
    futureWatcher.setFuture(future);

    if (!future.isFinished())
        loop.exec();

    return future.result();
}

QString PackageManagerCorePrivate::targetDir() const
{
    return m_core->value(scTargetDir);
}

bool PackageManagerCorePrivate::directoryWritable(const QString &path) const
{
    QTemporaryFile tempFile(path + QLatin1String("/tempFile.XXXXXX"));
    return (tempFile.open() && tempFile.isWritable());
}

QString PackageManagerCorePrivate::configurationFileName() const
{
    return m_core->value(scTargetConfigurationFile, QLatin1String("components.xml"));
}

QString PackageManagerCorePrivate::componentsXmlPath() const
{
    return QDir::toNativeSeparators(QDir(QDir::cleanPath(targetDir()))
        .absoluteFilePath(configurationFileName()));
}

bool PackageManagerCorePrivate::buildComponentTree(QHash<QString, Component*> &components, bool loadScript)
{
    try {
        if (statusCanceledOrFailed())
            return false;
        // append all components to their respective parents
        QHash<QString, Component*>::const_iterator it;
        for (it = components.constBegin(); it != components.constEnd(); ++it) {
            QString id = it.key();
            QInstaller::Component *component = it.value();
            while (!id.isEmpty() && component->parentComponent() == nullptr) {
                id = id.section(QLatin1Char('.'), 0, -2);
                if (components.contains(id))
                    components[id]->appendComponent(component);
            }
        }

        // append all components w/o parent to the direct list
        foreach (QInstaller::Component *component, components) {
            if (component->parentComponent() == nullptr)
                m_core->appendRootComponent(component);
        }

        // after everything is set up, load the scripts if needed
        if (loadScript) {
            foreach (QInstaller::Component *component, components)
                component->loadComponentScript();
        }

        // now we can preselect components in the tree
        foreach (QInstaller::Component *component, components) {
            // set the checked state for all components without child (means without tristate)
            // set checked state also for installed virtual tristate componets as otherwise
            // those will be uninstalled
            if (component->isCheckable() && !component->isTristate()) {
                if (component->isDefault() && isInstaller())
                    component->setCheckState(Qt::Checked);
                else if (component->isInstalled())
                    component->setCheckState(Qt::Checked);
            } else if (component->isVirtual() && component->isInstalled() && component->isTristate()) {
                component->setCheckState(Qt::Checked);
            }
        }

        std::sort(m_rootComponents.begin(), m_rootComponents.end(), Component::SortingPriorityGreaterThan());

        storeCheckState();

        foreach (QInstaller::Component *component, components)
            component->setCheckState(Qt::Checked);

        clearInstallerCalculator();
        if (installerCalculator()->appendComponentsToInstall(components.values()) == false) {
            setStatus(PackageManagerCore::Failure, installerCalculator()->componentsToInstallError());
            MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(), QLatin1String("Error"),
                tr("Unresolved dependencies"), installerCalculator()->componentsToInstallError());
            return false;
        }

        restoreCheckState();

        if (LoggingHandler::instance().verboseLevel() == LoggingHandler::Detailed) {
            foreach (QInstaller::Component *component, components) {
                const QStringList warnings = ComponentChecker::checkComponent(component);
                foreach (const QString &warning, warnings)
                    qCWarning(lcDeveloperBuild).noquote() << warning;
            }
        }

    } catch (const Error &error) {
        clearAllComponentLists();
        emit m_core->finishAllComponentsReset(QList<QInstaller::Component*>());
        setStatus(PackageManagerCore::Failure, error.message());

        // TODO: make sure we remove all message boxes inside the library at some point.
        MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(), QLatin1String("Error"),
            tr("Error"), error.message());
        return false;
    }
    return true;
}

void PackageManagerCorePrivate::cleanUpComponentEnvironment()
{
    // clean up registered (downloaded) data
    if (m_core->isMaintainer())
        BinaryFormatEngineHandler::instance()->clear();

    // there could be still some references to already deleted components,
    // so we need to remove the current component script engine
    delete m_componentScriptEngine;
    m_componentScriptEngine = nullptr;
}

ScriptEngine *PackageManagerCorePrivate::componentScriptEngine() const
{
    if (!m_componentScriptEngine)
        m_componentScriptEngine = new ScriptEngine(m_core);
    return m_componentScriptEngine;
}

ScriptEngine *PackageManagerCorePrivate::controlScriptEngine() const
{
    if (!m_controlScriptEngine)
        m_controlScriptEngine = new ScriptEngine(m_core);
    return m_controlScriptEngine;
}

void PackageManagerCorePrivate::clearAllComponentLists()
{
    QList<QInstaller::Component*> toDelete;

    toDelete << m_rootComponents;
    m_rootComponents.clear();

    m_rootDependencyReplacements.clear();

    const QList<QPair<Component*, Component*> > list = m_componentsToReplaceAllMode.values();
    for (int i = 0; i < list.count(); ++i)
        toDelete << list.at(i).second;
    m_componentsToReplaceAllMode.clear();
    m_componentsToInstallCalculated = false;

    qDeleteAll(toDelete);
    cleanUpComponentEnvironment();
}

void PackageManagerCorePrivate::clearUpdaterComponentLists()
{
    QSet<Component*> usedComponents =
        QSet<Component*>::fromList(m_updaterComponents + m_updaterComponentsDeps);

    const QList<QPair<Component*, Component*> > list = m_componentsToReplaceUpdaterMode.values();
    for (int i = 0; i < list.count(); ++i) {
        if (usedComponents.contains(list.at(i).second))
            qCWarning(QInstaller::lcDeveloperBuild) << "a replacement was already in the list - is that correct?";
        else
            usedComponents.insert(list.at(i).second);
    }

    m_updaterComponents.clear();
    m_updaterComponentsDeps.clear();

    m_updaterDependencyReplacements.clear();

    m_componentsToReplaceUpdaterMode.clear();
    m_componentsToInstallCalculated = false;

    qDeleteAll(usedComponents);
    cleanUpComponentEnvironment();
}

QList<Component *> &PackageManagerCorePrivate::replacementDependencyComponents()
{
    return (!isUpdater()) ? m_rootDependencyReplacements : m_updaterDependencyReplacements;
}

QHash<QString, QPair<Component*, Component*> > &PackageManagerCorePrivate::componentsToReplace()
{
    return (!isUpdater()) ? m_componentsToReplaceAllMode : m_componentsToReplaceUpdaterMode;
}

void PackageManagerCorePrivate::clearInstallerCalculator()
{
    delete m_installerCalculator;
    m_installerCalculator = nullptr;
}

InstallerCalculator *PackageManagerCorePrivate::installerCalculator() const
{
    if (!m_installerCalculator) {
        PackageManagerCorePrivate *const pmcp = const_cast<PackageManagerCorePrivate *> (this);
        pmcp->m_installerCalculator = new InstallerCalculator(
            m_core->components(PackageManagerCore::ComponentType::AllNoReplacements));
    }
    return m_installerCalculator;
}

void PackageManagerCorePrivate::clearUninstallerCalculator()
{
    delete m_uninstallerCalculator;
    m_uninstallerCalculator = nullptr;
}

UninstallerCalculator *PackageManagerCorePrivate::uninstallerCalculator() const
{
    if (!m_uninstallerCalculator) {
        PackageManagerCorePrivate *const pmcp = const_cast<PackageManagerCorePrivate *> (this);

        QList<Component*> installedComponents;
        foreach (const QString &name, pmcp->localInstalledPackages().keys()) {
            if (Component *component = m_core->componentByName(PackageManagerCore::checkableName(name))) {
                if (!component->uninstallationRequested())
                    installedComponents.append(component);
            }
        }

        pmcp->m_uninstallerCalculator = new UninstallerCalculator(installedComponents);
    }
    return m_uninstallerCalculator;
}

void PackageManagerCorePrivate::initialize(const QHash<QString, QString> &params)
{
    m_coreCheckedHash.clear();
    m_data = PackageManagerCoreData(params, isInstaller());
    m_componentsToInstallCalculated = false;

#ifdef Q_OS_LINUX
    if (m_launchedAsRoot)
        m_data.setValue(scTargetDir, replaceVariables(m_data.settings().adminTargetDir()));
#endif

    if (!m_core->isInstaller()) {
#ifdef Q_OS_MACOS
        readMaintenanceConfigFiles(QCoreApplication::applicationDirPath() + QLatin1String("/../../.."));
#else
        readMaintenanceConfigFiles(QCoreApplication::applicationDirPath());
#endif
        // Maintenancetool might have overwritten the variables
        // user has given from command line. Reset those variables.
        m_data.setUserDefinedVariables(params);
    }
    processFilesForDelayedDeletion();

    // Set shortcut path for command line interface, in GUI version
    // we have a separate page where the whole path is set.
#ifdef Q_OS_WIN
    if (m_core->isCommandLineInstance() && m_core->isInstaller()) {
        QString startMenuPath;
        if (params.value(QLatin1String("AllUsers")) == scTrue)
            startMenuPath = m_data.value(scAllUsersStartMenuProgramsPath).toString();
        else
            startMenuPath = m_data.value(scUserStartMenuProgramsPath).toString();
        QString startMenuDir = m_core->value(scStartMenuDir, m_core->value(QLatin1String("ProductName")));
        m_data.setValue(scStartMenuDir, startMenuPath + QDir::separator() + startMenuDir);
    }
#endif

    disconnect(this, &PackageManagerCorePrivate::installationStarted,
               ProgressCoordinator::instance(), &ProgressCoordinator::reset);
    connect(this, &PackageManagerCorePrivate::installationStarted,
            ProgressCoordinator::instance(), &ProgressCoordinator::reset);
    disconnect(this, &PackageManagerCorePrivate::uninstallationStarted,
               ProgressCoordinator::instance(), &ProgressCoordinator::reset);
    connect(this, &PackageManagerCorePrivate::uninstallationStarted,
            ProgressCoordinator::instance(), &ProgressCoordinator::reset);
    disconnect(this, &PackageManagerCorePrivate::offlineGenerationStarted,
               ProgressCoordinator::instance(), &ProgressCoordinator::reset);
    connect(this, &PackageManagerCorePrivate::offlineGenerationStarted,
            ProgressCoordinator::instance(), &ProgressCoordinator::reset);

    if (!isInstaller())
        m_localPackageHub->setFileName(componentsXmlPath());

    if (isInstaller() || m_localPackageHub->applicationName().isEmpty()) {
        // TODO: this seems to be wrong, we should ask for ProductName defaulting to applicationName...
        m_localPackageHub->setApplicationName(m_data.settings().applicationName());
    }

    if (isInstaller() || m_localPackageHub->applicationVersion().isEmpty())
        m_localPackageHub->setApplicationVersion(QLatin1String(QUOTE(IFW_REPOSITORY_FORMAT_VERSION)));

    if (isInstaller())
        m_packageSources.insert(PackageSource(QUrl(QLatin1String("resource://metadata/")), 0));

    m_metadataJob.disconnect();
    m_metadataJob.setAutoDelete(false);
    m_metadataJob.setPackageManagerCore(m_core);
    connect(&m_metadataJob, &Job::infoMessage, this, &PackageManagerCorePrivate::infoMessage);
    connect(&m_metadataJob, &Job::progress, this, &PackageManagerCorePrivate::infoProgress);
    connect(&m_metadataJob, &Job::totalProgress, this, &PackageManagerCorePrivate::totalProgress);
    KDUpdater::FileDownloaderFactory::instance().setProxyFactory(m_core->proxyFactory());
}

bool PackageManagerCorePrivate::isOfflineOnly() const
{
    if (!isInstaller())
        return false;

    QSettings confInternal(QLatin1String(":/config/config-internal.ini"), QSettings::IniFormat);
    return confInternal.value(QLatin1String("offlineOnly"), false).toBool();
}

QString PackageManagerCorePrivate::installerBinaryPath() const
{
    return qApp->applicationFilePath();
}

bool PackageManagerCorePrivate::isInstaller() const
{
    return m_magicBinaryMarker == BinaryContent::MagicInstallerMarker;
}

bool PackageManagerCorePrivate::isUninstaller() const
{
    return m_magicBinaryMarker == BinaryContent::MagicUninstallerMarker;
}

bool PackageManagerCorePrivate::isUpdater() const
{
    return m_magicBinaryMarker == BinaryContent::MagicUpdaterMarker;
}

bool PackageManagerCorePrivate::isPackageManager() const
{
    return m_magicBinaryMarker == BinaryContent::MagicPackageManagerMarker;
}

bool PackageManagerCorePrivate::isOfflineGenerator() const
{
    return m_magicMarkerSupplement == BinaryContent::OfflineGenerator;
}

bool PackageManagerCorePrivate::isPackageViewer() const
{
    return m_magicMarkerSupplement == BinaryContent::PackageViewer;
}

bool PackageManagerCorePrivate::statusCanceledOrFailed() const
{
    return m_status == PackageManagerCore::Canceled || m_status == PackageManagerCore::Failure;
}

void PackageManagerCorePrivate::setStatus(int status, const QString &error)
{
    m_error = error;
    if (!error.isEmpty())
        qCWarning(QInstaller::lcInstallerInstallLog) << m_error;
    if (m_status != status) {
        m_status = status;
        emit m_core->statusChanged(PackageManagerCore::Status(m_status));
    }
}

QString PackageManagerCorePrivate::replaceVariables(const QString &str) const
{
    return m_data.replaceVariables(str);
}

QByteArray PackageManagerCorePrivate::replaceVariables(const QByteArray &ba) const
{
    return m_data.replaceVariables(ba);
}

/*!
    \internal
    Creates an update operation owned by the installer, not by any component.
 */
Operation *PackageManagerCorePrivate::createOwnedOperation(const QString &type)
{
    m_ownedOperations.append(KDUpdater::UpdateOperationFactory::instance().create(type, m_core));
    return m_ownedOperations.last();
}

/*!
    \internal
    Removes \a operation from the operations owned by the installer, returns the very same operation if the
    operation was found, otherwise 0.
 */
Operation *PackageManagerCorePrivate::takeOwnedOperation(Operation *operation)
{
    if (!m_ownedOperations.contains(operation))
        return nullptr;

    m_ownedOperations.removeAll(operation);
    return operation;
}

QString PackageManagerCorePrivate::maintenanceToolName() const
{
    QString filename = m_data.settings().maintenanceToolName();
#if defined(Q_OS_MACOS)
    if (QInstaller::isInBundle(QCoreApplication::applicationDirPath()))
        filename += QLatin1String(".app/Contents/MacOS/") + filename;
#elif defined(Q_OS_WIN)
    filename += QLatin1String(".exe");
#endif
    return QString::fromLatin1("%1/%2").arg(targetDir()).arg(filename);
}

QString PackageManagerCorePrivate::offlineBinaryName() const
{
    QString filename = m_core->value(scOfflineBinaryName, qApp->applicationName()
        + QLatin1String("_offline-") + QDate::currentDate().toString(Qt::ISODate));
#if defined(Q_OS_WIN)
    const QString suffix = QLatin1String(".exe");
    if (!filename.endsWith(suffix))
        filename += suffix;
#endif
    return QString::fromLatin1("%1/%2").arg(targetDir()).arg(filename);
}

static QNetworkProxy readProxy(QXmlStreamReader &reader)
{
    QNetworkProxy proxy(QNetworkProxy::HttpProxy);
    while (reader.readNextStartElement()) {
        if (reader.name() == QLatin1String("Host"))
            proxy.setHostName(reader.readElementText());
        else if (reader.name() == QLatin1String("Port"))
            proxy.setPort(reader.readElementText().toInt());
        else if (reader.name() == QLatin1String("Username"))
            proxy.setUser(reader.readElementText());
        else if (reader.name() == QLatin1String("Password"))
            proxy.setPassword(reader.readElementText());
        else
            reader.skipCurrentElement();
    }
    return proxy;
}

static QSet<Repository> readRepositories(QXmlStreamReader &reader, bool isDefault)
{
    QSet<Repository> set;
    while (reader.readNextStartElement()) {
        if (reader.name() == QLatin1String("Repository")) {
            Repository repo(QString(), isDefault);
            while (reader.readNextStartElement()) {
                if (reader.name() == QLatin1String("Host"))
                    repo.setUrl(reader.readElementText());
                else if (reader.name() == QLatin1String("Username"))
                    repo.setUsername(reader.readElementText());
                else if (reader.name() == QLatin1String("Password"))
                    repo.setPassword(reader.readElementText());
                else if (reader.name() == QLatin1String("DisplayName"))
                    repo.setDisplayName(reader.readElementText());
                else if (reader.name() == QLatin1String("Enabled"))
                    repo.setEnabled(bool(reader.readElementText().toInt()));
                else
                    reader.skipCurrentElement();
            }
            set.insert(repo);
        } else {
            reader.skipCurrentElement();
        }
    }
    return set;
}

void PackageManagerCorePrivate::writeMaintenanceConfigFiles()
{
    // write current state (variables) to the maintenance tool ini file
    const QString iniPath = targetDir() + QLatin1Char('/') + m_data.settings().maintenanceToolIniFile();

    QVariantHash variables; // Do not change to QVariantMap! Breaks existing .ini files,
    // cause the variant types do not match while restoring the variables from the file.
    QSettingsWrapper cfg(iniPath, QSettingsWrapper::IniFormat);
    foreach (const QString &key, m_data.keys()) {
        if (key == scRunProgramDescription || key == scRunProgram || key == scRunProgramArguments)
            continue;
        QVariant value = m_data.value(key);
        if (value.canConvert(QVariant::String))
            value = replacePath(value.toString(), targetDir(), QLatin1String(scRelocatable));
        variables.insert(key, value);
    }
    cfg.setValue(QLatin1String("Variables"), variables);

    QVariantList repos; // Do not change either!
    if (m_data.settings().saveDefaultRepositories()) {
        foreach (const Repository &repo, m_data.settings().defaultRepositories())
            repos.append(QVariant().fromValue(repo));
    }
    cfg.setValue(QLatin1String("DefaultRepositories"), repos);
    cfg.setValue(QLatin1String("FilesForDelayedDeletion"), m_filesForDelayedDeletion);

    cfg.sync();
    if (cfg.status() != QSettingsWrapper::NoError) {
        const QString reason = cfg.status() == QSettingsWrapper::AccessError ? tr("Access error")
            : tr("Format error");
        throw Error(tr("Cannot write installer configuration to %1: %2").arg(iniPath, reason));
    }
    setDefaultFilePermissions(iniPath, DefaultFilePermissions::NonExecutable);

    QFile file(targetDir() + QLatin1Char('/') + QLatin1String("network.xml"));
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QXmlStreamWriter writer(&file);
        writer.setCodec("UTF-8");
        writer.setAutoFormatting(true);
        writer.writeStartDocument();

        writer.writeStartElement(QLatin1String("Network"));
            writer.writeTextElement(QLatin1String("ProxyType"), QString::number(m_data.settings().proxyType()));
            writer.writeStartElement(QLatin1String("Ftp"));
                const QNetworkProxy &ftpProxy = m_data.settings().ftpProxy();
                writer.writeTextElement(QLatin1String("Host"), ftpProxy.hostName());
                writer.writeTextElement(QLatin1String("Port"), QString::number(ftpProxy.port()));
                writer.writeTextElement(QLatin1String("Username"), ftpProxy.user());
                writer.writeTextElement(QLatin1String("Password"), ftpProxy.password());
            writer.writeEndElement();
            writer.writeStartElement(QLatin1String("Http"));
                const QNetworkProxy &httpProxy = m_data.settings().httpProxy();
                writer.writeTextElement(QLatin1String("Host"), httpProxy.hostName());
                writer.writeTextElement(QLatin1String("Port"), QString::number(httpProxy.port()));
                writer.writeTextElement(QLatin1String("Username"), httpProxy.user());
                writer.writeTextElement(QLatin1String("Password"), httpProxy.password());
            writer.writeEndElement();

            writer.writeStartElement(QLatin1String("Repositories"));
            foreach (const Repository &repo, m_data.settings().userRepositories()) {
                writer.writeStartElement(QLatin1String("Repository"));
                    writer.writeTextElement(QLatin1String("Host"), repo.url().toString());
                    writer.writeTextElement(QLatin1String("Username"), repo.username());
                    writer.writeTextElement(QLatin1String("Password"), repo.password());
                    writer.writeTextElement(QLatin1String("Enabled"), QString::number(repo.isEnabled()));
                writer.writeEndElement();
            }
            writer.writeEndElement();
        writer.writeEndElement();
    }
    setDefaultFilePermissions(&file, DefaultFilePermissions::NonExecutable);
}

void PackageManagerCorePrivate::readMaintenanceConfigFiles(const QString &targetDir)
{
    QSettingsWrapper cfg(targetDir + QLatin1Char('/') + m_data.settings().maintenanceToolIniFile(),
        QSettingsWrapper::IniFormat);
    const QVariantHash v = cfg.value(QLatin1String("Variables")).toHash(); // Do not change to
    // QVariantMap! Breaks reading from existing .ini files, cause the variant types do not match.
    for (QVariantHash::const_iterator it = v.constBegin(); it != v.constEnd(); ++it) {
        if (!m_data.contains(it.key()) || m_data.value(it.key()).isNull())
            m_data.setValue(it.key(), replacePath(it.value().toString(), QLatin1String(scRelocatable), targetDir));
    }
    QSet<Repository> repos;
    const QVariantList variants = cfg.value(QLatin1String("DefaultRepositories"))
        .toList(); // Do not change either!
    foreach (const QVariant &variant, variants)
        repos.insert(variant.value<Repository>());
    if (!repos.isEmpty())
        m_data.settings().setDefaultRepositories(repos);

    m_filesForDelayedDeletion = cfg.value(QLatin1String("FilesForDelayedDeletion")).toStringList();

    QFile file(targetDir + QLatin1String("/network.xml"));
    if (!file.open(QIODevice::ReadOnly))
        return;

    QXmlStreamReader reader(&file);
    while (!reader.atEnd()) {
        switch (reader.readNext()) {
            case QXmlStreamReader::StartElement: {
                if (reader.name() == QLatin1String("Network")) {
                    while (reader.readNextStartElement()) {
                        const QStringRef name = reader.name();
                        if (name == QLatin1String("Ftp")) {
                            m_data.settings().setFtpProxy(readProxy(reader));
                        } else if (name == QLatin1String("Http")) {
                            m_data.settings().setHttpProxy(readProxy(reader));
                        } else if (reader.name() == QLatin1String("Repositories")) {
                            m_data.settings().addUserRepositories(readRepositories(reader, false));
                        } else if (name == QLatin1String("ProxyType")) {
                            m_data.settings().setProxyType(Settings::ProxyType(reader.readElementText().toInt()));
                        } else {
                            reader.skipCurrentElement();
                        }
                    }
                }
            }   break;

            case QXmlStreamReader::Invalid: {
                qCWarning(QInstaller::lcInstallerInstallLog) << reader.errorString();
            }   break;

            default:
                break;
        }
    }
}

void PackageManagerCorePrivate::callBeginInstallation(const QList<Component*> &componentList)
{
    foreach (Component *component, componentList)
        component->beginInstallation();
}

void PackageManagerCorePrivate::stopProcessesForUpdates(const QList<Component*> &components)
{
    QStringList processList;
    foreach (const Component *component, components)
        processList << m_core->replaceVariables(component->stopProcessForUpdateRequests());

    std::sort(processList.begin(), processList.end());
    processList.erase(std::unique(processList.begin(), processList.end()), processList.end());
    if (processList.isEmpty())
        return;

    uint retryCount = 5;
    while (true) {
        const QStringList processes = checkRunningProcessesFromList(processList);
        if (processes.isEmpty())
            return;

        const QMessageBox::StandardButton button =
            MessageBoxHandler::warning(MessageBoxHandler::currentBestSuitParent(),
            QLatin1String("stopProcessesForUpdates"), tr("Stop Processes"), tr("These processes "
            "should be stopped to continue:\n\n%1").arg(QDir::toNativeSeparators(processes
            .join(QLatin1String("\n")))), QMessageBox::Retry | QMessageBox::Ignore
            | QMessageBox::Cancel, QMessageBox::Cancel);
        if (button == QMessageBox::Ignore)
            return;
        if (button == QMessageBox::Cancel) {
            m_core->setCanceled();
            throw Error(tr("Installation canceled by user"));
        }
        if (!m_core->isCommandLineInstance())
            continue;

        // Do not allow infinite retries with cli instance
        if (--retryCount == 0)
            throw Error(tr("Retry count exceeded"));
    }
}

int PackageManagerCorePrivate::countProgressOperations(const OperationList &operations)
{
    int operationCount = 0;
    foreach (Operation *operation, operations) {
        if (QObject *operationObject = dynamic_cast<QObject*> (operation)) {
            const QMetaObject *const mo = operationObject->metaObject();
            if (mo->indexOfSignal(QMetaObject::normalizedSignature("progressChanged(double)")) > -1)
                operationCount++;
        }
    }
    return operationCount;
}

int PackageManagerCorePrivate::countProgressOperations(const QList<Component*> &components)
{
    int operationCount = 0;
    foreach (Component *component, components)
        operationCount += countProgressOperations(component->operations());

    return operationCount;
}

void PackageManagerCorePrivate::connectOperationToInstaller(Operation *const operation, double operationPartSize)
{
    Q_ASSERT(operationPartSize);
    QObject *const operationObject = dynamic_cast< QObject*> (operation);
    if (operationObject != nullptr) {
        const QMetaObject *const mo = operationObject->metaObject();
        if (mo->indexOfSignal(QMetaObject::normalizedSignature("outputTextChanged(QString)")) > -1) {
            connect(operationObject, SIGNAL(outputTextChanged(QString)), ProgressCoordinator::instance(),
                SLOT(emitDetailTextChanged(QString)));
        }

        if (mo->indexOfSlot(QMetaObject::normalizedSignature("cancelOperation()")) > -1)
            connect(m_core, SIGNAL(installationInterrupted()), operationObject, SLOT(cancelOperation()));

        if (mo->indexOfSignal(QMetaObject::normalizedSignature("progressChanged(double)")) > -1) {
            ProgressCoordinator::instance()->registerPartProgress(operationObject,
                SIGNAL(progressChanged(double)), operationPartSize);
        }
    }
}

Operation *PackageManagerCorePrivate::createPathOperation(const QFileInfo &fileInfo,
    const QString &componentName)
{
    const bool isDir = fileInfo.isDir();
    // create an operation with the dir/ file as target, it will get deleted on undo
    Operation *operation = createOwnedOperation(QLatin1String(isDir ? "Mkdir" : "Copy"));
    if (isDir)
        operation->setValue(QLatin1String("createddir"), fileInfo.absoluteFilePath());
    operation->setValue(QLatin1String("component"), componentName);
    operation->setArguments(isDir ? QStringList() << fileInfo.absoluteFilePath()
        : QStringList() << QString() << fileInfo.absoluteFilePath());
    return operation;
}

/*!
    This creates fake operations which remove stuff which was registered for uninstallation afterwards
*/
void PackageManagerCorePrivate::registerPathsForUninstallation(
    const QList<QPair<QString, bool> > &pathsForUninstallation, const QString &componentName)
{
    if (pathsForUninstallation.isEmpty())
        return;

    QList<QPair<QString, bool> >::const_iterator it;
    for (it = pathsForUninstallation.begin(); it != pathsForUninstallation.end(); ++it) {
        const bool wipe = it->second;
        const QString path = replaceVariables(it->first);

        const QFileInfo fi(path);
        // create a copy operation with the file as target -> it will get deleted on undo
        Operation *op = createPathOperation(fi, componentName);
        if (fi.isDir())
            op->setValue(QLatin1String("forceremoval"), wipe ? scTrue : scFalse);
        addPerformed(takeOwnedOperation(op));

        // get recursive afterwards
        if (fi.isDir() && !wipe) {
            QDirIterator dirIt(path, QDir::Hidden | QDir::AllEntries | QDir::System
                | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
            while (dirIt.hasNext()) {
                dirIt.next();
                op = createPathOperation(dirIt.fileInfo(), componentName);
                addPerformed(takeOwnedOperation(op));
            }
        }
    }
}

void PackageManagerCorePrivate::writeMaintenanceToolBinary(QFile *const input, qint64 size, bool writeBinaryLayout)
{
    QString maintenanceToolRenamedName = maintenanceToolName() + QLatin1String(".new");
    qCDebug(QInstaller::lcInstallerInstallLog) << "Writing maintenance tool:" << maintenanceToolRenamedName;
    ProgressCoordinator::instance()->emitLabelAndDetailTextChanged(tr("Writing maintenance tool."));

    QFile out(generateTemporaryFileName());
    QInstaller::openForWrite(&out); // throws an exception in case of error

    if (!input->seek(0))
        throw Error(tr("Failed to seek in file %1: %2").arg(input->fileName(), input->errorString()));

    QInstaller::appendData(&out, input, size);
    if (writeBinaryLayout) {

        QDir resourcePath(QFileInfo(maintenanceToolRenamedName).dir());
#ifdef Q_OS_MACOS
        if (!resourcePath.path().endsWith(QLatin1String("Contents/MacOS")))
            throw Error(tr("Maintenance tool is not a bundle"));
        resourcePath.cdUp();
        resourcePath.cd(QLatin1String("Resources"));
#endif
        // It's a bit odd to have only the magic in the data file, but this simplifies
        // other code a lot (since installers don't have any appended data either)
        QFile dataOut(generateTemporaryFileName());
        QInstaller::openForWrite(&dataOut);
        QInstaller::appendInt64(&dataOut, 0);   // operations start
        QInstaller::appendInt64(&dataOut, 0);   // operations end
        QInstaller::appendInt64(&dataOut, 0);   // resource count
        QInstaller::appendInt64(&dataOut, 4 * sizeof(qint64));   // data block size
        QInstaller::appendInt64(&dataOut, BinaryContent::MagicUninstallerMarker);
        QInstaller::appendInt64(&dataOut, BinaryContent::MagicCookie);

        {
            QFile dummy(resourcePath.filePath(QLatin1String("installer.dat")));
            if (dummy.exists() && !dummy.remove()) {
                throw Error(tr("Cannot remove data file \"%1\": %2").arg(dummy.fileName(),
                    dummy.errorString()));
            }
        }

        if (!dataOut.rename(resourcePath.filePath(QLatin1String("installer.dat")))) {
            throw Error(tr("Cannot write maintenance tool data to %1: %2").arg(dataOut.fileName(),
                dataOut.errorString()));
        }
        setDefaultFilePermissions(&dataOut, DefaultFilePermissions::NonExecutable);
    }

    {
        QFile dummy(maintenanceToolRenamedName);
        if (dummy.exists() && !dummy.remove()) {
            throw Error(tr("Cannot remove data file \"%1\": %2").arg(dummy.fileName(),
                dummy.errorString()));
        }
    }

    if (!out.copy(maintenanceToolRenamedName)) {
        throw Error(tr("Cannot write maintenance tool to \"%1\": %2").arg(maintenanceToolRenamedName,
            out.errorString()));
    }

    QFile mt(maintenanceToolRenamedName);
    if (setDefaultFilePermissions(&mt, DefaultFilePermissions::Executable))
        qCDebug(QInstaller::lcInstallerInstallLog) << "Wrote permissions for maintenance tool.";
    else
        qCWarning(QInstaller::lcInstallerInstallLog) << "Failed to write permissions for maintenance tool.";

    if (out.exists() && !out.remove()) {
        qCWarning(QInstaller::lcInstallerInstallLog) << tr("Cannot remove temporary data file \"%1\": %2")
            .arg(out.fileName(), out.errorString());
    }
}

void PackageManagerCorePrivate::writeMaintenanceToolBinaryData(QFileDevice *output, QFile *const input,
    const OperationList &performedOperations, const BinaryLayout &layout)
{
    const qint64 dataBlockStart = output->pos();

    QVector<Range<qint64> >resourceSegments;
    QVector<Range<qint64> >existingResourceSegments = layout.metaResourceSegments;

    const QString newDefaultResource = m_core->value(QString::fromLatin1("DefaultResourceReplacement"));
    if (!newDefaultResource.isEmpty()) {
        QFile file(newDefaultResource);
        if (file.open(QIODevice::ReadOnly)) {
            resourceSegments.append(Range<qint64>::fromStartAndLength(output->pos(), file.size()));
            QInstaller::appendData(output, &file, file.size());
            existingResourceSegments.remove(0);

            file.remove();  // clear all possible leftovers
            m_core->setValue(QString::fromLatin1("DefaultResourceReplacement"), QString());
        } else {
            qCWarning(QInstaller::lcInstallerInstallLog) << "Cannot replace default resource with"
                << QDir::toNativeSeparators(newDefaultResource);
        }
    }

    foreach (const Range<qint64> &segment, existingResourceSegments) {
        input->seek(segment.start());
        resourceSegments.append(Range<qint64>::fromStartAndLength(output->pos(), segment.length()));
        QInstaller::appendData(output, input, segment.length());
    }

    const qint64 operationsStart = output->pos();
    QInstaller::appendInt64(output, performedOperations.count());
    foreach (Operation *operation, performedOperations) {
        QInstaller::appendString(output, operation->name());
        QInstaller::appendString(output, operation->toXml().toString());

        // for the ui not to get blocked
        qApp->processEvents();
    }
    QInstaller::appendInt64(output, performedOperations.count());
    const qint64 operationsEnd = output->pos();

    // we don't save any component-indexes.
    const qint64 numComponents = 0;
    QInstaller::appendInt64(output, numComponents); // for the indexes
    // we don't save any components.
    const qint64 compIndexStart = output->pos();
    QInstaller::appendInt64(output, numComponents); // and 2 times number of components,
    QInstaller::appendInt64(output, numComponents); // one before and one after the components
    const qint64 compIndexEnd = output->pos();

    QInstaller::appendInt64Range(output, Range<qint64>::fromStartAndEnd(compIndexStart, compIndexEnd)
        .moved(-dataBlockStart));
    foreach (const Range<qint64> segment, resourceSegments)
        QInstaller::appendInt64Range(output, segment.moved(-dataBlockStart));
    QInstaller::appendInt64Range(output, Range<qint64>::fromStartAndEnd(operationsStart, operationsEnd)
        .moved(-dataBlockStart));
    QInstaller::appendInt64(output, layout.metaResourceSegments.count());
    // data block size, from end of .exe to end of file
    QInstaller::appendInt64(output, output->pos() + 3 * sizeof(qint64) -dataBlockStart);
    QInstaller::appendInt64(output, BinaryContent::MagicUninstallerMarker);
}

void PackageManagerCorePrivate::writeMaintenanceTool(OperationList performedOperations)
{
    if (m_disableWriteMaintenanceTool) {
        qCDebug(QInstaller::lcInstallerInstallLog()) << "Maintenance tool writing disabled.";
        return;
    }

    bool gainedAdminRights = false;
    if (!directoryWritable(targetDir())) {
        m_core->gainAdminRights();
        gainedAdminRights = true;
    }

    const QString targetAppDirPath = QFileInfo(maintenanceToolName()).path();
    if (!QDir().exists(targetAppDirPath)) {
        // create the directory containing the maintenance tool (like a bundle structure on macOS...)
        Operation *op = createOwnedOperation(QLatin1String("Mkdir"));
        op->setArguments(QStringList() << targetAppDirPath);
        performOperationThreaded(op, Operation::Backup);
        performOperationThreaded(op);
        performedOperations.append(takeOwnedOperation(op));
    }

#ifdef Q_OS_MACOS
    // if it is a bundle, we need some stuff in it...
    const QString sourceAppDirPath = QCoreApplication::applicationDirPath();
    if (isInstaller() && QInstaller::isInBundle(sourceAppDirPath)) {
        Operation *op = createOwnedOperation(QLatin1String("Copy"));
        op->setArguments(QStringList() << (sourceAppDirPath + QLatin1String("/../PkgInfo"))
            << (targetAppDirPath + QLatin1String("/../PkgInfo")));
        performOperationThreaded(op, Operation::Backup);
        performOperationThreaded(op);

        // copy Info.plist to target directory
        op = createOwnedOperation(QLatin1String("Copy"));
        op->setArguments(QStringList() << (sourceAppDirPath + QLatin1String("/../Info.plist"))
            << (targetAppDirPath + QLatin1String("/../Info.plist")));
        performOperationThreaded(op, Operation::Backup);
        performOperationThreaded(op);

        // patch the Info.plist after copying it
        QFile sourcePlist(sourceAppDirPath + QLatin1String("/../Info.plist"));
        QInstaller::openForRead(&sourcePlist);
        QFile targetPlist(targetAppDirPath + QLatin1String("/../Info.plist"));
        QInstaller::openForWrite(&targetPlist);

        QTextStream in(&sourcePlist);
        QTextStream out(&targetPlist);
        const QString before = QLatin1String("<string>") + QFileInfo(QCoreApplication::applicationFilePath())
            .fileName() + QLatin1String("</string>");
        const QString after = QLatin1String("<string>") + QFileInfo(maintenanceToolName()).baseName()
            + QLatin1String("</string>");
        while (!in.atEnd())
            out << in.readLine().replace(before, after) << endl;

        // copy qt_menu.nib if it exists
        op = createOwnedOperation(QLatin1String("Mkdir"));
        op->setArguments(QStringList() << (targetAppDirPath + QLatin1String("/../Resources/qt_menu.nib")));
        if (!op->performOperation()) {
            qCWarning(QInstaller::lcInstallerInstallLog) << "ERROR in Mkdir operation:"
                << op->errorString();
        }

        op = createOwnedOperation(QLatin1String("CopyDirectory"));
        op->setArguments(QStringList() << (sourceAppDirPath + QLatin1String("/../Resources/qt_menu.nib"))
            << (targetAppDirPath + QLatin1String("/../Resources/qt_menu.nib")));
        performOperationThreaded(op);

        op = createOwnedOperation(QLatin1String("Mkdir"));
        op->setArguments(QStringList() << (QFileInfo(targetAppDirPath).path() + QLatin1String("/Resources")));
        performOperationThreaded(op, Operation::Backup);
        performOperationThreaded(op);

        // copy application icons if it exists.
        const QString icon = QFileInfo(QCoreApplication::applicationFilePath()).fileName()
            + QLatin1String(".icns");
        op = createOwnedOperation(QLatin1String("Copy"));
        op->setArguments(QStringList() << (sourceAppDirPath + QLatin1String("/../Resources/") + icon)
            << (targetAppDirPath + QLatin1String("/../Resources/") + icon));
        performOperationThreaded(op, Operation::Backup);
        performOperationThreaded(op);

        // finally, copy everything within Frameworks and plugins
        op = createOwnedOperation(QLatin1String("CopyDirectory"));
        op->setArguments(QStringList() << (sourceAppDirPath + QLatin1String("/../Frameworks"))
            << (targetAppDirPath + QLatin1String("/../Frameworks")));
        performOperationThreaded(op);

        op = createOwnedOperation(QLatin1String("CopyDirectory"));
        op->setArguments(QStringList() << (sourceAppDirPath + QLatin1String("/../plugins"))
            << (targetAppDirPath + QLatin1String("/../plugins")));
        performOperationThreaded(op);
    }
#endif

    try {
        // 1 - check if we have a installer base replacement
        //   |--- if so, write out the new tool and remove the replacement
        //   |--- remember to restart and that we need to replace the original binary
        //
        // 2 - if we do not have a replacement, try to open the binary data file as input
        //   |--- try to read the binary layout
        //      |--- on error (see 2.1)
        //          |--- remember we might to append uncompressed resource data (see 3)
        //          |--- set the installer or maintenance binary as input to take over binary data
        //          |--- in case we did not have a replacement, write out an new maintenance tool binary
        //              |--- remember that we need to replace the original binary
        //
        // 3 - open a new binary data file
        //   |--- try to write out the binary data based on the loaded input file (see 2)
        //      |--- on error (see 3.1)
        //          |--- if we wrote a new maintenance tool, take this as output - if not, write out a new
        //                  one and set it as output file, remember we did this
        //          |--- append the binary data based on the loaded input file (see 2), make sure we force
        //                 uncompressing the resource section if we read from a binary data file (see 4.1).
        //
        // 4 - force a deferred rename on the .dat file (see 4.1)
        // 5 - force a deferred rename on the maintenance file (see 5.1)

        // 2.1 - Error cases are: no data file (in fact we are the installer or an old installation),
        //          could not find the data file magic cookie (unknown .dat file), failed to read binary
        //          layout (mostly likely the resource section or we couldn't seek inside the file)
        //
        // 3.1 - most likely the commit operation will fail
        // 4.1 - if 3 failed, this makes sure the .dat file will get removed and on the next run all
        //          binary data is read from the maintenance tool, otherwise it get replaced be the new one
        // 5.1 - this will only happen -if- we wrote out a new binary

        bool newBinaryWritten = false;
        bool replacementExists = false;
        const QString installerBaseBinary = replaceVariables(m_installerBaseBinaryUnreplaced);
        if (!installerBaseBinary.isEmpty() && QFileInfo(installerBaseBinary).exists()) {
            qCDebug(QInstaller::lcInstallerInstallLog) << "Got a replacement installer base binary:"
                << installerBaseBinary;

            QFile replacementBinary(installerBaseBinary);
            try {
                QInstaller::openForRead(&replacementBinary);
                writeMaintenanceToolBinary(&replacementBinary, replacementBinary.size(), true);
                qCDebug(QInstaller::lcInstallerInstallLog) << "Wrote the binary with the new replacement.";

                newBinaryWritten = true;
                replacementExists = true;
            } catch (const Error &error) {
                qCWarning(QInstaller::lcInstallerInstallLog) << error.message();
            }

            if (!replacementBinary.remove()) {
                // Is there anything more sensible we can do with this error? I think not. It's not serious
                // enough for throwing / aborting the process.
                qCDebug(QInstaller::lcInstallerInstallLog) << "Cannot remove installer base binary"
                    << installerBaseBinary << "after updating the maintenance tool:"
                    << replacementBinary.errorString();
            } else {
                qCDebug(QInstaller::lcInstallerInstallLog) << "Removed installer base binary"
                    << installerBaseBinary << "after updating the maintenance tool.";
            }
            m_installerBaseBinaryUnreplaced.clear();
        } else if (!installerBaseBinary.isEmpty() && !QFileInfo(installerBaseBinary).exists()) {
            qCWarning(QInstaller::lcInstallerInstallLog) << "The current maintenance tool could not be updated."
                << installerBaseBinary << "does not exist. Please fix the \"setInstallerBaseBinary"
                "(<temp_installer_base_binary_path>)\" call in your script.";
        }

        QFile input;
        BinaryLayout layout;
        const QString dataFile = targetDir() + QLatin1Char('/') + m_data.settings().maintenanceToolName()
            + QLatin1String(".dat");
        try {
            if (isInstaller()) {
                if (QFile::exists(dataFile)) {
                    qCWarning(QInstaller::lcInstallerInstallLog) << "Found binary data file" << dataFile
                        << "but deliberately not used. Running as installer requires to read the "
                        "resources from the application binary.";
                }
                throw Error();
            }
            input.setFileName(dataFile);
            QInstaller::openForRead(&input);
            layout = BinaryContent::binaryLayout(&input, BinaryContent::MagicCookieDat);
        } catch (const Error &/*error*/) {
            // We are only here when using installer
            QString binaryName = installerBinaryPath();
            // On Mac data is always in a separate file so that the binary can be signed.
            // On other platforms data is in separate file only after install so that the
            // maintenancetool sign does not break.
#ifdef Q_OS_MACOS
            QDir dataPath(QFileInfo(binaryName).dir());
            dataPath.cdUp();
            dataPath.cd(QLatin1String("Resources"));
            input.setFileName(dataPath.filePath(QLatin1String("installer.dat")));
#else
            input.setFileName(binaryName);
#endif
            QInstaller::openForRead(&input);
            layout = BinaryContent::binaryLayout(&input, BinaryContent::MagicCookie);

            if (!newBinaryWritten) {
                newBinaryWritten = true;
                QFile tmp(binaryName);
                QInstaller::openForRead(&tmp);
#ifdef Q_OS_MACOS
                writeMaintenanceToolBinary(&tmp, tmp.size(), true);
#else
                writeMaintenanceToolBinary(&tmp, layout.endOfBinaryContent - layout.binaryContentSize, true);
#endif
            }
        }

        performedOperations = sortOperationsBasedOnComponentDependencies(performedOperations);
        m_core->setValue(QLatin1String("installedOperationAreSorted"), QLatin1String("true"));

        try {
            QFile file(generateTemporaryFileName());
            QInstaller::openForWrite(&file);
            writeMaintenanceToolBinaryData(&file, &input, performedOperations, layout);
            QInstaller::appendInt64(&file, BinaryContent::MagicCookieDat);

            QFile dummy(dataFile + QLatin1String(".new"));
            if (dummy.exists() && !dummy.remove()) {
                throw Error(tr("Cannot remove data file \"%1\": %2").arg(dummy.fileName(),
                    dummy.errorString()));
            }

            if (!file.rename(dataFile + QLatin1String(".new"))) {
                throw Error(tr("Cannot write maintenance tool binary data to %1: %2")
                    .arg(file.fileName(), file.errorString()));
            }
            setDefaultFilePermissions(&file, DefaultFilePermissions::NonExecutable);
        } catch (const Error &/*error*/) {
            if (!newBinaryWritten) {
                newBinaryWritten = true;
                QFile tmp(isInstaller() ? installerBinaryPath() : maintenanceToolName());
                QInstaller::openForRead(&tmp);
                BinaryLayout tmpLayout = BinaryContent::binaryLayout(&tmp, BinaryContent::MagicCookie);
                writeMaintenanceToolBinary(&tmp, tmpLayout.endOfBinaryContent
                    - tmpLayout.binaryContentSize, false);
            }

            QFile file(maintenanceToolName() + QLatin1String(".new"));
            QInstaller::openForAppend(&file);
            file.seek(file.size());
            writeMaintenanceToolBinaryData(&file, &input, performedOperations, layout);
            QInstaller::appendInt64(&file, BinaryContent::MagicCookie);
        }
        input.close();
        if (m_core->isInstaller())
            registerMaintenanceTool();
        writeMaintenanceConfigFiles();
        deferredRename(dataFile + QLatin1String(".new"), dataFile, false);

        if (newBinaryWritten) {
            const bool restart = replacementExists && isUpdater() && (!statusCanceledOrFailed()) && m_needsHardRestart;
            deferredRename(maintenanceToolName() + QLatin1String(".new"), maintenanceToolName(), restart);
            qCDebug(QInstaller::lcInstallerInstallLog) << "Maintenance tool restart:"
                << (restart ? "true." : "false.");
        }
    } catch (const Error &err) {
        setStatus(PackageManagerCore::Failure);
        if (gainedAdminRights)
            m_core->dropAdminRights();
        m_needToWriteMaintenanceTool = false;
        throw err;
    }

    if (gainedAdminRights)
        m_core->dropAdminRights();

    commitSessionOperations();

    m_needToWriteMaintenanceTool = false;
}

void PackageManagerCorePrivate::writeOfflineBaseBinary()
{
    qint64 size;
    QFile input(installerBinaryPath());

    QInstaller::openForRead(&input);
#ifndef Q_OS_MACOS
    BinaryLayout layout = BinaryContent::binaryLayout(&input, BinaryContent::MagicCookie);
    size = layout.endOfExectuable;
#else
    // On macOS the data is on a separate file so we can just get the size
    size = input.size();
#endif

    const QString offlineBinaryTempName = offlineBinaryName() + QLatin1String(".new");
    qCDebug(QInstaller::lcInstallerInstallLog) << "Writing offline base binary:" << offlineBinaryTempName;
    ProgressCoordinator::instance()->emitLabelAndDetailTextChanged(tr("Writing offline base binary."));

    QFile out(generateTemporaryFileName());
    QInstaller::openForWrite(&out); // throws an exception in case of error

    if (!input.seek(0))
        throw Error(tr("Failed to seek in file %1: %2").arg(input.fileName(), input.errorString()));

    QInstaller::appendData(&out, &input, size);

    {
        // Check if we have an existing binary, for any reason
        QFile dummy(offlineBinaryTempName);
        if (dummy.exists() && !dummy.remove()) {
            throw Error(tr("Cannot remove file \"%1\": %2").arg(dummy.fileName(),
                dummy.errorString()));
        }
        // Offline binary name might contain non-existing leading directories
        const QString offlineBinaryAbsolutePath = QFileInfo(offlineBinaryTempName).absolutePath();
        QDir dummyDir(offlineBinaryAbsolutePath);
        if (!dummyDir.exists() && !dummyDir.mkpath(offlineBinaryAbsolutePath)) {
            throw Error(tr("Cannot create directory \"%1\".").arg(dummyDir.absolutePath()));
        }
    }

    if (!out.copy(offlineBinaryTempName)) {
        throw Error(tr("Cannot write offline binary to \"%1\": %2").arg(offlineBinaryTempName,
            out.errorString()));
    }

    if (out.exists() && !out.remove()) {
        qCWarning(QInstaller::lcInstallerInstallLog) << tr("Cannot remove temporary file \"%1\": %2")
            .arg(out.fileName(), out.errorString());
    }
}

QString PackageManagerCorePrivate::registerPath()
{
#ifdef Q_OS_WIN
    QString guid = m_data.value(scProductUUID).toString();
    if (guid.isEmpty()) {
        guid = QUuid::createUuid().toString();
        m_data.setValue(scProductUUID, guid);
        writeMaintenanceConfigFiles(); // save uuid persistently
    }

    QString path = QLatin1String("HKEY_CURRENT_USER");
    if (m_data.value(scAllUsers, scFalse).toString() == scTrue)
        path = QLatin1String("HKEY_LOCAL_MACHINE");

    return path + QLatin1String("\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\")
        + guid;
#endif
    return QString();
}

bool PackageManagerCorePrivate::runInstaller()
{
    bool adminRightsGained = false;
    try {
        setStatus(PackageManagerCore::Running);
        emit installationStarted(); // resets also the ProgressCoordninator

        // to have some progress for writeMaintenanceTool
        ProgressCoordinator::instance()->addReservePercentagePoints(1);

        const QString target = QDir::cleanPath(targetDir().replace(QLatin1Char('\\'), QLatin1Char('/')));
        if (target.isEmpty())
            throw Error(tr("Variable 'TargetDir' not set."));

        if (!QDir(target).exists()) {
            const QString &pathToTarget = target.mid(0, target.lastIndexOf(QLatin1Char('/')));
            if (!QDir(pathToTarget).exists()) {
                Operation *pathToTargetOp = createOwnedOperation(QLatin1String("Mkdir"));
                pathToTargetOp->setArguments(QStringList() << pathToTarget);
                if (!performOperationThreaded(pathToTargetOp)) {
                    adminRightsGained = m_core->gainAdminRights();
                    if (!performOperationThreaded(pathToTargetOp))
                        throw Error(pathToTargetOp->errorString());
                }
            }
        } else if (QDir(target).exists()) {
            if (!directoryWritable(targetDir()))
                adminRightsGained = m_core->gainAdminRights();
        }

        // add the operation to create the target directory
        Operation *mkdirOp = createOwnedOperation(QLatin1String("Mkdir"));
        mkdirOp->setArguments(QStringList() << target);
        mkdirOp->setValue(QLatin1String("forceremoval"), true);
        mkdirOp->setValue(QLatin1String("uninstall-only"), true);

        performOperationThreaded(mkdirOp, Operation::Backup);
        if (!performOperationThreaded(mkdirOp)) {
            // if we cannot create the target dir, we try to activate the admin rights
            adminRightsGained = m_core->gainAdminRights();
            if (!performOperationThreaded(mkdirOp))
                throw Error(mkdirOp->errorString());
        }
        setDefaultFilePermissions(target, DefaultFilePermissions::Executable);

        const QString remove = m_core->value(scRemoveTargetDir);
        if (QVariant(remove).toBool())
            addPerformed(takeOwnedOperation(mkdirOp));

        // to show that there was some work
        ProgressCoordinator::instance()->addManualPercentagePoints(1);
        ProgressCoordinator::instance()->emitLabelAndDetailTextChanged(tr("Preparing the installation..."));

        const QList<Component*> componentsToInstall = m_core->orderedComponentsToInstall();
        qCDebug(QInstaller::lcInstallerInstallLog) << "Install size:" << componentsToInstall.size()
            << "components";

        callBeginInstallation(componentsToInstall);
        stopProcessesForUpdates(componentsToInstall);

        if (m_dependsOnLocalInstallerBinary && !KDUpdater::pathIsOnLocalDevice(qApp->applicationFilePath())) {
            throw Error(tr("It is not possible to install from network location"));
        }

        if (!adminRightsGained) {
            foreach (Component *component, componentsToInstall) {
                if (component->value(scRequiresAdminRights, scFalse) == scFalse)
                    continue;

                m_core->gainAdminRights();
                m_core->dropAdminRights();
                break;
            }
        }

        const double downloadPartProgressSize = double(1) / double(3);
        double componentsInstallPartProgressSize = double(2) / double(3);
        const int downloadedArchivesCount = m_core->downloadNeededArchives(downloadPartProgressSize);

        // if there was no download we have the whole progress for installing components
        if (!downloadedArchivesCount)
            componentsInstallPartProgressSize = double(1);

        // Force an update on the components xml as the install dir might have changed.
        m_localPackageHub->setFileName(componentsXmlPath());
        // Clear the packages as we might install into an already existing installation folder.
        m_localPackageHub->clearPackageInfos();
        // also update the application name, might be set from a script as well
        m_localPackageHub->setApplicationName(m_data.value(QLatin1String("ProductName"),
            m_data.settings().applicationName()).toString());
        m_localPackageHub->setApplicationVersion(QLatin1String(QUOTE(IFW_REPOSITORY_FORMAT_VERSION)));

        const int progressOperationCount = countProgressOperations(componentsToInstall)
            // add one more operation as we support progress
            + (PackageManagerCore::createLocalRepositoryFromBinary() ? 1 : 0);
        double progressOperationSize = componentsInstallPartProgressSize / progressOperationCount;

        foreach (Component *component, componentsToInstall)
            installComponent(component, progressOperationSize, adminRightsGained);

        if (m_core->isOfflineOnly() && PackageManagerCore::createLocalRepositoryFromBinary()) {
            emit m_core->titleMessageChanged(tr("Creating local repository"));
            ProgressCoordinator::instance()->emitLabelAndDetailTextChanged(QString());
            ProgressCoordinator::instance()->emitLabelAndDetailTextChanged(tr("Creating local repository"));

            Operation *createRepo = createOwnedOperation(QLatin1String("CreateLocalRepository"));
            if (createRepo) {
                QString binaryFile = QCoreApplication::applicationFilePath();
#ifdef Q_OS_MACOS
                // The installer binary on macOS does not contain the binary content, it's put into
                // the resources folder as separate file. Adjust the actual binary path. No error
                // checking here since we will fail later while reading the binary content.
                QDir resourcePath(QFileInfo(binaryFile).dir());
                resourcePath.cdUp();
                resourcePath.cd(QLatin1String("Resources"));
                binaryFile = resourcePath.filePath(QLatin1String("installer.dat"));
#endif
                createRepo->setValue(QLatin1String("uninstall-only"), true);
                createRepo->setArguments(QStringList() << binaryFile << target
                    + QLatin1String("/repository"));

                connectOperationToInstaller(createRepo, progressOperationSize);

                bool success = performOperationThreaded(createRepo);
                if (!success) {
                    adminRightsGained = m_core->gainAdminRights();
                    success = performOperationThreaded(createRepo);
                }

                if (success) {
                    QSet<Repository> repos;
                    foreach (Repository repo, m_data.settings().defaultRepositories()) {
                        repo.setEnabled(false);
                        repos.insert(repo);
                    }
                    repos.insert(Repository(QUrl::fromUserInput(createRepo
                        ->value(QLatin1String("local-repo")).toString()), true));
                    m_data.settings().setDefaultRepositories(repos);
                    addPerformed(takeOwnedOperation(createRepo));
                } else {
                    MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(),
                        QLatin1String("installationError"), tr("Error"), createRepo->errorString());
                    createRepo->undoOperation();
                }
            }
        }
        emit m_core->titleMessageChanged(tr("Creating Maintenance Tool"));

        m_needToWriteMaintenanceTool = true;
        m_core->writeMaintenanceTool();

        // fake a possible wrong value to show a full progress bar
        const int progress = ProgressCoordinator::instance()->progressInPercentage();
        // usually this should be only the reserved one from the beginning
        if (progress < 100)
            ProgressCoordinator::instance()->addManualPercentagePoints(100 - progress);

        ProgressCoordinator::instance()->emitLabelAndDetailTextChanged(tr("\nInstallation finished!"));

        if (adminRightsGained)
            m_core->dropAdminRights();

        setStatus(PackageManagerCore::Success);
        emit installationFinished();
    } catch (const Error &err) {
        if (m_core->status() != PackageManagerCore::Canceled) {
            setStatus(PackageManagerCore::Failure);
            MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(),
                QLatin1String("installationError"), tr("Error"), err.message());
            qCDebug(QInstaller::lcInstallerInstallLog) << "ROLLING BACK operations="
                << m_performedOperationsCurrentSession.count();
        }

        m_core->rollBackInstallation();

        ProgressCoordinator::instance()->emitLabelAndDetailTextChanged(tr("\nInstallation aborted!"));
        if (adminRightsGained)
            m_core->dropAdminRights();
        emit installationFinished();
        return false;
    }
    return true;
}

bool PackageManagerCorePrivate::runPackageUpdater()
{
    bool adminRightsGained = false;
    if (m_completeUninstall) {
        return runUninstaller();
    }
    try {
        setStatus(PackageManagerCore::Running);
        emit installationStarted(); //resets also the ProgressCoordninator

        //to have some progress for the cleanup/write component.xml step
        ProgressCoordinator::instance()->addReservePercentagePoints(1);

        // check if we need admin rights and ask before the action happens
        if (!directoryWritable(targetDir()))
            adminRightsGained = m_core->gainAdminRights();

        const QList<Component *> componentsToInstall = m_core->orderedComponentsToInstall();
        qCDebug(QInstaller::lcInstallerInstallLog) << "Install size:" << componentsToInstall.size()
            << "components";

        callBeginInstallation(componentsToInstall);
        stopProcessesForUpdates(componentsToInstall);

        if (m_dependsOnLocalInstallerBinary && !KDUpdater::pathIsOnLocalDevice(qApp->applicationFilePath())) {
            throw Error(tr("It is not possible to run that operation from a network location"));
        }

        bool updateAdminRights = false;
        if (!adminRightsGained) {
            foreach (Component *component, componentsToInstall) {
                if (component->value(scRequiresAdminRights, scFalse) == scFalse)
                    continue;

                updateAdminRights = true;
                break;
            }
        }

        OperationList undoOperations;
        OperationList nonRevertedOperations;
        QHash<QString, Component *> componentsByName;

        // order the operations in the right component dependency order
        // next loop will save the needed operations in reverse order for uninstallation
        OperationList performedOperationsOld = m_performedOperationsOld;
        if (m_core->value(QLatin1String("installedOperationAreSorted")) != QLatin1String("true"))
            performedOperationsOld = sortOperationsBasedOnComponentDependencies(m_performedOperationsOld);

        // build a list of undo operations based on the checked state of the component
        foreach (Operation *operation, performedOperationsOld) {
            const QString &name = operation->value(QLatin1String("component")).toString();
            Component *component = componentsByName.value(name, nullptr);
            if (!component)
                component = m_core->componentByName(PackageManagerCore::checkableName(name));
            if (component)
                componentsByName.insert(name, component);

            if (isUpdater()) {
                // We found the component, the component is not scheduled for update, the dependency solver
                // did not add the component as install dependency and there is no replacement, keep it.
                if ((component && !component->updateRequested() && !componentsToInstall.contains(component)
                    && !m_componentsToReplaceUpdaterMode.contains(name))) {
                        nonRevertedOperations.append(operation);
                        continue;
                }

                // There is a replacement, but the replacement is not scheduled for update, keep it as well.
                if (m_componentsToReplaceUpdaterMode.contains(name)
                    && !m_componentsToReplaceUpdaterMode.value(name).first->updateRequested()) {
                        nonRevertedOperations.append(operation);
                        continue;
                }
            } else if (isPackageManager()) {
                // We found the component, the component is still checked and the dependency solver did not
                // add the component as install dependency, keep it.
                if (component
                        && component->installAction() == ComponentModelHelper::KeepInstalled
                        && !componentsToInstall.contains(component)) {
                    nonRevertedOperations.append(operation);
                    continue;
                }

                // There is a replacement, but the replacement is not scheduled for update, keep it as well.
                if (m_componentsToReplaceAllMode.contains(name)
                    && !m_componentsToReplaceAllMode.value(name).first->isSelectedForInstallation()) {
                        nonRevertedOperations.append(operation);
                        continue;
                }
            } else {
                Q_ASSERT_X(false, Q_FUNC_INFO, "Invalid package manager mode!");
            }

            // Filter out the create target dir undo operation, it's only needed for full uninstall.
            // Note: We filter for unnamed operations as well, since old installations had the remove target
            //  dir operation without the "uninstall-only", which will result in a complete uninstallation
            //  during an update for the maintenance tool.
            if (operation->value(QLatin1String("uninstall-only")).toBool()
                || operation->value(QLatin1String("component")).toString().isEmpty()) {
                    nonRevertedOperations.append(operation);
                    continue;
            }

            // uninstallation should be in reverse order so prepend it here
            undoOperations.prepend(operation);
            updateAdminRights |= operation->value(QLatin1String("admin")).toBool();
        }

        // we did not request admin rights till we found out that a component/ undo needs admin rights
        if (updateAdminRights && !adminRightsGained) {
            m_core->gainAdminRights();
            m_core->dropAdminRights();
        }

        double undoOperationProgressSize = 0;
        const double downloadPartProgressSize = double(2) / double(5);
        double componentsInstallPartProgressSize = double(3) / double(5);
        if (undoOperations.count() > 0) {
            undoOperationProgressSize = double(1) / double(5);
            componentsInstallPartProgressSize = downloadPartProgressSize;
            undoOperationProgressSize /= countProgressOperations(undoOperations);
        }

        ProgressCoordinator::instance()->emitLabelAndDetailTextChanged(tr("Preparing the installation..."));

        // following, we download the needed archives
        m_core->downloadNeededArchives(downloadPartProgressSize);

        if (undoOperations.count() > 0) {
            ProgressCoordinator::instance()->emitLabelAndDetailTextChanged(tr("Removing deselected components..."));
            runUndoOperations(undoOperations, undoOperationProgressSize, adminRightsGained, true);
        }
        m_performedOperationsOld = nonRevertedOperations; // these are all operations left: those not reverted

        const double progressOperationCount = countProgressOperations(componentsToInstall);
        const double progressOperationSize = componentsInstallPartProgressSize / progressOperationCount;

        foreach (Component *component, componentsToInstall)
            installComponent(component, progressOperationSize, adminRightsGained);

        emit m_core->titleMessageChanged(tr("Creating Maintenance Tool"));

        commitSessionOperations(); //end session, move ops to "old"
        m_needToWriteMaintenanceTool = true;

        // fake a possible wrong value to show a full progress bar
        const int progress = ProgressCoordinator::instance()->progressInPercentage();
        // usually this should be only the reserved one from the beginning
        if (progress < 100)
            ProgressCoordinator::instance()->addManualPercentagePoints(100 - progress);
        ProgressCoordinator::instance()->emitLabelAndDetailTextChanged(tr("\nUpdate finished!"));

        if (adminRightsGained)
            m_core->dropAdminRights();
        if (m_foundEssentialUpdate)
            setStatus(PackageManagerCore::EssentialUpdated);
        else
            setStatus(PackageManagerCore::Success);
        emit installationFinished();
    } catch (const Error &err) {
        if (m_core->status() != PackageManagerCore::Canceled) {
            setStatus(PackageManagerCore::Failure);
            MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(),
                QLatin1String("installationError"), tr("Error"), err.message());
            qCDebug(QInstaller::lcInstallerInstallLog) << "ROLLING BACK operations="
                << m_performedOperationsCurrentSession.count();
        }

        m_core->rollBackInstallation();

        ProgressCoordinator::instance()->emitLabelAndDetailTextChanged(tr("\nUpdate aborted!"));
        if (adminRightsGained)
            m_core->dropAdminRights();
        emit installationFinished();
        return false;
    }
    return true;
}

bool PackageManagerCorePrivate::runUninstaller()
{
    emit uninstallationStarted();
    bool adminRightsGained = false;

    try {
        setStatus(PackageManagerCore::Running);

        // check if we need to run elevated and ask before the action happens
        if (!directoryWritable(targetDir()))
            adminRightsGained = m_core->gainAdminRights();

        OperationList undoOperations = m_performedOperationsOld;
        std::reverse(undoOperations.begin(), undoOperations.end());

        bool updateAdminRights = false;
        if (!adminRightsGained) {
            foreach (Operation *op, m_performedOperationsOld) {
                updateAdminRights |= op->value(QLatin1String("admin")).toBool();
                if (updateAdminRights)
                    break;  // an operation needs elevation to be able to perform their undo
            }
        }

        // We did not yet request elevated permissions but they are required.
        if (updateAdminRights && !adminRightsGained) {
            m_core->gainAdminRights();
            m_core->dropAdminRights();
        }

        const int uninstallOperationCount = countProgressOperations(undoOperations);
        const double undoOperationProgressSize = double(1) / double(uninstallOperationCount);

        runUndoOperations(undoOperations, undoOperationProgressSize, adminRightsGained, false);
        // No operation delete here, as all old undo operations are deleted in the destructor.

        deleteMaintenanceTool();    // this will also delete the TargetDir on Windows

        // If not on Windows, we need to remove TargetDir manually.
#ifndef Q_OS_WIN
        if (QVariant(m_core->value(scRemoveTargetDir)).toBool() && !targetDir().isEmpty()) {
            if (updateAdminRights && !adminRightsGained)
                adminRightsGained = m_core->gainAdminRights();
            removeDirectoryThreaded(targetDir(), true);
            qCDebug(QInstaller::lcInstallerInstallLog) << "Complete uninstallation was chosen.";
        }
#endif

        unregisterMaintenanceTool();
        m_needToWriteMaintenanceTool = false;
        setStatus(PackageManagerCore::Success);
    } catch (const Error &err) {
        if (m_core->status() != PackageManagerCore::Canceled) {
            setStatus(PackageManagerCore::Failure);
            MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(),
                QLatin1String("installationError"), tr("Error"), err.message());
        }
    }

    const bool success = (m_core->status() == PackageManagerCore::Success);
    if (adminRightsGained)
        m_core->dropAdminRights();

    ProgressCoordinator::instance()->emitLabelAndDetailTextChanged(QString::fromLatin1("\n%1").arg(
        success ? tr("Uninstallation completed successfully.") : tr("Uninstallation aborted.")));

    emit uninstallationFinished();
    return success;
}

bool PackageManagerCorePrivate::runOfflineGenerator()
{
    const QString offlineBinaryTempName = offlineBinaryName() + QLatin1String(".new");
    const QString tempSettingsFilePath = generateTemporaryFileName()
        + QDir::separator() + QLatin1String("config.xml");

    bool adminRightsGained = false;
    try {
        setStatus(PackageManagerCore::Running);
        emit offlineGenerationStarted(); // Resets also the ProgressCoordninator

        // Never write the maintenance tool when generating offline installer
        m_needToWriteMaintenanceTool = false;

        // Reserve some progress for the final writing, it should take
        // only a fraction of time spent in the download part
        ProgressCoordinator::instance()->addReservePercentagePoints(1);

        const QString target = QDir::cleanPath(targetDir().replace(QLatin1Char('\\'), QLatin1Char('/')));
        if (target.isEmpty())
            throw Error(tr("Variable 'TargetDir' not set."));

        // Create target directory for installer to be generated
        if (!QDir(target).exists()) {
            if (!QDir().mkpath(target)) {
                adminRightsGained = m_core->gainAdminRights();
                // Try again with admin privileges
                if (!QDir().mkpath(target))
                    throw Error(tr("Cannot create target directory for installer."));
            }
        } else if (QDir(target).exists()) {
            if (!directoryWritable(targetDir()))
                adminRightsGained = m_core->gainAdminRights();
        }
        setDefaultFilePermissions(target, DefaultFilePermissions::Executable);

        // Show that there was some work
        ProgressCoordinator::instance()->addManualPercentagePoints(1);
        ProgressCoordinator::instance()->emitLabelAndDetailTextChanged(tr("Preparing offline generation..."));

        const QList<Component*> componentsToInclude = m_core->orderedComponentsToInstall();
        qCDebug(QInstaller::lcInstallerInstallLog) << "Included components:" << componentsToInclude.size();

        // Give full part progress size as this is the most time consuming step
        m_core->downloadNeededArchives(double(1));

        const QString installerBaseReplacement = replaceVariables(m_offlineBaseBinaryUnreplaced);
        if (!installerBaseReplacement.isEmpty() && QFileInfo(installerBaseReplacement).exists()) {
            qCDebug(QInstaller::lcInstallerInstallLog) << "Got a replacement installer base binary:"
                << offlineBinaryTempName;

            if (!QFile::copy(installerBaseReplacement, offlineBinaryTempName)) {
                qCWarning(QInstaller::lcInstallerInstallLog) << QString::fromLatin1("Cannot copy "
                    "replacemement binary to temporary location \"%1\" from \"%2\".")
                    .arg(offlineBinaryTempName, installerBaseReplacement);
            }
        } else {
            writeOfflineBaseBinary();
        }

        ProgressCoordinator::instance()->emitLabelAndDetailTextChanged(tr("Preparing installer configuration..."));

        // Create copy of internal config file and data, remove repository related elements
        QInstaller::trimmedCopyConfigData(m_data.settingsFilePath(), tempSettingsFilePath,
            QStringList() << scRemoteRepositories << scRepositoryCategories);

        // Assemble final installer binary
        QInstallerTools::BinaryCreatorArgs args;
        args.target = offlineBinaryName();
#ifdef Q_OS_MACOS
        // Target is a disk image on macOS
        args.target.append(QLatin1String(".dmg"));
#endif
        args.templateBinary = offlineBinaryTempName;
        args.offlineOnly = true;
        args.configFile = tempSettingsFilePath;
        args.ftype = QInstallerTools::Include;
        // Add possible custom resources
        if (!m_offlineGeneratorResourceCollections.isEmpty())
            args.resources = m_offlineGeneratorResourceCollections;

        foreach (auto component, componentsToInclude) {
            args.filteredPackages.append(component->name());
            args.repositoryDirectories.append(component->localTempPath());
        }
        args.repositoryDirectories.removeDuplicates();

        ProgressCoordinator::instance()->emitLabelAndDetailTextChanged(tr("Creating the installer..."));

        QString errorMessage;
        if (QInstallerTools::createBinary(args, errorMessage) == EXIT_FAILURE) {
            throw Error(tr("Failed to create offline installer. %1").arg(errorMessage));
        } else {
            setStatus(PackageManagerCore::Success);
        }

        // Fake a possible wrong value to show a full progress
        const int progress = ProgressCoordinator::instance()->progressInPercentage();
        // This should be only the reserved one (1) from the beginning
        if (progress < 100)
            ProgressCoordinator::instance()->addManualPercentagePoints(100 - progress);

    } catch (const Error &err) {
        if (m_core->status() != PackageManagerCore::Canceled) {
            setStatus(PackageManagerCore::Failure);
            MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(),
                QLatin1String("installationError"), tr("Error"), err.message());
        }
    }
    QFile tempBinary(offlineBinaryTempName);
    if (tempBinary.exists() && !tempBinary.remove()) {
        qCWarning(QInstaller::lcInstallerInstallLog) << tr("Cannot remove temporary file \"%1\": %2")
            .arg(tempBinary.fileName(), tempBinary.errorString());
    }

    QDir tempSettingsDir(QFileInfo(tempSettingsFilePath).absolutePath());
    if (tempSettingsDir.exists() && !tempSettingsDir.removeRecursively()) {
        qCWarning(QInstaller::lcInstallerInstallLog) << tr("Cannot remove temporary directory \"%1\".")
            .arg(tempSettingsDir.path());
    }

    const bool success = m_core->status() == PackageManagerCore::Success;
    if (adminRightsGained)
        m_core->dropAdminRights();

    ProgressCoordinator::instance()->emitLabelAndDetailTextChanged(QString::fromLatin1("\n%1").arg(
        success ? tr("Offline generation completed successfully.") : tr("Offline generation aborted!")));

    emit offlineGenerationFinished();
    return success;
}

void PackageManagerCorePrivate::installComponent(Component *component, double progressOperationSize,
    bool adminRightsGained)
{
    const OperationList operations = component->operations();
    if (!component->operationsCreatedSuccessfully())
        m_core->setCanceled();

    const int opCount = operations.count();
    // show only components which do something, MinimumProgress is only for progress calculation safeness
    bool showDetailsLog = false;
    if (opCount > 1 || (opCount == 1 && operations.at(0)->name() != QLatin1String("MinimumProgress"))) {
        ProgressCoordinator::instance()->emitLabelAndDetailTextChanged(tr("\nInstalling component %1")
            .arg(component->displayName()));
        showDetailsLog = true;
    }

    foreach (Operation *operation, operations) {
        if (statusCanceledOrFailed())
            throw Error(tr("Installation canceled by user"));

        // maybe this operations wants us to be admin...
        bool becameAdmin = false;
        if (!adminRightsGained && operation->value(QLatin1String("admin")).toBool()) {
            becameAdmin = m_core->gainAdminRights();
            qCDebug(QInstaller::lcInstallerInstallLog) << operation->name() << "as admin:" << becameAdmin;
        }

        connectOperationToInstaller(operation, progressOperationSize);
        connectOperationCallMethodRequest(operation);

        // allow the operation to backup stuff before performing the operation
        performOperationThreaded(operation, Operation::Backup);

        bool ignoreError = false;
        bool ok = performOperationThreaded(operation);
        while (!ok && !ignoreError && m_core->status() != PackageManagerCore::Canceled) {
            qCDebug(QInstaller::lcInstallerInstallLog) << QString::fromLatin1("Operation \"%1\" with arguments "
                "\"%2\" failed: %3").arg(operation->name(), operation->arguments()
                .join(QLatin1String("; ")), operation->errorString());
            const QMessageBox::StandardButton button =
                MessageBoxHandler::warning(MessageBoxHandler::currentBestSuitParent(),
                QLatin1String("installationErrorWithCancel"), tr("Installer Error"),
                tr("Error during installation process (%1):\n%2").arg(component->name(),
                operation->errorString()),
                QMessageBox::Retry | QMessageBox::Ignore | QMessageBox::Cancel, QMessageBox::Cancel);

            if (button == QMessageBox::Retry)
                ok = performOperationThreaded(operation);
            else if (button == QMessageBox::Ignore)
                ignoreError = true;
            else if (button == QMessageBox::Cancel)
                m_core->interrupt();
        }

        if (ok || operation->error() > Operation::InvalidArguments) {
            // Remember that the operation was performed, that allows us to undo it if a following operation
            // fails or if this operation failed but still needs an undo call to cleanup.
            addPerformed(operation);
        }

        if (becameAdmin)
            m_core->dropAdminRights();

        if (!ok && !ignoreError)
            throw Error(operation->errorString());

        if (((component->value(scEssential, scFalse) == scTrue) || (component->value(scForcedUpdate, scFalse) == scTrue))
             && !m_core->isCommandLineInstance()) {
            m_needsHardRestart = true;
        }
    }

    registerPathsForUninstallation(component->pathsForUninstallation(), component->name());

    if (!component->stopProcessForUpdateRequests().isEmpty()) {
        Operation *stopProcessForUpdatesOp = KDUpdater::UpdateOperationFactory::instance()
            .create(QLatin1String("FakeStopProcessForUpdate"), m_core);
        const QStringList arguments(component->stopProcessForUpdateRequests().join(QLatin1String(",")));
        stopProcessForUpdatesOp->setArguments(arguments);
        addPerformed(stopProcessForUpdatesOp);
        stopProcessForUpdatesOp->setValue(QLatin1String("component"), component->name());
    }

    // now mark the component as installed
    m_localPackageHub->addPackage(component->name(),
                                  component->value(scVersion),
                                  component->value(scDisplayName),
                                  component->value(scTreeName),
                                  component->value(scDescription),
                                  component->dependencies(),
                                  component->autoDependencies(),
                                  component->forcedInstallation(),
                                  component->isVirtual(),
                                  component->value(scUncompressedSize).toULongLong(),
                                  component->value(scInheritVersion),
                                  component->isCheckable(),
                                  component->isExpandedByDefault(),
                                  component->value(scContentSha1));
    m_localPackageHub->writeToDisk();

    component->setInstalled();
    component->markAsPerformedInstallation();

    if (showDetailsLog)
        ProgressCoordinator::instance()->emitDetailTextChanged(tr("Done"));
}

bool PackageManagerCorePrivate::runningProcessesFound()
{
    //Check if there are processes running in the install
    QStringList excludeFiles = m_allowedRunningProcesses;
    excludeFiles.append(maintenanceToolName());

    const QString performModeWarning = m_completeUninstall
        ? QLatin1String("Unable to remove components.")
        : QLatin1String("Unable to update components.");

    QStringList runningProcesses = runningInstallerProcesses(excludeFiles);
    if (!runningProcesses.isEmpty()) {
        qCWarning(QInstaller::lcInstallerInstallLog).noquote().nospace() << performModeWarning
                 << " Please stop these processes: " << runningProcesses << " and try again.";
        return true;
    }
    return false;
}

void PackageManagerCorePrivate::setComponentSelection(const QString &id, Qt::CheckState state)
{
    ComponentModel *model = m_core->isUpdater() ? m_core->updaterComponentModel() : m_core->defaultComponentModel();
    Component *component = m_core->componentByName(id);
    if (!component) {
        qCWarning(QInstaller::lcInstallerInstallLog).nospace()
            << "Unable to set selection for: " << id << ". Component not found.";
        return;
    }
    const QModelIndex &idx = model->indexFromComponentName(component->treeName());
    if (idx.isValid())
        model->setData(idx, state, Qt::CheckStateRole);
}

// -- private

void PackageManagerCorePrivate::deleteMaintenanceTool()
{
#ifdef Q_OS_WIN
    // Since Windows does not support that the maintenance tool deletes itself we have to go with a rather dirty
    // hack. What we do is to create a batchfile that will try to remove the maintenance tool once per second. Then
    // we start that batchfile detached, finished our job and close ourselves. Once that's done the batchfile
    // will succeed in deleting our uninstall.exe and, if the installation directory was created but us and if
    // it's empty after the uninstall, deletes the installation-directory.
    const QString batchfile = QDir::toNativeSeparators(QFileInfo(QDir::tempPath(),
        QLatin1String("uninstall.vbs")).absoluteFilePath());
    QFile f(batchfile);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        throw Error(tr("Cannot prepare uninstall"));

    QTextStream batch(&f);
    batch << "Set fso = WScript.CreateObject(\"Scripting.FileSystemObject\")\n";
    batch << "file = WScript.Arguments.Item(0)\n";
    batch << "folderpath = WScript.Arguments.Item(1)\n";
    batch << "Set folder = fso.GetFolder(folderpath)\n";
    batch << "on error resume next\n";

    batch << "while fso.FileExists(file)\n";
    batch << "    fso.DeleteFile(file)\n";
    batch << "    WScript.Sleep(1000)\n";
    batch << "wend\n";
//    batch << "if folder.SubFolders.Count = 0 and folder.Files.Count = 0 then\n";
    batch << "    Set folder = Nothing\n";
    batch << "    fso.DeleteFolder folderpath, true\n";
//    batch << "end if\n";
    batch << "fso.DeleteFile(WScript.ScriptFullName)\n";

    f.close();

    QStringList arguments;
    arguments << QLatin1String("//Nologo") << batchfile; // execute the batchfile
    arguments << QDir::toNativeSeparators(QFileInfo(installerBinaryPath()).absoluteFilePath());
    if (!m_performedOperationsOld.isEmpty()) {
        const Operation *const op = m_performedOperationsOld.first();
        if (op->name() == QLatin1String("Mkdir")) // the target directory name
            arguments << QDir::toNativeSeparators(QFileInfo(op->arguments().first()).absoluteFilePath());
    }

    if (!QProcessWrapper::startDetached(QLatin1String("cscript"), arguments, QDir::rootPath()))
        throw Error(tr("Cannot start uninstall"));
#else
    // every other platform has no problem if we just delete ourselves now
    QFile maintenanceTool(QFileInfo(installerBinaryPath()).absoluteFilePath());
    maintenanceTool.remove();
# ifdef Q_OS_MACOS
    if (QInstaller::isInBundle(installerBinaryPath())) {
        const QLatin1String cdUp("/../../..");
        removeDirectoryThreaded(QFileInfo(installerBinaryPath() + cdUp).absoluteFilePath());
        QFile::remove(QFileInfo(installerBinaryPath() + cdUp).absolutePath()
            + QLatin1String("/") + configurationFileName());
    } else
# endif
#endif
    {
        // finally remove the components.xml, since it still exists now
        QFile::remove(QFileInfo(installerBinaryPath()).absolutePath() + QLatin1String("/")
            + configurationFileName());
    }
}

void PackageManagerCorePrivate::registerMaintenanceTool()
{
#ifdef Q_OS_WIN
    QSettingsWrapper settings(registerPath(), QSettingsWrapper::NativeFormat);
    settings.setValue(scDisplayName, m_data.value(QLatin1String("ProductName")));
    settings.setValue(QLatin1String("DisplayVersion"), m_data.value(QLatin1String("ProductVersion")));
    const QString maintenanceTool = QDir::toNativeSeparators(maintenanceToolName());
    settings.setValue(QLatin1String("DisplayIcon"), maintenanceTool);
    settings.setValue(scPublisher, m_data.value(scPublisher));
    settings.setValue(QLatin1String("UrlInfoAbout"), m_data.value(QLatin1String("Url")));
    settings.setValue(QLatin1String("Comments"), m_data.value(scTitle));
    settings.setValue(QLatin1String("InstallDate"), QDateTime::currentDateTime().toString());
    settings.setValue(QLatin1String("InstallLocation"), QDir::toNativeSeparators(targetDir()));
    settings.setValue(QLatin1String("UninstallString"), maintenanceTool);
    settings.setValue(QLatin1String("ModifyPath"), QString(maintenanceTool
        + QLatin1String(" --manage-packages")));
    // required disk space of the installed components
    quint64 estimatedSizeKB = m_core->requiredDiskSpace() / 1024;
    // add required space for the maintenance tool
    estimatedSizeKB += QFileInfo(maintenanceTool).size() / 1024;
    if (m_core->createLocalRepositoryFromBinary()) {
        // add required space for a local repository
        quint64 result(0);
        foreach (QInstaller::Component *component,
            m_core->components(PackageManagerCore::ComponentType::All)) {
            result += m_core->size(component, scCompressedSize);
        }
        estimatedSizeKB += result / 1024;
    }
    // Windows can only handle 32bit REG_DWORD (max. recordable installation size is 4TiB)
    const quint64 limit = std::numeric_limits<quint32>::max(); // maximum 32 bit value
    if (estimatedSizeKB <= limit)
        settings.setValue(QLatin1String("EstimatedSize"), static_cast<quint32>(estimatedSizeKB));

    const bool supportsModify = m_core->value(scSupportsModify, scTrue) == scTrue;
    if (supportsModify)
        settings.setValue(QLatin1String("NoModify"), 0);
    else
        settings.setValue(QLatin1String("NoModify"), 1);

    settings.setValue(QLatin1String("NoRepair"), 1);
#endif
}

void PackageManagerCorePrivate::unregisterMaintenanceTool()
{
#ifdef Q_OS_WIN
    QSettingsWrapper settings(registerPath(), QSettingsWrapper::NativeFormat);
    settings.remove(QString());
#endif
}

void PackageManagerCorePrivate::runUndoOperations(const OperationList &undoOperations, double progressSize,
    bool adminRightsGained, bool deleteOperation)
{
    try {
        foreach (Operation *undoOperation, undoOperations) {
            if (statusCanceledOrFailed())
                throw Error(tr("Installation canceled by user"));

            bool becameAdmin = false;
            if (!adminRightsGained && undoOperation->value(QLatin1String("admin")).toBool())
                becameAdmin = m_core->gainAdminRights();

            connectOperationToInstaller(undoOperation, progressSize);
            qCDebug(QInstaller::lcInstallerInstallLog) << "undo operation=" << undoOperation->name();

            bool ignoreError = false;
            bool ok = performOperationThreaded(undoOperation, Operation::Undo);

            const QString componentName = undoOperation->value(QLatin1String("component")).toString();

            if (!componentName.isEmpty()) {
                while (!ok && !ignoreError && m_core->status() != PackageManagerCore::Canceled) {
                    const QMessageBox::StandardButton button =
                        MessageBoxHandler::warning(MessageBoxHandler::currentBestSuitParent(),
                        QLatin1String("installationErrorWithIgnore"), tr("Installer Error"),
                        tr("Error during uninstallation process:\n%1").arg(undoOperation->errorString()),
                        QMessageBox::Retry | QMessageBox::Ignore, QMessageBox::Ignore);

                    if (button == QMessageBox::Retry)
                        ok = performOperationThreaded(undoOperation, Operation::Undo);
                    else if (button == QMessageBox::Ignore)
                        ignoreError = true;
                }
                Component *component = m_core->componentByName(PackageManagerCore::checkableName(componentName));
                if (!component)
                    component = componentsToReplace().value(componentName).second;
                if (component) {
                    component->setUninstalled();
                    m_localPackageHub->removePackage(component->name());
                }
            }

            if (becameAdmin)
                m_core->dropAdminRights();

            if (deleteOperation)
                delete undoOperation;
        }
    } catch (const Error &error) {
        m_localPackageHub->writeToDisk();
        throw Error(error.message());
    } catch (...) {
        m_localPackageHub->writeToDisk();
        throw Error(tr("Unknown error"));
    }
    m_localPackageHub->writeToDisk();
}

PackagesList PackageManagerCorePrivate::remotePackages()
{
    if (m_updates && m_updateFinder)
        return m_updateFinder->updates();

    m_updates = false;
    delete m_updateFinder;

    m_updateFinder = new KDUpdater::UpdateFinder;
    m_updateFinder->setAutoDelete(false);
    m_updateFinder->setPackageSources(m_packageSources);
    m_updateFinder->setLocalPackageHub(m_localPackageHub);
    m_updateFinder->run();

    if (m_updateFinder->updates().isEmpty()) {
        setStatus(PackageManagerCore::Failure, tr("Cannot retrieve remote tree %1.")
            .arg(m_updateFinder->errorString()));
        return PackagesList();
    }

    m_updates = true;
    return m_updateFinder->updates();
}

PackagesList PackageManagerCorePrivate::compressedPackages()
{
    if (m_compressedUpdates && m_compressedFinder)
        return m_compressedFinder->updates();
    m_compressedUpdates = false;
    delete m_compressedFinder;

    m_compressedFinder = new KDUpdater::UpdateFinder;
    m_compressedFinder->setAutoDelete(false);
    m_compressedFinder->addCompressedPackage(true);
    m_compressedFinder->setPackageSources(m_compressedPackageSources);

    m_compressedFinder->setLocalPackageHub(m_localPackageHub);
    m_compressedFinder->run();
    if (m_compressedFinder->updates().isEmpty()) {
        setStatus(PackageManagerCore::Failure, tr("Cannot retrieve remote tree %1.")
            .arg(m_compressedFinder->errorString()));
        return PackagesList();
    }
    m_compressedUpdates = true;
    return m_compressedFinder->updates();
}

/*!
    Returns a hash containing the installed package name and it's associated package information. If
    the application is running in installer mode or the local components file could not be parsed, the
    hash is empty.
*/
LocalPackagesHash PackageManagerCorePrivate::localInstalledPackages()
{
    if (isInstaller())
        return LocalPackagesHash();

    LocalPackagesHash installedPackages;
    if (m_localPackageHub->error() != LocalPackageHub::NoError) {
        if (m_localPackageHub->fileName().isEmpty())
            m_localPackageHub->setFileName(componentsXmlPath());
        else
            m_localPackageHub->refresh();

        if (m_localPackageHub->applicationName().isEmpty())
            m_localPackageHub->setApplicationName(m_data.settings().applicationName());
        if (m_localPackageHub->applicationVersion().isEmpty())
            m_localPackageHub->setApplicationVersion(QLatin1String(QUOTE(IFW_REPOSITORY_FORMAT_VERSION)));
    }

    if (m_localPackageHub->error() != LocalPackageHub::NoError) {
        setStatus(PackageManagerCore::Failure, tr("Failure to read packages from %1.")
            .arg(componentsXmlPath()));
    }

    foreach (const LocalPackage &package, m_localPackageHub->packageInfos()) {
        if (statusCanceledOrFailed())
            break;
        installedPackages.insert(package.name, package);
    }

    return installedPackages;
}

bool PackageManagerCorePrivate::fetchMetaInformationFromRepositories(DownloadType type)
{
    m_updates = false;
    m_repoFetched = false;
    m_updateSourcesAdded = false;

    try {
        m_metadataJob.addDownloadType(type);
        m_metadataJob.start();
        m_metadataJob.waitForFinished();
    } catch (Error &error) {
        setStatus(PackageManagerCore::Failure, tr("Cannot retrieve meta information: %1")
            .arg(error.message()));
        return m_repoFetched;
    }

    if (m_metadataJob.error() != Job::NoError) {
        switch (m_metadataJob.error()) {
            case QInstaller::UserIgnoreError:
                break;  // we can simply ignore this error, the user knows about it
            default:
                setStatus(PackageManagerCore::Failure, m_metadataJob.errorString());
                return m_repoFetched;
        }
    }

    m_repoFetched = true;
    return m_repoFetched;
}

bool PackageManagerCorePrivate::fetchMetaInformationFromCompressedRepositories()
{
    bool compressedRepoFetched = false;

    m_compressedUpdates = false;
    m_updateSourcesAdded = false;

    try {
        //Tell MetadataJob that only compressed packages needed to be fetched and not all.
        //We cannot do this in general fetch meta method as the compressed packages might be
        //installed after components tree is generated
        m_metadataJob.addDownloadType(DownloadType::CompressedPackage);
        m_metadataJob.start();
        m_metadataJob.waitForFinished();
    } catch (Error &error) {
        setStatus(PackageManagerCore::Failure, tr("Cannot retrieve meta information: %1")
            .arg(error.message()));
        return compressedRepoFetched;
    }

    if (m_metadataJob.error() != Job::NoError) {
        switch (m_metadataJob.error()) {
            case QInstaller::UserIgnoreError:
                break;  // we can simply ignore this error, the user knows about it
            default:
                //Do not change core status here, we can recover if there is invalid
                //compressed repository
                setStatus(m_core->status(), m_metadataJob.errorString());
                return compressedRepoFetched;
        }
    }

    compressedRepoFetched = true;
    return compressedRepoFetched;
}

bool PackageManagerCorePrivate::addUpdateResourcesFromRepositories(bool parseChecksum, bool compressedRepository)
{
    if (!compressedRepository && m_updateSourcesAdded)
        return m_updateSourcesAdded;

    const QList<Metadata> metadata = m_metadataJob.metadata();
    if (metadata.isEmpty()) {
        m_updateSourcesAdded = true;
        return m_updateSourcesAdded;
    }
    if (compressedRepository) {
        m_compressedPackageSources.clear();
    }
    else {
        m_packageSources.clear();
        m_updates = false;
        m_updateSourcesAdded = false;
        if (isInstaller())
            m_packageSources.insert(PackageSource(QUrl(QLatin1String("resource://metadata/")), 0));
    }

    foreach (const Metadata &data, metadata) {
        if (compressedRepository && !data.repository.isCompressed()) {
            continue;
        }
        if (statusCanceledOrFailed())
            return false;

        if (data.directory.isEmpty())
            continue;

        if (parseChecksum) {
            const QString updatesXmlPath = data.directory + QLatin1String("/Updates.xml");
            QFile updatesFile(updatesXmlPath);
            try {
                QInstaller::openForRead(&updatesFile);
            } catch(const Error &e) {
                qCWarning(QInstaller::lcInstallerInstallLog) << "Error opening Updates.xml:"
                    << e.message();
                setStatus(PackageManagerCore::Failure, tr("Cannot add temporary update source information."));
                return false;
            }

            int line = 0;
            int column = 0;
            QString error;
            QDomDocument doc;
            if (!doc.setContent(&updatesFile, &error, &line, &column)) {
                qCWarning(QInstaller::lcInstallerInstallLog).nospace() << "Parse error in file "
                    << updatesFile.fileName() << ": " << error << " at line " << line
                    << " col " << column;
                setStatus(PackageManagerCore::Failure, tr("Cannot add temporary update source information."));
                return false;
            }

            const QDomNode checksum = doc.documentElement().firstChildElement(QLatin1String("Checksum"));
            if (!checksum.isNull())
                m_core->setTestChecksum(checksum.toElement().text().toLower() == scTrue);
        }
        if (compressedRepository)
            m_compressedPackageSources.insert(PackageSource(QUrl::fromLocalFile(data.directory), 1));
        else
            m_packageSources.insert(PackageSource(QUrl::fromLocalFile(data.directory), 1));

        ProductKeyCheck::instance()->addPackagesFromXml(data.directory + QLatin1String("/Updates.xml"));
    }
    if ((compressedRepository && m_compressedPackageSources.count() == 0 ) ||
         (!compressedRepository && m_packageSources.count() == 0)) {
        setStatus(PackageManagerCore::Failure, tr("Cannot find any update source information."));
        return false;
    }

    m_updateSourcesAdded = true;
    return m_updateSourcesAdded;
}

void PackageManagerCorePrivate::restoreCheckState()
{
    if (m_coreCheckedHash.isEmpty())
        return;

    foreach (Component *component, m_coreCheckedHash.keys()) {
        component->setCheckState(m_coreCheckedHash.value(component));
        // Never allow component to be checked when it is unstable
        // and not installed
        if (component->isUnstable() && !component->isInstalled())
            component->setCheckState(Qt::Unchecked);
    }

    m_coreCheckedHash.clear();
    m_componentsToInstallCalculated = false;
}

void PackageManagerCorePrivate::storeCheckState()
{
    m_coreCheckedHash.clear();

   const QList<Component *> components =
       m_core->components(PackageManagerCore::ComponentType::AllNoReplacements);
    foreach (Component *component, components)
        m_coreCheckedHash.insert(component, component->checkState());
}

void PackageManagerCorePrivate::connectOperationCallMethodRequest(Operation *const operation)
{
    QObject *const operationObject = dynamic_cast<QObject *> (operation);
    if (operationObject != nullptr) {
        const QMetaObject *const mo = operationObject->metaObject();
        if (mo->indexOfSignal(QMetaObject::normalizedSignature("requestBlockingExecution(QString)")) > -1) {
            connect(operationObject, SIGNAL(requestBlockingExecution(QString)),
                    this, SLOT(handleMethodInvocationRequest(QString)), Qt::BlockingQueuedConnection);
        }
    }
}

OperationList PackageManagerCorePrivate::sortOperationsBasedOnComponentDependencies(const OperationList &operationList)
{
    OperationList sortedOperations;
    QHash<QString, OperationList> componentOperationHash;

    // sort component unrelated operations to the beginning
    foreach (Operation *operation, operationList) {
        const QString componentName = operation->value(QLatin1String("component")).toString();
        if (componentName.isEmpty())
            sortedOperations.append(operation);
        else
            componentOperationHash[componentName].append(operation);
    }

    Graph<QString> componentGraph;  // create the complete component graph
    foreach (const Component* node, m_core->components(PackageManagerCore::ComponentType::All)) {
        componentGraph.addNode(node->name());
        componentGraph.addEdges(node->name(), m_core->parseNames(node->dependencies()));
    }

    const QStringList resolvedComponents = componentGraph.sort();
    if (componentGraph.hasCycle()) {
        throw Error(tr("Dependency cycle between components \"%1\" and \"%2\" detected.")
            .arg(componentGraph.cycle().first, componentGraph.cycle().second));
    }
    foreach (const QString &componentName, resolvedComponents)
        sortedOperations.append(componentOperationHash.value(componentName));

    return sortedOperations;
}

void PackageManagerCorePrivate::handleMethodInvocationRequest(const QString &invokableMethodName)
{
    QObject *obj = QObject::sender();
    if (obj != nullptr)
        QMetaObject::invokeMethod(obj, qPrintable(invokableMethodName));
}

void PackageManagerCorePrivate::processFilesForDelayedDeletion()
{
    if (m_filesForDelayedDeletion.isEmpty())
        return;

    const QStringList filesForDelayedDeletion = std::move(m_filesForDelayedDeletion);
    foreach (const QString &i, filesForDelayedDeletion) {
        QFile file(i);   //TODO: this should happen asnyc and report errors, I guess
        if (file.exists() && !file.remove()) {
            qCWarning(QInstaller::lcInstallerInstallLog) << "Cannot delete file " << qPrintable(i) <<
                ": " << qPrintable(file.errorString());

            m_filesForDelayedDeletion << i; // try again next time
        }
    }
}

void PackageManagerCorePrivate::findExecutablesRecursive(const QString &path, const QStringList &excludeFiles, QStringList *result)
{
    QDirIterator it(path, QDir::NoDotAndDotDot | QDir::Executable | QDir::Files | QDir::System, QDirIterator::Subdirectories );

    while (it.hasNext())
        result->append(QDir::toNativeSeparators(it.next().toLower()));

    foreach (const QString &process, excludeFiles)
        result->removeAll(QDir::toNativeSeparators(process.toLower()));
}

QStringList PackageManagerCorePrivate::runningInstallerProcesses(const QStringList &excludeFiles)
{
    QStringList resultFiles;
    findExecutablesRecursive(QCoreApplication::applicationDirPath(), excludeFiles, &resultFiles);
    return checkRunningProcessesFromList(resultFiles);
}

bool PackageManagerCorePrivate::calculateComponentsAndRun()
{
    QString htmlOutput;
    bool componentsOk = m_core->calculateComponents(&htmlOutput);
    if (statusCanceledOrFailed()) {
        qCDebug(QInstaller::lcInstallerInstallLog) << "Installation canceled.";
    } else if (componentsOk && acceptLicenseAgreements()) {
        qCDebug(QInstaller::lcInstallerInstallLog).noquote() << htmlToString(htmlOutput);

        QString spaceInfo;
        const bool spaceOk = m_core->checkAvailableSpace(spaceInfo);
        qCDebug(QInstaller::lcInstallerInstallLog) << spaceInfo;

        if (!spaceOk || !(m_autoConfirmCommand || askUserConfirmCommand())) {
            qCDebug(QInstaller::lcInstallerInstallLog) << "Installation aborted.";
        } else if (m_core->run()) {
            // Write maintenance tool if required
            m_core->writeMaintenanceTool();
            return true;
        }
    }
    return false;
}

bool PackageManagerCorePrivate::acceptLicenseAgreements() const
{
    // Always skip for uninstaller
    if (isUninstaller())
        return true;

    foreach (Component *component, m_core->orderedComponentsToInstall()) {
        // Package manager or updater, no need to accept again as long as
        // the component is installed.
        if (m_core->isMaintainer() && component->isInstalled())
            continue;
        m_core->addLicenseItem(component->licenses());
    }

    QHash<QString, QMap<QString, QString>> priorityHash = m_core->sortedLicenses();
    QStringList priorities = priorityHash.keys();
    priorities.sort();
    for (int i = priorities.length() - 1; i >= 0; --i) {
        QString priority = priorities.at(i);
        QMap<QString, QString> licenses = priorityHash.value(priority);

        QStringList licenseNames = licenses.keys();
        licenseNames.sort(Qt::CaseInsensitive);
        for (QString licenseName : licenseNames) {
            if (m_autoAcceptLicenses
                    || askUserAcceptLicense(licenseName, licenses.value(licenseName))) {
                qCDebug(QInstaller::lcInstallerInstallLog) << "License"
                    << licenseName << "accepted by user.";
            } else {
                qCDebug(QInstaller::lcInstallerInstallLog) << "License"
                    << licenseName<< "not accepted by user. Aborting.";
                return false;
            }
        }
    }
    return true;
}

bool PackageManagerCorePrivate::askUserAcceptLicense(const QString &name, const QString &content) const
{
    qCDebug(QInstaller::lcInstallerInstallLog) << "You must accept "
        "the terms contained in the following license agreement "
        "before continuing with the installation:" << name;

    forever {
        const QString input = m_core->readConsoleLine(QLatin1String("Accept|Reject|Show"));

        if (QString::compare(input, QLatin1String("Accept"), Qt::CaseInsensitive) == 0
                || QString::compare(input, QLatin1String("A"), Qt::CaseInsensitive) == 0) {
            return true;
        } else if (QString::compare(input, QLatin1String("Reject"), Qt::CaseInsensitive) == 0
                || QString::compare(input, QLatin1String("R"), Qt::CaseInsensitive) == 0) {
            return false;
        } else if (QString::compare(input, QLatin1String("Show"), Qt::CaseInsensitive) == 0
                || QString::compare(input, QLatin1String("S"), Qt::CaseInsensitive) == 0) {
            qCDebug(QInstaller::lcInstallerInstallLog).noquote() << content;
        } else {
            qCDebug(QInstaller::lcInstallerInstallLog) << "Unknown answer:" << input;
        }
    }
}

bool PackageManagerCorePrivate::askUserConfirmCommand() const
{
    qCDebug(QInstaller::lcInstallerInstallLog) << "Do you want to continue?";

    forever {
        const QString input = m_core->readConsoleLine(QLatin1String("Yes|No"));

        if (QString::compare(input, QLatin1String("Yes"), Qt::CaseInsensitive) == 0
                || QString::compare(input, QLatin1String("Y"), Qt::CaseInsensitive) == 0) {
            return true;
        } else if (QString::compare(input, QLatin1String("No"), Qt::CaseInsensitive) == 0
                || QString::compare(input, QLatin1String("N"), Qt::CaseInsensitive) == 0) {
            return false;
        } else {
            qCDebug(QInstaller::lcInstallerInstallLog) << "Unknown answer:" << input;
        }
    }
}

bool PackageManagerCorePrivate::packageNeedsUpdate(const LocalPackage &localPackage, const Package *update) const
{
    bool updateNeeded = true;
    const QString contentSha1 = update->data(scContentSha1).toString();
    if (!contentSha1.isEmpty()) {
        if (contentSha1 == localPackage.contentSha1)
            updateNeeded = false;
    } else {
        const QString updateVersion = update->data(scVersion).toString();
        if (KDUpdater::compareVersion(updateVersion, localPackage.version) <= 0)
            updateNeeded = false;
    }
    return updateNeeded;
}

} // namespace QInstaller
