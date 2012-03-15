/**************************************************************************
**
** This file is part of Installer Framework
**
** Copyright (c) 2010-2012 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (qt-info@nokia.com)
**
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
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** Other Usage
**
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
** If you have questions regarding the use of this file, please contact
** Nokia at qt-info@nokia.com.
**
**************************************************************************/
#ifndef PACKAGEMANAGERCORE_H
#define PACKAGEMANAGERCORE_H

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
    explicit PackageManagerCore();
    explicit PackageManagerCore(qint64 magicmaker, const OperationList &oldOperations = OperationList());
    ~PackageManagerCore();

    // status
    enum Status {
        Success = EXIT_SUCCESS,
        Failure = EXIT_FAILURE,
        Running,
        Canceled,
        Unfinished
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

    bool fetchLocalPackagesTree();
    LocalPackagesHash localInstalledPackages();

    void networkSettingsChanged();
    KDUpdater::FileDownloaderProxyFactory *proxyFactory() const;
    void setProxyFactory(KDUpdater::FileDownloaderProxyFactory *factory);

    PackagesList remotePackages();
    bool fetchRemotePackagesTree();

    bool run();
    RunMode runMode() const;
    void reset(const QHash<QString, QString> &params);

    Q_INVOKABLE QList<QVariant> execute(const QString &program,
        const QStringList &arguments = QStringList(), const QString &stdIn = QString()) const;
    Q_INVOKABLE bool executeDetached(const QString &program,
        const QStringList &arguments = QStringList()) const;
    Q_INVOKABLE QString environmentVariable(const QString &name) const;

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

    void writeUninstaller();
    QString uninstallerName() const;
    QString installerBinaryPath() const;

    bool testChecksum() const;
    void setTestChecksum(bool test);

    void addUserRepositories(const QSet<Repository> &repositories);
    void setTemporaryRepositories(const QSet<Repository> &repositories, bool replace = false);

    Q_INVOKABLE void autoAcceptMessageBoxes();
    Q_INVOKABLE void autoRejectMessageBoxes();
    Q_INVOKABLE void setMessageBoxAutomaticAnswer(const QString &identifier, int button);

    Q_INVOKABLE bool isFileExtensionRegistered(const QString &extension) const;

public:
    // component handling
    int rootComponentCount() const;
    Component *rootComponent(int i) const;
    QList<Component*> rootComponents() const;
    void appendRootComponent(Component *components);

    Q_INVOKABLE int updaterComponentCount() const;
    Component *updaterComponent(int i) const;
    QList<Component*> updaterComponents() const;
    void appendUpdaterComponent(Component *components);

    QList<Component*> availableComponents() const;
    Component *componentByName(const QString &identifier) const;

    bool calculateComponentsToInstall() const;
    QList<Component*> orderedComponentsToInstall() const;

    bool calculateComponentsToUninstall() const;
    QList<Component*> componentsToUninstall() const;

    QString componentsToInstallError() const;
    QString installReason(Component *component) const;

    QList<Component*> dependees(const Component *component) const;
    QList<Component*> dependencies(const Component *component, QStringList &missingComponents) const;

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

    Settings &settings() const;

    Q_INVOKABLE bool addWizardPage(QInstaller::Component *component, const QString &name, int page);
    Q_INVOKABLE bool removeWizardPage(QInstaller::Component *component, const QString &name);
    Q_INVOKABLE bool addWizardPageItem(QInstaller::Component *component, const QString &name, int page);
    Q_INVOKABLE bool removeWizardPageItem(QInstaller::Component *component, const QString &name);
    Q_INVOKABLE bool setDefaultPageVisible(int page, bool visible);

    void rollBackInstallation();

    int downloadNeededArchives(double partProgressSize);
    void installComponent(Component *component, double progressOperationSize);

    bool needsRestart() const;
    bool finishedWithSuccess() const;

    Q_INVOKABLE bool createLocalRepositoryFromBinary() const;
    Q_INVOKABLE void setCreateLocalRepositoryFromBinary(bool create);

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
    void componentAdded(QInstaller::Component *comp);
    void rootComponentsAdded(QList<QInstaller::Component*> components);
    void updaterComponentsAdded(QList<QInstaller::Component*> components);
    void componentsAboutToBeCleared();
    void valueChanged(const QString &key, const QString &value);
    void statusChanged(QInstaller::PackageManagerCore::Status);
    void currentPageChanged(int page);
    void finishButtonClicked();

    void metaJobInfoMessage(const QString &message);

    void startAllComponentsReset();
    void finishAllComponentsReset();

    void startUpdaterComponentsReset();
    void finishUpdaterComponentsReset();

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

    void setAutomatedPageSwitchEnabled(bool request);
    void coreNetworkSettingsChanged();

private:
    struct Data {
        RunMode runMode;
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
