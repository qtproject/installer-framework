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
#include "packagemanagergui.h"

#include "component.h"
#include "componentmodel.h"
#include "errors.h"
#include "fileutils.h"
#include "messageboxhandler.h"
#include "packagemanagercore.h"
#include "progresscoordinator.h"
#include "performinstallationform.h"
#include "settings.h"
#include "utils.h"
#include "scriptengine.h"
#include "productkeycheck.h"

#include "kdsysinfo.h"

#include <QApplication>

#include <QtCore/QDir>
#include <QtCore/QPair>
#include <QtCore/QProcess>
#include <QtCore/QTimer>

#include <QCheckBox>
#include <QDesktopServices>
#include <QFileDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QTextBrowser>
#include <QTreeView>
#include <QVBoxLayout>
#include <QShowEvent>

#ifdef Q_OS_WIN
# include <qt_windows.h>
# include <QWinTaskbarButton>
# include <QWinTaskbarProgress>
#endif

using namespace KDUpdater;
using namespace QInstaller;


class DynamicInstallerPage : public PackageManagerPage
{
    Q_OBJECT
    Q_DISABLE_COPY(DynamicInstallerPage)

    Q_PROPERTY(bool final READ isFinal WRITE setFinal)
    Q_PROPERTY(bool commit READ isCommit WRITE setCommit)
    Q_PROPERTY(bool complete READ isComplete WRITE setComplete)

public:
    explicit DynamicInstallerPage(QWidget *widget, PackageManagerCore *core = 0)
        : PackageManagerPage(core)
        , m_widget(widget)
    {
        setObjectName(QLatin1String("Dynamic") + widget->objectName());
        setPixmap(QWizard::WatermarkPixmap, QPixmap());

        setColoredSubTitle(QLatin1String(" "));
        setColoredTitle(widget->windowTitle());
        m_widget->setProperty("complete", true);
        m_widget->setProperty("final", false);
        m_widget->setProperty("commit", false);
        widget->installEventFilter(this);

        setLayout(new QVBoxLayout);
        layout()->addWidget(widget);
        layout()->setContentsMargins(0, 0, 0, 0);
        layout()->addItem(new QSpacerItem(20, 20, QSizePolicy::Minimum, QSizePolicy::Expanding));

        addPageAndProperties(packageManagerCore()->controlScriptEngine());
        addPageAndProperties(packageManagerCore()->componentScriptEngine());
    }

    QWidget *widget() const
    {
        return m_widget;
    }

    bool isComplete() const
    {
        return m_widget->property("complete").toBool();
    }

    void setFinal(bool final) {
        if (isFinal() == final)
            return;
        m_widget->setProperty("final", final);
    }
    bool isFinal() const {
        return m_widget->property("final").toBool();
    }

    void setCommit(bool commit) {
        if (isCommit() == commit)
            return;
        m_widget->setProperty("commit", commit);
    }
    bool isCommit() const {
        return m_widget->property("commit").toBool();
    }

    void setComplete(bool complete) {
        if (isComplete() == complete)
            return;
        m_widget->setProperty("complete", complete);
    }

protected:
    bool eventFilter(QObject *obj, QEvent *event)
    {
        if (obj == m_widget) {
            switch(event->type()) {
            case QEvent::WindowTitleChange:
                setColoredTitle(m_widget->windowTitle());
                break;

            case QEvent::DynamicPropertyChange:
                emit completeChanged();
                if (m_widget->property("final").toBool() != isFinalPage())
                    setFinalPage(m_widget->property("final").toBool());
                if (m_widget->property("commit").toBool() != isCommitPage())
                    setCommitPage(m_widget->property("commit").toBool());
                break;

            default:
                break;
            }
        }
        return PackageManagerPage::eventFilter(obj, event);
    }

    void addPageAndProperties(ScriptEngine *engine)
    {
        engine->addToGlobalObject(this);
        engine->addToGlobalObject(widget());

        static const QStringList properties = QStringList() << QStringLiteral("final")
            << QStringLiteral("commit") << QStringLiteral("complete");
        foreach (const QString &property, properties) {
            engine->evaluate(QString::fromLatin1(
                "Object.defineProperty(%1, \"%2\", {"
                    "get : function() { return Dynamic%1.%2; },"
                    "set: function(val) { Dynamic%1.%2 = val; }"
                "});"
            ).arg(m_widget->objectName(), property));
        }
    }

private:
    QWidget *const m_widget;
};
Q_DECLARE_METATYPE(DynamicInstallerPage*)


// -- PackageManagerGui::Private

class PackageManagerGui::Private
{
public:
    Private()
        : m_currentId(-1)
        , m_modified(false)
        , m_autoSwitchPage(true)
        , m_showSettingsButton(false)
    {
        m_wizardButtonTypes.insert(QWizard::BackButton, QLatin1String("QWizard::BackButton"));
        m_wizardButtonTypes.insert(QWizard::NextButton, QLatin1String("QWizard::NextButton"));
        m_wizardButtonTypes.insert(QWizard::CommitButton, QLatin1String("QWizard::CommitButton"));
        m_wizardButtonTypes.insert(QWizard::FinishButton, QLatin1String("QWizard::FinishButton"));
        m_wizardButtonTypes.insert(QWizard::CancelButton, QLatin1String("QWizard::CancelButton"));
        m_wizardButtonTypes.insert(QWizard::HelpButton, QLatin1String("QWizard::HelpButton"));
        m_wizardButtonTypes.insert(QWizard::CustomButton1, QLatin1String("QWizard::CustomButton1"));
        m_wizardButtonTypes.insert(QWizard::CustomButton2, QLatin1String("QWizard::CustomButton2"));
        m_wizardButtonTypes.insert(QWizard::CustomButton3, QLatin1String("QWizard::CustomButton3"));
        m_wizardButtonTypes.insert(QWizard::Stretch, QLatin1String("QWizard::Stretch"));
    }

    QString buttonType(int wizardButton)
    {
        return m_wizardButtonTypes.value(static_cast<QWizard::WizardButton>(wizardButton),
            QLatin1String("unknown button"));
    }

    int m_currentId;
    bool m_modified;
    bool m_autoSwitchPage;
    bool m_showSettingsButton;
    QHash<int, QWizardPage*> m_defaultPages;
    QHash<int, QString> m_defaultButtonText;

    QJSValue m_controlScriptContext;
    QHash<QWizard::WizardButton, QString> m_wizardButtonTypes;
};


// -- PackageManagerGui

/*!
    \class QInstaller::PackageManagerGui
    \inmodule QtInstallerFramework
    \brief The PackageManagerGui class provides the core functionality for non-interactive
        installations.
*/

/*!
    \fn void PackageManagerGui::interrupted()
    \sa {gui::interrupted}{gui.interrupted}
*/

/*!
    \fn void PackageManagerGui::languageChanged()
    \sa {gui::languageChanged}{gui.languageChanged}
*/

/*!
    \fn void PackageManagerGui::finishButtonClicked()
    \sa {gui::finishButtonClicked}{gui.finishButtonClicked}
*/

/*!
    \fn void PackageManagerGui::gotRestarted()
    \sa {gui::gotRestarted}{gui.gotRestarted}
*/

/*!
    \fn void PackageManagerGui::settingsButtonClicked()
    \sa {gui::settingsButtonClicked}{gui.settingsButtonClicked}
*/

/*!
    \fn void PackageManagerGui::setValidatorForCustomPageRequested(QInstaller::Component *component,
        const QString &name,
        const QString &callbackName)

    Sets a validator for the custom page specified by \a name and
    \a callbackName requested by \a component.
*/

/*!
    \fn void PackageManagerGui::packageManagerCore() const

    Returns the package manager core.
*/

/*!
    Constructs a package manager UI with package manager specified by \a core
    and \a parent as parent.
*/
PackageManagerGui::PackageManagerGui(PackageManagerCore *core, QWidget *parent)
    : QWizard(parent)
    , d(new Private)
    , m_core(core)
{
    if (m_core->isInstaller())
        setWindowTitle(tr("%1 Setup").arg(m_core->value(scTitle)));
    else
        setWindowTitle(tr("Maintain %1").arg(m_core->value(scTitle)));
    setWindowFlags(windowFlags() &~ Qt::WindowContextHelpButtonHint);

#ifndef Q_OS_OSX
    setWindowIcon(QIcon(m_core->settings().installerWindowIcon()));
#else
    setPixmap(QWizard::BackgroundPixmap, m_core->settings().background());
#endif
#ifdef Q_OS_LINUX
    setWizardStyle(QWizard::ModernStyle);
    setSizeGripEnabled(true);
#endif

    if (!m_core->settings().wizardStyle().isEmpty())
        setWizardStyle(getStyle(m_core->settings().wizardStyle()));

    setOption(QWizard::NoBackButtonOnStartPage);
    setOption(QWizard::NoBackButtonOnLastPage);

    connect(this, SIGNAL(rejected()), m_core, SLOT(setCanceled()));
    connect(this, SIGNAL(interrupted()), m_core, SLOT(interrupt()));

    // both queued to show the finished page once everything is done
    connect(m_core, SIGNAL(installationFinished()), this, SLOT(showFinishedPage()),
        Qt::QueuedConnection);
    connect(m_core, SIGNAL(uninstallationFinished()), this, SLOT(showFinishedPage()),
        Qt::QueuedConnection);

    connect(this, SIGNAL(currentIdChanged(int)), this, SLOT(currentPageChanged(int)));
    connect(this, SIGNAL(currentIdChanged(int)), m_core, SIGNAL(currentPageChanged(int)));
    connect(button(QWizard::FinishButton), SIGNAL(clicked()), this, SIGNAL(finishButtonClicked()));
    connect(button(QWizard::FinishButton), SIGNAL(clicked()), m_core, SIGNAL(finishButtonClicked()));

    // make sure the QUiLoader's retranslateUi is executed first, then the script
    connect(this, SIGNAL(languageChanged()), m_core, SLOT(languageChanged()), Qt::QueuedConnection);
    connect(this, SIGNAL(languageChanged()), this, SLOT(onLanguageChanged()), Qt::QueuedConnection);

    connect(m_core,
        SIGNAL(wizardPageInsertionRequested(QWidget*,QInstaller::PackageManagerCore::WizardPage)),
        this, SLOT(wizardPageInsertionRequested(QWidget*,QInstaller::PackageManagerCore::WizardPage)));
    connect(m_core, SIGNAL(wizardPageRemovalRequested(QWidget*)), this,
        SLOT(wizardPageRemovalRequested(QWidget*)));
    connect(m_core,
        SIGNAL(wizardWidgetInsertionRequested(QWidget*,QInstaller::PackageManagerCore::WizardPage)),
        this, SLOT(wizardWidgetInsertionRequested(QWidget*,QInstaller::PackageManagerCore::WizardPage)));
    connect(m_core, SIGNAL(wizardWidgetRemovalRequested(QWidget*)), this,
        SLOT(wizardWidgetRemovalRequested(QWidget*)));
    connect(m_core, SIGNAL(wizardPageVisibilityChangeRequested(bool,int)), this,
        SLOT(wizardPageVisibilityChangeRequested(bool,int)), Qt::QueuedConnection);

    connect(m_core,
        SIGNAL(setValidatorForCustomPageRequested(QInstaller::Component*,QString,QString)), this,
        SLOT(setValidatorForCustomPageRequested(QInstaller::Component*,QString,QString)));

    connect(m_core, SIGNAL(setAutomatedPageSwitchEnabled(bool)), this,
        SLOT(setAutomatedPageSwitchEnabled(bool)));

    connect(this, SIGNAL(customButtonClicked(int)), this, SLOT(customButtonClicked(int)));

    for (int i = QWizard::BackButton; i < QWizard::CustomButton1; ++i)
        d->m_defaultButtonText.insert(i, buttonText(QWizard::WizardButton(i)));

    m_core->setGuiObject(this);
}

/*!
    Destructs a package manager UI.
*/
PackageManagerGui::~PackageManagerGui()
{
    m_core->setGuiObject(0);
    delete d;
}

/*!
    Returns the style of the package manager UI depending on \a name:

    \list
        \li \c Classic - Classic UI style for Windows 7 and earlier.
        \li \c Modern - Modern UI style for Windows 8.
        \li \c Mac - UI style for OS X.
        \li \c Aero - Aero Peek for Windows 7.
    \endlist
*/
QWizard::WizardStyle PackageManagerGui::getStyle(const QString &name)
{
    if (name == QLatin1String("Classic"))
        return QWizard::ClassicStyle;

    if (name == QLatin1String("Modern"))
        return QWizard::ModernStyle;

    if (name == QLatin1String("Mac"))
        return QWizard::MacStyle;

    if (name == QLatin1String("Aero"))
        return QWizard::AeroStyle;
    return QWizard::ModernStyle;
}

/*!
    Enables automatic page switching when \a request is \c true.
*/
void PackageManagerGui::setAutomatedPageSwitchEnabled(bool request)
{
    d->m_autoSwitchPage = request;
}

/*!
    Returns the default text for the button specified by \a wizardButton.

    \sa {gui::defaultButtonText}{gui.defaultButtonText}
*/
QString PackageManagerGui::defaultButtonText(int wizardButton) const
{
    return d->m_defaultButtonText.value(wizardButton);
}

