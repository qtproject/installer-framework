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

#ifndef PACKAGEMANAGERGUI_H
#define PACKAGEMANAGERGUI_H

#include "packagemanagercore.h"

#include <QtCore/QEvent>
#include <QtCore/QMetaType>
#include <QtCore/QTimer>

#include <QWizard>
#include <QWizardPage>

// FIXME: move to private classes
QT_BEGIN_NAMESPACE
class QAbstractButton;
class QCheckBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QProgressBar;
class QRadioButton;
class QTextBrowser;
class QWinTaskbarButton;
QT_END_NAMESPACE

namespace QInstaller {

class PackageManagerCore;
class PackageManagerPage;
class PerformInstallationForm;
class ComponentSelectionPagePrivate;

// -- PackageManagerGui

class INSTALLER_EXPORT PackageManagerGui : public QWizard
{
    Q_OBJECT

public:
    explicit PackageManagerGui(PackageManagerCore *core, QWidget *parent = 0);
    virtual ~PackageManagerGui() = 0;

    void loadControlScript(const QString& scriptPath);
    void callControlScriptMethod(const QString& methodName);

    QWidget *pageById(int id) const;
    QWidget *pageByObjectName(const QString &name) const;

    QWidget *currentPageWidget() const;
    QWidget *pageWidgetByObjectName(const QString &name) const;

    QString defaultButtonText(int wizardButton) const;
    void clickButton(int wizardButton, int delayInMs = 0);
    bool isButtonEnabled(int wizardButton);

    void showSettingsButton(bool show);
    void setSettingsButtonEnabled(bool enable);

    void updateButtonLayout();
    static QWizard::WizardStyle getStyle(const QString &name);

    void setSilent(bool silent);
    bool isSilent() const;

    void setTextItems(QObject *object, const QStringList &items);

Q_SIGNALS:
    void interrupted();
    void languageChanged();
    void finishButtonClicked();
    void gotRestarted();
    void settingsButtonClicked();

public Q_SLOTS:
    void cancelButtonClicked();
    void reject();
    void rejectWithoutPrompt();
    void showFinishedPage();
    void setModified(bool value);
    void setMaxSize();

protected Q_SLOTS:
    void wizardPageInsertionRequested(QWidget *widget, QInstaller::PackageManagerCore::WizardPage page);
    void wizardPageRemovalRequested(QWidget *widget);
    void wizardWidgetInsertionRequested(QWidget *widget, QInstaller::PackageManagerCore::WizardPage page);
    void wizardWidgetRemovalRequested(QWidget *widget);
    void wizardPageVisibilityChangeRequested(bool visible, int page);
    void setValidatorForCustomPageRequested(QInstaller::Component *component, const QString &name,
                                            const QString &callbackName);

    void setAutomatedPageSwitchEnabled(bool request);

private Q_SLOTS:
    void onLanguageChanged();
    void customButtonClicked(int which);
    void dependsOnLocalInstallerBinary();
    void currentPageChanged(int newId);

protected:
    bool event(QEvent *event);
    void showEvent(QShowEvent *event);
    PackageManagerCore *packageManagerCore() const { return m_core; }
    void executeControlScript(int pageId);

private:
    class Private;
    Private *const d;
    PackageManagerCore *m_core;
};


// -- PackageManagerPage

class INSTALLER_EXPORT PackageManagerPage : public QWizardPage
{
    Q_OBJECT

public:
    explicit PackageManagerPage(PackageManagerCore *core);
    virtual ~PackageManagerPage() {}

    virtual QPixmap logoPixmap() const;
    virtual QString productName() const;
    virtual QPixmap watermarkPixmap() const;
    virtual QPixmap bannerPixmap() const;

    void setColoredTitle(const QString &title);
    void setColoredSubTitle(const QString &subTitle);

    virtual bool isComplete() const;
    void setComplete(bool complete);

    virtual bool isInterruptible() const { return false; }
    PackageManagerGui* gui() const { return qobject_cast<PackageManagerGui*>(wizard()); }

