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
#ifndef PACKAGEMANAGERCORE_H
#define PACKAGEMANAGERCORE_H

#include "binaryformat.h"
#include "repository.h"
#include "qinstallerglobal.h"

#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QStringList>
#include <QtCore/QVector>

namespace KDUpdater {
    class FileDownloaderProxyFactory;
}

namespace QInstaller {

class Component;
class ComponentModel;
class ScriptEngine;
class PackageManagerCorePrivate;
class Settings;

// -- PackageManagerCore

class INSTALLER_EXPORT PackageManagerCore : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(PackageManagerCore)

    Q_ENUMS(Status WizardPage)
    Q_PROPERTY(int status READ status NOTIFY statusChanged)

public:
    PackageManagerCore();
    PackageManagerCore(qint64 magicmaker, const QList<OperationBlob> &ops);
    ~PackageManagerCore();

    // status
    enum Status {
        Success = EXIT_SUCCESS,
        Failure = EXIT_FAILURE,
        Running,
        Canceled,
        Unfinished,
        ForceUpdate
    };
    Status status() const;
    QString error() const;

    enum WizardPage {
        Introduction = 0x1000,
        TargetDirectory = 0x2000,
        ComponentSelection = 0x3000,
        LicenseCheck = 0x4000,
        StartMenuSelection = 0x5000,
        ReadyForInstallation = 0x6000,
        PerformInstallation = 0x7000,
        InstallationFinished = 0x8000,
        End = 0xffff
    };

    static QFont virtualComponentsFont();
    static void setVirtualComponentsFont(const QFont &font);

    static bool virtualComponentsVisible();
    static void setVirtualComponentsVisible(bool visible);

    static bool noForceInstallation();
    static void setNoForceInstallation(bool value);

    static bool createLocalRepositoryFromBinary();
    static void setCreateLocalRepositoryFromBinary(bool create);

    bool fetchLocalPackagesTree();
    LocalPackagesHash localInstalledPackages();

    void networkSettingsChanged();
    KDUpdater::FileDownloaderProxyFactory *proxyFactory() const;
    void setProxyFactory(KDUpdater::FileDownloaderProxyFactory *factory);

    PackagesList remotePackages();
    bool fetchRemotePackagesTree();

    bool run();
    void reset(const QHash<QString, QString> &params);

    void setGuiObject(QObject *gui);
    QObject *guiObject() const;

    Q_INVOKABLE void setDependsOnLocalInstallerBinary();
    Q_INVOKABLE bool localInstallerBinaryUsed();

    Q_INVOKABLE QList<QVariant> execute(const QString &program,
        const QStringList &arguments = QStringList(), const QString &stdIn = QString()) const;
    Q_INVOKABLE bool executeDetached(const QString &program,
        const QStringList &arguments = QStringList(),
        const QString &workingDirectory = QString()) const;
    Q_INVOKABLE QString environmentVariable(const QString &name) const;

    Q_INVOKABLE bool operationExists(const QString &name);
    Q_INVOKABLE bool performOperation(const QString &name, const QStringList &arguments);

    Q_INVOKABLE static bool versionMatches(const QString &version, const QString &requirement);

    Q_INVOKABLE static QString findLibrary(const QString &name, const QStringList &paths = QStringList());
    Q_INVOKABLE static QString findPath(const QString &name, const QStringList &paths = QStringList());

    Q_INVOKABLE void setInstallerBaseBinary(const QString &path);

    // parameter handling
    Q_INVOKABLE bool containsValue(const QString &key) const;
    Q_INVOKABLE void setValue(const QString &key, const QString &value);
    Q_INVOKABLE QString value(const QString &key, const QString &defaultValue = QString()) const;

    //a way to have global flags share able from a component script to another one
    Q_INVOKABLE bool sharedFlag(const QString &key) const;
    Q_INVOKABLE void setSharedFlag(const QString &key, bool value = true);

    QString replaceVariables(const QString &str) const;
    QByteArray replaceVariables(const QByteArray &str) const;
    QStringList replaceVariables(const QStringList &str) const;

    void writeMaintenanceTool();
    void writeMaintenanceConfigFiles();

    QString maintenanceToolName() const;
    QString installerBinaryPath() const;

    bool testChecksum() const;
    void setTestChecksum(bool test);

    Q_INVOKABLE void addUserRepositories(const QStringList &repositories);
    Q_INVOKABLE void setTemporaryRepositories(const QStringList &repositories, bool replace = false);

    Q_INVOKABLE void autoAcceptMessageBoxes();
    Q_INVOKABLE void autoRejectMessageBoxes();
    Q_INVOKABLE void setMessageBoxAutomaticAnswer(const QString &identifier, int button);

    Q_INVOKABLE bool isFileExtensionRegistered(const QString &extension) const;
    Q_INVOKABLE bool fileExists(const QString &filePath) const;

public:
    ScriptEngine *componentScriptEngine() const;
    ScriptEngine *controlScriptEngine() const;

    // component handling

    int rootComponentCount() const;
    Component *rootComponent(int i) const;
    QList<Component*> rootComponents() const;
    void appendRootComponent(Component *components);

    QList<Component*> rootAndChildComponents() const;

    Q_INVOKABLE int updaterComponentCount() const;
    Component *updaterComponent(int i) const;
    QList<Component*> updaterComponents() const;
    void appendUpdaterComponent(Component *components);

    QList<Component*> availableComponents() const;
    Component *componentByName(const QString &identifier) const;

    Q_INVOKABLE bool calculateComponentsToInstall() const;
    QList<Component*> orderedComponentsToInstall() const;

    Q_INVOKABLE bool calculateComponentsToUninstall() const;
    QList<Component*> componentsToUninstall() const;