/*
    Check if we need to "transform" the finish button into a cancel button, caused by the misuse of
    cancel as the finish button on the FinishedPage. This is only a problem if we run as updater or
    package manager, as then there will be two button shown on the last page with the cancel button
    renamed to "Finish".
*/
static bool swapFinishButton(PackageManagerCore *core, int currentId, int button)
{
    if (button != QWizard::FinishButton)
        return false;

    if (currentId != PackageManagerCore::InstallationFinished)
        return false;

    if (core->isInstaller() || core->isUninstaller())
        return false;

    return true;
}

/*!
    Clicks the button specified by \a wb after the delay specified by \a delay.

    \sa {gui::clickButton}{gui.clickButton}
*/
void PackageManagerGui::clickButton(int wb, int delay)
{
    // We need to to swap here, cause scripts expect to call this function with FinishButton on the
    // finish page.
    if (swapFinishButton(m_core, currentId(), wb))
        wb = QWizard::CancelButton;

    if (QAbstractButton *b = button(static_cast<QWizard::WizardButton>(wb)))
        QTimer::singleShot(delay, b, SLOT(click()));
    else
        qWarning() << "Button with type: " << d->buttonType(wb) << "not found!";
}

/*!
    Returns \c true if the button specified by \a wb is enabled. Returns \c false
    if a button of the specified type is not found.

    \sa {gui::isButtonEnabled}{gui.isButtonEnabled}
*/
bool PackageManagerGui::isButtonEnabled(int wb)
{
    // We need to to swap here, cause scripts expect to call this function with FinishButton on the
    // finish page.
    if (swapFinishButton(m_core, currentId(), wb))
            wb = QWizard::CancelButton;

    if (QAbstractButton *b = button(static_cast<QWizard::WizardButton>(wb)))
        return b->isEnabled();

    qWarning() << "Button with type: " << d->buttonType(wb) << "not found!";
    return false;
}

/*!
    Sets a validator for the custom page specified by \a name and
    \a callbackName requested by \a component.
*/
void PackageManagerGui::setValidatorForCustomPageRequested(Component *component,
    const QString &name, const QString &callbackName)
{
    component->setValidatorCallbackName(callbackName);

    const QString componentName = QLatin1String("Dynamic") + name;
    const QList<int> ids = pageIds();
    foreach (const int i, ids) {
        PackageManagerPage *const p = qobject_cast<PackageManagerPage*> (page(i));
        if (p && p->objectName() == componentName) {
            p->setValidatePageComponent(component);
            return;
        }
    }
}

/*!
    Loads the script specified by \a scriptPath to perform the installation non-interactively.
    Throws QInstaller::Error if the script is not readable or it cannot be
    parsed.
*/
void PackageManagerGui::loadControlScript(const QString &scriptPath)
{
    d->m_controlScriptContext = m_core->controlScriptEngine()->loadInContext(
        QLatin1String("Controller"), scriptPath);
    qDebug() << "Loaded control script" << scriptPath;
}

/*!
    Calls the control script method specified by \a methodName.
*/
void PackageManagerGui::callControlScriptMethod(const QString &methodName)
{
    if (d->m_controlScriptContext.isUndefined())
        return;
    try {
        const QJSValue returnValue = m_core->controlScriptEngine()->callScriptMethod(
            d->m_controlScriptContext, methodName);
        if (returnValue.isUndefined()) {
            qDebug() << "Control script callback" << methodName << "does not exist.";
            return;
        }
    } catch (const QInstaller::Error &e) {
        qCritical() << qPrintable(e.message());
    }
}

/*!
    Executes the control script on the page specified by \a pageId.
*/
void PackageManagerGui::executeControlScript(int pageId)
{
    if (PackageManagerPage *const p = qobject_cast<PackageManagerPage*> (page(pageId)))
        callControlScriptMethod(p->objectName() + QLatin1String("Callback"));
}

/*!
    Replaces the default button text with translated text when the application
    language changes.
*/
void PackageManagerGui::onLanguageChanged()
{
    d->m_defaultButtonText.clear();
    for (int i = QWizard::BackButton; i < QWizard::CustomButton1; ++i)
        d->m_defaultButtonText.insert(i, buttonText(QWizard::WizardButton(i)));
}

/*!
    \reimp
*/
bool PackageManagerGui::event(QEvent *event)
{
    switch(event->type()) {
    case QEvent::LanguageChange:
        emit languageChanged();
        break;
    default:
        break;
    }
    return QWizard::event(event);
}

/*!
    \reimp
*/
void PackageManagerGui::showEvent(QShowEvent *event)
{
    if (!event->spontaneous()) {
        foreach (int id, pageIds()) {
            const QString subTitle = page(id)->subTitle();
            if (subTitle.isEmpty()) {
                const QWizard::WizardStyle style = wizardStyle();
                if ((style == QWizard::ClassicStyle) || (style == QWizard::ModernStyle)) {
                    // otherwise the colors might screw up
                    page(id)->setSubTitle(QLatin1String(" "));
                }
            }
        }
        setMinimumSize(size());
        if (minimumWidth() < m_core->settings().wizardDefaultWidth())
            resize(m_core->settings().wizardDefaultWidth(), height());
        if (minimumHeight() < m_core->settings().wizardDefaultHeight())
            resize(width(), m_core->settings().wizardDefaultHeight());
    }
    QWizard::showEvent(event);
    QMetaObject::invokeMethod(this, "dependsOnLocalInstallerBinary", Qt::QueuedConnection);
}

/*!
    Requests the insertion of the page specified by \a widget at the position specified by \a page.
    If that position is already occupied by another page, the value is decremented until an empty
    slot is found.
*/
void PackageManagerGui::wizardPageInsertionRequested(QWidget *widget,
    QInstaller::PackageManagerCore::WizardPage page)
{
    // just in case it was already in there...
    wizardPageRemovalRequested(widget);

    int pageId = static_cast<int>(page) - 1;
    while (QWizard::page(pageId) != 0)
        --pageId;

    // add it
    setPage(pageId, new DynamicInstallerPage(widget, m_core));
}

/*!
    Requests the removal of the page specified by \a widget.
*/
void PackageManagerGui::wizardPageRemovalRequested(QWidget *widget)
{
    foreach (int pageId, pageIds()) {
        DynamicInstallerPage *const dynamicPage = qobject_cast<DynamicInstallerPage*>(page(pageId));
        if (dynamicPage == 0)
            continue;
        if (dynamicPage->widget() != widget)
            continue;
        removePage(pageId);
        d->m_defaultPages.remove(pageId);
        packageManagerCore()->controlScriptEngine()->removeFromGlobalObject(dynamicPage);
        packageManagerCore()->componentScriptEngine()->removeFromGlobalObject(dynamicPage);
    }
}

/*!
    Requests the insertion of \a widget on \a page.
*/
void PackageManagerGui::wizardWidgetInsertionRequested(QWidget *widget,
    QInstaller::PackageManagerCore::WizardPage page)
{
    Q_ASSERT(widget);
    if (QWizardPage *const p = QWizard::page(page)) {
        p->layout()->addWidget(widget);
        packageManagerCore()->controlScriptEngine()->addToGlobalObject(p);
        packageManagerCore()->componentScriptEngine()->addToGlobalObject(p);
    }
}

/*!
    Requests the removal of \a widget from installer pages.
*/
void PackageManagerGui::wizardWidgetRemovalRequested(QWidget *widget)
{
    Q_ASSERT(widget);
    widget->setParent(0);
    packageManagerCore()->controlScriptEngine()->removeFromGlobalObject(widget);
    packageManagerCore()->componentScriptEngine()->removeFromGlobalObject(widget);
}

/*!
    Requests changing the visibility of the page specified by \a p to
    \a visible.
*/
void PackageManagerGui::wizardPageVisibilityChangeRequested(bool visible, int p)
{
    if (visible && page(p) == 0) {
        setPage(p, d->m_defaultPages[p]);
    } else if (!visible && page(p) != 0) {
        d->m_defaultPages[p] = page(p);
        removePage(p);
    }
}

/*!
    Returns the page specified by \a id.

    \sa {gui::pageById}{gui.pageById}
*/
QWidget *PackageManagerGui::pageById(int id) const
{
    return page(id);
}

/*!
    Returns the page specified by the object name \a name from a UI file.

    \sa {gui::pageByObjectName}{gui.pageByObjectName}
*/
QWidget *PackageManagerGui::pageByObjectName(const QString &name) const
{
    const QList<int> ids = pageIds();
    foreach (const int i, ids) {
        PackageManagerPage *const p = qobject_cast<PackageManagerPage*> (page(i));
        if (p && p->objectName() == name)
            return p;
    }
    qWarning() << "No page found for object name" << name;
    return 0;
}

/*!
    \sa {gui::currentPageWidget}{gui.currentPageWidget}
*/
QWidget *PackageManagerGui::currentPageWidget() const
{
    return currentPage();
}

/*!
    For dynamic pages, returns the widget specified by \a name read from the UI
    file.

    \sa {gui::pageWidgetByObjectName}{gui.pageWidgetByObjectName}
*/
QWidget *PackageManagerGui::pageWidgetByObjectName(const QString &name) const
{
    QWidget *const widget = pageByObjectName(name);
    if (PackageManagerPage *const p = qobject_cast<PackageManagerPage*> (widget)) {
        // For dynamic pages, return the contained widget (as read from the UI file), not the
        // wrapper page
        if (DynamicInstallerPage *dp = qobject_cast<DynamicInstallerPage *>(p))
            return dp->widget();
        return p;
    }
    qWarning() << "No page found for object name" << name;
    return 0;
}

/*!
    \sa {gui::cancelButtonClicked}{gui.cancelButtonClicked}
*/
void PackageManagerGui::cancelButtonClicked()
{
    const int id = currentId();
    if (id == PackageManagerCore::Introduction || id == PackageManagerCore::InstallationFinished) {
        m_core->setNeedsHardRestart(false);
        QDialog::reject(); return;
    }

    QString question;
    bool interrupt = false;
    PackageManagerPage *const page = qobject_cast<PackageManagerPage*> (currentPage());
    if (page && page->isInterruptible()
        && m_core->status() != PackageManagerCore::Canceled
        && m_core->status() != PackageManagerCore::Failure) {
            interrupt = true;
            question = tr("Do you want to cancel the installation process?");
            if (m_core->isUninstaller())
                question = tr("Do you want to cancel the uninstallation process?");
    } else {
        question = tr("Do you want to quit the installer application?");
        if (m_core->isUninstaller())
            question = tr("Do you want to quit the uninstaller application?");
        if (m_core->isUpdater() || m_core->isPackageManager())
            question = tr("Do you want to quit the maintenance application?");
    }

    const QMessageBox::StandardButton button =
        MessageBoxHandler::question(MessageBoxHandler::currentBestSuitParent(),
        QLatin1String("cancelInstallation"), tr("Question"), question,
        QMessageBox::Yes | QMessageBox::No);

    if (button == QMessageBox::Yes) {
        if (interrupt)
            emit interrupted();
        else
            QDialog::reject();
    }
}

/*!
   \sa {gui::rejectWithoutPrompt}{gui.rejectWithoutPrompt}
*/
void PackageManagerGui::rejectWithoutPrompt()
{
    QDialog::reject();
}

/*!
    \reimp
*/
void PackageManagerGui::reject()
{
    cancelButtonClicked();
}

/*!
    \internal
*/
void PackageManagerGui::setModified(bool value)
{
    d->m_modified = value;
}

/*!
    \sa {gui::showFinishedPage}{gui.showFinishedPage}
*/
void PackageManagerGui::showFinishedPage()
{
    qDebug() << "SHOW FINISHED PAGE";
    if (d->m_autoSwitchPage)
        next();
    else
        qobject_cast<QPushButton*>(button(QWizard::CancelButton))->setEnabled(false);
}

/*!
    Shows the \uicontrol Settings button if \a show is \c true.

    \sa {gui::showSettingsButton}{gui.showSettingsButton}
*/
void PackageManagerGui::showSettingsButton(bool show)
{
    if (d->m_showSettingsButton == show)
        return;

    d->m_showSettingsButton = show;
    setOption(QWizard::HaveCustomButton1, show);
    setButtonText(QWizard::CustomButton1, tr("Settings"));

    updateButtonLayout();
}

/*!
    Forces an update of our own button layout. Needs to be called whenever a
    button option has been set.
*/
void PackageManagerGui::updateButtonLayout()
{
    QVector<QWizard::WizardButton> buttons(12, QWizard::NoButton);
    if (options() & QWizard::HaveHelpButton)
        buttons[(options() & QWizard::HelpButtonOnRight) ? 11 : 0] = QWizard::HelpButton;

    buttons[1] = QWizard::Stretch;
    if (options() & QWizard::HaveCustomButton1) {
        buttons[1] = QWizard::CustomButton1;
        buttons[2] = QWizard::Stretch;
    }

    if (options() & QWizard::HaveCustomButton2)
        buttons[3] = QWizard::CustomButton2;

    if (options() & QWizard::HaveCustomButton3)
        buttons[4] = QWizard::CustomButton3;

    if (!(options() & QWizard::NoCancelButton))
        buttons[(options() & QWizard::CancelButtonOnLeft) ? 5 : 10] = QWizard::CancelButton;

    buttons[6] = QWizard::BackButton;
    buttons[7] = QWizard::NextButton;
    buttons[8] = QWizard::CommitButton;
    buttons[9] = QWizard::FinishButton;

    setOption(QWizard::NoBackButtonOnLastPage, true);
    setOption(QWizard::NoBackButtonOnStartPage, true);

    setButtonLayout(buttons.toList());
}