    void setValidatePageComponent(QInstaller::Component *component);

    bool validatePage();

    bool settingsButtonRequested() const { return m_needsSettingsButton; }
    void setSettingsButtonRequested(bool request) { m_needsSettingsButton = request; }

signals:
    void entered();
    void left();

protected:
    PackageManagerCore *packageManagerCore() const;

    // Inserts widget into the same layout like a sibling identified
    // by its name. Default position is just behind the sibling.
    virtual void insertWidget(QWidget *widget, const QString &siblingName, int offset = 1);
    virtual QWidget *findWidget(const QString &objectName) const;

    virtual int nextId() const; // reimp

    // Used to support some kind of initializePage() in the case the wizard has been set
    // to QWizard::IndependentPages. If that option has been set, initializePage() would be only
    // called once. So we provide entering() and leaving() based on currentPageChanged() signal.
    virtual void entering() {} // called on entering
    virtual void leaving() {}  // called on leaving

private:
    bool m_complete;
    QString m_titleColor;
    bool m_needsSettingsButton;

    PackageManagerCore *m_core;
    QInstaller::Component *validatorComponent;

    friend class PackageManagerGui;
};


// -- IntroductionPage

class INSTALLER_EXPORT IntroductionPage : public PackageManagerPage
{
    Q_OBJECT

public:
    explicit IntroductionPage(PackageManagerCore *core);

    void setText(const QString &text);

    int nextId() const;
    bool validatePage();

    void showAll();
    void hideAll();
    void showMetaInfoUpdate();
    void showMaintenanceTools();
    void setMaintenanceToolsEnabled(bool enable);

public Q_SLOTS:
    void onCoreNetworkSettingsChanged();
    void setMessage(const QString &msg);
    void onProgressChanged(int progress);
    void setTotalProgress(int totalProgress);
    void setErrorMessage(const QString &error);

Q_SIGNALS:
    void packageManagerCoreTypeChanged();

private Q_SLOTS:
    void setUpdater(bool value);
    void setUninstaller(bool value);
    void setPackageManager(bool value);

private:
    void entering();
    void leaving();

    void showWidgets(bool show);
    bool validRepositoriesAvailable() const;

private:
    bool m_updatesFetched;
    bool m_allPackagesFetched;

    QLabel *m_label;
    QLabel *m_msgLabel;
    QLabel *m_errorLabel;
    QProgressBar *m_progressBar;
    QRadioButton *m_packageManager;
    QRadioButton *m_updateComponents;
    QRadioButton *m_removeAllComponents;

#ifdef Q_OS_WIN
    QWinTaskbarButton *m_taskButton;
#endif
};


// -- LicenseAgreementPage

class INSTALLER_EXPORT LicenseAgreementPage : public PackageManagerPage
{
    Q_OBJECT
    class ClickForwarder;

public:
    explicit LicenseAgreementPage(PackageManagerCore *core);

    void entering();
    bool isComplete() const;

private Q_SLOTS:
    void openLicenseUrl(const QUrl &url);
    void currentItemChanged(QListWidgetItem *current);

private:
    void addLicenseItem(const QHash<QString, QPair<QString, QString> > &hash);
    void updateUi();

private:
    QTextBrowser *m_textBrowser;
    QListWidget *m_licenseListWidget;

    QRadioButton *m_acceptRadioButton;
    QRadioButton *m_rejectRadioButton;

    QLabel *m_acceptLabel;
    QLabel *m_rejectLabel;
};


// -- ComponentSelectionPage

class INSTALLER_EXPORT ComponentSelectionPage : public PackageManagerPage
{
    Q_OBJECT

public:
    explicit ComponentSelectionPage(PackageManagerCore *core);
    ~ComponentSelectionPage();

    bool isComplete() const;