    QString componentsToInstallError() const;
    QString installReason(Component *component) const;

    QList<Component*> dependees(const Component *component) const;

    ComponentModel *defaultComponentModel() const;
    ComponentModel *updaterComponentModel() const;

    // convenience
    Q_INVOKABLE bool isInstaller() const;
    Q_INVOKABLE bool isOfflineOnly() const;

    Q_INVOKABLE void setUninstaller();
    Q_INVOKABLE bool isUninstaller() const;

    Q_INVOKABLE void setUpdater();
    Q_INVOKABLE bool isUpdater() const;

    Q_INVOKABLE void setPackageManager();
    Q_INVOKABLE bool isPackageManager() const;

    bool isVerbose() const;
    void setVerbose(bool on);

    Q_INVOKABLE bool gainAdminRights();
    Q_INVOKABLE void dropAdminRights();

    Q_INVOKABLE quint64 requiredDiskSpace() const;
    Q_INVOKABLE quint64 requiredTemporaryDiskSpace() const;

    Q_INVOKABLE bool isProcessRunning(const QString &name) const;
    Q_INVOKABLE bool killProcess(const QString &absoluteFilePath) const;

    Settings &settings() const;

    Q_INVOKABLE bool addWizardPage(QInstaller::Component *component, const QString &name, int page);
    Q_INVOKABLE bool removeWizardPage(QInstaller::Component *component, const QString &name);
    Q_INVOKABLE bool addWizardPageItem(QInstaller::Component *component, const QString &name, int page);
    Q_INVOKABLE bool removeWizardPageItem(QInstaller::Component *component, const QString &name);
    Q_INVOKABLE bool setDefaultPageVisible(int page, bool visible);
    Q_INVOKABLE void setValidatorForCustomPage(QInstaller::Component *component, const QString &name,
                                               const QString &callbackName);

    void rollBackInstallation();

    int downloadNeededArchives(double partProgressSize);

    bool needsHardRestart() const;
    void setNeedsHardRestart(bool needsHardRestart = true);
    bool finishedWithSuccess() const;

public Q_SLOTS:
    bool runInstaller();
    bool runUninstaller();
    bool runPackageUpdater();
    void interrupt();
    void setCanceled();
    void languageChanged();
    void setCompleteUninstallation(bool complete);
    void cancelMetaInfoJob();
    void componentsToInstallNeedsRecalculation();

Q_SIGNALS:
    void aboutCalculateComponentsToInstall() const;
    void finishedCalculateComponentsToInstall() const;
    void aboutCalculateComponentsToUninstall() const;
    void finishedCalculateComponentsToUninstall() const;
    void componentAdded(QInstaller::Component *comp);
    void rootComponentsAdded(QList<QInstaller::Component*> components);
    void updaterComponentsAdded(QList<QInstaller::Component*> components);
    void componentsAboutToBeCleared();
    void valueChanged(const QString &key, const QString &value);
    void statusChanged(QInstaller::PackageManagerCore::Status);
    void currentPageChanged(int page);
    void finishButtonClicked();

    void metaJobProgress(int progress);
    void metaJobInfoMessage(const QString &message);

    void startAllComponentsReset();
    void finishAllComponentsReset(const QList<QInstaller::Component*> &rootComponents);

    void startUpdaterComponentsReset();
    void finishUpdaterComponentsReset(const QList<QInstaller::Component*> &componentsWithUpdates);

    void installationStarted();
    void installationInterrupted();
    void installationFinished();
    void updateFinished();
    void uninstallationStarted();
    void uninstallationFinished();
    void titleMessageChanged(const QString &title);

    void wizardPageInsertionRequested(QWidget *widget, QInstaller::PackageManagerCore::WizardPage page);
    void wizardPageRemovalRequested(QWidget *widget);
    void wizardWidgetInsertionRequested(QWidget *widget, QInstaller::PackageManagerCore::WizardPage page);
    void wizardWidgetRemovalRequested(QWidget *widget);
    void wizardPageVisibilityChangeRequested(bool visible, int page);
    void setValidatorForCustomPageRequested(QInstaller::Component *component, const QString &name,
                                            const QString &callbackName);

    void setAutomatedPageSwitchEnabled(bool request);
    void coreNetworkSettingsChanged();

    void guiObjectChanged(QObject *gui);

private:
    struct Data {
        Package *package;
        QHash<QString, Component*> *components;
        const LocalPackagesHash *installedPackages;
        QHash<Component*, QStringList> replacementToExchangeables;
    };

    bool updateComponentData(struct Data &data, QInstaller::Component *component);
    void storeReplacedComponents(QHash<QString, Component*> &components, const struct Data &data);
    bool fetchAllPackages(const PackagesList &remotePackages, const LocalPackagesHash &localPackages);
    bool fetchUpdaterPackages(const PackagesList &remotePackages, const LocalPackagesHash &localPackages);

    static Component *subComponentByName(const QInstaller::PackageManagerCore *installer, const QString &name,
        const QString &version = QString(), Component *check = 0);

    void updateDisplayVersions(const QString &displayKey);
    QString findDisplayVersion(const QString &componentName, const QHash<QString, QInstaller::Component*> &components,
                               const QString& versionKey, QHash<QString, bool> &visited);
    ComponentModel *componentModel(PackageManagerCore *core, const QString &objectName) const;

private:
    PackageManagerCorePrivate *const d;
    friend class PackageManagerCorePrivate;

private:
    // remove once we deprecate isSelected, setSelected etc...
    friend class ComponentSelectionPage;
    void resetComponentsToUserCheckedState();
};

}

Q_DECLARE_METATYPE(QInstaller::PackageManagerCore*)

#endif  // PACKAGEMANAGERCORE_H
