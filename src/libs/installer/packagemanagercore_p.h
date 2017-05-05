/**************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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
class Component;
class ScriptEngine;
class ComponentModel;
class TempDirDeleter;
class InstallerCalculator;
class UninstallerCalculator;
class RemoteFileEngineHandler;

class PackageManagerCorePrivate : public QObject
{
    Q_OBJECT
    friend class PackageManagerCore;
    Q_DISABLE_COPY(PackageManagerCorePrivate)

public:
    enum OperationType {
        Backup,
        Perform,
        Undo
    };

    explicit PackageManagerCorePrivate(PackageManagerCore *core);
    explicit PackageManagerCorePrivate(PackageManagerCore *core, qint64 magicInstallerMaker,
        const QList<OperationBlob> &performedOperations);
    ~PackageManagerCorePrivate();

    static bool isProcessRunning(const QString &name, const QList<ProcessInfo> &processes);

    static bool performOperationThreaded(Operation *op, PackageManagerCorePrivate::OperationType type
        = PackageManagerCorePrivate::Perform);

    void initialize(const QHash<QString, QString> &params);
    bool isOfflineOnly() const;

    bool statusCanceledOrFailed() const;
    void setStatus(int status, const QString &error = QString());

    QString targetDir() const;
    QString registerPath();

    QString maintenanceToolName() const;
    QString installerBinaryPath() const;

    void writeMaintenanceConfigFiles();
    void readMaintenanceConfigFiles(const QString &targetDir);

    void writeMaintenanceTool(OperationList performedOperations);

    QString componentsXmlPath() const;
    QString configurationFileName() const;

    bool buildComponentTree(QHash<QString, Component*> &components, bool loadScript);

    void cleanUpComponentEnvironment();
    ScriptEngine *componentScriptEngine() const;
    ScriptEngine *controlScriptEngine() const;

    void clearAllComponentLists();
    void clearUpdaterComponentLists();
    QList<Component*> &replacementDependencyComponents();
    QHash<QString, QPair<Component*, Component*> > &componentsToReplace();

    void clearInstallerCalculator();
    InstallerCalculator *installerCalculator() const;

    void clearUninstallerCalculator();
    UninstallerCalculator *uninstallerCalculator() const;

    bool runInstaller();
    bool isInstaller() const;

    bool runUninstaller();
    bool isUninstaller() const;

    void runUpdater();
    bool isUpdater() const;

    bool runPackageUpdater();
    bool isPackageManager() const;

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

    void installComponent(Component *component, double progressOperationSize,
        bool adminRightsGained = false);

signals:
    void installationStarted();
    void installationFinished();
    void uninstallationStarted();
    void uninstallationFinished();

public:
    UpdateFinder *m_updateFinder;
    UpdateFinder *m_compressedFinder;
    QSet<PackageSource> m_packageSources;
    QSet<PackageSource> m_compressedPackageSources;
    std::shared_ptr<LocalPackageHub> m_localPackageHub;
    QStringList m_filesForDelayedDeletion;

    int m_status;
    QString m_error;

    bool m_needsHardRestart;
    bool m_testChecksum;
    bool m_launchedAsRoot;
    bool m_completeUninstall;
    bool m_needToWriteMaintenanceTool;
    PackageManagerCoreData m_data;
    QHash<QString, bool> m_sharedFlags;
    QString m_installerBaseBinaryUnreplaced;

    QList<QInstaller::Component*> m_rootComponents;
    QList<QInstaller::Component*> m_rootDependencyReplacements;

    QList<QInstaller::Component*> m_updaterComponents;
    QList<QInstaller::Component*> m_updaterComponentsDeps;
    QList<QInstaller::Component*> m_updaterDependencyReplacements;

    OperationList m_ownedOperations;
    OperationList m_performedOperationsOld;
    OperationList m_performedOperationsCurrentSession;

    bool m_dependsOnLocalInstallerBinary;

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

private:
    void deleteMaintenanceTool();
    void registerMaintenanceTool();
    void unregisterMaintenanceTool();

    void writeMaintenanceToolBinary(QFile *const input, qint64 size, bool writeBinaryLayout);
    void writeMaintenanceToolBinaryData(QFileDevice *output, QFile *const input,
        const OperationList &performed, const BinaryLayout &layout);

    void runUndoOperations(const OperationList &undoOperations, double undoOperationProgressSize,
        bool adminRightsGained, bool deleteOperation);

    PackagesList remotePackages();
    PackagesList compressedPackages();
    LocalPackagesHash localInstalledPackages();
    bool fetchMetaInformationFromRepositories();
    bool fetchMetaInformationFromCompressedRepositories();
    bool addUpdateResourcesFromRepositories(bool parseChecksum, bool compressedRepository = false);
    void processFilesForDelayedDeletion();
    void findExecutablesRecursive(const QString &path, const QStringList &excludeFiles, QStringList *result);
    QStringList runningInstallerProcesses(const QStringList &exludeFiles);

private:
    PackageManagerCore *m_core;
    MetadataJob m_metadataJob;

    bool m_updates;
    bool m_compressedUpdates;
    bool m_repoFetched;
    bool m_updateSourcesAdded;
    qint64 m_magicBinaryMarker;
    bool m_componentsToInstallCalculated;

    mutable ScriptEngine *m_componentScriptEngine;
    mutable ScriptEngine *m_controlScriptEngine;
    // < name (component to replace), < replacement component, component to replace > >
    QHash<QString, QPair<Component*, Component*> > m_componentsToReplaceAllMode;
    QHash<QString, QPair<Component*, Component*> > m_componentsToReplaceUpdaterMode;

    InstallerCalculator *m_installerCalculator;
    UninstallerCalculator *m_uninstallerCalculator;

    PackageManagerProxyFactory *m_proxyFactory;

    ComponentModel *m_defaultModel;
    ComponentModel *m_updaterModel;

    QObject *m_guiObject;
    QScopedPointer<RemoteFileEngineHandler> m_remoteFileEngineHandler;

private:
    // remove once we deprecate isSelected, setSelected etc...
    void restoreCheckState();
    void storeCheckState();
    QHash<Component*, Qt::CheckState> m_coreCheckedHash;
};

} // namespace QInstaller

#endif  // PACKAGEMANAGERCORE_P_H
