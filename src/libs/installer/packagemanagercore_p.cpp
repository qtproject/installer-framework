/**************************************************************************
**
** Copyright (C) 2024 The Qt Company Ltd.
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
#include "componentalias.h"
#include "componentchecker.h"
#include "globals.h"
#include "binarycreator.h"
#include "loggingutils.h"
#include "concurrentoperationrunner.h"
#include "remoteclient.h"
#include "operationtracer.h"

#include "selfrestarter.h"
#include "filedownloaderfactory.h"
#include "updateoperationfactory.h"
#include "constants.h"

#include <productkeycheck.h>

#include <QSettings>
#include <QtConcurrentRun>
#include <QtConcurrent>
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

static bool filterMissingAliasesToInstall(const QString& component, const QList<ComponentAlias *> packages)
{
    bool packageFound = false;
    for (qsizetype i = 0; i < packages.size(); ++i) {
        packageFound = (packages.at(i)->name() == component);
        if (packageFound)
            break;
    }
    return !packageFound;
}

static bool filterMissingPackagesToInstall(const QString& component, const PackagesList& packages)
{
    bool packageFound = false;
    for (qsizetype i = 0; i < packages.size(); ++i) {
        packageFound = (packages.at(i)->data(scName).toString() == component);
        if (packageFound)
            break;
    }
    return !packageFound;
}

static QString getAppBundlePath() {
    QString appDirPath = QCoreApplication::applicationDirPath();
    QDir dir(appDirPath);
    while (!dir.isRoot()) {
        if (dir.dirName().endsWith(QLatin1String(".app")))
            return dir.absolutePath();
        dir.cdUp();
    }
    return QString();
}

// -- PackageManagerCorePrivate

PackageManagerCorePrivate::PackageManagerCorePrivate(PackageManagerCore *core)
    : m_updateFinder(nullptr)
    , m_aliasFinder(nullptr)
    , m_localPackageHub(std::make_shared<LocalPackageHub>())
    , m_status(PackageManagerCore::Unfinished)
    , m_needsHardRestart(false)
    , m_testChecksum(false)
    , m_launchedAsRoot(AdminAuthorization::hasAdminRights())
    , m_commandLineInstance(false)
    , m_defaultInstall(false)
    , m_userSetBinaryMarker(false)
    , m_checkAvailableSpace(true)
    , m_completeUninstall(false)
    , m_needToWriteMaintenanceTool(false)
    , m_dependsOnLocalInstallerBinary(false)
    , m_autoAcceptLicenses(false)
    , m_disableWriteMaintenanceTool(false)
    , m_autoConfirmCommand(false)
    , m_core(core)
    , m_updates(false)
    , m_aliases(false)
    , m_repoFetched(false)
    , m_updateSourcesAdded(false)
    , m_magicBinaryMarker(0) // initialize with pseudo marker
    , m_magicMarkerSupplement(BinaryContent::Default)
    , m_foundEssentialUpdate(false)
    , m_componentScriptEngine(nullptr)
    , m_controlScriptEngine(nullptr)
    , m_installerCalculator(nullptr)
    , m_uninstallerCalculator(nullptr)
    , m_proxyFactory(nullptr)
    , m_defaultModel(nullptr)
    , m_updaterModel(nullptr)
    , m_componentSortFilterProxyModel(nullptr)
    , m_guiObject(nullptr)
    , m_remoteFileEngineHandler(nullptr)
    , m_datFileName(QString())
#ifdef INSTALLCOMPRESSED
    , m_allowCompressedRepositoryInstall(true)
#else
    , m_allowCompressedRepositoryInstall(false)
#endif
    , m_connectedOperations(0)
{
}

PackageManagerCorePrivate::PackageManagerCorePrivate(PackageManagerCore *core, qint64 magicInstallerMaker,
        const QList<OperationBlob> &performedOperations, const QString &datFileName)
    : m_updateFinder(nullptr)
    , m_aliasFinder(nullptr)
    , m_localPackageHub(std::make_shared<LocalPackageHub>())
    , m_status(PackageManagerCore::Unfinished)
    , m_needsHardRestart(false)
    , m_testChecksum(false)
    , m_launchedAsRoot(AdminAuthorization::hasAdminRights())
    , m_commandLineInstance(false)
    , m_defaultInstall(false)
    , m_userSetBinaryMarker(false)
    , m_checkAvailableSpace(true)
    , m_completeUninstall(false)
    , m_needToWriteMaintenanceTool(false)
    , m_dependsOnLocalInstallerBinary(false)
    , m_autoAcceptLicenses(false)
    , m_disableWriteMaintenanceTool(false)
    , m_autoConfirmCommand(false)
    , m_core(core)
    , m_updates(false)
    , m_aliases(false)
    , m_repoFetched(false)
    , m_updateSourcesAdded(false)
    , m_magicBinaryMarker(magicInstallerMaker)
    , m_magicMarkerSupplement(BinaryContent::Default)
    , m_foundEssentialUpdate(false)
    , m_componentScriptEngine(nullptr)
    , m_controlScriptEngine(nullptr)
    , m_installerCalculator(nullptr)
    , m_uninstallerCalculator(nullptr)
    , m_proxyFactory(nullptr)
    , m_defaultModel(nullptr)
    , m_updaterModel(nullptr)
    , m_componentSortFilterProxyModel(nullptr)
    , m_guiObject(nullptr)
    , m_remoteFileEngineHandler(new RemoteFileEngineHandler)
    , m_datFileName(datFileName)
#ifdef INSTALLCOMPRESSED
    , m_allowCompressedRepositoryInstall(true)
#else
    , m_allowCompressedRepositoryInstall(false)
#endif
    , m_connectedOperations(0)
{
    foreach (const OperationBlob &operation, performedOperations) {
        std::unique_ptr<QInstaller::Operation> op(KDUpdater::UpdateOperationFactory::instance()
            .create(operation.name, core));
        if (!op) {
            qCWarning(QInstaller::lcInstallerInstallLog) << "Failed to load unknown operation"
                << operation.name;
            continue;
        }

        if (!op->fromXml(operation.xml)) {
            qCWarning(QInstaller::lcInstallerInstallLog) << "Failed to load XML for operation"
                << operation.name;
            continue;
        }
        m_performedOperationsOld.append(op.release());
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
    delete m_aliasFinder;
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
bool PackageManagerCorePrivate::performOperationThreaded(Operation *operation,
    Operation::OperationType type)
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

        // after everything is set up, load the scripts if needed and create helper hashes
        // for autodependency and dependency components for quicker search later
        if (loadScript && !loadComponentScripts(components))
            return false;

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
        if (installerCalculator()->solve(components.values()) == false) {
            setStatus(PackageManagerCore::Failure, installerCalculator()->error());
            MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(), QLatin1String("Error"),
                tr("Unresolved dependencies"), installerCalculator()->error());
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
        setStatus(PackageManagerCore::Failure, error.message());

        // TODO: make sure we remove all message boxes inside the library at some point.
        MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(), QLatin1String("Error"),
            tr("Error"), error.message());
        return false;
    }
    return true;
}

bool PackageManagerCorePrivate::buildComponentAliases()
{
    // For now, aliases are only used for command line runs
    if (!m_core->isCommandLineInstance())
        return true;

    {
        const QList<ComponentAlias *> aliasList = componentAliases();
        if (aliasList.isEmpty())
            return true;

        for (const auto *alias : aliasList) {
            // Create a new alias object for package manager core to take ownership of
            ComponentAlias *newAlias = new ComponentAlias(m_core);
            for (const QString &key : alias->keys())
                newAlias->setValue(key, alias->value(key));

            m_componentAliases.insert(alias->name(), newAlias);
        }
    }

    if (m_core->isPackageViewer())
        return true;
    // After aliases are loaded, perform sanity checks:

    // 1. Component check state is changed by alias selection, so store the initial state
    storeCheckState();

    QStringList aliasNamesSelectedForInstall;

    // 2. Get a list of alias names to be installed, dependency aliases needs to be listed first
    // to get proper unstable state for parents
    std::function<void(QStringList)> fetchAliases = [&](QStringList aliases) {
        for (const QString &aliasName : aliases) {
            ComponentAlias *alias = m_componentAliases.value(aliasName);
            if (!alias || aliasNamesSelectedForInstall.contains(aliasName))
                continue;
            if (!aliasNamesSelectedForInstall.contains(aliasName))
                aliasNamesSelectedForInstall.prepend(aliasName);
            fetchAliases(QStringList() << QInstaller::splitStringWithComma(alias->value(scRequiredAliases))
                << QInstaller::splitStringWithComma(alias->value(scOptionalAliases)));
        }
    };
    for (const QString &installComponent : m_componentsToBeInstalled) {
        ComponentAlias *alias = m_componentAliases.value(installComponent);
        if (!alias)
            continue;
        if (!aliasNamesSelectedForInstall.contains(installComponent))
            aliasNamesSelectedForInstall.prepend(installComponent);
        fetchAliases(QStringList() << QInstaller::splitStringWithComma(alias->value(scRequiredAliases))
            << QInstaller::splitStringWithComma(alias->value(scOptionalAliases)));
    }

    Graph<QString> aliasGraph;
    QList<ComponentAlias *> aliasesSelectedForInstall;
    for (auto &aliasName : std::as_const(aliasNamesSelectedForInstall)) {
        ComponentAlias *alias = m_componentAliases.value(aliasName);
        if (!alias)
            continue;
        aliasGraph.addNode(alias->name());
        aliasGraph.addEdges(alias->name(),
            QInstaller::splitStringWithComma(alias->value(scRequiredAliases)) <<
            QInstaller::splitStringWithComma(alias->value(scOptionalAliases)));

        if (!m_core->componentByName(alias->name())) {
            // Name ok, select for sanity check calculation
            alias->setSelected(true);
        } else {
            alias->setUnstable(ComponentAlias::ComponentNameConfict,
                tr("Alias declares name that conflicts with an existing component \"%1\"")
                .arg(alias->name()));
        }
        if (!aliasesSelectedForInstall.contains(alias))
            aliasesSelectedForInstall.append(alias);
    }

    const QList<QString> sortedAliases = aliasGraph.sort();
    // 3. Check for cyclic dependency errors
    if (aliasGraph.hasCycle()) {
        setStatus(PackageManagerCore::Failure, installerCalculator()->error());
        MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(), QLatin1String("Error"),
            tr("Unresolved component aliases"),
            tr("Cyclic dependency between aliases \"%1\" and \"%2\" detected.")
            .arg(aliasGraph.cycle().first, aliasGraph.cycle().second));

        return false;
    }

    // 4. Test for required aliases and components, this triggers setting the
    // alias unstable in case of a broken reference.
    for (const auto &aliasName : sortedAliases) {
        ComponentAlias *alias = m_componentAliases.value(aliasName);
        if (!alias) // sortedAliases may contain dependencies that don't exist, we don't know it yet
            continue;

        alias->components();
        alias->aliases();
    }

    clearInstallerCalculator();
    // 5. Check for other errors preventing resolving components to install
    if (!installerCalculator()->solve(aliasesSelectedForInstall)) {
        setStatus(PackageManagerCore::Failure, installerCalculator()->error());
        MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(), QLatin1String("Error"),
            tr("Unresolved component aliases"), installerCalculator()->error());

        return false;
    }

    for (auto *alias : std::as_const(m_componentAliases))
        alias->setSelected(false);

    // 6. Restore original state
    restoreCheckState();

    return true;
}

template <typename T>
bool PackageManagerCorePrivate::loadComponentScripts(const T &components, const bool postScript)
{
    infoMessage(nullptr, tr("Loading component scripts..."));

    quint64 loadedComponents = 0;
    for (auto *component : components) {
        if (statusCanceledOrFailed())
            return false;

        component->loadComponentScript(postScript);
        ++loadedComponents;

        const int currentProgress = qRound(double(loadedComponents) / components.count() * 100);
        infoProgress(nullptr, currentProgress, 100);
        qApp->processEvents();
    }
    return true;
}

template bool PackageManagerCorePrivate::loadComponentScripts<QList<Component *>>(const QList<Component *> &, const bool);
template bool PackageManagerCorePrivate::loadComponentScripts<QHash<QString, Component *>>(const QHash<QString, Component *> &, const bool);

void PackageManagerCorePrivate::cleanUpComponentEnvironment()
{
    m_componentReplaces.clear();
    m_autoDependencyComponentHash.clear();
    m_localDependencyComponentHash.clear();
    m_localVirtualComponents.clear();
    m_componentByNameHash.clear();
    // clean up registered (downloaded) data
    if (m_core->isMaintainer())
        BinaryFormatEngineHandler::instance()->clear();

    // there could be still some references to already deleted components,
    // so we need to remove the current component script engine
    delete m_componentScriptEngine;
    m_componentScriptEngine = nullptr;

    // Calculators become invalid after clearing components
    clearInstallerCalculator();
    clearUninstallerCalculator();
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
    qDeleteAll(m_componentAliases);
    m_componentAliases.clear();

    QList<QInstaller::Component*> toDelete;

    toDelete << m_rootComponents << m_deletedReplacedComponents;
    m_rootComponents.clear();
    m_rootDependencyReplacements.clear();
    m_deletedReplacedComponents.clear();

    m_componentsToReplaceAllMode.clear();
    m_foundEssentialUpdate = false;

    qDeleteAll(toDelete);
    cleanUpComponentEnvironment();
}

void PackageManagerCorePrivate::clearUpdaterComponentLists()
{

    QSet<Component*> usedComponents(m_updaterComponents.begin(), m_updaterComponents.end());
    usedComponents.unite(QSet<Component*>(m_updaterComponentsDeps.begin(),
                                          m_updaterComponentsDeps.end()));

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
    m_foundEssentialUpdate = false;

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

QHash<QString, QStringList> &PackageManagerCorePrivate::componentReplaces()
{
    return m_componentReplaces;
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
        pmcp->m_installerCalculator = new InstallerCalculator(m_core, pmcp->m_autoDependencyComponentHash);
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

        pmcp->m_uninstallerCalculator = new UninstallerCalculator(m_core,
            pmcp->m_autoDependencyComponentHash, pmcp->m_localDependencyComponentHash, pmcp->m_localVirtualComponents);
    }
    return m_uninstallerCalculator;
}

void PackageManagerCorePrivate::initialize(const QHash<QString, QString> &params)
{
    m_coreCheckedHash.clear();
    m_data = PackageManagerCoreData(params, isInstaller());

#ifdef Q_OS_LINUX
    if (m_launchedAsRoot && isInstaller())
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
        m_packageSources.insert(PackageSource(QUrl(QLatin1String("resource://metadata/")), 1));

    const QString aliasFilePath = m_core->settings().aliasDefinitionsFile();
    if (!aliasFilePath.isEmpty())
        m_aliasSources.insert(AliasSource(AliasSource::SourceFileFormat::Xml, aliasFilePath, -1));

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
    QString filename;
    if (isInstaller())
        filename = m_data.settings().maintenanceToolName();
    else
        filename = QCoreApplication::applicationName();

#if defined(Q_OS_MACOS)
    if (QInstaller::isInBundle(QCoreApplication::applicationDirPath()))
        filename += QLatin1String(".app/Contents/MacOS/") + filename;
#elif defined(Q_OS_WIN)
    filename += QLatin1String(".exe");
#endif
    return QString::fromLatin1("%1/%2").arg(targetDir()).arg(filename);
}

QString PackageManagerCorePrivate::maintenanceToolAliasPath() const
{
#ifdef Q_OS_MACOS
    const QString aliasName = m_data.settings().maintenanceToolAlias();
    if (aliasName.isEmpty())
        return QString();

    const bool isRoot = m_core->hasAdminRights();
    const QString applicationsDir = m_core->value(
        isRoot ? QLatin1String("ApplicationsDir") : QLatin1String("ApplicationsDirUser")
    );
    QString maintenanceToolAlias = QString::fromLatin1("%1/%2")
        .arg(applicationsDir, aliasName);
    if (!maintenanceToolAlias.endsWith(QLatin1String(".app")))
        maintenanceToolAlias += QLatin1String(".app");

    return maintenanceToolAlias;
#endif
    return QString();
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

QString PackageManagerCorePrivate::datFileName()
{
    if (m_datFileName.isEmpty())
        m_datFileName = targetDir() + QLatin1Char('/') + m_data.settings().maintenanceToolName() + QLatin1String(".dat");
    return m_datFileName;
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
    QSettingsWrapper cfg(iniPath, QSettings::IniFormat);
    foreach (const QString &key, m_data.keys()) {
        if (key == scRunProgramDescription || key == scRunProgram || key == scRunProgramArguments)
            continue;
        QVariant value = m_data.value(key);
        if (value.canConvert<QString>())
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
        QString outputStr;
        QXmlStreamWriter writer(&outputStr);
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
            writer.writeTextElement(QLatin1String("LocalCachePath"), m_data.settings().localCachePath());
        writer.writeEndElement();

        file.write(outputStr.toUtf8());
    }
    setDefaultFilePermissions(&file, DefaultFilePermissions::NonExecutable);
}

void PackageManagerCorePrivate::readMaintenanceConfigFiles(const QString &targetDir)
{
    QSettingsWrapper cfg(targetDir + QLatin1Char('/') + m_data.settings().maintenanceToolIniFile(),
        QSettings::IniFormat);
    const QVariantHash v = cfg.value(QLatin1String("Variables")).toHash(); // Do not change to
    // QVariantMap! Breaks reading from existing .ini files, cause the variant types do not match.
    for (QVariantHash::const_iterator it = v.constBegin(); it != v.constEnd(); ++it) {
        if (m_data.contains(it.key()) && !m_data.value(it.key()).isNull()) {
            // Exception: StartMenuDir should be permanent after initial installation
            // and must be read from maintenancetool.ini
            if (it.key() != scStartMenuDir)
                continue;
        }
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
                        const QStringView name = reader.name();
                        if (name == QLatin1String("Ftp")) {
                            m_data.settings().setFtpProxy(readProxy(reader));
                        } else if (name == QLatin1String("Http")) {
                            m_data.settings().setHttpProxy(readProxy(reader));
                        } else if (reader.name() == QLatin1String("Repositories")) {
                            m_data.settings().addUserRepositories(readRepositories(reader, false));
                        } else if (name == QLatin1String("ProxyType")) {
                            m_data.settings().setProxyType(Settings::ProxyType(reader.readElementText().toInt()));
                        } else if (name == QLatin1String("LocalCachePath")) {
                            m_data.settings().setLocalCachePath(reader.readElementText());
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
            // create unique object names for progress information track
            operationObject->setObjectName(QLatin1String("operation_%1").arg(QString::number(m_connectedOperations)));
            ProgressCoordinator::instance()->registerPartProgress(operationObject,
                SIGNAL(progressChanged(double)), operationPartSize);
            m_connectedOperations++;
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
        QInstallerTools::createMTDatFile(dataOut);

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

void PackageManagerCorePrivate::writeMaintenanceToolAppBundle(OperationList &performedOperations)
{
#ifdef Q_OS_MACOS
    const QString targetAppDirPath = QFileInfo(maintenanceToolName()).path();
    if (!QDir().exists(targetAppDirPath)) {
        // create the directory containing the maintenance tool (like a bundle structure on macOS...)
        Operation *op = createOwnedOperation(QLatin1String("Mkdir"));
        op->setArguments(QStringList() << targetAppDirPath);
        performOperationThreaded(op, Operation::Backup);
        performOperationThreaded(op);
        performedOperations.append(takeOwnedOperation(op));
    }
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
            out << in.readLine().replace(before, after) << Qt::endl;

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
#else
    Q_UNUSED(performedOperations);
#endif
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
        QString mtName = maintenanceToolName();
        const QString installerBaseBinary = replaceVariables(m_installerBaseBinaryUnreplaced);
        bool macOsMTBundleExtracted = false;
        if (!installerBaseBinary.isEmpty() && QFileInfo::exists(installerBaseBinary)) {
            qCDebug(QInstaller::lcInstallerInstallLog) << "Got a replacement installer base binary:"
                << installerBaseBinary;
            if (QInstaller::isInBundle(installerBaseBinary)) {
                // In macOS the installerbase is a whole app bundle. We do not modify the maintenancetool name in app bundle
                // so that possible signing and notarization will remain. Therefore, the actual maintenance tool name might
                // differ from the one defined in the settings.
                newBinaryWritten = true;
                mtName = installerBaseBinary;
                macOsMTBundleExtracted = true;
            } else {
                writeMaintenanceToolAppBundle(performedOperations);
                QFile replacementBinary(installerBaseBinary);
                try {
                    QInstaller::openForRead(&replacementBinary);
                    writeMaintenanceToolBinary(&replacementBinary, replacementBinary.size(), true);
                    qCDebug(QInstaller::lcInstallerInstallLog) << "Wrote the binary with the new replacement.";

                    newBinaryWritten = true;
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
            }
            m_installerBaseBinaryUnreplaced.clear();
        } else if (!installerBaseBinary.isEmpty() && !QFileInfo::exists(installerBaseBinary)) {
            qCWarning(QInstaller::lcInstallerInstallLog) << "The current maintenance tool could not be updated."
                << installerBaseBinary << "does not exist. Please fix the \"setInstallerBaseBinary"
                "(<temp_installer_base_binary_path>)\" call in your script.";
            writeMaintenanceToolAppBundle(performedOperations);
        } else {
            writeMaintenanceToolAppBundle(performedOperations);
        }

        QFile input;
        BinaryLayout layout;
        const QString dataFile = datFileName();
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
#ifdef Q_OS_MACOS
        if (newBinaryWritten) {
            // Remove old alias as the name may have changed.
            deleteMaintenanceToolAlias();
            const QString aliasPath = maintenanceToolAliasPath();
            if (!aliasPath.isEmpty()) {
                // The new alias file is created after the maintenance too binary is renamed,
                // but we need to set the value before the variables get written to disk.
                m_core->setValue(QLatin1String("CreatedMaintenanceToolAlias"), aliasPath);
            }
        }
#endif
        writeMaintenanceConfigFiles();

        QFile::remove(dataFile);
        QFileInfo fi(mtName);
        //Rename the dat file according to maintenancetool name
        QFile::rename(dataFile + QLatin1String(".new"), targetDir() + QLatin1Char('/') + fi.baseName() + QLatin1String(".dat"));

        const bool restart = !statusCanceledOrFailed() && m_needsHardRestart;
        qCDebug(QInstaller::lcInstallerInstallLog) << "Maintenance tool hard restart:"
            << (restart ? "true." : "false.");

        if (newBinaryWritten) {
            if (macOsMTBundleExtracted)
                deferredRename(mtName, targetDir() + QDir::separator() + fi.fileName(), restart);
            else if (isInstaller())
                QFile::rename(mtName + QLatin1String(".new"), mtName);
            else
                deferredRename(mtName + QLatin1String(".new"), mtName, restart);
            QFileInfo mtFileName(mtName);
            writeMaintenanceToolAlias(mtFileName.fileName());
        } else if (restart) {
            SelfRestarter::setRestartOnQuit(true);
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

void PackageManagerCorePrivate::writeMaintenanceToolAlias(const QString &maintenanceToolName)
{
#ifdef Q_OS_MACOS
    const QString aliasPath = maintenanceToolAliasPath();
    if (aliasPath.isEmpty())
        return;

    QString maintenanceToolBundle = QString::fromLatin1("%1/%2")
        .arg(targetDir(), maintenanceToolName);
    if (!maintenanceToolBundle.endsWith(QLatin1String(".app")))
        maintenanceToolBundle += QLatin1String(".app");

    const QDir targetDir(QFileInfo(aliasPath).absolutePath());
    if (!targetDir.exists())
        targetDir.mkpath(targetDir.absolutePath());

    mkalias(maintenanceToolBundle, aliasPath);
#else
    Q_UNUSED(maintenanceToolName)
#endif
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

        if (!m_core->hasAdminRights()) {
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

        // Now install the requested components
        unpackAndInstallComponents(componentsToInstall, progressOperationSize);

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

        ProgressCoordinator::instance()->emitLabelAndDetailTextChanged(QLatin1Char('\n')
            + tr("Installation finished!"));

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

        ProgressCoordinator::instance()->emitLabelAndDetailTextChanged(QLatin1Char('\n')
            + tr("Installation aborted!"));

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
                    && !m_installerCalculator->resolvedComponents().contains(m_componentsToReplaceUpdaterMode.value(name).first)) {
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
        if (updateAdminRights && !m_core->hasAdminRights()) {
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
            runUndoOperations(undoOperations, undoOperationProgressSize, true);
        }
        m_performedOperationsOld = nonRevertedOperations; // these are all operations left: those not reverted

        const double progressOperationCount = countProgressOperations(componentsToInstall);
        const double progressOperationSize = componentsInstallPartProgressSize / progressOperationCount;

        // Now install the requested new components
        unpackAndInstallComponents(componentsToInstall, progressOperationSize);

        emit m_core->titleMessageChanged(tr("Creating Maintenance Tool"));

        commitSessionOperations(); //end session, move ops to "old"
        m_needToWriteMaintenanceTool = true;

        // fake a possible wrong value to show a full progress bar
        const int progress = ProgressCoordinator::instance()->progressInPercentage();
        // usually this should be only the reserved one from the beginning
        if (progress < 100)
            ProgressCoordinator::instance()->addManualPercentagePoints(100 - progress);
        ProgressCoordinator::instance()->emitLabelAndDetailTextChanged(QLatin1Char('\n')
            + tr("Update finished!"));

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

        ProgressCoordinator::instance()->emitLabelAndDetailTextChanged(QLatin1Char('\n')
            + tr("Update aborted!"));

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
        if (updateAdminRights && !m_core->hasAdminRights()) {
            m_core->gainAdminRights();
            m_core->dropAdminRights();
        }

        const int uninstallOperationCount = countProgressOperations(undoOperations);
        const double undoOperationProgressSize = double(1) / double(uninstallOperationCount);

        runUndoOperations(undoOperations, undoOperationProgressSize, false);
        // No operation delete here, as all old undo operations are deleted in the destructor.

        deleteMaintenanceTool();    // this will also delete the TargetDir on Windows
        deleteMaintenanceToolAlias();

        // If not on Windows, we need to remove TargetDir manually.
#ifndef Q_OS_WIN
        if (QVariant(m_core->value(scRemoveTargetDir)).toBool() && !targetDir().isEmpty()) {
            if (updateAdminRights && !m_core->hasAdminRights())
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
        success ? tr("Removal completed successfully.") : tr("Removal aborted.")));

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
        if (!installerBaseReplacement.isEmpty() && QFileInfo::exists(installerBaseReplacement)) {
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

void PackageManagerCorePrivate::unpackComponents(const QList<Component *> &components,
    double progressOperationSize)
{
    OperationList unpackOperations;
    bool becameAdmin = false;

    ProgressCoordinator::instance()->emitLabelAndDetailTextChanged(QLatin1Char('\n')
        + tr("Preparing to unpack components..."));

    // 1. Collect operations
    quint64 totalOperationsSizeHint = 0;
    for (auto *component : components) {
        const OperationList operations = component->operations(Operation::Unpack);
        if (!component->operationsCreatedSuccessfully())
            m_core->setCanceled();

        for (auto &op : operations) {
            if (statusCanceledOrFailed())
                throw Error(tr("Installation canceled by user"));

            unpackOperations.append(op);
            totalOperationsSizeHint += op->sizeHint();

            // There's currently no way to control this on a per-operation basis, so
            // any op requesting execution as admin means all extracts are done as admin.
            if (!m_core->hasAdminRights() && op->value(QLatin1String("admin")).toBool())
                becameAdmin = m_core->gainAdminRights();
        }
    }

    // 2. Calculate proportional progress sizes
    const double totalOperationsSize = progressOperationSize * unpackOperations.size();
    for (auto *op : unpackOperations) {
        const double ratio = static_cast<double>(op->sizeHint()) / totalOperationsSizeHint;
        const double proportionalProgressOperationSize = totalOperationsSize * ratio;

        connectOperationToInstaller(op, proportionalProgressOperationSize);
        connectOperationCallMethodRequest(op);
    }

    // 3. Backup operations
    ConcurrentOperationRunner runner(&unpackOperations, Operation::Backup);
    runner.setMaxThreadCount(m_core->maxConcurrentOperations());

    connect(m_core, &PackageManagerCore::installationInterrupted,
        &runner, &ConcurrentOperationRunner::cancel);

    connect(&runner, &ConcurrentOperationRunner::progressChanged, [](const int completed, const int total) {
        const QString statusText = tr("%1 of %2 operations completed.")
            .arg(QString::number(completed), QString::number(total));
        ProgressCoordinator::instance()->emitAdditionalProgressStatus(statusText);
    });
    connect(&runner, &ConcurrentOperationRunner::finished, [] {
        // Clear the status text to not cause confusion when installations begin.
        ProgressCoordinator::instance()->emitAdditionalProgressStatus(QLatin1String(""));
    });

    const QHash<Operation *, bool> backupResults = runner.run();
    const OperationList backupOperations = backupResults.keys();

    for (auto &operation : backupOperations) {
        if (m_core->status() == PackageManagerCore::Canceled)
            break; // User canceled, no need to print warnings

        if (!backupResults.value(operation) || operation->error() != Operation::NoError) {
            // For Extract, backup stops only on read errors. That means the perform step will
            // also fail later on, which handles the user selection on what to do with the error.
            qCWarning(QInstaller::lcInstallerInstallLog) << QString::fromLatin1("Backup of operation "
                "\"%1\" with arguments \"%2\" failed: %3").arg(operation->name(), operation->arguments()
                .join(QLatin1String("; ")), operation->errorString());
            continue;
        }
        // Backup may request performing operation as admin
        if (!m_core->hasAdminRights() && operation->value(QLatin1String("admin")).toBool())
            becameAdmin = m_core->gainAdminRights();
    }

    // TODO: Do we need to rollback backups too? For Extract op this works without,
    // as the backup files are not used in Undo step.
    if (statusCanceledOrFailed())
        throw Error(tr("Installation canceled by user"));

    // 4. Perform operations
    std::sort(unpackOperations.begin(), unpackOperations.end(), [](Operation *lhs, Operation *rhs) {
        // We want to run the longest taking operations first
        return lhs->sizeHint() > rhs->sizeHint();
    });

    ProgressCoordinator::instance()->emitLabelAndDetailTextChanged(QLatin1Char('\n')
        + tr("Unpacking components..."));

    runner.setType(Operation::Perform);
    const QHash<Operation *, bool> results = runner.run();
    const OperationList performedOperations = results.keys();

    QString error;
    for (auto &operation : performedOperations) {
        const QString component = operation->value(QLatin1String("component")).toString();

        bool ignoreError = false;
        bool ok = results.value(operation);

        while (!ok && !ignoreError && m_core->status() != PackageManagerCore::Canceled) {
            qCDebug(QInstaller::lcInstallerInstallLog) << QString::fromLatin1("Operation \"%1\" with arguments "
                "\"%2\" failed: %3").arg(operation->name(), operation->arguments()
                .join(QLatin1String("; ")), operation->errorString());

            const QMessageBox::StandardButton button =
                MessageBoxHandler::warning(MessageBoxHandler::currentBestSuitParent(),
                QLatin1String("installationErrorWithCancel"), tr("Installer Error"),
                tr("Error during installation process (%1):\n%2").arg(component, operation->errorString()),
                QMessageBox::Retry | QMessageBox::Ignore | QMessageBox::Cancel, QMessageBox::Cancel);

            if (button == QMessageBox::Retry)
                ok = performOperationThreaded(operation);
            else if (button == QMessageBox::Ignore)
                ignoreError = true;
            else if (button == QMessageBox::Cancel)
                m_core->interrupt();
        }

        if (ok || operation->error() > Operation::InvalidArguments) {
            // Remember that the operation was performed, that allows us to undo it
            // if this operation failed but still needs an undo call to cleanup.
            addPerformed(operation);
        }

        // Catch the error message from first failure, but throw only after all
        // operations requiring undo step are marked as performed.
        if (!ok && !ignoreError && error.isEmpty())
            error = operation->errorString();
    }

    if (becameAdmin)
        m_core->dropAdminRights();

    if (!error.isEmpty())
        throw Error(error);

    ProgressCoordinator::instance()->emitDetailTextChanged(tr("Done"));
}

void PackageManagerCorePrivate::installComponent(Component *component, double progressOperationSize)
{
    OperationList operations = component->operations(Operation::Install);
    if (!component->operationsCreatedSuccessfully())
        m_core->setCanceled();

    const int opCount = operations.count();
    // show only components which do something, MinimumProgress is only for progress calculation safeness
    bool showDetailsLog = false;
    if (opCount > 1 || (opCount == 1 && operations.at(0)->name() != QLatin1String("MinimumProgress"))) {
        ProgressCoordinator::instance()->emitLabelAndDetailTextChanged(QLatin1Char('\n')
            + tr("Installing component %1").arg(component->displayName()));
        showDetailsLog = true;
    }

    foreach (Operation *operation, operations) {
        if (statusCanceledOrFailed())
            throw Error(tr("Installation canceled by user"));

        // maybe this operations wants us to be admin...
        bool becameAdmin = false;
        if (!m_core->hasAdminRights() && operation->value(QLatin1String("admin")).toBool()) {
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
    }

    if (!m_core->isCommandLineInstance()) {
        if ((component->value(scEssential, scFalse) == scTrue) && !isInstaller())
            m_needsHardRestart = true;
        else if ((component->value(scForcedUpdate, scFalse) == scTrue) && isUpdater())
            m_needsHardRestart = true;
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
                                  QPair<QString, bool>(component->value(scTreeName),
                                                       component->treeNameMoveChildren()),
                                  component->value(scDescription),
                                  component->value(scSortingPriority).toInt(),
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

PackageManagerCore::Status PackageManagerCorePrivate::fetchComponentsAndInstall(const QStringList& components)
{
    // init default model before fetching remote packages tree
    ComponentModel *model = m_core->defaultComponentModel();
    Q_UNUSED(model);

    bool fallbackReposFetched = false;
    auto fetchComponents = [&]() {
        bool packagesFound = m_core->fetchPackagesWithFallbackRepositories(components, fallbackReposFetched);

        if (!packagesFound) {
            qCDebug(QInstaller::lcInstallerInstallLog).noquote().nospace()
                << "No components available with the current selection.";
            setStatus(PackageManagerCore::Canceled);
            return false;
        }
        QString errorMessage;
        bool unstableAliasFound = false;
        if (m_core->checkComponentsForInstallation(components, errorMessage, unstableAliasFound, fallbackReposFetched)) {
            if (!errorMessage.isEmpty())
                qCDebug(QInstaller::lcInstallerInstallLog).noquote().nospace() << errorMessage;
            if (calculateComponentsAndRun()) {
                if (m_core->isOfflineGenerator())
                    qCDebug(QInstaller::lcInstallerInstallLog) << "Created installer to:" << offlineBinaryName();
                else
                    qCDebug(QInstaller::lcInstallerInstallLog) << "Components installed successfully";
            }
        } else {
            // We found unstable alias and all repos were not fetched. Alias might have dependency to component
            // which exists in non-default repository. Fetch all repositories now.
            if (unstableAliasFound && !fallbackReposFetched) {
                return false;
            } else {
                for (const QString &possibleAliasName : components) {
                    if (ComponentAlias *alias = m_core->aliasByName(possibleAliasName)) {
                        if (alias->componentErrorMessage().isEmpty())
                            continue;
                        qCWarning(QInstaller::lcInstallerInstallLog).noquote().nospace() << alias->componentErrorMessage();
                    }
                }
                qCDebug(QInstaller::lcInstallerInstallLog).noquote().nospace() << errorMessage
                    << "No components available with the current selection.";
            }
        }
        return true;
    };

    if (!fetchComponents() && !fallbackReposFetched) {
        fallbackReposFetched = true;
        setStatus(PackageManagerCore::Running);
        qCDebug(QInstaller::lcInstallerInstallLog).noquote()
            << "Components not found with the current selection."
            << "Searching from additional repositories";
        if (!ProductKeyCheck::instance()->securityWarning().isEmpty()) {
            qCWarning(QInstaller::lcInstallerInstallLog) << ProductKeyCheck::instance()->securityWarning();
        }
        enableAllCategories();
        fetchComponents();
    }

    return m_core->status();
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
    QDir resourcePath(QFileInfo(maintenanceToolName()).dir());
    resourcePath.remove(QLatin1String("installer.dat"));
    QDir installDir(targetDir());
    installDir.remove(m_data.settings().maintenanceToolName() + QLatin1String(".dat"));
    installDir.remove(QLatin1String("network.xml"));
    installDir.remove(m_data.settings().maintenanceToolIniFile());
    QInstaller::VerboseWriter::instance()->setFileName(QString());
    installDir.remove(m_core->value(QLatin1String("LogFileName"), QLatin1String("InstallationLog.txt")));
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
        throw Error(tr("Cannot prepare removal"));

    const bool removeTargetDir = QVariant(m_core->value(scRemoveTargetDir)).toBool();
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
    if (!removeTargetDir)
        batch << "if folder.SubFolders.Count = 0 and folder.Files.Count = 0 then\n";
    batch << "    Set folder = Nothing\n";
    batch << "    fso.DeleteFolder folderpath, true\n";
    if (!removeTargetDir)
        batch << "end if\n";
    batch << "fso.DeleteFile(WScript.ScriptFullName)\n";

    f.close();

    QStringList arguments;
    arguments << QLatin1String("//Nologo") << batchfile; // execute the batchfile
    arguments << QDir::toNativeSeparators(QFileInfo(installerBinaryPath()).absoluteFilePath());
    arguments << targetDir();

    if (!QProcessWrapper::startDetached(QLatin1String("cscript"), arguments, QDir::rootPath()))
        throw Error(tr("Cannot start removal"));
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

void PackageManagerCorePrivate::deleteMaintenanceToolAlias()
{
#ifdef Q_OS_MACOS
    const QString maintenanceToolAlias = m_core->value(QLatin1String("CreatedMaintenanceToolAlias"));
    QFile aliasFile(maintenanceToolAlias);
    if (!maintenanceToolAlias.isEmpty() && !aliasFile.remove()) {
        // Not fatal
        qWarning(lcInstallerInstallLog) << "Cannot remove alias file"
            << maintenanceToolAlias << "for maintenance tool:" << aliasFile.errorString();
    }
#endif
}

void PackageManagerCorePrivate::registerMaintenanceTool()
{
#ifdef Q_OS_WIN
    auto quoted = [](const QString &s) {
        return QString::fromLatin1("\"%1\"").arg(s);
    };

    QSettingsWrapper settings(registerPath(), QSettings::NativeFormat);
    settings.setValue(scDisplayName, m_data.value(QLatin1String("ProductName")));
    settings.setValue(QLatin1String("DisplayVersion"), m_data.value(QLatin1String("ProductVersion")));
    const QString maintenanceTool = QDir::toNativeSeparators(maintenanceToolName());
    settings.setValue(QLatin1String("DisplayIcon"), maintenanceTool);
    settings.setValue(scPublisher, m_data.value(scPublisher));
    settings.setValue(QLatin1String("UrlInfoAbout"), m_data.value(QLatin1String("Url")));
    settings.setValue(QLatin1String("Comments"), m_data.value(scTitle));
    settings.setValue(QLatin1String("InstallDate"), QDateTime::currentDateTime().toString());
    settings.setValue(QLatin1String("InstallLocation"), QDir::toNativeSeparators(targetDir()));
    settings.setValue(QLatin1String("UninstallString"), QString(quoted(maintenanceTool)
        + QLatin1String(" --") + CommandLineOptions::scStartUninstallerLong));
    if (!isOfflineOnly()) {
        settings.setValue(QLatin1String("ModifyPath"), QString(quoted(maintenanceTool)
            + QLatin1String(" --") + CommandLineOptions::scStartPackageManagerLong));
    }
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
    QSettingsWrapper settings(registerPath(), QSettings::NativeFormat);
    settings.remove(QString());
#endif
}

void PackageManagerCorePrivate::runUndoOperations(const OperationList &undoOperations,
    double progressSize, bool deleteOperation)
{
    try {
        const int operationsCount = undoOperations.size();
        int rolledBackOperations = 0;

        foreach (Operation *undoOperation, undoOperations) {
            if (statusCanceledOrFailed())
                throw Error(tr("Installation canceled by user"));

            bool becameAdmin = false;
            if (!m_core->hasAdminRights() && undoOperation->value(QLatin1String("admin")).toBool())
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
                        tr("Error during removal process:\n%1").arg(undoOperation->errorString()),
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

            ++rolledBackOperations;
            ProgressCoordinator::instance()->emitAdditionalProgressStatus(tr("%1 of %2 operations rolled back.")
                .arg(QString::number(rolledBackOperations), QString::number(operationsCount)));

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
    ProgressCoordinator::instance()->emitAdditionalProgressStatus(tr("Rollbacks complete."));
}

PackagesList PackageManagerCorePrivate::remotePackages()
{
    if (m_updates && m_updateFinder)
        return m_updateFinder->updates();

    m_updates = false;
    delete m_updateFinder;

    m_updateFinder = new KDUpdater::UpdateFinder;
    m_updateFinder->setAutoDelete(false);
    m_updateFinder->setPackageSources(m_packageSources + m_compressedPackageSources);
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

/*!
    Returns a hash containing the installed package name and it's associated package information. If
    the application is running in installer mode or the local components file could not be parsed, the
    hash is empty.
*/
LocalPackagesMap PackageManagerCorePrivate::localInstalledPackages()
{
    if (isInstaller())
        return LocalPackagesMap();


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
    return m_localPackageHub->localPackages();
}

