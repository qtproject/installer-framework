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
#ifndef QINSTALLER_GUI_H
#define QINSTALLER_GUI_H

#include "qinstaller.h"
#include "messageboxhandler.h"

#include <QtCore/QEvent>
#include <QtCore/QMetaType>

#include <QtGui/QAbstractButton>
#include <QtGui/QWizard>

// FIXME: move to private classes
QT_BEGIN_NAMESPACE
class QCheckBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QProgressBar;
class QRadioButton;
class QTextBrowser;
class QTreeView;
class QScriptEngine;
QT_END_NAMESPACE

namespace QInstaller {

class Installer;
class IntroductionPage;
class Page;
class PerformInstallationForm;


// -- MessageBoxHandlerImpl

class INSTALLER_EXPORT MessageBoxHandlerImpl : public MessageBoxHandler
{
public:
    explicit MessageBoxHandlerImpl(QWidget* parentWidget);
    void setAutomaticAnswer(const QString& identifier, QMessageBox::StandardButton answer);
    void setDefaultAction(DefaultAction da);

    QMessageBox::StandardButton critical(const QString& identifier, const QString& title,
        const QString& text, QMessageBox::StandardButtons buttons = QMessageBox::Ok,
        QMessageBox::StandardButton button = QMessageBox::NoButton) const;
    QMessageBox::StandardButton information(const QString& identifier, const QString& title,
        const QString& text, QMessageBox::StandardButtons buttons = QMessageBox::Ok,
        QMessageBox::StandardButton button = QMessageBox::NoButton) const;
    QMessageBox::StandardButton question(const QString& identifier, const QString& title,
        const QString& text, QMessageBox::StandardButtons buttons = QMessageBox::Yes|QMessageBox::No,
        QMessageBox::StandardButton button = QMessageBox::NoButton) const;
    QMessageBox::StandardButton warning(const QString& identifier, const QString& title,
        const QString& text, QMessageBox::StandardButtons buttons = QMessageBox::Ok,
        QMessageBox::StandardButton button = QMessageBox::NoButton) const;
    QMessageBox::StandardButton critical(QWidget* parent, const QString& identifier,
        const QString& title, const QString& text,
        QMessageBox::StandardButtons buttons = QMessageBox::Ok,
        QMessageBox::StandardButton button = QMessageBox::NoButton) const;
    QMessageBox::StandardButton information(QWidget* parent, const QString& identifier,
        const QString& title, const QString& text,
        QMessageBox::StandardButtons buttons = QMessageBox::Ok,
        QMessageBox::StandardButton button = QMessageBox::NoButton) const;
    QMessageBox::StandardButton question(QWidget* parent, const QString& identifier,
        const QString& title, const QString& text,
        QMessageBox::StandardButtons buttons = QMessageBox::Yes|QMessageBox::No,
        QMessageBox::StandardButton button = QMessageBox::NoButton) const;
    QMessageBox::StandardButton warning(QWidget* parent, const QString& identifier,
        const QString& title, const QString& text,
        QMessageBox::StandardButtons buttons = QMessageBox::Ok,
        QMessageBox::StandardButton button = QMessageBox::NoButton) const;

private:
    QMessageBox::StandardButton autoReply(QMessageBox::StandardButtons buttons) const;

private:
    QList<QMessageBox::Button> m_buttonOrder;
    QWidget* m_parentWidget;
    DefaultAction m_defaultAction;
    QHash<QString,QMessageBox::StandardButton> m_automaticAnswers;
};


// -- Gui

class INSTALLER_EXPORT Gui : public QWizard
{
    Q_OBJECT

public:
    explicit Gui(Installer *installer, QWidget *parent = 0);
    ~Gui();
    virtual void init() = 0;

    void loadControlScript(const QString& scriptPath);
    void callControlScriptMethod(const QString& methodName);
    void triggerControlScriptForCurrentPage();

    QScriptEngine* controlScriptEngine() const;

    Q_INVOKABLE Page* page(int pageId) const;
    Q_INVOKABLE QWidget* pageWidgetByObjectName(const QString& name) const;
    Q_INVOKABLE QWidget* currentPageWidget() const;
    Q_INVOKABLE QString defaultButtonText(int wizardButton) const;
    Q_INVOKABLE void clickButton(int wizardButton, int delayInMs = 0);

Q_SIGNALS:
    void interrupted();
    void languageChanged();
    void finishButtonClicked();
    void gotRestarted();

public slots:
    void cancelButtonClicked();
    void reject();
    void rejectWithoutPrompt();
    void showFinishedPage();
    void setModified(bool value);

protected Q_SLOTS:
    void wizardPageInsertionRequested(QWidget* widget, Installer::WizardPage page);
    void wizardPageRemovalRequested(QWidget* widget);
    void wizardWidgetInsertionRequested(QWidget* widget, Installer::WizardPage page);
    void wizardWidgetRemovalRequested(QWidget* widget);
    void wizardPageVisibilityChangeRequested(bool visible, int page);
    void slotCurrentPageChanged(int id);
    void delayedControlScriptExecution(int id);