/*!
    Enables the \uicontrol Settings button by setting \a enabled to \c true.

    \sa {gui::setSettingsButtonEnabled}{gui.setSettingsButtonEnabled}
*/
void PackageManagerGui::setSettingsButtonEnabled(bool enabled)
{
    if (QAbstractButton *btn = button(QWizard::CustomButton1))
        btn->setEnabled(enabled);
}

/*!
    Emits the settingsButtonClicked() signal when the custom button specified by \a which is
    clicked if \a which is the \uicontrol Settings button.
*/
void PackageManagerGui::customButtonClicked(int which)
{
    if (QWizard::WizardButton(which) == QWizard::CustomButton1 && d->m_showSettingsButton)
        emit settingsButtonClicked();
}

/*!
    Prevents installation from a network location by determining that a local
    installer binary must be used.
*/
void PackageManagerGui::dependsOnLocalInstallerBinary()
{
    if (m_core->settings().dependsOnLocalInstallerBinary() && !m_core->localInstallerBinaryUsed()) {
        MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(),
            QLatin1String("Installer_Needs_To_Be_Local_Error"), tr("Error"),
            tr("It is not possible to install from network location.\n"
               "Please copy the installer to a local drive"), QMessageBox::Ok);
        rejectWithoutPrompt();
    }
}

/*!
    Called when the current page changes to \a newId. Calls the leaving() method for the old page
    and the entering() method for the new one. Also, executes the control script associated with the
    new page by calling executeControlScript().

    Emits the left() and entered() signals.
*/
void PackageManagerGui::currentPageChanged(int newId)
{
    PackageManagerPage *oldPage = qobject_cast<PackageManagerPage *>(page(d->m_currentId));
    if (oldPage) {
        oldPage->leaving();
        emit oldPage->left();
    }

    d->m_currentId = newId;

    PackageManagerPage *newPage = qobject_cast<PackageManagerPage *>(page(d->m_currentId));
    if (newPage) {
        newPage->entering();
        emit newPage->entered();
    }

    executeControlScript(newId);
}

// -- PackageManagerPage

/*!
    \class QInstaller::PackageManagerPage
    \inmodule QtInstallerFramework
    \brief The PackageManagerPage class displays information about the product
    to install.
*/

/*!
    \fn PackageManagerPage::~PackageManagerPage()

    Destructs a package manager page.
*/

/*!
    \fn PackageManagerPage::gui() const

    Returns the wizard this page belongs to.
*/

/*!
    \fn PackageManagerPage::isInterruptible() const

    Returns \c true if the installation can be interrupted.
*/

/*!
    \fn PackageManagerPage::setValidatePageComponent(QInstaller::Component *component)

    Sets \a component as the component that validates the page.
*/

/*!
    \fn PackageManagerPage::settingsButtonRequested() const

    Returns \c true if the page requests the wizard to show the \uicontrol Settings button.
*/

/*!
    \fn PackageManagerPage::setSettingsButtonRequested(bool request)

    Determines that the page should request the \uicontrol Settings button if \a request is \c true.
*/

/*!
    \fn PackageManagerPage::entered()

    This signal is called when a page is entered.
*/

/*!
    \fn PackageManagerPage::left()

    This signal is called when a page is left.
*/

/*!
    \fn PackageManagerPage::entering()

    Called when end users enter the page and the PackageManagerGui:currentPageChanged()
    signal is triggered. Supports the QWizardPage::​initializePage() function to ensure
    that the page's fields are properly initialized based on fields from previous pages.
    Otherwise, \c initializePage() would only be called once if the installer has been
    set to QWizard::IndependentPages.
*/

/*!
    \fn PackageManagerPage::leaving()

    Called when end users leave the page and the PackageManagerGui:currentPageChanged()
    signal is triggered.
*/

/*!
    Constructs a package manager page with \a core as parent.
*/
PackageManagerPage::PackageManagerPage(PackageManagerCore *core)
    : m_complete(true)
    , m_needsSettingsButton(false)
    , m_core(core)
    , validatorComponent(0)
{
    if (!m_core->settings().titleColor().isEmpty()) {
        m_titleColor = m_core->settings().titleColor();
    } else {
        QColor defaultColor = style()->standardPalette().text().color();
        m_titleColor = defaultColor.name();
    }
    setPixmap(QWizard::WatermarkPixmap, watermarkPixmap());
    setPixmap(QWizard::BannerPixmap, bannerPixmap());
    setPixmap(QWizard::LogoPixmap, logoPixmap());
}

/*!
    Returns the package manager core.
*/
PackageManagerCore *PackageManagerPage::packageManagerCore() const
{
    return m_core;
}

/*!
    Returns the watermark pixmap specified in the \c <Watermark> element of the package information
    file.
*/
QPixmap PackageManagerPage::watermarkPixmap() const
{
    return QPixmap(m_core->value(QLatin1String("WatermarkPixmap")));
}

/*!
    Returns the banner pixmap specified in the \c <Banner> element of the package information file.
    Only used by the modern UI style.
*/
QPixmap PackageManagerPage::bannerPixmap() const
{
    return QPixmap(m_core->value(QLatin1String("BannerPixmap")));
}

/*!
    Returns the logo pixmap specified in the \c <Logo> element of the package information file.
*/
QPixmap PackageManagerPage::logoPixmap() const
{
    return QPixmap(m_core->value(QLatin1String("LogoPixmap")));
}

/*!
    Returns the product name of the application being installed.
*/
QString PackageManagerPage::productName() const
{
    return m_core->value(QLatin1String("ProductName"));
}

/*!
    Sets the font color of \a title. The title is specified in the \c <Title>
    element of the package information file. It is the name of the installer as
    displayed on the title bar.
*/
void PackageManagerPage::setColoredTitle(const QString &title)
{
    setTitle(QString::fromLatin1("<font color=\"%1\">%2</font>").arg(m_titleColor, title));
}

/*!
    Sets the font color of \a subTitle.
*/
void PackageManagerPage::setColoredSubTitle(const QString &subTitle)
{
    setSubTitle(QString::fromLatin1("<font color=\"%1\">%2</font>").arg(m_titleColor, subTitle));
}

/*!
    Returns \c true if the page is complete; otherwise, returns \c false.
*/
bool PackageManagerPage::isComplete() const
{
    return m_complete;
}

/*!
    Sets the package manager page to complete if \a complete is \c true. Emits
    the completeChanged() signal.
*/
void PackageManagerPage::setComplete(bool complete)
{
    m_complete = complete;
    if (QWizard *w = wizard()) {
        if (QAbstractButton *cancel = w->button(QWizard::CancelButton)) {
            if (cancel->hasFocus()) {
                if (QAbstractButton *next = w->button(QWizard::NextButton))
                    next->setFocus();
            }
        }
    }
    emit completeChanged();
}

/*!
    Sets the \a component that validates the page.
*/
void PackageManagerPage::setValidatePageComponent(Component *component)
{
    validatorComponent = component;
}

/*!
    Returns \c true if the end user has entered complete and valid information.
*/
bool PackageManagerPage::validatePage()
{
    if (validatorComponent)
        return validatorComponent->validatePage();
    return true;
}

/*!
    Inserts \a widget at the position specified by \a offset in relation to
    another widget specified by \a siblingName. The default position is directly
    behind the sibling.
*/
void PackageManagerPage::insertWidget(QWidget *widget, const QString &siblingName, int offset)
{
    QWidget *sibling = findChild<QWidget *>(siblingName);
    QWidget *parent = sibling ? sibling->parentWidget() : 0;
    QLayout *layout = parent ? parent->layout() : 0;
    QBoxLayout *blayout = qobject_cast<QBoxLayout *>(layout);

    if (blayout) {
        const int index = blayout->indexOf(sibling) + offset;
        blayout->insertWidget(index, widget);
    }
}

/*!
    Returns the widget specified by \a objectName.
*/
QWidget *PackageManagerPage::findWidget(const QString &objectName) const
{
    return findChild<QWidget*> (objectName);
}

/*!
    Determines which page should be shown next depending on whether the
    application is being installed, updated, or uninstalled.

    The license check page is shown only if a component that provides a license
    is selected for installation. It is hidden during uninstallation and update.
*/
int PackageManagerPage::nextId() const
{
    const int next = QWizardPage::nextId(); // the page to show next
    if (next == PackageManagerCore::LicenseCheck) {
        // calculate the page after the license page
        const int nextNextId = gui()->pageIds().value(gui()->pageIds().indexOf(next) + 1, -1);
        const PackageManagerCore *const core = packageManagerCore();
        if (core->isUninstaller())
            return nextNextId;  // forcibly hide the license page if we run as uninstaller

        core->calculateComponentsToInstall();
        foreach (Component* component, core->orderedComponentsToInstall()) {
            if ((core->isPackageManager() || core->isUpdater()) && component->isInstalled())
                continue; // package manager or updater, hide as long as the component is installed

            // The component is about to be installed and provides a license, so the page needs to
            // be shown.
            if (!component->licenses().isEmpty())
                return next;
        }
        return nextNextId;  // no component with a license or all components with license installed
    }
    return next;    // default, show the next page
}

// -- IntroductionPage

/*!
    \class QInstaller::IntroductionPage
    \inmodule QtInstallerFramework
    \brief The IntroductionPage class displays information about the product to
    install.
*/

/*!
    \fn IntroductionPage::packageManagerCoreTypeChanged()

    This signal is emitted when the package manager core type changes.
*/

/*!
    Constructs an introduction page with \a core as parent.
*/
IntroductionPage::IntroductionPage(PackageManagerCore *core)
    : PackageManagerPage(core)
    , m_updatesFetched(false)
    , m_allPackagesFetched(false)
    , m_label(0)
    , m_msgLabel(0)
    , m_errorLabel(0)
    , m_progressBar(0)
    , m_packageManager(0)
    , m_updateComponents(0)
    , m_removeAllComponents(0)
{
    setObjectName(QLatin1String("IntroductionPage"));
    setColoredTitle(tr("Setup - %1").arg(productName()));

    QVBoxLayout *layout = new QVBoxLayout(this);
    setLayout(layout);

    m_msgLabel = new QLabel(this);
    m_msgLabel->setWordWrap(true);
    m_msgLabel->setObjectName(QLatin1String("MessageLabel"));
    m_msgLabel->setText(tr("Welcome to the %1 Setup Wizard.").arg(productName()));

    QWidget *widget = new QWidget(this);
    QVBoxLayout *boxLayout = new QVBoxLayout(widget);

    m_packageManager = new QRadioButton(tr("Add or remove components"), this);
    m_packageManager->setObjectName(QLatin1String("PackageManagerRadioButton"));
    boxLayout->addWidget(m_packageManager);
    m_packageManager->setChecked(core->isPackageManager());
    connect(m_packageManager, SIGNAL(toggled(bool)), this, SLOT(setPackageManager(bool)));

    m_updateComponents = new QRadioButton(tr("Update components"), this);
    m_updateComponents->setObjectName(QLatin1String("UpdaterRadioButton"));
    boxLayout->addWidget(m_updateComponents);
    m_updateComponents->setChecked(core->isUpdater());
    connect(m_updateComponents, SIGNAL(toggled(bool)), this, SLOT(setUpdater(bool)));

    m_removeAllComponents = new QRadioButton(tr("Remove all components"), this);
    m_removeAllComponents->setObjectName(QLatin1String("UninstallerRadioButton"));
    boxLayout->addWidget(m_removeAllComponents);
    m_removeAllComponents->setChecked(core->isUninstaller());
    connect(m_removeAllComponents, SIGNAL(toggled(bool)), this, SLOT(setUninstaller(bool)));
    connect(m_removeAllComponents, SIGNAL(toggled(bool)), core, SLOT(setCompleteUninstallation(bool)));

    boxLayout->addItem(new QSpacerItem(1, 1, QSizePolicy::Minimum, QSizePolicy::Expanding));

    m_label = new QLabel(this);
    m_label->setWordWrap(true);
    m_label->setObjectName(QLatin1String("InformationLabel"));
    m_label->setText(tr("Retrieving information from remote installation sources..."));
    boxLayout->addWidget(m_label);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 0);
    boxLayout->addWidget(m_progressBar);
    m_progressBar->setObjectName(QLatin1String("InformationProgressBar"));

    boxLayout->addItem(new QSpacerItem(1, 1, QSizePolicy::Minimum, QSizePolicy::Expanding));

    m_errorLabel = new QLabel(this);
    m_errorLabel->setWordWrap(true);
    boxLayout->addWidget(m_errorLabel);
    m_errorLabel->setObjectName(QLatin1String("ErrorLabel"));

    layout->addWidget(m_msgLabel);
    layout->addWidget(widget);
    layout->addItem(new QSpacerItem(20, 20, QSizePolicy::Minimum, QSizePolicy::Expanding));

    core->setCompleteUninstallation(core->isUninstaller());

    connect(core, SIGNAL(metaJobProgress(int)), this, SLOT(onProgressChanged(int)));
    connect(core, SIGNAL(metaJobInfoMessage(QString)), this, SLOT(setMessage(QString)));
    connect(core, SIGNAL(coreNetworkSettingsChanged()), this, SLOT(onCoreNetworkSettingsChanged()));

    m_updateComponents->setEnabled(ProductKeyCheck::instance()->hasValidKey());

