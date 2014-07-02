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

#ifndef PACKAGEMANAGERCORE_P_H
#define PACKAGEMANAGERCORE_P_H

#include "metadatajob.h"
#include "packagemanagercore.h"
#include "packagemanagercoredata.h"
#include "qinstallerglobal.h"
#include "remotefileengine.h"

#include "kdsysinfo.h"
#include "kdupdaterapplication.h"
#include "kdupdaterfiledownloaderfactory.h"
#include "kdupdaterupdatefinder.h"

#include <QObject>

class KDJob;

QT_FORWARD_DECLARE_CLASS(QFile)
QT_FORWARD_DECLARE_CLASS(QFileInfo)

using namespace KDUpdater;

namespace QInstaller {

struct BinaryLayout;
class Component;
class ScriptEngine;
class ComponentModel;
class TempDirDeleter;

/*
    The default configuration interface implementation does call QSettings to save files for later deletion,
    though according to QSettings there should nothing be written if QSettings is not setup properly (which
    we do not in our case). Still, caused by a broken QSettings implementation at least on Linux we write an
    empty config file which resulted in QTIFW-196. To workaround the issue we now use this empty dummy class.
*/
class DummyConfigurationInterface : public KDUpdater::ConfigurationInterface
{
public:
    QVariant value(const QString &key) const
    {
        Q_UNUSED(key)
        return QVariant();
    }
    void setValue(const QString &key, const QVariant &value)
    {
        if (value.isNull())
            qDebug() << "DummyConfigurationInterface called with key:" << key << "and value:" << value;
    }
};

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
        const OperationList &performedOperations);
    ~PackageManagerCorePrivate();

    static bool isProcessRunning(const QString &name, const QList<ProcessInfo> &processes);

    static bool performOperationThreaded(Operation *op, PackageManagerCorePrivate::OperationType type
        = PackageManagerCorePrivate::Perform);

    void initialize(const QHash<QString, QString> &params);
    bool isOfflineOnly() const;

    bool statusCanceledOrFailed() const;
    void setStatus(int status, const QString &error = QString());

    QString targetDir() const;
    QString registerPath() const;

    QString uninstallerName() const;
    QString installerBinaryPath() const;

    void writeMaintenanceConfigFiles();
    void readMaintenanceConfigFiles(const QString &targetDir);

    void writeUninstaller(OperationList performedOperations);

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

    void clearComponentsToInstall();
    bool appendComponentsToInstall(const QList<Component*> &components);
    bool appendComponentToInstall(Component *components);
    QString installReason(Component *component);

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

    bool appendComponentToUninstall(Component *component);
    bool appendComponentsToUninstall(const QList<Component*> &components);

signals:
    void installationStarted();
    void installationFinished();
    void uninstallationStarted();
    void uninstallationFinished();

public:
    UpdateFinder *m_updateFinder;
    Application m_updaterApplication;

    int m_status;
    QString m_error;

    bool m_needsHardRestart;
    bool m_testChecksum;
    bool m_launchedAsRoot;
    bool m_completeUninstall;
    bool m_needToWriteUninstaller;
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
    void infoMessage(KDJob *, const QString &message) {
        emit m_core->metaJobInfoMessage(message);
    }
    void infoProgress(KDJob *, quint64 progress, quint64) {
        emit m_core->metaJobProgress(progress);
    }

    void handleMethodInvocationRequest(const QString &invokableMethodName);

private:
    void deleteUninstaller();
    void registerUninstaller();
    void unregisterUninstaller();

    void writeUninstallerBinary(QFile *const input, qint64 size, bool writeBinaryLayout);
    void writeUninstallerBinaryData(QFileDevice *output, QFile *const input,
        const OperationList &performed, const BinaryLayout &layout);

    void runUndoOperations(const OperationList &undoOperations, double undoOperationProgressSize,
        bool adminRightsGained, bool deleteOperation);

    PackagesList remotePackages();
    LocalPackagesHash localInstalledPackages();
    bool fetchMetaInformationFromRepositories();
    bool addUpdateResourcesFromRepositories(bool parseChecksum);
    void realAppendToInstallComponents(Component *component);
    void insertInstallReason(Component *component, const QString &reason);

private:
    PackageManagerCore *m_core;
    MetadataJob m_metadataJob;

    bool m_updates;
    bool m_repoFetched;
    bool m_updateSourcesAdded;
    qint64 m_magicBinaryMarker;
    bool m_componentsToInstallCalculated;

    mutable ScriptEngine *m_componentScriptEngine;
    mutable ScriptEngine *m_controlScriptEngine;
    // < name (component to replace), < replacement component, component to replace > >
    QHash<QString, QPair<Component*, Component*> > m_componentsToReplaceAllMode;
    QHash<QString, QPair<Component*, Component*> > m_componentsToReplaceUpdaterMode;

    //calculate installation order variables
    QList<Component*> m_orderedComponentsToInstall;
    QHash<Component*, QSet<Component*> > m_visitedComponents;

    QSet<QString> m_toInstallComponentIds; //for faster lookups

    //we can't use this reason hash as component id hash, because some reasons are ready before
    //the component is added
    QHash<QString, QString> m_toInstallComponentIdReasonHash;

    QSet<Component*> m_componentsToUninstall;
    QString m_componentsToInstallError;
    FileDownloaderProxyFactory *m_proxyFactory;

    ComponentModel *m_defaultModel;
    ComponentModel *m_updaterModel;

    QObject *m_guiObject;
    QScopedPointer<RemoteFileEngineHandler> m_remoteFileEngineHandler;

private:
    // remove once we deprecate isSelected, setSelected etc...
    void resetComponentsToUserCheckedState();
    QHash<Component*, Qt::CheckState> m_coreCheckedHash;
    void setCheckedState(Component *component, Qt::CheckState state);
};

} // namespace QInstaller

#endif  // PACKAGEMANAGERCORE_P_H
