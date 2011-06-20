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
#ifndef PACKAGEMANAGERCORE_P_H
#define PACKAGEMANAGERCORE_P_H

#include "common/installersettings.h"

#include <KDToolsCore/KDSysInfo>

#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QPair>
#include <QtCore/QPointer>

class FSEngineClientHandler;
QT_FORWARD_DECLARE_CLASS(QFile)
QT_FORWARD_DECLARE_CLASS(QFileInfo)

namespace KDUpdater {
    class Application;
    class UpdateOperation;
}

namespace QInstaller {

struct BinaryLayout;

class Component;
class TempDirDeleter;

class PackageManagerCorePrivate : public QObject
{
    Q_OBJECT;
    friend class PackageManagerCore;

public:
    enum OperationType {
        Backup,
        Perform,
        Undo
    };

    PackageManagerCorePrivate(PackageManagerCore *core);
    explicit PackageManagerCorePrivate(PackageManagerCore *core, qint64 magicInstallerMaker,
        const QList<KDUpdater::UpdateOperation*> &performedOperations);
    ~PackageManagerCorePrivate();

    static bool isProcessRunning(const QString &name,
        const QList<KDSysInfo::ProcessInfo> &processes);

    static bool performOperationThreaded(KDUpdater::UpdateOperation *op,
        PackageManagerCorePrivate::OperationType type = PackageManagerCorePrivate::Perform);

    void initialize();

    void setStatus(int status);
    bool statusCanceledOrFailed() const;

    QString targetDir() const;
    QString registerPath() const;

    QString uninstallerName() const;
    QString installerBinaryPath() const;
    void readUninstallerIniFile(const QString &targetDir);
    void writeUninstaller(QList<KDUpdater::UpdateOperation*> performedOperations);

    QString configurationFileName() const;
    QString componentsXmlPath() const;
    QString localComponentsXmlPath() const;

    void clearAllComponentLists();
    void clearUpdaterComponentLists();
    QHash<QString, QPair<Component*, Component*> > &componentsToReplace();

    void runInstaller();
    bool isInstaller() const;

    void runUninstaller();
    bool isUninstaller() const;

    void runUpdater();
    bool isUpdater() const;

    void runPackageUpdater();
    bool isPackageManager() const;

    QString replaceVariables(const QString &str) const;
    QByteArray replaceVariables(const QByteArray &str) const;

    void stopProcessesForUpdates(const QList<Component*> &components);
    int countProgressOperations(const QList<Component*> &components);
    int countProgressOperations(const QList<KDUpdater::UpdateOperation*> &operations);
    void connectOperationToInstaller(KDUpdater::UpdateOperation* const operation,
        double progressOperationPartSize);

    KDUpdater::UpdateOperation* createOwnedOperation(const QString &type);
    KDUpdater::UpdateOperation* takeOwnedOperation(KDUpdater::UpdateOperation *operation);

    KDUpdater::UpdateOperation* createPathOperation(const QFileInfo &fileInfo,
        const QString &componentName);
    void registerPathesForUninstallation(const QList<QPair<QString, bool> > &pathesForUninstallation,
        const QString &componentName);

    void addPerformed(KDUpdater::UpdateOperation* op) {
        m_performedOperationsCurrentSession.append(op);
    }

    void commitSessionOperations() {
        m_performedOperationsOld += m_performedOperationsCurrentSession;
        m_performedOperationsCurrentSession.clear();
    }

    void installComponent(Component *component, double progressOperationSize, bool adminRightsGained = false);

signals:
    void installationStarted();
    void installationFinished();
    void uninstallationStarted();
    void uninstallationFinished();

public:
    KDUpdater::Application *m_app;
    TempDirDeleter *m_tempDirDeleter;
    FSEngineClientHandler *m_FSEngineClientHandler;

    int m_status;
    Settings m_Settings;
    bool m_forceRestart;
    int m_silentRetries;
    bool m_testChecksum;
    bool m_launchedAsRoot;
    bool m_completeUninstall;
    bool m_needToWriteUninstaller;
    QHash<QString, QString> m_vars;
    QHash<QString, bool> m_sharedFlags;
    QString m_installerBaseBinaryUnreplaced;

    qint64 m_globalDictOffset;

    qint64 m_componentsCount;
    qint64 m_firstComponentStart;
    qint64 m_componentOffsetTableStart;

    qint64 m_componentsDictCount;
    qint64 m_firstComponentDictStart;
    qint64 m_componentDictOffsetTableStart;

    QList<Component*> m_rootComponents;
    QList<Component*> m_updaterComponents;
    QList<Component*> m_updaterComponentsDeps;

    QList<KDUpdater::UpdateOperation*> m_ownedOperations;
    QList<KDUpdater::UpdateOperation*> m_performedOperationsOld;
    QList<KDUpdater::UpdateOperation*> m_performedOperationsCurrentSession;

private:
    void deleteUninstaller();
    void registerUninstaller();
    void unregisterUninstaller();

    void writeUninstallerBinary(QFile *const input, qint64 size, bool writeBinaryLayout);
    void writeUninstallerBinaryData(QIODevice *output, QFile *const input,
        const QList<KDUpdater::UpdateOperation*> &performedOperations, const BinaryLayout &layout,
        bool compressOperations, bool forceUncompressedResources);

    void runUndoOperations(const QList<KDUpdater::UpdateOperation*> &undoOperations,
        double undoOperationProgressSize, bool adminRightsGained, bool deleteOperation);

private:
    PackageManagerCore *m_core;
    qint64 m_magicBinaryMarker;

    // < name (component to replace), < replacement component, component to replace > >
    QHash<QString, QPair<Component*, Component*> > m_componentsToReplaceAllMode;
    QHash<QString, QPair<Component*, Component*> > m_componentsToReplaceUpdaterMode;
};

}   // QInstaller

#endif  // PACKAGEMANAGERCORE_P_H