#ifdef Q_OS_WIN
    if (QSysInfo::windowsVersion() >= QSysInfo::WV_WINDOWS7) {
        m_taskButton = new QWinTaskbarButton(this);
        connect(core, SIGNAL(metaJobProgress(int)), m_taskButton->progress(), SLOT(setValue(int)));
    } else {
        m_taskButton = 0;
    }
#endif
}

/*!
    Determines which page should be shown next depending on whether the
    application is being installed, updated, or uninstalled.
*/
int IntroductionPage::nextId() const
{
    if (packageManagerCore()->isUninstaller())
        return PackageManagerCore::ReadyForInstallation;

    if (packageManagerCore()->isUpdater() || packageManagerCore()->isPackageManager())
        return PackageManagerCore::ComponentSelection;

    return PackageManagerPage::nextId();
}

/*!
    For an uninstaller, always returns \c true. For the package manager and updater, at least
    one valid repository is required. For the online installer, package manager, and updater, valid
    meta data has to be fetched successfully to return \c true.
*/
bool IntroductionPage::validatePage()
{
    PackageManagerCore *core = packageManagerCore();
    if (core->isUninstaller())
        return true;

    setComplete(false);
    if (!validRepositoriesAvailable()) {
        setErrorMessage(QLatin1String("<font color=\"red\">") + tr("At least one valid and enabled "
            "repository required for this action to succeed.") + QLatin1String("</font>"));
        return isComplete();
    }

    gui()->setSettingsButtonEnabled(false);
    const bool maintenance = core->isUpdater() || core->isPackageManager();
    if (maintenance) {
        showAll();
        setMaintenanceToolsEnabled(false);
    } else {
        showMetaInfoUpdate();
    }

#ifdef Q_OS_WIN
    if (m_taskButton) {
        if (!m_taskButton->window()) {
            if (QWidget *widget = QApplication::activeWindow())
                m_taskButton->setWindow(widget->windowHandle());
        }

        m_taskButton->progress()->reset();
        m_taskButton->progress()->resume();
        m_taskButton->progress()->setVisible(true);
    }
#endif

    // fetch updater packages
    if (core->isUpdater()) {
        if (!m_updatesFetched) {
            m_updatesFetched = core->fetchRemotePackagesTree();
            if (!m_updatesFetched)
                setErrorMessage(core->error());
        }

        if (m_updatesFetched) {
            if (core->components(QInstaller::PackageManagerCore::ComponentType::Root).count() <= 0)
                setErrorMessage(QString::fromLatin1("<b>%1</b>").arg(tr("No updates available.")));
            else
                setComplete(true);
        }
    }

    // fetch common packages
    if (core->isInstaller() || core->isPackageManager()) {
        bool localPackagesTreeFetched = false;
        if (!m_allPackagesFetched) {
            // first try to fetch the server side packages tree
            m_allPackagesFetched = core->fetchRemotePackagesTree();
            if (!m_allPackagesFetched) {
                QString error = core->error();
                if (core->isPackageManager() && core->status() != PackageManagerCore::ForceUpdate) {
                    // if that fails and we're in maintenance mode, try to fetch local installed tree
                    localPackagesTreeFetched = core->fetchLocalPackagesTree();
                    if (localPackagesTreeFetched) {
                        // if that succeeded, adjust error message
                        error = QLatin1String("<font color=\"red\">") + error + tr(" Only local package "
                            "management available.") + QLatin1String("</font>");
                    }
                }
                setErrorMessage(error);
            }
        }

        if (m_allPackagesFetched || localPackagesTreeFetched)
            setComplete(true);
    }

    if (maintenance) {
        showMaintenanceTools();
        setMaintenanceToolsEnabled(true);
    } else {
        hideAll();
    }
    gui()->setSettingsButtonEnabled(true);

#ifdef Q_OS_WIN
    if (m_taskButton)
        m_taskButton->progress()->setVisible(!isComplete());
#endif
    return isComplete();
}

/*!
    Shows all widgets on the page.
*/
void IntroductionPage::showAll()
{
    showWidgets(true);
}

/*!
    Hides all widgets on the page.
*/
void IntroductionPage::hideAll()
{
    showWidgets(false);
}

/*!
    Hides the widgets on the page except a text label and progress bar.
*/
void IntroductionPage::showMetaInfoUpdate()
{
    showWidgets(false);
    m_label->setVisible(true);
    m_progressBar->setVisible(true);
}

/*!
    Shows the options to install, add, and unistall components on the page.
*/
void IntroductionPage::showMaintenanceTools()
{
    showWidgets(true);
    m_label->setVisible(false);
    m_progressBar->setVisible(false);
}

/*!
    Sets \a enable to \c true to enable the options to install, add, and
    uninstall components on the page.
*/
void IntroductionPage::setMaintenanceToolsEnabled(bool enable)
{
    m_packageManager->setEnabled(enable);
    m_updateComponents->setEnabled(enable && ProductKeyCheck::instance()->hasValidKey());
    m_removeAllComponents->setEnabled(enable);
}

// -- public slots

/*!
    Displays the message \a msg on the page.
*/
void IntroductionPage::setMessage(const QString &msg)
{
    m_label->setText(msg);
}

/*!
    Updates the value of \a progress on the progress bar.
*/
void IntroductionPage::onProgressChanged(int progress)
{
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(progress);
}

/*!
    Displays the error message \a error on the page.
*/
void IntroductionPage::setErrorMessage(const QString &error)
{
    QPalette palette;
    const PackageManagerCore::Status s = packageManagerCore()->status();
    if (s == PackageManagerCore::Failure || s == PackageManagerCore::Failure) {
        palette.setColor(QPalette::WindowText, Qt::red);
    } else {
        palette.setColor(QPalette::WindowText, palette.color(QPalette::WindowText));
    }

    m_errorLabel->setText(error);
    m_errorLabel->setPalette(palette);

#ifdef Q_OS_WIN
    if (m_taskButton) {
        m_taskButton->progress()->stop();
        m_taskButton->progress()->setValue(100);
    }
#endif
}

/*!
    Returns \c true if at least one valid and enabled repository is available.
*/
bool IntroductionPage::validRepositoriesAvailable() const
{
    const PackageManagerCore *const core = packageManagerCore();
    bool valid = (core->isInstaller() && core->isOfflineOnly()) || core->isUninstaller();

    if (!valid) {
        foreach (const Repository &repo, core->settings().repositories()) {
            if (repo.isEnabled() && repo.isValid()) {
                valid = true;
                break;
            }
        }
    }
    return valid;
}

// -- private slots

void IntroductionPage::setUpdater(bool value)
{
    if (value) {
        entering();
        gui()->showSettingsButton(true);
        packageManagerCore()->setUpdater();
        emit packageManagerCoreTypeChanged();
    }
}

void IntroductionPage::setUninstaller(bool value)
{
    if (value) {
        entering();
        gui()->showSettingsButton(false);
        packageManagerCore()->setUninstaller();
        emit packageManagerCoreTypeChanged();
    }
}

void IntroductionPage::setPackageManager(bool value)
{
    if (value) {
        entering();
        gui()->showSettingsButton(true);
        packageManagerCore()->setPackageManager();
        emit packageManagerCoreTypeChanged();
    }
}

/*!
    Resets the internal page state, so that on clicking \uicontrol Next the metadata needs to be
    fetched again.
*/
void IntroductionPage::onCoreNetworkSettingsChanged()
{
    m_updatesFetched = false;
    m_allPackagesFetched = false;
}

// -- private

/*!
    Initializes the page's fields.
*/
void IntroductionPage::entering()
{
    setComplete(true);
    showWidgets(false);
    setMessage(QString());
    setErrorMessage(QString());
    setButtonText(QWizard::CancelButton, tr("Quit"));

    m_progressBar->setValue(0);
    m_progressBar->setRange(0, 0);
    PackageManagerCore *core = packageManagerCore();
    if (core->isUninstaller() || core->isUpdater() || core->isPackageManager()) {
        showMaintenanceTools();
        setMaintenanceToolsEnabled(true);
    }
    setSettingsButtonRequested((!core->isOfflineOnly()) && (!core->isUninstaller()));
}

/*!
    Called when end users leave the page and the PackageManagerGui:currentPageChanged()
    signal is triggered.
*/
void IntroductionPage::leaving()
{
    m_progressBar->setValue(0);
    m_progressBar->setRange(0, 0);
    setButtonText(QWizard::CancelButton, gui()->defaultButtonText(QWizard::CancelButton));
}

/*!
    Displays widgets on the page.
*/
void IntroductionPage::showWidgets(bool show)
{
    m_label->setVisible(show);
    m_progressBar->setVisible(show);
    m_packageManager->setVisible(show);
    m_updateComponents->setVisible(show);
    m_removeAllComponents->setVisible(show);
}

/*!
    Displays the text \a text on the page.
*/
void IntroductionPage::setText(const QString &text)
{
    m_msgLabel->setText(text);
}


// -- LicenseAgreementPage::ClickForwarder

class LicenseAgreementPage::ClickForwarder : public QObject
{
    Q_OBJECT

public:
    explicit ClickForwarder(QAbstractButton *button)
        : QObject(button)
        , m_abstractButton(button) {}

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
    QAbstractButton *m_abstractButton;
};


// -- LicenseAgreementPage

/*!
    \class QInstaller::LicenseAgreementPage
    \inmodule QtInstallerFramework
    \brief The LicenseAgreementPage presents a license agreement to the end
    users for acceptance.

    The license check page is displayed if you specify a license file in the
    package information file and copy the file to the meta directory. End users must
    accept the terms of the license agreement for the installation to continue.
*/

/*!
    Constructs a license check page with \a core as parent.
*/
LicenseAgreementPage::LicenseAgreementPage(PackageManagerCore *core)
    : PackageManagerPage(core)
{
    setPixmap(QWizard::WatermarkPixmap, QPixmap());
    setObjectName(QLatin1String("LicenseAgreementPage"));
    setColoredTitle(tr("License Agreement"));

    m_licenseListWidget = new QListWidget(this);
    m_licenseListWidget->setObjectName(QLatin1String("LicenseListWidget"));
    m_licenseListWidget->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Expanding);
    connect(m_licenseListWidget, SIGNAL(currentItemChanged(QListWidgetItem*,QListWidgetItem*)),
        this, SLOT(currentItemChanged(QListWidgetItem*)));

    m_textBrowser = new QTextBrowser(this);
    m_textBrowser->setReadOnly(true);
    m_textBrowser->setOpenLinks(false);
    m_textBrowser->setOpenExternalLinks(true);
    m_textBrowser->setObjectName(QLatin1String("LicenseTextBrowser"));
    m_textBrowser->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
    connect(m_textBrowser, SIGNAL(anchorClicked(QUrl)), this, SLOT(openLicenseUrl(QUrl)));

    QHBoxLayout *licenseBoxLayout = new QHBoxLayout();
    licenseBoxLayout->addWidget(m_licenseListWidget);
    licenseBoxLayout->addWidget(m_textBrowser);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addLayout(licenseBoxLayout);

    m_acceptRadioButton = new QRadioButton(this);
    m_acceptRadioButton->setShortcut(QKeySequence(tr("Alt+A", "agree license")));
    m_acceptRadioButton->setObjectName(QLatin1String("AcceptLicenseRadioButton"));
    ClickForwarder *acceptClickForwarder = new ClickForwarder(m_acceptRadioButton);

    m_acceptLabel = new QLabel;
    m_acceptLabel->setWordWrap(true);
    m_acceptLabel->installEventFilter(acceptClickForwarder);
    m_acceptLabel->setObjectName(QLatin1String("AcceptLicenseLabel"));
    m_acceptLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);

    m_rejectRadioButton = new QRadioButton(this);
    ClickForwarder *rejectClickForwarder = new ClickForwarder(m_rejectRadioButton);
    m_rejectRadioButton->setObjectName(QString::fromUtf8("RejectLicenseRadioButton"));
    m_rejectRadioButton->setShortcut(QKeySequence(tr("Alt+D", "do not agree license")));

    m_rejectLabel = new QLabel;
    m_rejectLabel->setWordWrap(true);
    m_rejectLabel->installEventFilter(rejectClickForwarder);
    m_rejectLabel->setObjectName(QLatin1String("RejectLicenseLabel"));
    m_rejectLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);

    QGridLayout *gridLayout = new QGridLayout;
    gridLayout->setColumnStretch(1, 1);
    gridLayout->addWidget(m_acceptRadioButton, 0, 0);
    gridLayout->addWidget(m_acceptLabel, 0, 1);
    gridLayout->addWidget(m_rejectRadioButton, 1, 0);
    gridLayout->addWidget(m_rejectLabel, 1, 1);
    layout->addLayout(gridLayout);

    connect(m_acceptRadioButton, SIGNAL(toggled(bool)), this, SIGNAL(completeChanged()));
    connect(m_rejectRadioButton, SIGNAL(toggled(bool)), this, SIGNAL(completeChanged()));

    m_rejectRadioButton->setChecked(true);
}

