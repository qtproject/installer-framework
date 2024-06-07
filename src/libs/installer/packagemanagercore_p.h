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

#ifndef PACKAGEMANAGERCORE_P_H
#define PACKAGEMANAGERCORE_P_H

#include "metadatajob.h"
#include "packagemanagercore.h"
#include "packagemanagercoredata.h"
#include "packagemanagerproxyfactory.h"
#include "packagesource.h"
#include "qinstallerglobal.h"
#include "component.h"
#include "fileutils.h"

#include "sysinfo.h"
#include "updatefinder.h"

#include <QObject>

class Job;

QT_FORWARD_DECLARE_CLASS(QFile)
QT_FORWARD_DECLARE_CLASS(QFileDevice)
QT_FORWARD_DECLARE_CLASS(QFileInfo)

using namespace KDUpdater;

namespace QInstaller {

struct BinaryLayout;
struct AliasSource;
class AliasFinder;
class ScriptEngine;
class ComponentModel;
class ComponentAlias;
class InstallerCalculator;
class UninstallerCalculator;
class RemoteFileEngineHandler;
class ComponentSortFilterProxyModel;

class PackageManagerCorePrivate : public QObject
{
    Q_OBJECT
    friend class PackageManagerCore;
    Q_DISABLE_COPY(PackageManagerCorePrivate)

public:
    explicit PackageManagerCorePrivate(PackageManagerCore *core);
    explicit PackageManagerCorePrivate(PackageManagerCore *core, qint64 magicInstallerMaker,
        const QList<OperationBlob> &performedOperations, const QString &datFileName);
    ~PackageManagerCorePrivate();

    static bool isProcessRunning(const QString &name, const QList<ProcessInfo> &processes);

    static bool performOperationThreaded(Operation *op, UpdateOperation::OperationType type
        = UpdateOperation::Perform);

    void initialize(const QHash<QString, QString> &params);
    bool isOfflineOnly() const;

    bool statusCanceledOrFailed() const;
    void setStatus(int status, const QString &error = QString());

    QString targetDir() const;
    QString registerPath();

    bool directoryWritable(const QString &path) const;

    QString maintenanceToolName() const;
    QString maintenanceToolAliasPath() const;
    QString installerBinaryPath() const;
    QString offlineBinaryName() const;
    QString datFileName();

    void writeMaintenanceConfigFiles();
    void readMaintenanceConfigFiles(const QString &targetDir);

    void writeMaintenanceTool(OperationList performedOperations);
    void writeOfflineBaseBinary();

    void writeMaintenanceToolAlias(const QString &maintenanceToolName);

    QString componentsXmlPath() const;
    QString configurationFileName() const;

    bool buildComponentTree(QHash<QString, Component*> &components, bool loadScript);
    bool buildComponentAliases();

    template <typename T>
    bool loadComponentScripts(const T &components, const bool postScript = false);

    void cleanUpComponentEnvironment();
    ScriptEngine *componentScriptEngine() const;
    ScriptEngine *controlScriptEngine() const;

    void clearAllComponentLists();
    void clearUpdaterComponentLists();
    QList<Component*> &replacementDependencyComponents();
    QHash<QString, QPair<Component*, Component*> > &componentsToReplace();
    QHash<QString, QStringList > &componentReplaces();

    void clearInstallerCalculator();
    InstallerCalculator *installerCalculator() const;

    void clearUninstallerCalculator();
    UninstallerCalculator *uninstallerCalculator() const;

    bool runInstaller();
    bool isInstaller() const;

    bool runUninstaller();
    bool isUninstaller() const;

    bool isUpdater() const;

    bool runPackageUpdater();
    bool isPackageManager() const;

    bool runOfflineGenerator();
    bool isOfflineGenerator() const;

    bool isPackageViewer() const;

    QString replaceVariables(const QString &str) const;
    QByteArray replaceVariables(const QByteArray &str) const;