    Q_INVOKABLE void selectAll();
    Q_INVOKABLE void deselectAll();
    Q_INVOKABLE void selectDefault();
    Q_INVOKABLE void selectComponent(const QString &id);
    Q_INVOKABLE void deselectComponent(const QString &id);
    Q_INVOKABLE void allowCompressedRepositoryInstall();
    Q_INVOKABLE bool addVirtualComponentToUninstall(const QString &name);

protected:
    void entering();
    void leaving();
    void showEvent(QShowEvent *event);

private Q_SLOTS:
    void setModified(bool modified);

private:
    friend class ComponentSelectionPagePrivate;
    ComponentSelectionPagePrivate *const d;
};


// -- TargetDirectoryPage

class INSTALLER_EXPORT TargetDirectoryPage : public PackageManagerPage
{
    Q_OBJECT

public:
    explicit TargetDirectoryPage(PackageManagerCore *core);
    QString targetDir() const;
    void setTargetDir(const QString &dirName);

    void initializePage();
    bool validatePage();
    bool isComplete() const;

protected:
    void entering();
    void leaving();

private Q_SLOTS:
    void dirRequested();

private:
    QString targetDirWarning() const;
    bool askQuestion(const QString &identifier, const QString &message);
    bool failWithError(const QString &identifier, const QString &message);

private:
    QLineEdit *m_lineEdit;
    QLabel *m_warningLabel;
    QTimer m_textChangeTimer;
};


// -- StartMenuDirectoryPage

class INSTALLER_EXPORT StartMenuDirectoryPage : public PackageManagerPage
{
    Q_OBJECT

public:
    explicit StartMenuDirectoryPage(PackageManagerCore *core);

    QString startMenuDir() const;
    void setStartMenuDir(const QString &startMenuDir);

protected:
    void leaving();

private Q_SLOTS:
    void currentItemChanged(QListWidgetItem* current);

private:
    QString startMenuPath;
    QLineEdit *m_lineEdit;
    QListWidget *m_listWidget;
};


// -- ReadyForInstallationPage

class INSTALLER_EXPORT ReadyForInstallationPage : public PackageManagerPage
{
    Q_OBJECT

public:
    explicit ReadyForInstallationPage(PackageManagerCore *core);

protected:
    void entering();
    void leaving();

private:
    QLabel *m_msgLabel;
    QTextBrowser* m_taskDetailsBrowser;
};


// -- PerformInstallationPage

class INSTALLER_EXPORT PerformInstallationPage : public PackageManagerPage
{
    Q_OBJECT

public:
    explicit PerformInstallationPage(PackageManagerCore *core);
    ~PerformInstallationPage();
    bool isAutoSwitching() const;

protected:
    void entering();
    void leaving();
    bool isInterruptible() const { return true; }

public Q_SLOTS:
    void setTitleMessage(const QString& title);

Q_SIGNALS:
    void setAutomatedPageSwitchEnabled(bool request);

private Q_SLOTS:
    void installationStarted();
    void installationFinished();

    void uninstallationStarted();
    void uninstallationFinished();

    void toggleDetailsWereChanged();

private:
    PerformInstallationForm *m_performInstallationForm;
};


// -- FinishedPage

class INSTALLER_EXPORT FinishedPage : public PackageManagerPage
{
    Q_OBJECT

public:
    explicit FinishedPage(PackageManagerCore *core);

public Q_SLOTS:
    void handleFinishClicked();
    void cleanupChangedConnects();

protected:
    void entering();
    void leaving();

private:
    QLabel *m_msgLabel;
    QCheckBox *m_runItCheckBox;
    QAbstractButton *m_commitButton;
};


// -- RestartPage

class INSTALLER_EXPORT RestartPage : public PackageManagerPage
{
    Q_OBJECT

public:
    explicit RestartPage(PackageManagerCore *core);

    virtual int nextId() const;

protected:
    void entering();
    void leaving();

Q_SIGNALS:
    void restart();
};

} //namespace QInstaller

#endif  // PACKAGEMANAGERGUI_H