/*!
    Initializes the page's fields based on values from fields on previous
    pages.
*/
void LicenseAgreementPage::entering()
{
    m_licenseListWidget->clear();
    m_textBrowser->setHtml(QString());
    m_licenseListWidget->setVisible(false);

    packageManagerCore()->calculateComponentsToInstall();
    foreach (QInstaller::Component *component, packageManagerCore()->orderedComponentsToInstall())
        addLicenseItem(component->licenses());

    const int licenseCount = m_licenseListWidget->count();
    if (licenseCount > 0) {
        m_licenseListWidget->setVisible(licenseCount > 1);
        m_licenseListWidget->setCurrentItem(m_licenseListWidget->item(0));
    }

    updateUi();
}

/*!
    Returns \c true if the accept license radio button is checked; otherwise,
    returns \c false.
*/
bool LicenseAgreementPage::isComplete() const
{
    return m_acceptRadioButton->isChecked();
}

void LicenseAgreementPage::openLicenseUrl(const QUrl &url)
{
    QDesktopServices::openUrl(url);
}

void LicenseAgreementPage::currentItemChanged(QListWidgetItem *current)
{
    if (current)
        m_textBrowser->setText(current->data(Qt::UserRole).toString());
}

void LicenseAgreementPage::addLicenseItem(const QHash<QString, QPair<QString, QString> > &hash)
{
    for (QHash<QString, QPair<QString, QString> >::const_iterator it = hash.begin();
        it != hash.end(); ++it) {
            QListWidgetItem *item = new QListWidgetItem(it.key(), m_licenseListWidget);
            item->setData(Qt::UserRole, it.value().second);
    }
}

void LicenseAgreementPage::updateUi()
{
    QString subTitleText;
    QString acceptButtonText;
    QString rejectButtonText;
    if (m_licenseListWidget->count() == 1) {
        subTitleText = tr("Please read the following license agreement. You must accept the terms "
                          "contained in this agreement before continuing with the installation.");
        acceptButtonText = tr("I accept the license.");
        rejectButtonText = tr("I do not accept the license.");
    } else {
        subTitleText = tr("Please read the following license agreements. You must accept the terms "
                          "contained in these agreements before continuing with the installation.");
        acceptButtonText = tr("I accept the licenses.");
        rejectButtonText = tr("I do not accept the licenses.");
    }

    setColoredSubTitle(subTitleText);

    m_acceptLabel->setText(acceptButtonText);
    m_rejectLabel->setText(rejectButtonText);

}


// -- ComponentSelectionPage::Private

class ComponentSelectionPage::Private : public QObject
{
    Q_OBJECT

public:
    Private(ComponentSelectionPage *qq, PackageManagerCore *core)
        : q(qq)
        , m_core(core)
        , m_treeView(new QTreeView(q))
        , m_allModel(m_core->defaultComponentModel())
        , m_updaterModel(m_core->updaterComponentModel())
        , m_currentModel(m_allModel)
    {
        m_treeView->setObjectName(QLatin1String("ComponentsTreeView"));

        connect(m_allModel, SIGNAL(checkStateChanged(QInstaller::ComponentModel::ModelState)), this,
            SLOT(onModelStateChanged(QInstaller::ComponentModel::ModelState)));
        connect(m_updaterModel, SIGNAL(checkStateChanged(QInstaller::ComponentModel::ModelState)),
            this, SLOT(onModelStateChanged(QInstaller::ComponentModel::ModelState)));

        QHBoxLayout *hlayout = new QHBoxLayout;
        hlayout->addWidget(m_treeView, 3);

        m_descriptionLabel = new QLabel(q);
        m_descriptionLabel->setWordWrap(true);
        m_descriptionLabel->setObjectName(QLatin1String("ComponentDescriptionLabel"));

        QVBoxLayout *vlayout = new QVBoxLayout;
        vlayout->addWidget(m_descriptionLabel);

        m_sizeLabel = new QLabel(q);
        m_sizeLabel->setWordWrap(true);
        vlayout->addWidget(m_sizeLabel);
        m_sizeLabel->setObjectName(QLatin1String("ComponentSizeLabel"));

        vlayout->addSpacerItem(new QSpacerItem(1, 1, QSizePolicy::MinimumExpanding,
            QSizePolicy::MinimumExpanding));
        hlayout->addLayout(vlayout, 2);

        QVBoxLayout *layout = new QVBoxLayout(q);
        layout->addLayout(hlayout, 1);

        m_checkDefault = new QPushButton;
        connect(m_checkDefault, SIGNAL(clicked()), this, SLOT(selectDefault()));
        if (m_core->isInstaller()) {
            m_checkDefault->setObjectName(QLatin1String("SelectDefaultComponentsButton"));
            m_checkDefault->setShortcut(QKeySequence(ComponentSelectionPage::tr("Alt+A",
                "select default components")));
            m_checkDefault->setText(ComponentSelectionPage::tr("Def&ault"));
        } else {
            m_checkDefault->setEnabled(false);
            m_checkDefault->setObjectName(QLatin1String("ResetComponentsButton"));
            m_checkDefault->setShortcut(QKeySequence(ComponentSelectionPage::tr("Alt+R",
                "reset to already installed components")));
            m_checkDefault->setText(ComponentSelectionPage::tr("&Reset"));
        }
        hlayout = new QHBoxLayout;
        hlayout->addWidget(m_checkDefault);

        m_checkAll = new QPushButton;
        hlayout->addWidget(m_checkAll);
        connect(m_checkAll, SIGNAL(clicked()), this, SLOT(selectAll()));
        m_checkAll->setObjectName(QLatin1String("SelectAllComponentsButton"));
        m_checkAll->setShortcut(QKeySequence(ComponentSelectionPage::tr("Alt+S",
            "select all components")));
        m_checkAll->setText(ComponentSelectionPage::tr("&Select All"));

        m_uncheckAll = new QPushButton;
        hlayout->addWidget(m_uncheckAll);
        connect(m_uncheckAll, SIGNAL(clicked()), this, SLOT(deselectAll()));
        m_uncheckAll->setObjectName(QLatin1String("DeselectAllComponentsButton"));
        m_uncheckAll->setShortcut(QKeySequence(ComponentSelectionPage::tr("Alt+D",
            "deselect all components")));
        m_uncheckAll->setText(ComponentSelectionPage::tr("&Deselect All"));

        hlayout->addSpacerItem(new QSpacerItem(1, 1, QSizePolicy::MinimumExpanding,
            QSizePolicy::MinimumExpanding));
        layout->addLayout(hlayout);
    }

    void updateTreeView()
    {
        m_checkDefault->setVisible(m_core->isInstaller() || m_core->isPackageManager());
        if (m_treeView->selectionModel()) {
            disconnect(m_treeView->selectionModel(), SIGNAL(currentChanged(QModelIndex,QModelIndex)),
                this, SLOT(currentSelectedChanged(QModelIndex)));
        }

        m_currentModel = m_core->isUpdater() ? m_updaterModel : m_allModel;
        m_treeView->setModel(m_currentModel);
        m_treeView->setExpanded(m_currentModel->index(0, 0), true);

        const bool installActionColumnVisible = false;
        if (!installActionColumnVisible)
            m_treeView->hideColumn(ComponentModelHelper::ActionColumn);

        if (m_core->isInstaller()) {
            m_treeView->setHeaderHidden(true);
            for (int i = ComponentModelHelper::InstalledVersionColumn; i < m_currentModel->columnCount(); ++i)
                m_treeView->hideColumn(i);

            if (installActionColumnVisible) {
                m_treeView->header()->setStretchLastSection(false);
                m_treeView->header()->setSectionResizeMode(
                            ComponentModelHelper::NameColumn, QHeaderView::Stretch);
                m_treeView->header()->setSectionResizeMode(
                            ComponentModelHelper::ActionColumn, QHeaderView::ResizeToContents);
            }
        } else {
            m_treeView->header()->setStretchLastSection(true);
            if (installActionColumnVisible) {
                m_treeView->header()->setSectionResizeMode(
                            ComponentModelHelper::NameColumn, QHeaderView::Interactive);
                m_treeView->header()->setSectionResizeMode(
                            ComponentModelHelper::ActionColumn, QHeaderView::Interactive);
            }
            for (int i = 0; i < m_currentModel->columnCount(); ++i)
                m_treeView->resizeColumnToContents(i);
        }

        bool hasChildren = false;
        const int rowCount = m_currentModel->rowCount();
        for (int row = 0; row < rowCount && !hasChildren; ++row)
            hasChildren = m_currentModel->hasChildren(m_currentModel->index(row, 0));
        m_treeView->setRootIsDecorated(hasChildren);

        connect(m_treeView->selectionModel(), SIGNAL(currentChanged(QModelIndex,QModelIndex)),
            this, SLOT(currentSelectedChanged(QModelIndex)));

        m_treeView->setCurrentIndex(m_currentModel->index(0, 0));
    }

public slots:
    void currentSelectedChanged(const QModelIndex &current)
    {
        if (!current.isValid())
            return;

        m_sizeLabel->setText(QString());
        m_descriptionLabel->setText(m_currentModel->data(m_currentModel->index(current.row(),
            ComponentModelHelper::NameColumn, current.parent()), Qt::ToolTipRole).toString());

        Component *component = m_currentModel->componentFromIndex(current);
        if ((m_core->isUninstaller()) || (!component))
            return;

        if (component->isSelected() && (component->value(scUncompressedSizeSum).toLongLong() > 0)) {
            m_sizeLabel->setText(ComponentSelectionPage::tr("This component "
                "will occupy approximately %1 on your hard disk drive.")
                .arg(humanReadableSize(component->value(scUncompressedSizeSum).toLongLong())));
        }
    }

    void selectAll()
    {
        m_currentModel->setCheckedState(ComponentModel::AllChecked);
    }

    void deselectAll()
    {
        m_currentModel->setCheckedState(ComponentModel::AllUnchecked);
    }

    void selectDefault()
    {
        m_currentModel->setCheckedState(ComponentModel::DefaultChecked);
    }

    void onModelStateChanged(QInstaller::ComponentModel::ModelState state)
    {
        q->setModified(state.testFlag(ComponentModel::DefaultChecked) == false);
        // If all components in the checked list are only checkable when run without forced
        // installation, set ComponentModel::AllUnchecked as well, as we cannot uncheck anything.
        // Helps to keep the UI correct.
        if ((!m_core->noForceInstallation())
            && (m_currentModel->checked() == m_currentModel->uncheckable())) {
                state |= ComponentModel::AllUnchecked;
        }
        // enable the button if the corresponding flag is not set
        m_checkAll->setEnabled(state.testFlag(ComponentModel::AllChecked) == false);
        m_uncheckAll->setEnabled(state.testFlag(ComponentModel::AllUnchecked) == false);
        m_checkDefault->setEnabled(state.testFlag(ComponentModel::DefaultChecked) == false);

        // update the current selected node (important to reflect possible sub-node changes)
        if (m_treeView->selectionModel())
            currentSelectedChanged(m_treeView->selectionModel()->currentIndex());
    }

public:
    ComponentSelectionPage *q;
    PackageManagerCore *m_core;
    QTreeView *m_treeView;
    ComponentModel *m_allModel;
    ComponentModel *m_updaterModel;
    ComponentModel *m_currentModel;
    QLabel *m_sizeLabel;
    QLabel *m_descriptionLabel;
    QPushButton *m_checkAll;
    QPushButton *m_uncheckAll;
    QPushButton *m_checkDefault;
};


// -- ComponentSelectionPage

/*!
    \class QInstaller::ComponentSelectionPage
    \inmodule QtInstallerFramework
    \brief The ComponentSelectionPage class changes the checked state of
    components.
*/

/*!
    Constructs a component selection page with \a core as parent.
*/
ComponentSelectionPage::ComponentSelectionPage(PackageManagerCore *core)
    : PackageManagerPage(core)
    , d(new Private(this, core))
{
    setPixmap(QWizard::WatermarkPixmap, QPixmap());
    setObjectName(QLatin1String("ComponentSelectionPage"));
    setColoredTitle(tr("Select Components"));
}

/*!
    Destructs a component selection page.
*/
ComponentSelectionPage::~ComponentSelectionPage()
{
    delete d;
}

/*!
    Initializes the page's fields based on values from fields on previous
    pages. The text to display depends on whether the page is being used in an
    installer, updater, or uninstaller.
*/
void ComponentSelectionPage::entering()
{
    static const char *strings[] = {
        QT_TR_NOOP("Please select the components you want to update."),
        QT_TR_NOOP("Please select the components you want to install."),
        QT_TR_NOOP("Please select the components you want to uninstall."),
        QT_TR_NOOP("Select the components to install. Deselect installed components to uninstall them.")
     };

    int index = 0;
    PackageManagerCore *core = packageManagerCore();
    if (core->isInstaller()) index = 1;
    if (core->isUninstaller()) index = 2;
    if (core->isPackageManager()) index = 3;
    setColoredSubTitle(tr(strings[index]));

    d->updateTreeView();
    setModified(isComplete());
}