    void callBeginInstallation(const QList<Component*> &componentList);
    void stopProcessesForUpdates(const QList<Component*> &components);
    int countProgressOperations(const QList<Component*> &components);
    int countProgressOperations(const OperationList &operations);
    void connectOperationToInstaller(Operation *const operation, double progressOperationPartSize);
    void connectOperationCallMethodRequest(Operation *const operation);
    OperationList sortOperationsBasedOnComponentDependencies(const OperationList &operationList);

    Operation *createOwnedOperation(const QString &type);
    Operation *takeOwnedOperation(Operation *operation);

    Operation *createPathOperation(const QFileInfo &fileInfo, const QString &componentName);
    void registerPathsForUninstallation(const QList<QPair<QString, bool> > &pathsForUninstallation,
        const QString &componentName);

    void addPerformed(Operation *op) {
        m_performedOperationsCurrentSession.append(op);
    }

    void commitSessionOperations() {
        m_performedOperationsOld += m_performedOperationsCurrentSession;
        m_performedOperationsCurrentSession.clear();
    }

    void unpackComponents(const QList<Component *> &components, double progressOperationSize);

    void installComponent(Component *component, double progressOperationSize);
    PackageManagerCore::Status fetchComponentsAndInstall(const QStringList& components);

    void setComponentSelection(const QString &id, Qt::CheckState state);

signals:
    void installationStarted();
    void installationFinished();
    void uninstallationStarted();
    void uninstallationFinished();
    void offlineGenerationStarted();
    void offlineGenerationFinished();

public:
    UpdateFinder *m_updateFinder;
    AliasFinder *m_aliasFinder;
    QSet<PackageSource> m_packageSources;
    QSet<PackageSource> m_compressedPackageSources;
    QSet<AliasSource> m_aliasSources;
    std::shared_ptr<LocalPackageHub> m_localPackageHub;
    QStringList m_filesForDelayedDeletion;

    int m_status;
    QString m_error;

    bool m_needsHardRestart;
    bool m_testChecksum;
    bool m_launchedAsRoot;
    bool m_commandLineInstance;
    bool m_defaultInstall;
    bool m_userSetBinaryMarker;
    bool m_checkAvailableSpace;
    bool m_completeUninstall;
    bool m_needToWriteMaintenanceTool;
    PackageManagerCoreData m_data;
    QString m_installerBaseBinaryUnreplaced;
    QString m_offlineBaseBinaryUnreplaced;
    QStringList m_offlineGeneratorResourceCollections;

    QList<QInstaller::Component*> m_rootComponents;
    QList<QInstaller::Component*> m_rootDependencyReplacements;

    QList<QInstaller::Component*> m_updaterComponents;
    QList<QInstaller::Component*> m_updaterComponentsDeps;
    QList<QInstaller::Component*> m_updaterDependencyReplacements;

    QHash<QString, QInstaller::ComponentAlias *> m_componentAliases;

    OperationList m_ownedOperations;
    OperationList m_performedOperationsOld;
    OperationList m_performedOperationsCurrentSession;

    bool m_dependsOnLocalInstallerBinary;
    QStringList m_allowedRunningProcesses;
    bool m_autoAcceptLicenses;
    bool m_disableWriteMaintenanceTool;
    bool m_autoConfirmCommand;

private slots:
    void infoMessage(Job *, const QString &message) {
        emit m_core->metaJobInfoMessage(message);
    }
    void infoProgress(Job *, quint64 progress, quint64) {
        emit m_core->metaJobProgress(progress);
    }

    void totalProgress(quint64 total) {
        emit m_core->metaJobTotalProgress(total);
    }

    void handleMethodInvocationRequest(const QString &invokableMethodName);
    void addPathForDeletion(const QString &path);

private:
    void unpackAndInstallComponents(const QList<Component *> &components,
        const double progressOperationSize);

    void deleteMaintenanceTool();
    void deleteMaintenanceToolAlias();
    void registerMaintenanceTool();
    void unregisterMaintenanceTool();