QList<ComponentAlias *> PackageManagerCorePrivate::componentAliases()
{
    if (m_aliases && m_aliasFinder)
        return m_aliasFinder->aliases();

    m_aliases = false;
    delete m_aliasFinder;

    m_aliasFinder = new AliasFinder(m_core);
    m_aliasFinder->setAliasSources(m_aliasSources);
    if (!m_aliasFinder->run()) {
        qCDebug(lcDeveloperBuild) << "No component aliases found." << Qt::endl;
        return QList<ComponentAlias *>();
    }

    m_aliases = true;
    return m_aliasFinder->aliases();
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

bool PackageManagerCorePrivate::addUpdateResourcesFromRepositories(bool compressedRepository)
{
    if (!compressedRepository && m_updateSourcesAdded)
        return m_updateSourcesAdded;

    const QList<Metadata *> metadata = m_metadataJob.metadata();
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
            m_packageSources.insert(PackageSource(QUrl(QLatin1String("resource://metadata/")), 1));
    }

    foreach (const Metadata *data, metadata) {
        if (compressedRepository && !data->repository().isCompressed()) {
            continue;
        }
        if (statusCanceledOrFailed())
            return false;

        if (data->path().isEmpty())
            continue;

        if (data->repository().isCompressed())
            m_compressedPackageSources.insert(PackageSource(QUrl::fromLocalFile(data->path()), 2, data->repository().postLoadComponentScript()));
        else
            m_packageSources.insert(PackageSource(QUrl::fromLocalFile(data->path()), 0, data->repository().postLoadComponentScript()));

        ProductKeyCheck::instance()->addPackagesFromXml(data->path() + QLatin1String("/Updates.xml"));
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
}

void PackageManagerCorePrivate::storeCheckState()
{
    m_coreCheckedHash.clear();

   const QList<Component *> components =
       m_core->components(PackageManagerCore::ComponentType::AllNoReplacements);
    foreach (Component *component, components)
        m_coreCheckedHash.insert(component, component->checkState());
}

void PackageManagerCorePrivate::updateComponentInstallActions()
{
    for (Component *component : m_core->components(PackageManagerCore::ComponentType::All)) {
        component->setInstallAction(component->isInstalled()
              ? ComponentModelHelper::KeepInstalled
              : ComponentModelHelper::KeepUninstalled);
    }
    for (Component *component : uninstallerCalculator()->resolvedComponents())
        component->setInstallAction(ComponentModelHelper::Uninstall);
    for (Component *component : installerCalculator()->resolvedComponents())
        component->setInstallAction(ComponentModelHelper::Install);
}

bool PackageManagerCorePrivate::enableAllCategories()
{
    QSet<RepositoryCategory> repoCategories = m_data.settings().repositoryCategories();
    bool additionalRepositoriesEnabled = false;
    for (const auto &category : repoCategories) {
        if (!category.isEnabled()) {
            additionalRepositoriesEnabled = true;
            enableRepositoryCategory(category, true);
        }
    }
    return additionalRepositoriesEnabled;
}

void PackageManagerCorePrivate::enableRepositoryCategory(const RepositoryCategory &repoCategory, const bool enable)
{
    RepositoryCategory replacement = repoCategory;
    replacement.setEnabled(enable);
    QSet<RepositoryCategory> tmpRepoCategories = m_data.settings().repositoryCategories();
    if (tmpRepoCategories.contains(repoCategory)) {
        tmpRepoCategories.remove(repoCategory);
        tmpRepoCategories.insert(replacement);
        m_data.settings().addRepositoryCategories(tmpRepoCategories);
    }
}

bool PackageManagerCorePrivate::installablePackagesFound(const QStringList& components)
{
    if (components.isEmpty())
        return true;

    PackagesList remotes = remotePackages();

    auto componentsNotFoundForInstall = QtConcurrent::blockingFiltered(
        components,
        [remotes](const QString& installerPackage) {
            return filterMissingPackagesToInstall(installerPackage, remotes);
        }
        );

    if (componentsNotFoundForInstall.count() > 0) {
        QList<ComponentAlias *> aliasList = componentAliases();
        auto aliasesNotFoundForInstall = QtConcurrent::blockingFiltered(
            componentsNotFoundForInstall,
            [aliasList](const QString& installerPackage) {
                return filterMissingAliasesToInstall(installerPackage, aliasList);
            }
            );

        if (aliasesNotFoundForInstall.count() > 0) {
            qCDebug(QInstaller::lcInstallerInstallLog).noquote().nospace() << "Cannot select " << aliasesNotFoundForInstall.join(QLatin1String(", ")) << ". Component(s) not found.";
            setStatus(PackageManagerCore::NoPackagesFound);
            return false;
        }
    }
    return true;
}

void PackageManagerCorePrivate::deferredRename(const QString &oldName, const QString &newName, bool restart)
{

#ifdef Q_OS_WINDOWS
    const QString currentExecutable = QCoreApplication::applicationFilePath();
    QString tmpExecutable = generateTemporaryFileName(currentExecutable);
    QFile::rename(currentExecutable, tmpExecutable);
    QFile::rename(oldName, newName);

    QStringList arguments;
    if (restart) {
        // Restart with same command line arguments as first executable
        arguments = QCoreApplication::arguments();
        arguments.removeFirst(); // Remove program name
        arguments.prepend(tmpExecutable);
        arguments.prepend(QLatin1String("--")
                          + CommandLineOptions::scCleanupUpdate);
    } else {
        arguments.append(QLatin1String("--")
                         + CommandLineOptions::scCleanupUpdateOnly);
        arguments.append(tmpExecutable);
    }
    QProcessWrapper::startDetached2(newName, arguments);
#elif defined Q_OS_MACOS
    // In macos oldName is the name of the maintenancetool we got from repository
    // It might be extracted to a folder to avoid overlapping with running maintenancetool
    // Here, ditto renames it to newName (and possibly moves from the subfolder).
    if (oldName != newName) {
        //1. Rename/move maintenancetool
        QProcessWrapper process;
        process.start(QLatin1String("ditto"), QStringList() << oldName << newName);
        if (!process.waitForFinished()) {
            qCDebug(QInstaller::lcInstallerInstallLog) << "Failed to rename maintenancetool from :" << oldName << " to "<<newName;
        }
        //2. Remove subfolder
        QDir subDirectory(oldName);
        subDirectory.cdUp();
        QString subDirectoryPath = QDir::cleanPath(subDirectory.absolutePath());
        QString targetDirectoryPath = QDir::cleanPath(targetDir());

        //Make sure there is subdirectory in the targetdir so we don't delete the installation
        if (subDirectoryPath.startsWith(targetDirectoryPath) && subDirectoryPath != targetDirectoryPath)
            subDirectory.removeRecursively();
    }

    //3. If new maintenancetool name differs from original, remove the original maintenance tool
    if (!isInstaller()) {
        QString currentAppBundlePath = getAppBundlePath();
        if (currentAppBundlePath != newName) {
            QDir oldBundlePath(currentAppBundlePath);
            oldBundlePath.removeRecursively();
        }
    }
    SelfRestarter::setRestartOnQuit(restart);

#elif defined Q_OS_LINUX
    QFile::remove(newName);
    QFile::rename(oldName, newName);
    SelfRestarter::setRestartOnQuit(restart);
#endif
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

/*
    Adds the \a path for deletetion. Unlike files for delayed deletion, which are deleted
    on the start of next installer run, these paths are deleted on exit.
*/
void PackageManagerCorePrivate::addPathForDeletion(const QString &path)
{
    m_tmpPathDeleter.add(path);
}

void PackageManagerCorePrivate::unpackAndInstallComponents(const QList<Component *> &components,
    const double progressOperationSize)
{
    // Perform extract operations
    unpackComponents(components, progressOperationSize);

    // Perform rest of the operations and mark component as installed
    const int componentsToInstallCount = components.size();
    int installedComponents = 0;
    foreach (Component *component, components) {
        installComponent(component, progressOperationSize);

        ++installedComponents;
        ProgressCoordinator::instance()->emitAdditionalProgressStatus(tr("%1 of %2 components installed.")
            .arg(QString::number(installedComponents), QString::number(componentsToInstallCount)));
    }
    ProgressCoordinator::instance()->emitAdditionalProgressStatus(tr("All components installed."));
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

bool PackageManagerCorePrivate::calculateComponentsAndRun()
{
    bool componentsOk = m_core->recalculateAllComponents();

    if (statusCanceledOrFailed()) {
        qCDebug(QInstaller::lcInstallerInstallLog) << "Installation canceled.";
    } else if (componentsOk && acceptLicenseAgreements()) {
        try {
            loadComponentScripts(installerCalculator()->resolvedComponents(), true);
        }  catch (const Error &error) {
            qCWarning(QInstaller::lcInstallerInstallLog) << error.message();
            return false;
        }
        qCDebug(QInstaller::lcInstallerInstallLog).noquote()
            << htmlToString(m_core->componentResolveReasons());

        const bool spaceOk = m_core->checkAvailableSpace();
        qCDebug(QInstaller::lcInstallerInstallLog) << m_core->availableSpaceMessage();

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

    const QString acceptanceText = ProductKeyCheck::instance()->licenseAcceptanceText();
    if (!acceptanceText.isEmpty()) {
        qCDebug(QInstaller::lcInstallerInstallLog).noquote() << acceptanceText;
        if (!m_autoAcceptLicenses && !acceptRejectCliQuery())
            return false;
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

bool PackageManagerCorePrivate::acceptRejectCliQuery() const
{
    forever {
        const QString input = m_core->readConsoleLine(QLatin1String("Accept|Reject"));

        if (QString::compare(input, QLatin1String("Accept"), Qt::CaseInsensitive) == 0
                || QString::compare(input, QLatin1String("A"), Qt::CaseInsensitive) == 0) {
            return true;
        } else if (QString::compare(input, QLatin1String("Reject"), Qt::CaseInsensitive) == 0
                || QString::compare(input, QLatin1String("R"), Qt::CaseInsensitive) == 0) {
            return false;
        }  else {
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

void PackageManagerCorePrivate::commitPendingUnstableComponents()
{
    if (m_pendingUnstableComponents.isEmpty())
        return;

    for (auto &componentName : m_pendingUnstableComponents.keys()) {
        Component *const component = m_core->componentByName(componentName);
        if (!component) {
            qCWarning(lcInstallerInstallLog) << "Failure while marking component "
                "unstable. No such component exists:" << componentName;
            continue;
        }

        const QPair<Component::UnstableError, QString> unstableError
            = m_pendingUnstableComponents.value(componentName);

        component->setUnstable(unstableError.first, unstableError.second);
    }
    m_pendingUnstableComponents.clear();
}

void PackageManagerCorePrivate::createAutoDependencyHash(const QString &component, const QString &oldDependencies, const QString &newDependencies)
{
    // User might have changed autodependencies with setValue. Remove the old values.
    const QStringList oldDependencyList = oldDependencies.split(QInstaller::commaRegExp(), Qt::SkipEmptyParts);
    for (const QString &removedDependency : oldDependencyList) {
        QStringList value = m_autoDependencyComponentHash.value(removedDependency);
        value.removeAll(component);
        if (value.isEmpty())
            m_autoDependencyComponentHash.remove(removedDependency);
        else
            m_autoDependencyComponentHash.insert(removedDependency, value);
    }

    const QStringList newDependencyList = newDependencies.split(QInstaller::commaRegExp(), Qt::SkipEmptyParts);
    for (const QString &autodepend : newDependencyList) {
        QStringList value = m_autoDependencyComponentHash.value(autodepend);
        if (!value.contains(component)) {
            value.append(component);
            m_autoDependencyComponentHash.insert(autodepend, value);
        }
    }
}

void PackageManagerCorePrivate::createLocalDependencyHash(const QString &component, const QString &dependencies)
{
    const QStringList localDependencies = dependencies.split(QInstaller::commaRegExp(), Qt::SkipEmptyParts);
    for (const QString &depend : localDependencies) {
        QStringList value = m_localDependencyComponentHash.value(depend);
        if (!value.contains(component)) {
            value.append(component);
            m_localDependencyComponentHash.insert(depend, value);
        }
    }
}

} // namespace QInstaller