/*!
    Called when the show event \a event occurs. Switching pages back and forth might restore or
    remove the checked state of certain components the end users have checked or not checked,
    because the dependencies are resolved and checked when clicking \uicontrol Next. So as not to
    confuse the end users with newly checked components they did not check, the state they left the
    page in is restored.
*/
void ComponentSelectionPage::showEvent(QShowEvent *event)
{
    // remove once we deprecate isSelected, setSelected etc...
    if (!event->spontaneous())
        packageManagerCore()->restoreCheckState();
    QWizardPage::showEvent(event);
}

/*!
    Selects all components in the component tree.
*/
void ComponentSelectionPage::selectAll()
{
    d->selectAll();
}

/*!
    Deselects all components in the component tree.
*/
void ComponentSelectionPage::deselectAll()
{
    d->deselectAll();
}

/*!
    Selects the components that have the \c <Default> element set to \c true in
    the package information file.
*/
void ComponentSelectionPage::selectDefault()
{
    if (packageManagerCore()->isInstaller())
        d->selectDefault();
}

/*!
    Selects the component with \a id in the component tree.
*/
void ComponentSelectionPage::selectComponent(const QString &id)
{
    const QModelIndex &idx = d->m_currentModel->indexFromComponentName(id);
    if (idx.isValid())
        d->m_currentModel->setData(idx, Qt::Checked, Qt::CheckStateRole);
}

/*!
    Deselects the component with \a id in the component tree.
*/
void ComponentSelectionPage::deselectComponent(const QString &id)
{
    const QModelIndex &idx = d->m_currentModel->indexFromComponentName(id);
    if (idx.isValid())
        d->m_currentModel->setData(idx, Qt::Unchecked, Qt::CheckStateRole);
}

void ComponentSelectionPage::setModified(bool modified)
{
    setComplete(modified);
}

/*!
    Returns \c true if at least one component is checked on the page.
*/
bool ComponentSelectionPage::isComplete() const
{
    if (packageManagerCore()->isInstaller() || packageManagerCore()->isUpdater())
        return d->m_currentModel->checked().count();
    return d->m_currentModel->checkedState().testFlag(ComponentModel::DefaultChecked) == false;
}


// -- TargetDirectoryPage

/*!
    \class QInstaller::TargetDirectoryPage
    \inmodule QtInstallerFramework
    \brief The TargetDirectoryPage class specifies the target directory for the
    installation.

    End users can leave the page to continue the installation only if certain criteria are
    fulfilled. Some of them are checked in the validatePage() function, some in the
    targetDirWarning() function:

    \list
        \li No empty path given as target.
        \li No relative path given as target.
        \li Only ASCII characters are allowed in the path if the <AllowNonAsciiCharacters> element
            in the configuration file is set to \c false.
        \li The following ambiguous characters are not allowed in the path: [\"~<>|?*!@#$%^&:,;]
        \li No root or home directory given as target.
        \li On Windows, path names must be less than 260 characters long.
        \li No spaces in the path if the <AllowSpaceInPath> element in the configuration file is set
            to \c false.
    \endlist
*/

/*!
    Constructs a target directory selection page with \a core as parent.
*/
TargetDirectoryPage::TargetDirectoryPage(PackageManagerCore *core)
    : PackageManagerPage(core)
{
    setPixmap(QWizard::WatermarkPixmap, QPixmap());
    setObjectName(QLatin1String("TargetDirectoryPage"));
    setColoredTitle(tr("Installation Folder"));

    QVBoxLayout *layout = new QVBoxLayout(this);

    QLabel *msgLabel = new QLabel(this);
    msgLabel->setWordWrap(true);
    msgLabel->setObjectName(QLatin1String("MessageLabel"));
    msgLabel->setText(tr("Please specify the folder where %1 will be installed.").arg(productName()));
    layout->addWidget(msgLabel);

    QHBoxLayout *hlayout = new QHBoxLayout;

    m_lineEdit = new QLineEdit(this);
    m_lineEdit->setObjectName(QLatin1String("TargetDirectoryLineEdit"));
    connect(m_lineEdit, SIGNAL(textChanged(QString)), this, SIGNAL(completeChanged()));
    hlayout->addWidget(m_lineEdit);

    QPushButton *browseButton = new QPushButton(this);
    browseButton->setObjectName(QLatin1String("BrowseDirectoryButton"));
    connect(browseButton, SIGNAL(clicked()), this, SLOT(dirRequested()));
    browseButton->setShortcut(QKeySequence(tr("Alt+R", "browse file system to choose a file")));
    browseButton->setText(tr("B&rowse..."));
    hlayout->addWidget(browseButton);

    layout->addLayout(hlayout);

    QPalette palette;
    palette.setColor(QPalette::WindowText, Qt::red);

    m_warningLabel = new QLabel(this);
    m_warningLabel->setPalette(palette);
    m_warningLabel->setWordWrap(true);
    m_warningLabel->setObjectName(QLatin1String("WarningLabel"));
    layout->addWidget(m_warningLabel);

    setLayout(layout);
}

/*!
    Returns the target directory for the installation.
*/
QString TargetDirectoryPage::targetDir() const
{
    return m_lineEdit->text().trimmed();
}

/*!
    Sets the directory specified by \a dirName as the target directory for the
    installation.
*/
void TargetDirectoryPage::setTargetDir(const QString &dirName)
{
    m_lineEdit->setText(dirName);
}

/*!
    Initializes the page.
*/
void TargetDirectoryPage::initializePage()
{
    QString targetDir = packageManagerCore()->value(scTargetDir);
    if (targetDir.isEmpty()) {
        targetDir = QDir::homePath() + QDir::separator();
        if (!packageManagerCore()->settings().allowSpaceInPath()) {
            // prevent spaces in the default target directory
            if (targetDir.contains(QLatin1Char(' ')))
                targetDir = QDir::rootPath();
            targetDir += productName().remove(QLatin1Char(' '));
        } else {
            targetDir += productName();
        }
    }
    m_lineEdit->setText(QDir::toNativeSeparators(QDir(targetDir).absolutePath()));

    PackageManagerPage::initializePage();
}

/*!
    Checks whether the target directory exists and has contents:

    \list
        \li Returns \c true if the directory exists and is empty.
        \li Returns \c false if the directory already exists and contains an installation.
        \li Returns \c false if the target is a file or a symbolic link.
        \li Returns \c true or \c false if the directory exists but is not empty, depending on the
            choice that the end users make in the displayed message box.
    \endlist
*/
bool TargetDirectoryPage::validatePage()
{
    if (!isVisible())
        return true;

    const QString remove = packageManagerCore()->value(QLatin1String("RemoveTargetDir"));
    if (!QVariant(remove).toBool())
        return true;

    const QString targetDir = this->targetDir();
    const QDir dir(targetDir);
    // the directory exists and is empty...
    if (dir.exists() && dir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty())
        return true;

    const QFileInfo fi(targetDir);
    if (fi.isDir()) {
        QString fileName = packageManagerCore()->settings().maintenanceToolName();
#if defined(Q_OS_OSX)
        if (QInstaller::isInBundle(QCoreApplication::applicationDirPath()))
            fileName += QLatin1String(".app/Contents/MacOS/") + fileName;
#elif defined(Q_OS_WIN)
        fileName += QLatin1String(".exe");
#endif

        QFileInfo fi2(targetDir + QDir::separator() + fileName);
        if (fi2.exists()) {
            return failWithError(QLatin1String("TargetDirectoryInUse"), tr("The folder you selected already "
                "exists and contains an installation. Choose a different target for installation."));
        }

        return askQuestion(QLatin1String("OverwriteTargetDirectory"),
            tr("You have selected an existing, non-empty folder for installation.\nNote that it will be "
            "completely wiped on uninstallation of this application.\nIt is not advisable to install into "
            "this folder as installation might fail.\nDo you want to continue?"));
    } else if (fi.isFile() || fi.isSymLink()) {
        return failWithError(QLatin1String("WrongTargetDirectory"), tr("You have selected an existing file "
            "or symlink, please choose a different target for installation."));
    }
    return true;
}

/*!
    Initializes the page's fields based on values from fields on previous
    pages.
*/
void TargetDirectoryPage::entering()
{
    if (QPushButton *const b = qobject_cast<QPushButton *>(gui()->button(QWizard::NextButton)))
        b->setDefault(true);
}

/*!
    Called when end users leave the page and the PackageManagerGui:currentPageChanged()
    signal is triggered.
*/
void TargetDirectoryPage::leaving()
{
    packageManagerCore()->setValue(scTargetDir, targetDir());
}

void TargetDirectoryPage::dirRequested()
{
    const QString newDirName = QFileDialog::getExistingDirectory(this,
        tr("Select Installation Folder"), targetDir());
    if (newDirName.isEmpty() || newDirName == targetDir())
        return;
    m_lineEdit->setText(QDir::toNativeSeparators(newDirName));
}

/*!
    Requests a warning message to be shown to end users upon invalid input. If the input is valid,
    the \uicontrol Next button is enabled.

    Returns \c true if a valid path to the target directory is set; otherwise returns \c false.
*/
bool TargetDirectoryPage::isComplete() const
{
    m_warningLabel->setText(targetDirWarning());
    return m_warningLabel->text().isEmpty();
}

/*!
    Returns a warning if the path to the target directory is not set or if it
    is invalid. Installation can continue only after a valid target path is given.
*/
QString TargetDirectoryPage::targetDirWarning() const
{
    if (targetDir().isEmpty())
        return tr("The installation path cannot be empty, please specify a valid folder.");

    QDir target(targetDir());
    if (target.isRelative())
        return tr("The installation path cannot be relative, please specify an absolute path.");

    QString nativeTargetDir = QDir::toNativeSeparators(target.absolutePath());
    if (!packageManagerCore()->settings().allowNonAsciiCharacters()) {
        for (int i = 0; i < nativeTargetDir.length(); ++i) {
            if (nativeTargetDir.at(i).unicode() & 0xff80) {
                return tr("The path or installation directory contains non ASCII characters. This "
                    "is currently not supported! Please choose a different path or installation "
                    "directory.");
            }
        }
    }

    target = target.canonicalPath();
    if (target == QDir::root() || target == QDir::home()) {
        return tr("As the install directory is completely deleted, installing in %1 is forbidden.")
            .arg(QDir::toNativeSeparators(target.path()));
    }

#ifdef Q_OS_WIN
    // folder length (set by user) + maintenance tool name length (no extension) + extra padding
    if ((nativeTargetDir.length()
        + packageManagerCore()->settings().maintenanceToolName().length() + 20) >= MAX_PATH) {
        return tr("The path you have entered is too long, please make sure to "
            "specify a valid path.");
    }

    static QRegularExpression reg(QLatin1String(
        "^(?<drive>[a-zA-Z]:\\\\)|"
        "^(\\\\\\\\(?<path>\\w+)\\\\)|"
        "^(\\\\\\\\(?<ip>\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3})\\\\)"));
    const QRegularExpressionMatch regMatch = reg.match(nativeTargetDir);

    const QString ipMatch = regMatch.captured(QLatin1String("ip"));
    const QString pathMatch = regMatch.captured(QLatin1String("path"));
    const QString driveMatch = regMatch.captured(QLatin1String("drive"));

    if (ipMatch.isEmpty() && pathMatch.isEmpty() && driveMatch.isEmpty()) {
        return tr("The path you have entered is not valid, please make sure to "
            "specify a valid target.");
    }

    if (!driveMatch.isEmpty()) {
        bool validDrive = false;
        const QFileInfo drive(driveMatch);
        foreach (const QFileInfo &driveInfo, QDir::drives()) {
            if (drive == driveInfo) {
                validDrive = true;
                break;
            }
        }
        if (!validDrive) {  // right now we can only verify local drives
            return tr("The path you have entered is not valid, please make sure to "
                "specify a valid drive.");
        }
        nativeTargetDir = nativeTargetDir.mid(2);
    }

    if (nativeTargetDir.endsWith(QLatin1Char('.')))
        return tr("The installation path must not end with '.', please specify a valid folder.");

    QString ambiguousChars = QLatin1String("[\"~<>|?*!@#$%^&:,; ]"
        "|(\\\\CON)(\\\\|$)|(\\\\PRN)(\\\\|$)|(\\\\AUX)(\\\\|$)|(\\\\NUL)(\\\\|$)|(\\\\COM\\d)(\\\\|$)|(\\\\LPT\\d)(\\\\|$)");
#else // Q_OS_WIN
    QString ambiguousChars = QStringLiteral("[~<>|?*!@#$%^&:,; \\\\]");
#endif // Q_OS_WIN

    if (packageManagerCore()->settings().allowSpaceInPath())
        ambiguousChars.remove(QLatin1Char(' '));

    static QRegularExpression ambCharRegEx(ambiguousChars, QRegularExpression::CaseInsensitiveOption);
    // check if there are not allowed characters in the target path
    QRegularExpressionMatch match = ambCharRegEx.match(nativeTargetDir);
    if (match.hasMatch()) {
        return tr("The installation path must not contain '%1', "
            "please specify a valid folder.").arg(match.captured(0));
    }

    return QString();
}

/*!
    Returns \c true if a warning message specified by \a message with the
    identifier \a identifier is presented to end users for acknowledgment.
*/
bool TargetDirectoryPage::askQuestion(const QString &identifier, const QString &message)
{
    QMessageBox::StandardButton bt =
        MessageBoxHandler::warning(MessageBoxHandler::currentBestSuitParent(), identifier,
        tr("Warning"), message, QMessageBox::Yes | QMessageBox::No);
    return bt == QMessageBox::Yes;
}