    void writeMaintenanceToolBinary(QFile *const input, qint64 size, bool writeBinaryLayout);
    void writeMaintenanceToolBinaryData(QFileDevice *output, QFile *const input,
        const OperationList &performed, const BinaryLayout &layout);
    void writeMaintenanceToolAppBundle(OperationList &performedOperations);

    void runUndoOperations(const OperationList &undoOperations, double undoOperationProgressSize,
        bool deleteOperation);

    PackagesList remotePackages();
    LocalPackagesMap localInstalledPackages();
    QList<ComponentAlias *> componentAliases();

    bool fetchMetaInformationFromRepositories(DownloadType type = DownloadType::All);
    bool addUpdateResourcesFromRepositories(bool compressedRepository = false);
    void processFilesForDelayedDeletion();
    bool calculateComponentsAndRun();
    bool acceptLicenseAgreements() const;
    bool askUserAcceptLicense(const QString &name, const QString &content) const;
    bool acceptRejectCliQuery() const;
    bool askUserConfirmCommand() const;
    bool packageNeedsUpdate(const LocalPackage &localPackage, const Package *update) const;
    void commitPendingUnstableComponents();
    void createAutoDependencyHash(const QString &componentName, const QString &oldValue, const QString &newValue);
    void createLocalDependencyHash(const QString &componentName, const QString &dependencies);
    void updateComponentInstallActions();

    bool enableAllCategories();
    void enableRepositoryCategory(const RepositoryCategory &repoCategory, const bool enable);

    bool installablePackagesFound(const QStringList& components);

    void deferredRename(const QString &oldName, const QString &newName, bool restart = false);

    // remove once we deprecate isSelected, setSelected etc...
    void restoreCheckState();
    void storeCheckState();

private:
    PackageManagerCore *m_core;
    MetadataJob m_metadataJob;
    TempPathDeleter m_tmpPathDeleter;

    bool m_updates;
    bool m_aliases;
    bool m_repoFetched;
    bool m_updateSourcesAdded;
    qint64 m_magicBinaryMarker;
    int m_magicMarkerSupplement;

    bool m_foundEssentialUpdate;

    mutable ScriptEngine *m_componentScriptEngine;
    mutable ScriptEngine *m_controlScriptEngine;
    // < name (component to replace), < replacement component, component to replace > >
    QHash<QString, QPair<Component*, Component*> > m_componentsToReplaceAllMode;
    QHash<QString, QPair<Component*, Component*> > m_componentsToReplaceUpdaterMode;

    QHash<QString, QPair<Component::UnstableError, QString>> m_pendingUnstableComponents;

    InstallerCalculator *m_installerCalculator;
    UninstallerCalculator *m_uninstallerCalculator;

    PackageManagerProxyFactory *m_proxyFactory;

    ComponentModel *m_defaultModel;
    ComponentModel *m_updaterModel;
    ComponentSortFilterProxyModel *m_componentSortFilterProxyModel;

    QObject *m_guiObject;
    QScopedPointer<RemoteFileEngineHandler> m_remoteFileEngineHandler;
    QHash<QString, QVariantMap> m_licenseItems;

    QHash<Component*, Qt::CheckState> m_coreCheckedHash;
    QList<Component*> m_deletedReplacedComponents;
    AutoDependencyHash m_autoDependencyComponentHash;
    LocalDependencyHash m_localDependencyComponentHash;
    QHash<QString, Component *> m_componentByNameHash;

    QStringList m_localVirtualComponents;

    // < name (component replacing others), components to replace>
    QHash<QString, QStringList > m_componentReplaces;

    QString m_datFileName;
    bool m_allowCompressedRepositoryInstall;
    int m_connectedOperations;
    QStringList m_componentsToBeInstalled;
};

} // namespace QInstaller

#endif  // PACKAGEMANAGERCORE_P_H
