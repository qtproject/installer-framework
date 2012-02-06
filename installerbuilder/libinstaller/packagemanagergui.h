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

#ifndef PACKAGEMANAGERGUI_H
#define PACKAGEMANAGERGUI_H

#include "packagemanagercore.h"

#include <QtCore/QEvent>
#include <QtCore/QMetaType>

#include <QtGui/QWizard>
#include <QtGui/QWizardPage>

// FIXME: move to private classes
QT_BEGIN_NAMESPACE
class QAbstractButton;
class QCheckBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QRadioButton;
class QTextBrowser;
class QScriptEngine;
QT_END_NAMESPACE

namespace QInstaller {

class PackageManagerCore;
class PackageManagerPage;
class PerformInstallationForm;


// -- PackageManagerGui

class INSTALLER_EXPORT PackageManagerGui : public QWizard
{
    Q_OBJECT

public:
    explicit PackageManagerGui(PackageManagerCore *core, QWidget *parent = 0);
    virtual ~PackageManagerGui();
    virtual void init() = 0;

    void loadControlScript(const QString& scriptPath);
    void callControlScriptMethod(const QString& methodName);

    QScriptEngine *controlScriptEngine() const;

    Q_INVOKABLE PackageManagerPage* page(int pageId) const;
    Q_INVOKABLE QWidget* pageWidgetByObjectName(const QString& name) const;
    Q_INVOKABLE QWidget* currentPageWidget() const;
    Q_INVOKABLE QString defaultButtonText(int wizardButton) const;
    Q_INVOKABLE void clickButton(int wizardButton, int delayInMs = 0);

    Q_INVOKABLE void showSettingsButton(bool show);
    Q_INVOKABLE void setSettingsButtonEnabled(bool enable);

    void updateButtonLayout();

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

protected Q_SLOTS:
    void wizardPageInsertionRequested(QWidget *widget, QInstaller::PackageManagerCore::WizardPage page);
    void wizardPageRemovalRequested(QWidget *widget);
    void wizardWidgetInsertionRequested(QWidget *widget, QInstaller::PackageManagerCore::WizardPage page);
    void wizardWidgetRemovalRequested(QWidget *widget);
    void wizardPageVisibilityChangeRequested(bool visible, int page);
    void slotCurrentPageChanged(int id);
    void delayedControlScriptExecution(int id);

    void setAutomatedPageSwitchEnabled(bool request);

private Q_SLOTS:
    void onLanguageChanged();
    void customButtonClicked(int which);

protected:
    bool event(QEvent *event);
    void showEvent(QShowEvent *event);
    PackageManagerCore *packageManagerCore() const { return m_core; }

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

    virtual bool isComplete() const;
    void setComplete(bool complete);

    virtual bool isInterruptible() const { return false; }
    PackageManagerGui* gui() const { return qobject_cast<PackageManagerGui*>(wizard()); }

protected:
    PackageManagerCore *packageManagerCore() const;
    QVariantHash elementsForPage(const QString &pageName) const;

    QString titleForPage(const QString &pageName, const QString &value = QString()) const;
    QString subTitleForPage(const QString &pageName, const QString &value = QString()) const;

    // Inserts widget into the same layout like a sibling identified
    // by its name. Default position is just behind the sibling.
    virtual void insertWidget(QWidget *widget, const QString &siblingName, int offset = 1);
    virtual QWidget *findWidget(const QString &objectName) const;

    virtual void setVisible(bool visible); // reimp
    virtual int nextId() const; // reimp

    virtual void entering() {} // called on entering
    virtual void leaving() {}  // called on leaving

    bool isConstructing() const { return m_fresh; }

private:
    QString titleFromHash(const QVariantHash &hash, const QString &value = QString()) const;

private:
    bool m_fresh;
    bool m_complete;

    PackageManagerCore *m_core;
};


// -- IntroductionPage

class INSTALLER_EXPORT IntroductionPage : public PackageManagerPage
{
    Q_OBJECT

public:
    explicit IntroductionPage(PackageManagerCore *core);

    void setWidget(QWidget *widget);
    void setText(const QString &text);

private:
    QLabel *m_msgLabel;
    QWidget *m_widget;
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

private:
    QTextBrowser *m_textBrowser;
    QListWidget *m_licenseListWidget;

    QRadioButton *m_acceptRadioButton;
    QRadioButton *m_rejectRadioButton;
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

protected:
    void entering();
    void showEvent(QShowEvent *event);

private Q_SLOTS:
    void setModified(bool modified);

private:
    class Private;
    Private *d;
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

protected:
    void entering();
    void leaving();

private Q_SLOTS:
    void targetDirSelected();
    void dirRequested();

private:
    QLineEdit *m_lineEdit;
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

    bool isComplete() const;

private slots:
    void toggleDetails();

protected:
    void entering();
    void leaving();

private:
    void refreshTaskDetailsBrowser();

private:
    QLabel *m_msgLabel;
    QPushButton *m_taskDetailsButton;
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
    void installationRequested();
    void setAutomatedPageSwitchEnabled(bool request);

private Q_SLOTS:
    void installationStarted();
    void installationFinished();
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
