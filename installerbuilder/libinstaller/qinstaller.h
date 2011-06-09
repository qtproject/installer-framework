/**************************************************************************
**
** This file is part of Qt SDK**
**
** Copyright (c) 2010 Nokia Corporation and/or its subsidiary(-ies).*
**
** Contact:  Nokia Corporation qt-info@nokia.com**
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
#ifndef QINSTALLER_H
#define QINSTALLER_H

#include "common/repository.h"
#include "qinstallerglobal.h"

#include <KDUpdater/KDUpdater>

#include <QtCore/QAbstractItemModel>
#include <QtCore/QObject>
#include <QtCore/QStringList>
#include <QtCore/QVector>
#include <QtCore/QHash>

#include <QtGui/QMessageBox>

#include <QtScript/QScriptable>
#include <QtScript/QScriptValue>

namespace KDUpdater {
    class Application;
    class PackagesInfo;
    class Update;
    class UpdateOperation;
}

QT_BEGIN_NAMESPACE
class QDir;
class QFile;
class QIODevice;
QT_END_NAMESPACE

class KDJob;

#define INSTALLERBASE_VERSION "2"

/*
 * TRANSLATOR QInstaller::Installer
 */
namespace QInstaller {

class Component;
class GetRepositoriesMetaInfoJob;
class InstallerPrivate;
class InstallerSettings;
class MessageBoxHandler;

class INSTALLER_EXPORT Installer : public QObject
{
    Q_OBJECT

    Q_ENUMS(Status WizardPage)
    Q_PROPERTY(Status status READ status NOTIFY statusChanged)

public:
    Installer();
    explicit Installer(qint64 magicmaker, const QVector<KDUpdater::UpdateOperation*> &performedOperations
            = QVector< KDUpdater::UpdateOperation*>());
    ~Installer();

    // status
    enum Status {
        Success = EXIT_SUCCESS,
        Failure = EXIT_FAILURE,
        Running,
        Canceled,
        Unfinished
    };
    Status status() const;

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

    QHash<QString, KDUpdater::PackageInfo> localInstalledPackages();
    GetRepositoriesMetaInfoJob* fetchMetaInformation(const InstallerSettings &settings);
    bool addUpdateResourcesFrom(GetRepositoriesMetaInfoJob *metaInfoJob, const InstallerSettings &settings,
        bool parseChecksum);

    bool fetchAllPackages();
    bool fetchUpdaterPackages();

    bool run();
    RunMode runMode() const;
    void reset(const QHash<QString, QString> &params);

    Q_INVOKABLE QList<QVariant> execute(const QString &program,
        const QStringList &arguments = QStringList(), const QString &stdIn = QString()) const;
    Q_INVOKABLE QString environmentVariable(const QString &name) const;

    Q_INVOKABLE bool performOperation(const QString &name, const QStringList &arguments);

    Q_INVOKABLE static bool versionMatches(const QString &version, const QString &requirement);

    Q_INVOKABLE static QString findLibrary(const QString &name, const QStringList &pathes = QStringList());
    Q_INVOKABLE static QString findPath(const QString &name, const QStringList &pathes = QStringList());

    Q_INVOKABLE void setInstallerBaseBinary(const QString &path);

    // parameter handling
    Q_INVOKABLE void setValue(const QString &key, const QString &value);
    Q_INVOKABLE virtual QString value(const QString &key, const QString &defaultValue = QString()) const;
    Q_INVOKABLE bool containsValue(const QString &key) const;

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

    KDUpdater::Application &updaterApplication() const;
    void setUpdaterApplication(KDUpdater::Application *app);

    void addRepositories(const QList<Repository> &repositories);
    void setTemporaryRepositories(const QList<Repository> &repositories, bool replace = false);

    Q_INVOKABLE void autoAcceptMessageBoxes();
    Q_INVOKABLE void autoRejectMessageBoxes();
    Q_INVOKABLE void setMessageBoxAutomaticAnswer(const QString &identifier, int button);

    Q_INVOKABLE bool isFileExtensionRegistered(const QString &extension) const;

public:
    // component handling
    int rootComponentCount(RunMode runMode) const;
    Component *rootComponent(int i, RunMode runMode) const;
    void appendRootComponent(Component *components, RunMode runMode);

    Component *componentByName(const QString &identifier) const;
    QList<Component*> components(bool recursive, RunMode runMode) const;
    QList<Component*> componentsToInstall(RunMode runMode) const;

    QList<Component*> dependees(const Component *component) const;
    QList<Component*> missingDependencies(const Component *component) const;
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

    const InstallerSettings &settings() const;

    Q_INVOKABLE bool addWizardPage(QInstaller::Component *component, const QString &name, int page);
    Q_INVOKABLE bool removeWizardPage(QInstaller::Component *component, const QString &name);
    Q_INVOKABLE bool addWizardPageItem(QInstaller::Component *component, const QString &name, int page);
    Q_INVOKABLE bool removeWizardPageItem(QInstaller::Component *component, const QString &name);
    Q_INVOKABLE bool setDefaultPageVisible(int page, bool visible);

    void installSelectedComponents();
    void rollBackInstallation();

    int downloadNeededArchives(RunMode runMode, double partProgressSize);
    void installComponent(Component *component, double progressOperationSize);

    bool needsRestart() const;
    bool finishedWithSuccess() const;

public Q_SLOTS:
    bool runInstaller();
    bool runUninstaller();
    bool runPackageUpdater();
    void interrupt();
    void setCanceled();
    void languageChanged();
    void setCompleteUninstallation(bool complete);

Q_SIGNALS:
    void componentAdded(QInstaller::Component *comp);
    void rootComponentsAdded(QList<QInstaller::Component*> components);
    void updaterComponentsAdded(QList<QInstaller::Component*> components);
    void componentsAboutToBeCleared();
    void valueChanged(const QString &key, const QString &value);
    void statusChanged(QInstaller::Installer::Status);
    void currentPageChanged(int page);
    void finishButtonClicked();

    void cancelMetaInfoJob();
    void metaJobInfoMessage(KDJob* job, const QString &message);

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

    void wizardPageInsertionRequested(QWidget *widget, Installer::WizardPage page);
    void wizardPageRemovalRequested(QWidget *widget);
    void wizardWidgetInsertionRequested(QWidget *widget, Installer::WizardPage page);
    void wizardWidgetRemovalRequested(QWidget *widget);
    void wizardPageVisibilityChangeRequested(bool visible, int page);

    void setAutomatedPageSwitchEnabled(bool request);

private:
    bool setAndParseLocalComponentsFile(KDUpdater::PackagesInfo &packagesInfo);
    Installer::Status handleComponentsFileSetOrParseError(const QString &arg1,
        const QString &arg2 = QString(), bool withRetry = true);

    struct Data {
        KDUpdater::Update *package;
        QMap<QString, Component*> *components;
        GetRepositoriesMetaInfoJob *metaInfoJob;
        QHash<Component*, QStringList> replacementToExchangeables;
        QHash<QString, KDUpdater::PackageInfo> *installedPackages;
    };
    bool updateComponentData(struct Data &data, QInstaller::Component *component);
    static Component *subComponentByName(const QInstaller::Installer *installer, const QString &name,
        const QString &version = QString(), Component *check = 0);
    void storeReplacedComponents(QMap<QString, Component*> &components,
        const QHash<Component*, QStringList> &replacementToExchangeables);

private:
    InstallerPrivate* const d;
    friend class InstallerPrivate;
};

}

Q_DECLARE_METATYPE(QInstaller::Installer*)

#endif // QINSTALLER_H