bool TargetDirectoryPage::failWithError(const QString &identifier, const QString &message)
{
    MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(), identifier,
        tr("Error"), message);
    return false;
}


// -- StartMenuDirectoryPage

/*!
    \class QInstaller::StartMenuDirectoryPage
    \inmodule QtInstallerFramework
    \brief The StartMenuDirectoryPage class specifies the program group for the
    product in the Windows Start menu.
*/

/*!
    Constructs a Start menu directory selection page with \a core as parent.
*/
StartMenuDirectoryPage::StartMenuDirectoryPage(PackageManagerCore *core)
    : PackageManagerPage(core)
{
    setPixmap(QWizard::WatermarkPixmap, QPixmap());
    setObjectName(QLatin1String("StartMenuDirectoryPage"));
    setColoredTitle(tr("Start Menu shortcuts"));
    setColoredSubTitle(tr("Select the Start Menu in which you would like to create the program's "
        "shortcuts. You can also enter a name to create a new folder."));

    m_lineEdit = new QLineEdit(this);
    m_lineEdit->setText(core->value(scStartMenuDir, productName()));
    m_lineEdit->setObjectName(QLatin1String("StartMenuPathLineEdit"));

    startMenuPath = core->value(QLatin1String("UserStartMenuProgramsPath"));
    QStringList dirs = QDir(startMenuPath).entryList(QDir::AllDirs | QDir::NoDotAndDotDot);
    if (core->value(QLatin1String("AllUsers")) == scTrue) {
        startMenuPath = core->value(QLatin1String("AllUsersStartMenuProgramsPath"));
        dirs += QDir(startMenuPath).entryList(QDir::AllDirs | QDir::NoDotAndDotDot);
    }
    dirs.removeDuplicates();

    m_listWidget = new QListWidget(this);
    foreach (const QString &dir, dirs)
        new QListWidgetItem(dir, m_listWidget);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(m_lineEdit);
    layout->addWidget(m_listWidget);

    setLayout(layout);

    connect(m_listWidget, SIGNAL(currentItemChanged(QListWidgetItem*,QListWidgetItem*)), this,
        SLOT(currentItemChanged(QListWidgetItem*)));
}

/*!
    Returns the program group for the product in the Windows Start menu.
*/
QString StartMenuDirectoryPage::startMenuDir() const
{
    return m_lineEdit->text().trimmed();
}

/*!
    Sets \a startMenuDir as the program group for the product in the Windows
    Start menu.
*/
void StartMenuDirectoryPage::setStartMenuDir(const QString &startMenuDir)
{
    m_lineEdit->setText(startMenuDir.trimmed());
}

/*!
    Called when end users leave the page and the PackageManagerGui:currentPageChanged()
    signal is triggered.
*/
void StartMenuDirectoryPage::leaving()
{
    packageManagerCore()->setValue(scStartMenuDir, startMenuPath + QDir::separator()
        + startMenuDir());
}

void StartMenuDirectoryPage::currentItemChanged(QListWidgetItem *current)
{
    if (current)
        setStartMenuDir(current->data(Qt::DisplayRole).toString());
}


// -- ReadyForInstallationPage

/*!
    \class QInstaller::ReadyForInstallationPage
    \inmodule QtInstallerFramework
    \brief The ReadyForInstallationPage class informs end users that the
    installation can begin.
*/

/*!
    Constructs a ready for installation page with \a core as parent.
*/
ReadyForInstallationPage::ReadyForInstallationPage(PackageManagerCore *core)
    : PackageManagerPage(core)
    , m_msgLabel(new QLabel)
{
    setPixmap(QWizard::WatermarkPixmap, QPixmap());
    setObjectName(QLatin1String("ReadyForInstallationPage"));

    QVBoxLayout *baseLayout = new QVBoxLayout();
    baseLayout->setObjectName(QLatin1String("BaseLayout"));

    QVBoxLayout *topLayout = new QVBoxLayout();
    topLayout->setObjectName(QLatin1String("TopLayout"));

    m_msgLabel->setWordWrap(true);
    m_msgLabel->setObjectName(QLatin1String("MessageLabel"));
    m_msgLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    topLayout->addWidget(m_msgLabel);
    baseLayout->addLayout(topLayout);

    QVBoxLayout *bottomLayout = new QVBoxLayout();
    bottomLayout->setObjectName(QLatin1String("BottomLayout"));
    bottomLayout->addStretch();

    m_taskDetailsBrowser = new QTextBrowser(this);
    m_taskDetailsBrowser->setReadOnly(true);
    m_taskDetailsBrowser->setObjectName(QLatin1String("TaskDetailsBrowser"));
    m_taskDetailsBrowser->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_taskDetailsBrowser->setVisible(false);
    bottomLayout->addWidget(m_taskDetailsBrowser);
    bottomLayout->setStretch(1, 10);
    baseLayout->addLayout(bottomLayout);

    setCommitPage(true);
    setLayout(baseLayout);
}

/*!
    Initializes the page's fields based on values from fields on previous
    pages. The text to display depends on whether the page is being used in an
    installer, updater, or uninstaller.
*/
void ReadyForInstallationPage::entering()
{
    setComplete(false);

    if (packageManagerCore()->isUninstaller()) {
        m_taskDetailsBrowser->setVisible(false);
        setButtonText(QWizard::CommitButton, tr("U&ninstall"));
        setColoredTitle(tr("Ready to Uninstall"));
        m_msgLabel->setText(tr("Setup is now ready to begin removing %1 from your computer.<br>"
            "<font color=\"red\">The program directory %2 will be deleted completely</font>, "
            "including all content in that directory!")
            .arg(productName(),
                QDir::toNativeSeparators(QDir(packageManagerCore()->value(scTargetDir))
            .absolutePath())));
        setComplete(true);
        return;
    } else if (packageManagerCore()->isPackageManager() || packageManagerCore()->isUpdater()) {
        setButtonText(QWizard::CommitButton, tr("U&pdate"));
        setColoredTitle(tr("Ready to Update Packages"));
        m_msgLabel->setText(tr("Setup is now ready to begin updating your installation."));
    } else {
        Q_ASSERT(packageManagerCore()->isInstaller());
        setButtonText(QWizard::CommitButton, tr("&Install"));
        setColoredTitle(tr("Ready to Install"));
        m_msgLabel->setText(tr("Setup is now ready to begin installing %1 on your computer.")
            .arg(productName()));
    }

    QString htmlOutput;
    bool componentsOk = calculateComponents(&htmlOutput);
    m_taskDetailsBrowser->setHtml(htmlOutput);
    m_taskDetailsBrowser->setVisible(!componentsOk || isVerbose());
    setComplete(componentsOk);

    const VolumeInfo tempVolume = VolumeInfo::fromPath(QDir::tempPath());
    const VolumeInfo targetVolume = VolumeInfo::fromPath(packageManagerCore()->value(scTargetDir));

    const quint64 tempVolumeAvailableSize = tempVolume.availableSize();
    const quint64 installVolumeAvailableSize = targetVolume.availableSize();

    // at the moment there is no better way to check this
    if (targetVolume.size() == 0 && installVolumeAvailableSize == 0) {
        qDebug() << QString::fromLatin1("Could not determine available space on device. Volume "
            "descriptor: %1, Mount path: %2. Continue silently.").arg(targetVolume
            .volumeDescriptor(), targetVolume.mountPath());
        return;     // TODO: Shouldn't this also disable the "Next" button?
    }

    const bool tempOnSameVolume = (targetVolume == tempVolume);
    if (tempOnSameVolume) {
        qDebug() << "Tmp and install folder are on the same volume. Volume mount point:"
            << targetVolume.mountPath() << "Free space available:"
            << humanReadableSize(installVolumeAvailableSize);
    } else {
        qDebug() << "Tmp is on a different volume than the install folder. Tmp volume mount point:"
            << tempVolume.mountPath() << "Free space available:"
            << humanReadableSize(tempVolumeAvailableSize) << "Install volume mount point:"
            << targetVolume.mountPath() << "Free space available:"
            << humanReadableSize(installVolumeAvailableSize);
    }

    const quint64 extraSpace = 256 * 1024 * 1024LL;
    quint64 required(packageManagerCore()->requiredDiskSpace());
    quint64 tempRequired(packageManagerCore()->requiredTemporaryDiskSpace());
    if (required < extraSpace) {
        required += 0.1 * required;
        tempRequired += 0.1 * tempRequired;
    } else {
        required += extraSpace;
        tempRequired += extraSpace;
    }

    quint64 repositorySize = 0;
    const bool createLocalRepository = packageManagerCore()->createLocalRepositoryFromBinary();
    if (createLocalRepository && packageManagerCore()->isInstaller()) {
        repositorySize = QFile(QCoreApplication::applicationFilePath()).size();
        // if we create a local repository, take that space into account as well
        required += repositorySize;
    }

    qDebug() << "Installation space required:" << humanReadableSize(required) << "Temporary space "
        "required:" << humanReadableSize(tempRequired) << "Local repository size:"
        << humanReadableSize(repositorySize);

    if (tempOnSameVolume && (installVolumeAvailableSize <= (required + tempRequired))) {
        m_msgLabel->setText(tr("Not enough disk space to store temporary files and the "
            "installation! Available space: %1, at least required %2.")
            .arg(humanReadableSize(installVolumeAvailableSize),
            humanReadableSize(required + tempRequired)));
        setComplete(false);
        return;
    }

    if (installVolumeAvailableSize < required) {
        m_msgLabel->setText(tr("Not enough disk space to store all selected components! Available "
            "space: %1, at least required: %2.").arg(humanReadableSize(installVolumeAvailableSize),
            humanReadableSize(required)));
        setComplete(false);
        return;
    }

    if (tempVolumeAvailableSize < tempRequired) {
        m_msgLabel->setText(tr("Not enough disk space to store temporary files! Available space: "
            "%1, at least required: %2.").arg(humanReadableSize(tempVolumeAvailableSize),
            humanReadableSize(tempRequired)));
        setComplete(false);
        return;
    }

    if (installVolumeAvailableSize - required < 0.01 * targetVolume.size()) {
        // warn for less than 1% of the volume's space being free
        m_msgLabel->setText(tr("The volume you selected for installation seems to have sufficient "
            "space for installation, but there will be less than 1% of the volume's space "
            "available afterwards. %1").arg(m_msgLabel->text()));
    } else if (installVolumeAvailableSize - required < 100 * 1024 * 1024LL) {
        // warn for less than 100MB being free
        m_msgLabel->setText(tr("The volume you selected for installation seems to have sufficient "
            "space for installation, but there will be less than 100 MB available afterwards. %1")
            .arg(m_msgLabel->text()));
    }

    m_msgLabel->setText(QString::fromLatin1("%1 %2").arg(m_msgLabel->text(),
            tr("Installation will use %1 of disk space.")
            .arg(humanReadableSize(packageManagerCore()->requiredDiskSpace()))));
}

bool ReadyForInstallationPage::calculateComponents(QString *displayString)
{
    QString htmlOutput;
    QString lastInstallReason;
    if (!packageManagerCore()->calculateComponentsToUninstall() ||
            !packageManagerCore()->calculateComponentsToInstall()) {
        htmlOutput.append(QString::fromLatin1("<h2><font color=\"red\">%1</font></h2><ul>")
                          .arg(tr("Cannot resolve all dependencies.")));
        //if we have a missing dependency or a recursion we can display it
        if (!packageManagerCore()->componentsToInstallError().isEmpty()) {
            htmlOutput.append(QString::fromLatin1("<li> %1 </li>").arg(
                                  packageManagerCore()->componentsToInstallError()));
        }
        htmlOutput.append(QLatin1String("</ul>"));
        if (displayString)
            *displayString = htmlOutput;
        return false;
    }

    // In case of updater mode we don't uninstall components.
    if (!packageManagerCore()->isUpdater()) {
        QList<Component*> componentsToRemove = packageManagerCore()->componentsToUninstall();
        if (!componentsToRemove.isEmpty()) {
            htmlOutput.append(QString::fromLatin1("<h3>%1</h3><ul>").arg(tr("Components about to "
                "be removed.")));
            foreach (Component *component, componentsToRemove)
                htmlOutput.append(QString::fromLatin1("<li> %1 </li>").arg(component->name()));
            htmlOutput.append(QLatin1String("</ul>"));
        }
    }

    foreach (Component *component, packageManagerCore()->orderedComponentsToInstall()) {
        const QString installReason = packageManagerCore()->installReason(component);
        if (lastInstallReason != installReason) {
            if (!lastInstallReason.isEmpty()) // means we had to close the previous list
                htmlOutput.append(QLatin1String("</ul>"));
            htmlOutput.append(QString::fromLatin1("<h3>%1</h3><ul>").arg(installReason));
            lastInstallReason = installReason;
        }
        htmlOutput.append(QString::fromLatin1("<li> %1 </li>").arg(component->name()));
    }
    if (displayString)
        *displayString = htmlOutput;
    return true;
}

