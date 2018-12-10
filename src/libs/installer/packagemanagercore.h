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
#ifndef PACKAGEMANAGERCORE_H
#define PACKAGEMANAGERCORE_H

#include "binaryformat.h"
#include "protocol.h"
#include "repository.h"
#include "qinstallerglobal.h"

#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QStringList>
#include <QtCore/QVector>

namespace QInstaller {

class Component;
class ComponentModel;
class ScriptEngine;
class PackageManagerCorePrivate;
class PackageManagerProxyFactory;
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
    PackageManagerCore(qint64 magicmaker, const QList<OperationBlob> &ops,
        const QString &socketName = QString(),
        const QString &key = QLatin1String(Protocol::DefaultAuthorizationKey),
        Protocol::Mode mode = Protocol::Mode::Production);
    ~PackageManagerCore();

    enum UnstableError {
        DepencyToUnstable = 0,
        ShaMismatch,
        ScriptLoadingFailed,
        MissingDependency
    };
     Q_ENUM(UnstableError)

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

    enum struct ComponentType {
        Root = 0x1,
        Descendants = 0x2,
        Dependencies = 0x4,
        Replacements = 0x8,
        AllNoReplacements = (Root | Descendants | Dependencies),
        All = (Root | Descendants | Dependencies | Replacements)
    };
    Q_DECLARE_FLAGS(ComponentTypes, ComponentType)

    static QFont virtualComponentsFont();
    static void setVirtualComponentsFont(const QFont &font);

    static bool virtualComponentsVisible();
    static void setVirtualComponentsVisible(bool visible);

    static bool noForceInstallation();
    static void setNoForceInstallation(bool value);

    static bool createLocalRepositoryFromBinary();
    static void setCreateLocalRepositoryFromBinary(bool create);

    static Component *componentByName(const QString &name, const QList<Component *> &components);

    bool fetchLocalPackagesTree();
    LocalPackagesHash localInstalledPackages();

    void networkSettingsChanged();
    PackageManagerProxyFactory *proxyFactory() const;
    void setProxyFactory(PackageManagerProxyFactory *factory);

    PackagesList remotePackages();
    bool fetchRemotePackagesTree();
    bool fetchCompressedPackagesTree();

    bool run();
    void reset();

    void setGuiObject(QObject *gui);
    QObject *guiObject() const;

    Q_INVOKABLE void setDependsOnLocalInstallerBinary();
    Q_INVOKABLE bool localInstallerBinaryUsed();

    Q_INVOKABLE QList<QVariant> execute(const QString &program,
        const QStringList &arguments = QStringList(), const QString &stdIn = QString(),
        const QString &stdInCodec = QLatin1String("latin1"),
        const QString &stdOutCodec = QLatin1String("latin1")) const;
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
    Q_INVOKABLE QStringList values(const QString &key, const QStringList &defaultValue = QStringList()) const;

    // a way to have global flags shareable from a component script to another one
    // Deprecated since 2.0.0
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
    Q_INVOKABLE void setTemporaryRepositories(const QStringList &repositories,
                                              bool replace = false, bool compressed = false);
    Q_INVOKABLE void autoAcceptMessageBoxes();
    Q_INVOKABLE void autoRejectMessageBoxes();
    Q_INVOKABLE void setMessageBoxAutomaticAnswer(const QString &identifier, int button);

    quint64 size(QInstaller::Component *component, const QString &value) const;

    Q_INVOKABLE bool isFileExtensionRegistered(const QString &extension) const;
    Q_INVOKABLE bool fileExists(const QString &filePath) const;
    Q_INVOKABLE QString readFile(const QString &filePath, const QString &codecName) const;

public:
    ScriptEngine *componentScriptEngine() const;
    ScriptEngine *controlScriptEngine() const;

    // component handling

    void appendRootComponent(Component *components);
    void appendUpdaterComponent(Component *components);

    QList<Component *> components(ComponentTypes mask) const;
    Component *componentByName(const QString &identifier) const;

    Q_INVOKABLE bool calculateComponentsToInstall() const;
    QList<Component*> orderedComponentsToInstall() const;
    bool calculateComponents(QString *displayString);

    Q_INVOKABLE bool calculateComponentsToUninstall() const;
    QList<Component*> componentsToUninstall() const;

    QString componentsToInstallError() const;
    QString installReason(Component *component) const;

    QList<Component*> dependees(const Component *component) const;

    ComponentModel *defaultComponentModel() const;
    ComponentModel *updaterComponentModel() const;
    void updateComponentsSilently();

    // convenience
    Q_INVOKABLE bool isInstaller() const;
    Q_INVOKABLE bool isOfflineOnly() const;

    Q_INVOKABLE void setUninstaller();
    Q_INVOKABLE bool isUninstaller() const;

    Q_INVOKABLE void setUpdater();
    Q_INVOKABLE bool isUpdater() const;

    Q_INVOKABLE void setPackageManager();
    Q_INVOKABLE bool isPackageManager() const;

    bool isMaintainer() const;

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

    QStringList filesForDelayedDeletion() const;
    void addFilesForDelayedDeletion(const QStringList &files);

    static QString checkableName(const QString &name);
    static void parseNameAndVersion(const QString &requirement, QString *name, QString *version);
    static QStringList parseNames(const QStringList &requirements);

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
    void valueChanged(const QString &key, const QString &value);
    void statusChanged(QInstaller::PackageManagerCore::Status);
    void currentPageChanged(int page);
    void finishButtonClicked();

    void metaJobProgress(int progress);
    void metaJobTotalProgress(int progress);
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
    void unstableComponentFound(const QString &type, const QString &errorMessage, const QString &component);

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

    void updateDisplayVersions(const QString &displayKey);
    QString findDisplayVersion(const QString &componentName, const QHash<QString, QInstaller::Component*> &components,
                               const QString& versionKey, QHash<QString, bool> &visited);
    ComponentModel *componentModel(PackageManagerCore *core, const QString &objectName) const;
    QList<Component *> componentsMarkedForInstallation() const;

    bool fetchPackagesTree(const PackagesList &packages, const LocalPackagesHash installedPackages);

private:
    PackageManagerCorePrivate *const d;
    friend class PackageManagerCorePrivate;

private:
    // remove once we deprecate isSelected, setSelected etc...
    friend class ComponentSelectionPage;
    void restoreCheckState();
};
Q_DECLARE_OPERATORS_FOR_FLAGS(PackageManagerCore::ComponentTypes)

}

Q_DECLARE_METATYPE(QInstaller::PackageManagerCore*)

#endif  // PACKAGEMANAGERCORE_H