    void setAutomatedPageSwitchEnabled(bool request);

protected:
    bool event(QEvent* event);
    Installer *m_installer;

private:
    class Private;
    Private* const d;
};


// -- Page

class INSTALLER_EXPORT Page : public QWizardPage
{
    Q_OBJECT

public:
    explicit Page(Installer *installer);

    virtual bool isInterruptible() const { return false; }
    virtual QPixmap watermarkPixmap() const;
    virtual QPixmap logoPixmap() const;
    virtual QString productName() const;

    virtual bool isComplete() const;
    void setComplete(bool complete);

    Gui* gui() const { return qobject_cast<Gui*>(wizard()); }

protected:
    Installer *installer() const;

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
    Installer *m_installer;
    bool m_fresh;
    bool m_complete;
};


// -- IntroductionPage

class INSTALLER_EXPORT IntroductionPage : public Page
{
    Q_OBJECT

public:
    explicit IntroductionPage(Installer *installer);
    void setText(const QString &text);
    void setWidget(QWidget *w);

private:
    QLabel *m_msgLabel;
    QWidget *m_widget;
};


// -- LicenseAgreementPage

class INSTALLER_EXPORT LicenseAgreementPage : public Page
{
    Q_OBJECT

public:
    explicit LicenseAgreementPage(Installer *installer);

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

class INSTALLER_EXPORT ComponentSelectionPage : public Page
{
    Q_OBJECT

public:
    explicit ComponentSelectionPage(Installer *installer);
    ~ComponentSelectionPage();

    bool isComplete() const;

    Q_INVOKABLE void selectAll();
    Q_INVOKABLE void deselectAll();
    Q_INVOKABLE void selectDefault();
    Q_INVOKABLE void selectComponent(const QString& id);
    Q_INVOKABLE void deselectComponent(const QString& id);

protected:
    void entering();

private Q_SLOTS:
    void setModified(bool modified);

private:
    class Private;
    Private *d;
};


// -- TargetDirectoryPage

class INSTALLER_EXPORT TargetDirectoryPage : public Page
{
    Q_OBJECT

public:
    explicit TargetDirectoryPage(Installer *installer);
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

class INSTALLER_EXPORT StartMenuDirectoryPage : public Page
{
    Q_OBJECT

public:
    explicit StartMenuDirectoryPage(Installer *installer);

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

class INSTALLER_EXPORT ReadyForInstallationPage : public Page
{
    Q_OBJECT

public:
    explicit ReadyForInstallationPage(Installer *installer);

    bool isComplete() const;

protected:
    void entering();
    void leaving();

private:
    QLabel* const msgLabel;
};


// -- PerformInstallationPage

class INSTALLER_EXPORT PerformInstallationPage : public Page
{
    Q_OBJECT

public:
    explicit PerformInstallationPage(Installer *installer);
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
    QString m_commitBtnText;
    PerformInstallationForm *m_performInstallationForm;
};


// -- FinishedPage

class INSTALLER_EXPORT FinishedPage : public Page
{
    Q_OBJECT

public:
    explicit FinishedPage(Installer *installer);

public Q_SLOTS:
    void handleFinishClicked();

protected:
    void entering();
    void leaving();

private:
    QLabel *m_msgLabel;
    QCheckBox *m_runItCheckBox;
};


// -- RestartPage

class INSTALLER_EXPORT RestartPage : public Page
{
    Q_OBJECT

public:
    explicit RestartPage(Installer *installer);

    virtual int nextId() const;

protected:
    void entering();
    void leaving();

Q_SIGNALS:
    void restart();
};

} //namespace QInstaller

class ClickForwarder : public QObject
{
    Q_OBJECT
public:
    explicit ClickForwarder(QAbstractButton* button) : m_abstractButton(button) {}

protected:
    bool eventFilter(QObject *object, QEvent *event)
    {
        if (event->type() == QEvent::MouseButtonRelease) {
            m_abstractButton->click();
            return true;
        }
        // standard event processing
        return QObject::eventFilter(object, event);
    }
private:
    QAbstractButton* m_abstractButton;
};

#endif // QINSTALLER_GUI_H