/*!
    Called when end users leave the page and the PackageManagerGui:currentPageChanged()
    signal is triggered.
*/
void ReadyForInstallationPage::leaving()
{
    setButtonText(QWizard::CommitButton, gui()->defaultButtonText(QWizard::CommitButton));
}

// -- PerformInstallationPage

/*!
    \class QInstaller::PerformInstallationPage
    \inmodule QtInstallerFramework
    \brief The PerformInstallationPage class shows progress information about the installation state.

    This class is a container for the PerformInstallationForm class, which
    constructs the actual UI for the page.
*/

/*!
    \fn PerformInstallationPage::isInterruptible() const

    Returns \c true if the installation can be interrupted.
*/

/*!
    \fn PerformInstallationPage::setAutomatedPageSwitchEnabled(bool request)

    Enables automatic switching of pages when \a request is \c true.
*/

/*!
    Constructs a perform installation page with \a core as parent. The page
    contains a PerformInstallationForm that defines the UI for the page.
*/
PerformInstallationPage::PerformInstallationPage(PackageManagerCore *core)
    : PackageManagerPage(core)
    , m_performInstallationForm(new PerformInstallationForm(this))
{
    setPixmap(QWizard::WatermarkPixmap, QPixmap());
    setObjectName(QLatin1String("PerformInstallationPage"));

    m_performInstallationForm->setupUi(this);

    connect(ProgressCoordinator::instance(), SIGNAL(detailTextChanged(QString)),
        m_performInstallationForm, SLOT(appendProgressDetails(QString)));
    connect(ProgressCoordinator::instance(), SIGNAL(detailTextResetNeeded()),
        m_performInstallationForm, SLOT(clearDetailsBrowser()));
    connect(m_performInstallationForm, SIGNAL(showDetailsChanged()), this,
        SLOT(toggleDetailsWereChanged()));

    connect(core, SIGNAL(installationStarted()), this, SLOT(installationStarted()));
    connect(core, SIGNAL(installationFinished()), this, SLOT(installationFinished()));

    connect(core, SIGNAL(uninstallationStarted()), this, SLOT(uninstallationStarted()));
    connect(core, SIGNAL(uninstallationFinished()), this, SLOT(uninstallationFinished()));

    connect(core, SIGNAL(titleMessageChanged(QString)), this, SLOT(setTitleMessage(QString)));
    connect(this, SIGNAL(setAutomatedPageSwitchEnabled(bool)), core,
        SIGNAL(setAutomatedPageSwitchEnabled(bool)));

    m_performInstallationForm->setDetailsWidgetVisible(true);

    setCommitPage(true);
}

/*!
    Destructs a perform installation page.
*/
PerformInstallationPage::~PerformInstallationPage()
{
    delete m_performInstallationForm;
}

/*!
    Returns \c true if automatically switching to the page is requested.
*/
bool PerformInstallationPage::isAutoSwitching() const
{
    return !m_performInstallationForm->isShowingDetails();
}

// -- protected

/*!
    Initializes the page's fields based on values from fields on previous
    pages. The text to display depends on whether the page is being used in an
    installer, updater, or uninstaller.
*/
void PerformInstallationPage::entering()
{
    setComplete(false);

    if (packageManagerCore()->isUninstaller()) {
        setButtonText(QWizard::CommitButton, tr("U&ninstall"));
        setColoredTitle(tr("Uninstalling %1").arg(productName()));

        QTimer::singleShot(30, packageManagerCore(), SLOT(runUninstaller()));
    } else if (packageManagerCore()->isPackageManager() || packageManagerCore()->isUpdater()) {
        setButtonText(QWizard::CommitButton, tr("&Update"));
        setColoredTitle(tr("Updating components of %1").arg(productName()));

        QTimer::singleShot(30, packageManagerCore(), SLOT(runPackageUpdater()));
    } else {
        setButtonText(QWizard::CommitButton, tr("&Install"));
        setColoredTitle(tr("Installing %1").arg(productName()));

        QTimer::singleShot(30, packageManagerCore(), SLOT(runInstaller()));
    }

    m_performInstallationForm->enableDetails();
    emit setAutomatedPageSwitchEnabled(true);

    if (isVerbose())
        m_performInstallationForm->toggleDetails();
}

/*!
    Called when end users leave the page and the PackageManagerGui:currentPageChanged()
    signal is triggered.
*/
void PerformInstallationPage::leaving()
{
    setButtonText(QWizard::CommitButton, gui()->defaultButtonText(QWizard::CommitButton));
}

// -- public slots

/*!
    Sets \a title as the title of the perform installation page.
*/
void PerformInstallationPage::setTitleMessage(const QString &title)
{
    setColoredTitle(title);
}

// -- private slots

void PerformInstallationPage::installationStarted()
{
    m_performInstallationForm->startUpdateProgress();
}

void PerformInstallationPage::installationFinished()
{
    m_performInstallationForm->stopUpdateProgress();
    if (!isAutoSwitching()) {
        m_performInstallationForm->scrollDetailsToTheEnd();
        m_performInstallationForm->setDetailsButtonEnabled(false);

        setComplete(true);
        setButtonText(QWizard::CommitButton, gui()->defaultButtonText(QWizard::NextButton));
    }
}

void PerformInstallationPage::uninstallationStarted()
{
    m_performInstallationForm->startUpdateProgress();
    if (QAbstractButton *cancel = gui()->button(QWizard::CancelButton))
        cancel->setEnabled(false);
}

void PerformInstallationPage::uninstallationFinished()
{
    installationFinished();
    if (QAbstractButton *cancel = gui()->button(QWizard::CancelButton))
        cancel->setEnabled(false);
}

void PerformInstallationPage::toggleDetailsWereChanged()
{
    emit setAutomatedPageSwitchEnabled(isAutoSwitching());
}


// -- FinishedPage

/*!
    \class QInstaller::FinishedPage
    \inmodule QtInstallerFramework
    \brief The FinishedPage class completes the installation wizard.

    You can add the option to open the installed application to the page.
*/

/*!
    Constructs an installation finished page with \a core as parent.
*/
FinishedPage::FinishedPage(PackageManagerCore *core)
    : PackageManagerPage(core)
    , m_commitButton(0)
{
    setObjectName(QLatin1String("FinishedPage"));
    setColoredTitle(tr("Completing the %1 Wizard").arg(productName()));

    m_msgLabel = new QLabel(this);
    m_msgLabel->setWordWrap(true);
    m_msgLabel->setObjectName(QLatin1String("MessageLabel"));

#ifdef Q_OS_OSX
    m_msgLabel->setText(tr("Click Done to exit the %1 Wizard.").arg(productName()));
#else
    m_msgLabel->setText(tr("Click Finish to exit the %1 Wizard.").arg(productName()));
#endif

    m_runItCheckBox = new QCheckBox(this);
    m_runItCheckBox->setObjectName(QLatin1String("RunItCheckBox"));
    m_runItCheckBox->setChecked(true);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(m_msgLabel);
    layout->addWidget(m_runItCheckBox);
    setLayout(layout);

    setCommitPage(true);
}

/*!
    Initializes the page's fields based on values from fields on previous
    pages.
*/
void FinishedPage::entering()
{
    if (m_commitButton) {
        disconnect(m_commitButton, SIGNAL(clicked()), this, SLOT(handleFinishClicked()));
        m_commitButton = 0;
    }

    if (packageManagerCore()->isUpdater() || packageManagerCore()->isPackageManager()) {
#ifdef Q_OS_OSX
        gui()->setOption(QWizard::NoCancelButton, false);
#endif
        if (QAbstractButton *cancel = gui()->button(QWizard::CancelButton)) {
            m_commitButton = cancel;
            cancel->setEnabled(true);
            cancel->setVisible(true);
            // we don't use the usual FinishButton so we need to connect the misused CancelButton
            connect(cancel, SIGNAL(clicked()), gui(), SIGNAL(finishButtonClicked()));
            connect(cancel, SIGNAL(clicked()), packageManagerCore(), SIGNAL(finishButtonClicked()));
            // for the moment we don't want the rejected signal connected
            disconnect(gui(), SIGNAL(rejected()), packageManagerCore(), SLOT(setCanceled()));

            connect(gui()->button(QWizard::CommitButton), SIGNAL(clicked()), this,
                SLOT(cleanupChangedConnects()));
        }
        setButtonText(QWizard::CommitButton, tr("Restart"));
        setButtonText(QWizard::CancelButton, gui()->defaultButtonText(QWizard::FinishButton));
    } else {
        if (packageManagerCore()->isInstaller()) {
            m_commitButton = wizard()->button(QWizard::FinishButton);
            if (QPushButton *const b = qobject_cast<QPushButton *>(m_commitButton))
                b->setDefault(true);
        }

        gui()->setOption(QWizard::NoCancelButton, true);
        if (QAbstractButton *cancel = gui()->button(QWizard::CancelButton))
            cancel->setVisible(false);
    }

    gui()->updateButtonLayout();

    if (m_commitButton) {
        disconnect(m_commitButton, SIGNAL(clicked()), this, SLOT(handleFinishClicked()));
        connect(m_commitButton, SIGNAL(clicked()), this, SLOT(handleFinishClicked()));
    }

    if (packageManagerCore()->status() == PackageManagerCore::Success) {
        const QString finishedText = packageManagerCore()->value(QLatin1String("FinishedText"));
        if (!finishedText.isEmpty())
            m_msgLabel->setText(finishedText);

        if (!packageManagerCore()->isUninstaller() && !packageManagerCore()->value(scRunProgram)
            .isEmpty()) {
                m_runItCheckBox->show();
                m_runItCheckBox->setText(packageManagerCore()->value(scRunProgramDescription,
                    tr("Run %1 now.")).arg(productName()));
            return; // job done
        }
    } else {
        // TODO: how to handle this using the config.xml
        setColoredTitle(tr("The %1 Wizard failed.").arg(productName()));
    }

    m_runItCheckBox->hide();
    m_runItCheckBox->setChecked(false);
}

/*!
    Called when end users leave the page and the PackageManagerGui:currentPageChanged()
    signal is triggered.
*/
void FinishedPage::leaving()
{
#ifdef Q_OS_OSX
    gui()->setOption(QWizard::NoCancelButton, true);
#endif

    if (QAbstractButton *cancel = gui()->button(QWizard::CancelButton))
        cancel->setVisible(false);
    gui()->updateButtonLayout();

    setButtonText(QWizard::CommitButton, gui()->defaultButtonText(QWizard::CommitButton));
    setButtonText(QWizard::CancelButton, gui()->defaultButtonText(QWizard::CancelButton));
}

/*!
    Performs the necessary operations when end users select the \uicontrol Finish
    button.
*/
void FinishedPage::handleFinishClicked()
{
    const QString program =
        packageManagerCore()->replaceVariables(packageManagerCore()->value(scRunProgram));

    const QStringList args = packageManagerCore()->replaceVariables(packageManagerCore()
        ->values(scRunProgramArguments));
    if (!m_runItCheckBox->isChecked() || program.isEmpty())
        return;

    qDebug() << "starting" << program << args;
    QProcess::startDetached(program, args);
}

/*!
    Removes changed connects from the page.
*/
void FinishedPage::cleanupChangedConnects()
{
    if (QAbstractButton *cancel = gui()->button(QWizard::CancelButton)) {
        // remove the workaround connect from entering page
        disconnect(cancel, SIGNAL(clicked()), gui(), SIGNAL(finishButtonClicked()));
        disconnect(cancel, SIGNAL(clicked()), packageManagerCore(), SIGNAL(finishButtonClicked()));
        connect(gui(), SIGNAL(rejected()), packageManagerCore(), SLOT(setCanceled()));

        disconnect(gui()->button(QWizard::CommitButton), SIGNAL(clicked()), this,
            SLOT(cleanupChangedConnects()));
    }
}

// -- RestartPage

/*!
    \class QInstaller::RestartPage
    \inmodule QtInstallerFramework
    \brief The RestartPage class enables restarting the installer.

    The restart installation page enables end users to restart the wizard.
    This is useful, for example, if the maintenance tool itself needs to be
    updated before updating the application components. When updating is done,
    end users can select \uicontrol Restart to start the maintenance tool.
*/

/*!
    \fn RestartPage::restart()

    This signal is emitted when the installer is restarted.
*/

/*!
    Constructs a restart installation page with \a core as parent.
*/
RestartPage::RestartPage(PackageManagerCore *core)
    : PackageManagerPage(core)
{
    setObjectName(QLatin1String("RestartPage"));
    setColoredTitle(tr("Completing the %1 Setup Wizard").arg(productName()));

    setFinalPage(false);
}

/*!
    Returns the introduction page.
*/
int RestartPage::nextId() const
{
    return PackageManagerCore::Introduction;
}

/*!
    Initializes the page's fields based on values from fields on previous
    pages.
*/
void RestartPage::entering()
{
    if (!packageManagerCore()->needsHardRestart()) {
        if (QAbstractButton *finish = wizard()->button(QWizard::FinishButton))
            finish->setVisible(false);
        QMetaObject::invokeMethod(this, "restart", Qt::QueuedConnection);
    } else {
        gui()->accept();
    }
}

/*!
    Called when end users leave the page and the PackageManagerGui:currentPageChanged()
    signal is triggered.
*/
void RestartPage::leaving()
{
}

#include "packagemanagergui.moc"
#include "moc_packagemanagergui.cpp"
