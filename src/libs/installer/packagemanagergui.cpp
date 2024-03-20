/**************************************************************************
**
** Copyright (C) 2024 The Qt Company Ltd.
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
#include "repositorycategory.h"
#include "componentselectionpage_p.h"
#include "loggingutils.h"

#include "sysinfo.h"
#include "globals.h"

#include <QApplication>

#include <QtCore/QDir>
#include <QtCore/QPair>
#include <QtCore/QProcess>
#include <QtCore/QTimer>

#include <QAbstractItemView>
#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QFileDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenuBar>
#include <QMenu>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QSplitter>
#include <QStringListModel>
#include <QTextBrowser>
#include <QFontDatabase>

#include <QVBoxLayout>
#include <QShowEvent>
#include <QFileDialog>
#include <QGroupBox>
#include <QScreen>

#ifdef Q_OS_WIN
# include <qt_windows.h>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
# include <QWinTaskbarButton>
# include <QWinTaskbarProgress>
#endif
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
    explicit DynamicInstallerPage(QWidget *widget, PackageManagerCore *core = nullptr)
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

        addPageAndProperties(packageManagerCore()->controlScriptEngine());
        addPageAndProperties(packageManagerCore()->componentScriptEngine());
    }

    QWidget *widget() const
    {
        return m_widget;
    }

    bool isComplete() const override
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
    Private(PackageManagerGui *qq)
        : q(qq)
        , m_currentId(-1)
        , m_modified(false)
        , m_autoSwitchPage(true)
        , m_showSettingsButton(false)
        , m_silent(false)
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

    void showSettingsButton(bool show)
    {
        if (m_showSettingsButton == show)
            return;
        q->setOption(QWizard::HaveCustomButton1, show);
        q->setButtonText(QWizard::CustomButton1, tr("&Settings"));
        q->button(QWizard::CustomButton1)->setToolTip(
            PackageManagerGui::tr("Specify proxy settings and configure repositories for add-on components."));

        q->updateButtonLayout();
        m_showSettingsButton = show;
    }

    PackageManagerGui *q;
    int m_currentId;
    bool m_modified;
    bool m_autoSwitchPage;
    bool m_showSettingsButton;
    bool m_silent;
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
    \fn void QInstaller::PackageManagerGui::interrupted()
    \sa {gui::interrupted}{gui.interrupted}
*/

/*!
    \fn void QInstaller::PackageManagerGui::languageChanged()
    \sa {gui::languageChanged}{gui.languageChanged}
*/

/*!
    \fn void QInstaller::PackageManagerGui::finishButtonClicked()
    \sa {gui::finishButtonClicked}{gui.finishButtonClicked}
*/

/*!
    \fn void QInstaller::PackageManagerGui::gotRestarted()
    \sa {gui::gotRestarted}{gui.gotRestarted}
*/

/*!
    \fn void QInstaller::PackageManagerGui::settingsButtonClicked()
    \sa {gui::settingsButtonClicked}{gui.settingsButtonClicked}
*/

/*!
    \fn void QInstaller::PackageManagerGui::aboutApplicationClicked()
    \sa {gui::aboutApplicationClicked}{gui.aboutApplicationClicked}
*/

/*!
    \fn void QInstaller::PackageManagerGui::packageManagerCore() const

    Returns the package manager core.
*/

/*!
    Constructs a package manager UI with package manager specified by \a core
    and \a parent as parent.
*/
PackageManagerGui::PackageManagerGui(PackageManagerCore *core, QWidget *parent)
    : QWizard(parent)
    , d(new Private(this))
    , m_core(core)
{
    if (m_core->isInstaller())
        setWindowTitle(tr("%1 Setup").arg(m_core->value(scTitle)));
    else
        setWindowTitle(tr("Maintain %1").arg(m_core->value(scTitle)));
    setWindowFlags(windowFlags() &~ Qt::WindowContextHelpButtonHint);

#ifdef Q_OS_MACOS
    QMenuBar *menuBar = new QMenuBar(this);
    QMenu *applicationMenu = new QMenu(menuBar);
    menuBar->addMenu(applicationMenu);

    QAction *aboutAction = new QAction(applicationMenu);
    aboutAction->setMenuRole(QAction::AboutRole);
    applicationMenu->addAction(aboutAction);

    connect(aboutAction, &QAction::triggered, this, &PackageManagerGui::aboutApplicationClicked);
#else
    setWindowIcon(QIcon(m_core->settings().installerWindowIcon()));
#endif
    if (!m_core->settings().wizardShowPageList()) {
        QString pixmapStr = m_core->settings().background();
        QInstaller::replaceHighDpiImage(pixmapStr);
        setPixmap(QWizard::BackgroundPixmap, pixmapStr);
    }
#ifdef Q_OS_LINUX
    setWizardStyle(QWizard::ModernStyle);
    setSizeGripEnabled(true);
#endif

    if (!m_core->settings().wizardStyle().isEmpty())
        setWizardStyle(getStyle(m_core->settings().wizardStyle()));

    // set custom stylesheet
    const QString styleSheetFile = m_core->settings().styleSheet();
    if (!styleSheetFile.isEmpty()) {
        QFile sheet(styleSheetFile);
        if (sheet.exists()) {
            if (sheet.open(QIODevice::ReadOnly)) {
                qApp->setStyleSheet(QString::fromLatin1(sheet.readAll()));
            } else {
                qCWarning(QInstaller::lcDeveloperBuild) << "The specified style sheet file "
                    "can not be opened.";
            }
        } else {
            qCWarning(QInstaller::lcDeveloperBuild) << "A style sheet file is specified, "
                "but it does not exist.";
        }
    }

    setOption(QWizard::NoBackButtonOnStartPage);
    setOption(QWizard::NoBackButtonOnLastPage);
#ifdef Q_OS_MACOS
    setOptions(options() & ~QWizard::NoDefaultButton);
    if (QPushButton *nextButton = qobject_cast<QPushButton *>(button(QWizard::NextButton)))
        nextButton->setFocusPolicy(Qt::StrongFocus);
#endif

    if (m_core->settings().wizardShowPageList()) {
        QWidget *sideWidget = new QWidget(this);
        sideWidget->setObjectName(QLatin1String("SideWidget"));

        m_pageListWidget = new QListWidget(sideWidget);
        m_pageListWidget->setObjectName(QLatin1String("PageListWidget"));
        m_pageListWidget->viewport()->setAutoFillBackground(false);
        m_pageListWidget->setFrameShape(QFrame::NoFrame);
        m_pageListWidget->setMinimumWidth(200);
        // The widget should be view-only but we do not want it to be grayed out,
        // so instead of calling setEnabled(false), do not accept focus.
        m_pageListWidget->setFocusPolicy(Qt::NoFocus);
        m_pageListWidget->setSelectionMode(QAbstractItemView::NoSelection);
        m_pageListWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        m_pageListWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

        QVBoxLayout *sideWidgetLayout = new QVBoxLayout(sideWidget);

        QString pageListPixmap = m_core->settings().pageListPixmap();
        if (!pageListPixmap.isEmpty()) {
            QInstaller::replaceHighDpiImage(pageListPixmap);
            QLabel *pageListPixmapLabel = new QLabel(sideWidget);
            pageListPixmapLabel->setObjectName(QLatin1String("PageListPixmapLabel"));
            pageListPixmapLabel->setPixmap(pageListPixmap);
            pageListPixmapLabel->setMinimumWidth(QPixmap(pageListPixmap).width());
            sideWidgetLayout->addWidget(pageListPixmapLabel);
        }
        sideWidgetLayout->addWidget(m_pageListWidget);
        sideWidget->setLayout(sideWidgetLayout);

        setSideWidget(sideWidget);
    }

    connect(this, &QDialog::rejected, m_core, &PackageManagerCore::setCanceled);
    connect(this, &PackageManagerGui::interrupted, m_core, &PackageManagerCore::interrupt);

    // all queued to show the finished page once everything is done
    connect(m_core, &PackageManagerCore::installationFinished,
            this, &PackageManagerGui::showFinishedPage,
        Qt::QueuedConnection);
    connect(m_core, &PackageManagerCore::offlineGenerationFinished,
            this, &PackageManagerGui::showFinishedPage,
        Qt::QueuedConnection);
    connect(m_core, &PackageManagerCore::uninstallationFinished,
            this, &PackageManagerGui::showFinishedPage,
        Qt::QueuedConnection);

    connect(this, &QWizard::currentIdChanged, this, &PackageManagerGui::currentPageChanged);
    connect(this, &QWizard::currentIdChanged, m_core, &PackageManagerCore::currentPageChanged);
    connect(button(QWizard::FinishButton), &QAbstractButton::clicked,
            this, &PackageManagerGui::finishButtonClicked);
    connect(button(QWizard::FinishButton), &QAbstractButton::clicked,
            m_core, &PackageManagerCore::finishButtonClicked);

    // make sure the QUiLoader's retranslateUi is executed first, then the script
    connect(this, &PackageManagerGui::languageChanged,
            m_core, &PackageManagerCore::languageChanged, Qt::QueuedConnection);
    connect(this, &PackageManagerGui::languageChanged,
            this, &PackageManagerGui::onLanguageChanged, Qt::QueuedConnection);

    connect(m_core,
        &PackageManagerCore::wizardPageInsertionRequested,
        this, &PackageManagerGui::wizardPageInsertionRequested);
    connect(m_core, &PackageManagerCore::wizardPageRemovalRequested,
            this, &PackageManagerGui::wizardPageRemovalRequested);
    connect(m_core, &PackageManagerCore::wizardWidgetInsertionRequested,
        this, &PackageManagerGui::wizardWidgetInsertionRequested);
    connect(m_core, &PackageManagerCore::wizardWidgetRemovalRequested,
            this, &PackageManagerGui::wizardWidgetRemovalRequested);
    connect(m_core, &PackageManagerCore::wizardPageVisibilityChangeRequested,
            this, &PackageManagerGui::wizardPageVisibilityChangeRequested, Qt::QueuedConnection);

    connect(m_core, &PackageManagerCore::setValidatorForCustomPageRequested,
            this, &PackageManagerGui::setValidatorForCustomPageRequested);

    connect(m_core, &PackageManagerCore::setAutomatedPageSwitchEnabled,
            this, &PackageManagerGui::setAutomatedPageSwitchEnabled);

    connect(this, &QWizard::customButtonClicked, this, &PackageManagerGui::customButtonClicked);

    for (int i = QWizard::BackButton; i < QWizard::CustomButton1; ++i)
        d->m_defaultButtonText.insert(i, buttonText(QWizard::WizardButton(i)));

    m_core->setGuiObject(this);

    // We need to create this ugly hack so that the installer doesn't exceed the maximum size of the
    // screen. The screen size where the widget lies is not available until the widget is visible.
    QTimer::singleShot(30, this, SLOT(setMaxSize()));
}

/*!
    Limits installer maximum size to screen size.
*/
void PackageManagerGui::setMaxSize()
{
    QSize size = this->screen()->availableGeometry().size();
    int windowFrameHeight = frameGeometry().height() - geometry().height();
    int availableHeight = size.height() - windowFrameHeight;

    size.setHeight(availableHeight);
    setMaximumSize(size);
}

/*!
    Updates the installer page list.
*/
void PackageManagerGui::updatePageListWidget()
{
    if (!m_core->settings().wizardShowPageList() || !m_pageListWidget)
        return;

    static const QRegularExpression regExp1 {QLatin1String("(.)([A-Z][a-z]+)")};
    static const QRegularExpression regExp2 {QLatin1String("([a-z0-9])([A-Z])")};

    m_pageListWidget->clear();
    foreach (int id, pageIds()) {
        PackageManagerPage *page = qobject_cast<PackageManagerPage *>(pageById(id));
        if (!page->showOnPageList())
            continue;

        // Use page list title if set, otherwise try to use the normal title. If that
        // is not set either, use the object name with spaces added between words.
        QString itemText;
        if (!page->pageListTitle().isEmpty()) {
            itemText = page->pageListTitle();
        } else if (!page->title().isEmpty()) {
            // Title may contain formatting, return only contained plain text
            QTextDocument doc;
            doc.setHtml(page->title());
            itemText = doc.toPlainText().trimmed();
        } else {
            // Remove "Page" suffix from object name if exists and add spaces between words
            itemText = page->objectName();
            itemText.remove(QLatin1String("Page"), Qt::CaseInsensitive);
            itemText.replace(regExp1, QLatin1String("\\1 \\2"));
            itemText.replace(regExp2, QLatin1String("\\1 \\2"));
        }
        QListWidgetItem *item = new QListWidgetItem(itemText, m_pageListWidget);
        item->setSizeHint(QSize(m_pageListWidget->width(), 30));

        // Give visual indication about current & non-visited pages
        if (id == d->m_currentId) {
            QFont currentItemFont = item->font();
            currentItemFont.setBold(true);
            item->setFont(currentItemFont);
            // Current item should be always visible on the list
            m_pageListWidget->scrollToItem(item);
        } else {
            item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
        }
    }
}

/*!
    Destructs a package manager UI.
*/
PackageManagerGui::~PackageManagerGui()
{
    m_core->setGuiObject(nullptr);
    delete d;
}

/*!
    Returns the style of the package manager UI depending on \a name:

    \list
        \li \c Classic - Classic UI style for Windows 7 and earlier.
        \li \c Modern - Modern UI style for Windows 8.
        \li \c Mac - UI style for macOS.
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
   Hides the GUI when \a silent is \c true.
*/
void PackageManagerGui::setSilent(bool silent)
{
  d->m_silent = silent;
  setVisible(!silent);
}

/*!
    Returns the current silent state.
*/
bool PackageManagerGui::isSilent() const
{
  return d->m_silent;
}

/*!
    Updates the model of \a object (which must be a QComboBox or
    QAbstractItemView) such that it contains the given \a items.
*/
void PackageManagerGui::setTextItems(QObject *object, const QStringList &items)
{
    if (QComboBox *comboBox = qobject_cast<QComboBox*>(object)) {
        comboBox->setModel(new QStringListModel(items));
        return;
    }

    if (QAbstractItemView *view = qobject_cast<QAbstractItemView*>(object)) {
        view->setModel(new QStringListModel(items));
        return;
    }

    qCWarning(QInstaller::lcDeveloperBuild) << "Cannot set text items on object of type"
             << object->metaObject()->className() << ".";
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
        QTimer::singleShot(delay, b, &QAbstractButton::click);
    else
        qCWarning(QInstaller::lcDeveloperBuild) << "Button with type: " << d->buttonType(wb) << "not found!";
}

/*!
    Clicks the button specified by \a objectName after the delay specified by \a delay.

    \sa {gui::clickButton}{gui.clickButton}
*/
void PackageManagerGui::clickButton(const QString &objectName, int delay) const
{
    QPushButton *button = findChild<QPushButton *>(objectName);
    if (button)
        QTimer::singleShot(delay, button, &QAbstractButton::click);
    else
        qCWarning(QInstaller::lcDeveloperBuild) << "Button with objectname: " << objectName << "not found!";
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

    qCWarning(QInstaller::lcDeveloperBuild) << "Button with type: " << d->buttonType(wb) << "not found!";
    return false;
}

/*!
    Sets \a buttonText for button specified by \a buttonId to a installer page \a pageId.

    \note In some pages, installer will change the button text when entering
    the page. In that case, you need to connect to \c entered() -signal of the
    page to change the \a buttonText.

    \sa {gui::setWizardPageButtonText}{gui.setWizardPageButtonText}
*/
void PackageManagerGui::setWizardPageButtonText(int pageId, int buttonId, const QString &buttonText)
{
    PackageManagerPage *const p = qobject_cast<PackageManagerPage*> (page(pageId));
    if (p)
        p->setButtonText(static_cast<QWizard::WizardButton>(buttonId), buttonText);
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
    qCDebug(QInstaller::lcInstallerInstallLog) << "Loaded control script" << scriptPath;
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
            qCDebug(QInstaller::lcDeveloperBuild) << "Control script callback" << methodName
                << "does not exist.";
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
        QSize minimumSize;
        minimumSize.setWidth(m_core->settings().wizardMinimumWidth()
            ? m_core->settings().wizardMinimumWidth()
            : width());

        minimumSize.setHeight(m_core->settings().wizardMinimumHeight()
            ? m_core->settings().wizardMinimumHeight()
            : height());

        setMinimumSize(minimumSize);
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
    while (QWizard::page(pageId) != nullptr)
        --pageId;

    // add it
    setPage(pageId, new DynamicInstallerPage(widget, m_core));
    updatePageListWidget();
}

/*!
    Requests the removal of the page specified by \a widget.
*/
void PackageManagerGui::wizardPageRemovalRequested(QWidget *widget)
{
    foreach (int pageId, pageIds()) {
        DynamicInstallerPage *const dynamicPage = qobject_cast<DynamicInstallerPage*>(page(pageId));
        if (dynamicPage == nullptr)
            continue;
        if (dynamicPage->widget() != widget)
            continue;
        removePage(pageId);
        d->m_defaultPages.remove(pageId);
        packageManagerCore()->controlScriptEngine()->removeFromGlobalObject(dynamicPage);
        packageManagerCore()->componentScriptEngine()->removeFromGlobalObject(dynamicPage);
    }
    updatePageListWidget();
}

/*!
    Requests the insertion of \a widget on \a page. Widget with lower
    \a position number will be inserted on top.
*/
void PackageManagerGui::wizardWidgetInsertionRequested(QWidget *widget,
    QInstaller::PackageManagerCore::WizardPage page, int position)
{
    Q_ASSERT(widget);

    if (PackageManagerPage *p = qobject_cast<PackageManagerPage *>(QWizard::page(page))) {
        p->m_customWidgets.insert(position, widget);
        if (p->m_customWidgets.count() > 1 ) {
            //Reorder the custom widgets based on their position
            QMultiMap<int, QWidget*>::Iterator it = p->m_customWidgets.begin();
            while (it != p->m_customWidgets.end()) {
                p->layout()->removeWidget(it.value());
                ++it;
            }
            it = p->m_customWidgets.begin();
            while (it != p->m_customWidgets.end()) {
                p->layout()->addWidget(it.value());
                ++it;
            }
        } else {
            p->layout()->addWidget(widget);
        }
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

    const QList<int> pages = pageIds();
    foreach (int id, pages) {
        PackageManagerPage *managerPage = qobject_cast<PackageManagerPage *>(page(id));
        managerPage->removeCustomWidget(widget);
    }
    widget->setParent(nullptr);
    packageManagerCore()->controlScriptEngine()->removeFromGlobalObject(widget);
    packageManagerCore()->componentScriptEngine()->removeFromGlobalObject(widget);
}

/*!
    Requests changing the visibility of the page specified by \a p to
    \a visible.
*/
void PackageManagerGui::wizardPageVisibilityChangeRequested(bool visible, int p)
{
    if (visible && page(p) == nullptr) {
        setPage(p, d->m_defaultPages[p]);
    } else if (!visible && page(p) != nullptr) {
        d->m_defaultPages[p] = page(p);
        removePage(p);
    }
    updatePageListWidget();
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
    qCDebug(QInstaller::lcDeveloperBuild) << "No page found for object name" << name;
    return nullptr;
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
    qCDebug(QInstaller::lcDeveloperBuild) << "No page found for object name" << name;
    return nullptr;
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
                question = tr("Do you want to cancel the removal process?");
    } else {
        question = tr("Do you want to quit the installer application?");
        if (m_core->isUninstaller())
            question = tr("Do you want to quit the uninstaller application?");
        if (m_core->isMaintainer())
            question = tr("Do you want to quit the maintenance application?");
    }

    const QMessageBox::StandardButton button =
        MessageBoxHandler::question(MessageBoxHandler::currentBestSuitParent(),
        QLatin1String("cancelInstallation"), tr("%1 Question").arg(m_core->value(scTitle)), question,
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
    m_core->setValue(QLatin1String("ShowSettingsButton"), QString::number(show));
    d->showSettingsButton(show);
}

/*!
    Shows the \uicontrol Settings button if \a request is \c true. If script has
    set the settings button visibility, this function has no effect.
*/
void PackageManagerGui::requestSettingsButtonByInstaller(bool request)
{
    if (m_core->value(QLatin1String("ShowSettingsButton")).isEmpty())
        d->showSettingsButton(request);
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
    new page by calling executeControlScript(). Updates the page list set as QWizard::sideWidget().


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
        updatePageListWidget();
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
    \fn QInstaller::PackageManagerPage::~PackageManagerPage()

    Destructs a package manager page.
*/

/*!
    \fn QInstaller::PackageManagerPage::gui() const

    Returns the wizard this page belongs to.
*/

/*!
    \fn QInstaller::PackageManagerPage::isInterruptible() const

    Returns \c true if the installation can be interrupted.
*/

/*!
    \fn QInstaller::PackageManagerPage::settingsButtonRequested() const

    Returns \c true if the page requests the wizard to show the \uicontrol Settings button.
*/

/*!
    \fn QInstaller::PackageManagerPage::setSettingsButtonRequested(bool request)

    Determines that the page should request the \uicontrol Settings button if \a request is \c true.
*/

/*!
    \fn QInstaller::PackageManagerPage::entered()

    This signal is called when a page is entered.
*/

/*!
    \fn QInstaller::PackageManagerPage::left()

    This signal is called when a page is left.
*/

/*!
    \fn QInstaller::PackageManagerPage::entering()

    Called when end users enter the page and the PackageManagerGui:currentPageChanged()
    signal is triggered. Supports the QWizardPage::​initializePage() function to ensure
    that the page's fields are properly initialized based on fields from previous pages.
    Otherwise, \c initializePage() would only be called once if the installer has been
    set to QWizard::IndependentPages.
*/

/*!
    \fn QInstaller::PackageManagerPage::showOnPageListChanged()

    Called when page visibility on page list has changed and refresh is needed.
*/

/*!
    \fn QInstaller::PackageManagerPage::leaving()

    Called when end users leave the page and the PackageManagerGui:currentPageChanged()
    signal is triggered.
*/

/*!
    Constructs a package manager page with \a core as parent.
*/
PackageManagerPage::PackageManagerPage(PackageManagerCore *core)
    : m_complete(true)
    , m_titleColor(QString())
    , m_showOnPageList(true)
    , m_needsSettingsButton(false)
    , m_core(core)
    , validatorComponent(nullptr)
{
    if (!m_core->settings().titleColor().isEmpty())
        m_titleColor = m_core->settings().titleColor();

    if (!m_core->settings().wizardShowPageList())
        setPixmap(QWizard::WatermarkPixmap, wizardPixmap(scWatermark));

    setPixmap(QWizard::BannerPixmap, wizardPixmap(scBanner));
    setPixmap(QWizard::LogoPixmap, wizardPixmap(scLogo));

    // Can't use PackageManagerPage::gui() here as the page is not set yet
    if (PackageManagerGui *gui = qobject_cast<PackageManagerGui *>(core->guiObject())) {
        connect(this, &PackageManagerPage::showOnPageListChanged,
                gui, &PackageManagerGui::updatePageListWidget);
    }
}

/*!
    Returns the package manager core.
*/
PackageManagerCore *PackageManagerPage::packageManagerCore() const
{
    return m_core;
}

/*!
    Returns the pixmap specified by \a pixmapType. \a pixmapType can be \c <Banner>,
    \c <Logo> or \c <Watermark> element of the package information file. If @2x image
    is provided, returns that instead for high DPI displays.
*/
QPixmap PackageManagerPage::wizardPixmap(const QString &pixmapType) const
{
    QString pixmapStr = m_core->value(pixmapType);
    QInstaller::replaceHighDpiImage(pixmapStr);
    QPixmap pixmap(pixmapStr);
    if (pixmapType == scBanner) {
        if (!pixmap.isNull()) {
            int width;
            if (m_core->settings().containsValue(QLatin1String("WizardDefaultWidth")) )
                width = m_core->settings().wizardDefaultWidth();
            else
                width = size().width();
            pixmap = pixmap.scaledToWidth(width, Qt::SmoothTransformation);
        }
    }
    return pixmap;
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
    setTitle(QString::fromLatin1("<center><font color=\"%1\">%2</font></center>").arg(m_titleColor, title));
}

/*!
    Sets the font color of \a subTitle.
*/
void PackageManagerPage::setColoredSubTitle(const QString &subTitle)
{
    setSubTitle(QString::fromLatin1("<center><font color=\"%1\">%2</font></center>").arg(m_titleColor, subTitle));
}

/*!
    Sets the title shown on installer page indicator for this page to \a title.
    Pages that do not set this will use a fallback title instead.
*/
void PackageManagerPage::setPageListTitle(const QString &title)
{
    m_pageListTitle = title;
}

/*!
    Returns the title shown on installer page indicator for this page. If empty,
    a fallback title is being used instead.
*/
QString PackageManagerPage::pageListTitle() const
{
    return m_pageListTitle;
}

/*!
    Sets the page visibility on installer page indicator based on \a show.
    All pages are shown by default.
*/
void PackageManagerPage::setShowOnPageList(bool show)
{
    if (m_showOnPageList != show)
        emit showOnPageListChanged();

    m_showOnPageList = show;
}

/*!
    Returns \c true if the page should be shown on installer page indicator.
*/
bool PackageManagerPage::showOnPageList() const
{
    return m_showOnPageList;
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
        if (QAbstractButton *nextButton = w->button(QWizard::NextButton))
            nextButton->setFocus();
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
    \internal
*/
void PackageManagerPage::removeCustomWidget(const QWidget *widget)
{
    for (auto it = m_customWidgets.begin(); it != m_customWidgets.end();) {
        if (it.value() == widget)
            it = m_customWidgets.erase(it);
        else
            ++it;
    }
}
/*!
    Inserts \a widget at the position specified by \a offset in relation to
    another widget specified by \a siblingName. The default position is directly
    behind the sibling.
*/
void PackageManagerPage::insertWidget(QWidget *widget, const QString &siblingName, int offset)
{
    QWidget *sibling = findChild<QWidget *>(siblingName);
    QWidget *parent = sibling ? sibling->parentWidget() : nullptr;
    QLayout *layout = parent ? parent->layout() : nullptr;
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
        PackageManagerCore *const core = packageManagerCore();
        if (core->isUninstaller())
            return nextNextId;  // forcibly hide the license page if we run as uninstaller
        core->recalculateAllComponents();
        if (core->hasLicenses())
            return next;
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
    \fn QInstaller::IntroductionPage::packageManagerCoreTypeChanged()

    This signal is emitted when the package manager core type changes.
*/

/*!
    Constructs an introduction page with \a core as parent.
*/
IntroductionPage::IntroductionPage(PackageManagerCore *core)
    : PackageManagerPage(core)
    , m_updatesFetched(false)
    , m_allPackagesFetched(false)
    , m_forceUpdate(false)
    , m_offlineMaintenanceTool(false)
    , m_label(nullptr)
    , m_msgLabel(nullptr)
    , m_errorLabel(nullptr)
    , m_progressBar(nullptr)
    , m_packageManager(nullptr)
    , m_updateComponents(nullptr)
    , m_removeAllComponents(nullptr)
{
    setObjectName(QLatin1String("IntroductionPage"));
    setColoredTitle(tr("Welcome"));

    QVBoxLayout *layout = new QVBoxLayout(this);
    setLayout(layout);

    m_msgLabel = new QLabel(this);
    m_msgLabel->setWordWrap(true);
    m_msgLabel->setObjectName(QLatin1String("MessageLabel"));
    m_msgLabel->setText(tr("Welcome to the %1 Setup.").arg(productName()));

    QWidget *widget = new QWidget(this);
    QVBoxLayout *boxLayout = new QVBoxLayout(widget);

    m_packageManager = new QRadioButton(tr("&Add or remove components"), this);
    m_packageManager->setObjectName(QLatin1String("PackageManagerRadioButton"));
    boxLayout->addWidget(m_packageManager);
    connect(m_packageManager, &QAbstractButton::toggled, this, &IntroductionPage::setPackageManager);

    m_updateComponents = new QRadioButton(tr("&Update components"), this);
    m_updateComponents->setObjectName(QLatin1String("UpdaterRadioButton"));
    boxLayout->addWidget(m_updateComponents);
    connect(m_updateComponents, &QAbstractButton::toggled, this, &IntroductionPage::setUpdater);

    m_removeAllComponents = new QRadioButton(tr("&Remove all components"), this);
    m_removeAllComponents->setObjectName(QLatin1String("UninstallerRadioButton"));
    boxLayout->addWidget(m_removeAllComponents);
    connect(m_removeAllComponents, &QAbstractButton::toggled,
            this, &IntroductionPage::setUninstaller);
    connect(m_removeAllComponents, &QAbstractButton::toggled,
            core, &PackageManagerCore::setCompleteUninstallation);

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
    m_errorLabel->setTextFormat(Qt::RichText);
    m_errorLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
    boxLayout->addWidget(m_errorLabel);
    m_errorLabel->setObjectName(QLatin1String("ErrorLabel"));

    layout->addWidget(m_msgLabel);
    layout->addWidget(widget);
    layout->addItem(new QSpacerItem(20, 20, QSizePolicy::Minimum, QSizePolicy::Expanding));

    connect(core, &PackageManagerCore::metaJobProgress, this, &IntroductionPage::onProgressChanged);
    connect(core, &PackageManagerCore::metaJobTotalProgress, this, &IntroductionPage::setTotalProgress);
    connect(core, &PackageManagerCore::metaJobInfoMessage, this, &IntroductionPage::setMessage);
    connect(core, &PackageManagerCore::coreNetworkSettingsChanged,
            this, &IntroductionPage::onCoreNetworkSettingsChanged);

    m_updateComponents->setEnabled(!m_offlineMaintenanceTool && ProductKeyCheck::instance()->hasValidKey());

#ifdef Q_OS_WIN
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    if (QSysInfo::windowsVersion() >= QSysInfo::WV_WINDOWS7) {
        m_taskButton = new QWinTaskbarButton(this);
        connect(core, &PackageManagerCore::metaJobProgress,
                m_taskButton->progress(), &QWinTaskbarProgress::setValue);
    } else {
        m_taskButton = nullptr;
    }
#endif
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
    setErrorMessage(QString());

    bool isOfflineOnlyInstaller = core->isInstaller() && core->isOfflineOnly();
    // If not offline only installer, at least one valid repository needs to be available
    if (!isOfflineOnlyInstaller && !core->validRepositoriesAvailable()) {
        setErrorMessage(QLatin1String("<font color=\"red\">") + tr("At least one valid and enabled "
            "repository required for this action to succeed.") + QLatin1String("</font>"));
        return isComplete();
    }

    gui()->setSettingsButtonEnabled(false);
    if (core->isMaintainer()) {
        showAll();
        setMaintenanceToolsEnabled(false);
    } else {
        showMetaInfoUpdate();
    }

#ifdef Q_OS_WIN
#if QT_VERSION < QT_VERSION_CHECK(6, 0 ,0)
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
        if (!m_allPackagesFetched) {
            // first try to fetch the server side packages tree
            m_allPackagesFetched = core->fetchRemotePackagesTree();
            if (!m_allPackagesFetched) {
                QString error = core->error();
                if (core->status() == PackageManagerCore::ForceUpdate) {
                    // replaces the error string from packagemanagercore
                    error = tr("There is an important update available. Please select '%1' first")
                        .arg(m_updateComponents->text().remove(QLatin1Char('&')));

                    m_forceUpdate = true;
                    // Don't call these directly. Need to finish the current validation first,
                    // because changing the selection updates the binary marker and resets the
                    // complete -state of the page.
                    QMetaObject::invokeMethod(m_updateComponents, "setChecked",
                        Qt::QueuedConnection, Q_ARG(bool, true));
                    QMetaObject::invokeMethod(this, "setErrorMessage",
                        Qt::QueuedConnection, Q_ARG(QString, error));
                }
                setErrorMessage(error);
            }
        }

        if (m_allPackagesFetched)
            setComplete(true);
    }

    if (core->isMaintainer()) {
        showMaintenanceTools();
        setMaintenanceToolsEnabled(true);
    } else {
        hideAll();
    }
    gui()->setSettingsButtonEnabled(true);

#ifdef Q_OS_WIN
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    if (m_taskButton)
        m_taskButton->progress()->setVisible(!isComplete());
#endif
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
    uninstall components on the page. For a maintenance tool without any enabled
    repositories, the package manager and updater stay disabled regardless of
    the value of \a enable.
*/
void IntroductionPage::setMaintenanceToolsEnabled(bool enable)
{
    m_packageManager->setEnabled(enable && !m_offlineMaintenanceTool);
    m_updateComponents->setEnabled(enable && !m_offlineMaintenanceTool
        && ProductKeyCheck::instance()->hasValidKey());
    m_removeAllComponents->setEnabled(enable);
}

/*!
    Enables or disables the options to add or update components based on the
    value of \a enable. For a maintenance tool without any enabled repositories,
    the package manager and updater stay disabled regardless of the value of \a enable.
*/
void IntroductionPage::setMaintainerToolsEnabled(bool enable)
{
    m_packageManager->setEnabled(enable && !m_offlineMaintenanceTool);
    m_updateComponents->setEnabled(enable && !m_offlineMaintenanceTool
        && ProductKeyCheck::instance()->hasValidKey());
}

/*!
    Resets the internal page state, so that on clicking \uicontrol Next the metadata needs to be
    fetched again.
*/
void IntroductionPage::resetFetchedState()
{
    m_updatesFetched = false;
    m_allPackagesFetched = false;
    m_forceUpdate = false;
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
    m_progressBar->setValue(progress);
}

/*!
    Sets total \a totalProgress value to progress bar.
*/
void IntroductionPage::setTotalProgress(int totalProgress)
{
    if (m_progressBar)
        m_progressBar->setRange(0, totalProgress);
}

/*!
    Displays the error message \a error on the page.
*/
void IntroductionPage::setErrorMessage(const QString &error)
{
    QPalette palette;
    const PackageManagerCore::Status s = packageManagerCore()->status();
    if (s == PackageManagerCore::Failure) {
        palette.setColor(QPalette::WindowText, Qt::red);
    } else {
        palette.setColor(QPalette::WindowText, palette.color(QPalette::WindowText));
    }

    m_errorLabel->setText(error);
    m_errorLabel->setPalette(palette);

#ifdef Q_OS_WIN
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    if (m_taskButton) {
        m_taskButton->progress()->stop();
        m_taskButton->progress()->setValue(100);
    }
#endif
#endif
}


// -- private slots

void IntroductionPage::setUpdater(bool value)
{
    if (value) {
        entering();
        gui()->requestSettingsButtonByInstaller(true);
        packageManagerCore()->setUpdater();
        emit packageManagerCoreTypeChanged();

        gui()->updatePageListWidget();
    }
}

void IntroductionPage::setUninstaller(bool value)
{
    if (value) {
        entering();
        gui()->requestSettingsButtonByInstaller(true);
        packageManagerCore()->setUninstaller();
        emit packageManagerCoreTypeChanged();

        gui()->updatePageListWidget();
    }
}

void IntroductionPage::setPackageManager(bool value)
{
    if (value) {
        entering();
        gui()->requestSettingsButtonByInstaller(true);
        packageManagerCore()->setPackageManager();
        emit packageManagerCoreTypeChanged();

        gui()->updatePageListWidget();
    }
}
/*!
    Initializes the page.
*/
void IntroductionPage::initializePage()
{
    PackageManagerCore *core = packageManagerCore();
    const bool repositoriesAvailable = core->validRepositoriesAvailable();

    if (core->isPackageManager()) {
        m_packageManager->setChecked(true);
    } else if (core->isUpdater()) {
        m_updateComponents->setChecked(true);
    } else if (core->isUninstaller()) {
        // If we are running maintenance tool and the default uninstaller
        // marker is not overridden, set the default checked radio button
        // based on if we have valid repositories available.
        if (!core->isUserSetBinaryMarker() && repositoriesAvailable) {
            m_packageManager->setChecked(true);
        } else {
            // No repositories available, default to complete uninstallation.
            m_removeAllComponents->setChecked(true);
            core->setCompleteUninstallation(true);
        }
        // Disable options that are unusable without repositories
        m_offlineMaintenanceTool = !repositoriesAvailable;
        setMaintainerToolsEnabled(repositoriesAvailable);
    }
}

/*!
    Resets the internal page state, so that on clicking \uicontrol Next the metadata needs to be
    fetched again. For maintenance tool, enables or disables options requiring enabled repositories
    based on the current repository settings.
*/
void IntroductionPage::onCoreNetworkSettingsChanged()
{
    resetFetchedState();

    PackageManagerCore *core = packageManagerCore();
    if (core->isUninstaller() || core->isMaintainer()) {
        m_offlineMaintenanceTool = !core->validRepositoriesAvailable();

        setMaintainerToolsEnabled(!m_offlineMaintenanceTool);
        m_removeAllComponents->setChecked(m_offlineMaintenanceTool);
    }
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
    setButtonText(QWizard::CancelButton, tr("&Quit"));

    m_progressBar->setValue(0);
    m_progressBar->setRange(0, 0);
    PackageManagerCore *core = packageManagerCore();
    if (core->isUninstaller() || core->isMaintainer()) {
        showMaintenanceTools();
        setMaintenanceToolsEnabled(true);
    }
    if (m_forceUpdate)
        m_packageManager->setEnabled(false);

    setSettingsButtonRequested((!core->isOfflineOnly()));
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
    connect(m_licenseListWidget, &QListWidget::currentItemChanged,
        this, &LicenseAgreementPage::currentItemChanged);

    m_textBrowser = new QTextBrowser(this);
    m_textBrowser->setReadOnly(true);
    m_textBrowser->setOpenLinks(false);
    m_textBrowser->setOpenExternalLinks(true);
    m_textBrowser->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_textBrowser->setObjectName(QLatin1String("LicenseTextBrowser"));
    connect(m_textBrowser, &QTextBrowser::anchorClicked, this, &LicenseAgreementPage::openLicenseUrl);

    QSplitter *licenseSplitter = new QSplitter(this);
    licenseSplitter->setOrientation(Qt::Vertical);
    licenseSplitter->setChildrenCollapsible(false);
    licenseSplitter->addWidget(m_licenseListWidget);
    licenseSplitter->addWidget(m_textBrowser);
    licenseSplitter->setStretchFactor(0, 1);
    licenseSplitter->setStretchFactor(1, 3);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(licenseSplitter);

    m_acceptCheckBox = new QCheckBox(this);
    m_acceptCheckBox->setShortcut(QKeySequence(tr("Alt+A", "Agree license")));
    m_acceptCheckBox->setObjectName(QLatin1String("AcceptLicenseCheckBox"));
    ClickForwarder *acceptClickForwarder = new ClickForwarder(m_acceptCheckBox);

    m_acceptLabel = new QLabel;
    m_acceptLabel->setWordWrap(true);
    m_acceptLabel->installEventFilter(acceptClickForwarder);
    m_acceptLabel->setObjectName(QLatin1String("AcceptLicenseLabel"));
    m_acceptLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);

    QGridLayout *gridLayout = new QGridLayout;
    gridLayout->setColumnStretch(1, 1);
    gridLayout->addWidget(m_acceptCheckBox, 0, 0);
    gridLayout->addWidget(m_acceptLabel, 0, 1);
    layout->addLayout(gridLayout);

    connect(m_acceptCheckBox, &QAbstractButton::toggled, this, &QWizardPage::completeChanged);
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

    foreach (QInstaller::Component *component, packageManagerCore()->orderedComponentsToInstall())
        packageManagerCore()->addLicenseItem(component->licenses());

    createLicenseWidgets();

    const int licenseCount = m_licenseListWidget->count();
    if (licenseCount > 0) {
        m_licenseListWidget->setVisible(licenseCount > 1);
        m_licenseListWidget->setCurrentItem(m_licenseListWidget->item(0));
    }

    packageManagerCore()->clearLicenses();

    updateUi();
}

/*!
    Returns \c true if the accept license radio button is checked; otherwise,
    returns \c false.
*/
bool LicenseAgreementPage::isComplete() const
{
    return m_acceptCheckBox->isChecked() && ProductKeyCheck::instance()->hasAcceptedAllLicenses();
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

void LicenseAgreementPage::createLicenseWidgets()
{
    QHash<QString, QMap<QString, QString>> priorityHash = packageManagerCore()->sortedLicenses();

    QStringList priorities = priorityHash.keys();
    priorities.sort();

    for (int i = priorities.length() - 1; i >= 0; --i) {
        QString priority = priorities.at(i);
        QMap<QString, QString> licenses = priorityHash.value(priority);
        QStringList licenseNames = licenses.keys();
        licenseNames.sort(Qt::CaseInsensitive);
        for (QString licenseName : licenseNames) {
            QListWidgetItem *item = new QListWidgetItem(licenseName, m_licenseListWidget);
            item->setData(Qt::UserRole, licenses.value(licenseName));
        }
    }
}

void LicenseAgreementPage::updateUi()
{
    QString subTitleText;
    QString acceptButtonText;
    if (m_licenseListWidget->count() == 1) {
        subTitleText = tr("Please read the following license agreement. You must accept the terms "
                          "contained in this agreement before continuing with the installation.");
        acceptButtonText = tr("I accept the license.");
    } else {
        subTitleText = tr("Please read the following license agreements. You must accept the terms "
                          "contained in these agreements before continuing with the installation.");
        acceptButtonText = tr("I accept the licenses.");
    }
    m_licenseListWidget->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    setColoredSubTitle(subTitleText);

    m_acceptLabel->setText(acceptButtonText);

}

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
    , d(new ComponentSelectionPagePrivate(this, core))
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
        QT_TR_NOOP("Select the components to install. Deselect installed components to uninstall them.<br>Any components already installed will not be updated."),
        QT_TR_NOOP("Mandatory components need to be updated first before you can select other components to update.")
     };

    int index = 0;
    PackageManagerCore *core = packageManagerCore();
    if (core->isInstaller()) index = 1;
    if (core->isUninstaller()) index = 2;
    if (core->isPackageManager()) index = 3;
    if (core->foundEssentialUpdate() && core->isUpdater()) index = 4;
    setColoredSubTitle(tr(strings[index]));

    d->updateTreeView();

    // check component model state so we can enable needed component selection buttons
    d->onModelStateChanged(d->m_currentModel->checkedState());

    setModified(isComplete());
    d->showCategoryLayout(core->showRepositoryCategories());
    d->showCompressedRepositoryButton();
    d->showCreateOfflineInstallerButton(true);

    // Reset to default supplement state. The page may set it to OfflineGenerator
    // which needs to be reset after navigating back to the page.
    core->resetBinaryMarkerSupplement();
}

/*!
    Called when end users leave the page and the PackageManagerGui:currentPageChanged()
    signal is triggered.
*/
void ComponentSelectionPage::leaving()
{
    d->hideCompressedRepositoryButton();
    d->showCreateOfflineInstallerButton(false);
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
    Called when \c ComponentSelectionPage is validated.
    Tries to load \c component scripts for components about to be installed.
    Returns \c true if the script loading succeeded and the next page is shown.
*/
bool ComponentSelectionPage::validatePage()
{
    PackageManagerCore *core = packageManagerCore();
    try {
        core->loadComponentScripts(core->orderedComponentsToInstall(), true);
    } catch (const Error &error) {
        // As component script loading failed, there is error in the script and component is
        // marked as unselected. Recalculate so that unselected component is removed from install.
        // User is then able to select other components for install.
        core->clearComponentsToInstallCalculated();
        core->calculateComponentsToInstall();
        MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(), QLatin1String("Error"),
                                    tr("Error"), error.message());
        return false;
    }
    return true;
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
    d->m_core->selectComponent(id);
}

/*!
    Deselects the component with \a id in the component tree.
*/
void ComponentSelectionPage::deselectComponent(const QString &id)
{
    d->m_core->deselectComponent(id);
}

/*!
    Adds an additional virtual component with the \a name to be installed.

    Returns \c true if the virtual component is found and not installed.
*/
bool ComponentSelectionPage::addVirtualComponentToUninstall(const QString &name)
{
    PackageManagerCore *core = packageManagerCore();
    const QList<Component *> allComponents = core->components(PackageManagerCore::ComponentType::All);
    Component *component = PackageManagerCore::componentByName(
                name, allComponents);
    if (component && component->isInstalled() && component->isVirtual()) {
        component->setCheckState(Qt::Unchecked);
        core->recalculateAllComponents();
        qCDebug(QInstaller::lcDeveloperBuild) << "Virtual component " << name << " was selected for uninstall by script.";
        return true;
    }
    return false;
}

void ComponentSelectionPage::setAllowCreateOfflineInstaller(bool allow)
{
    d->setAllowCreateOfflineInstaller(allow);
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
    if (!d->componentsResolved())
        return false;

    return d->m_currentModel->componentsSelected();
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
    msgLabel->setText(tr("Please specify the directory where %1 will be installed.").arg(productName()));
    layout->addWidget(msgLabel);

    QHBoxLayout *hlayout = new QHBoxLayout;

    m_textChangeTimer.setSingleShot(true);
    m_textChangeTimer.setInterval(200);
    connect(&m_textChangeTimer, &QTimer::timeout, this, &QWizardPage::completeChanged);

    m_lineEdit = new QLineEdit(this);
    m_lineEdit->setObjectName(QLatin1String("TargetDirectoryLineEdit"));
    connect(m_lineEdit, &QLineEdit::textChanged,
            &m_textChangeTimer, static_cast<void (QTimer::*)()>(&QTimer::start));
    hlayout->addWidget(m_lineEdit);

    QPushButton *browseButton = new QPushButton(this);
    browseButton->setObjectName(QLatin1String("BrowseDirectoryButton"));
    connect(browseButton, &QAbstractButton::clicked, this, &TargetDirectoryPage::dirRequested);
    browseButton->setShortcut(QKeySequence(tr("Alt+R", "Browse file system to choose a file")));
    browseButton->setText(tr("B&rowse..."));
    browseButton->setToolTip(TargetDirectoryPage::tr("Browse file system to choose the installation directory."));
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
    Returns \c true if the target directory exists and has correct content.
*/
bool TargetDirectoryPage::validatePage()
{
    m_textChangeTimer.stop();

    if (!isComplete())
        return false;

    if (!isVisible())
        return true;

    const QString remove = packageManagerCore()->value(QLatin1String("RemoveTargetDir"));
    if (!QVariant(remove).toBool())
        return true;

    return this->packageManagerCore()->installationAllowedToDirectory(targetDir());
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
    m_warningLabel->setText(packageManagerCore()->targetDirWarning(targetDir()));
    return m_warningLabel->text().isEmpty();
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
        "shortcuts. You can also enter a name to create a new directory."));

    m_lineEdit = new QLineEdit(this);
    m_lineEdit->setText(core->value(scStartMenuDir, productName()));
    m_lineEdit->setObjectName(QLatin1String("StartMenuPathLineEdit"));

    startMenuPath = core->value(QLatin1String("UserStartMenuProgramsPath"));
    QStringList dirs = QDir(startMenuPath).entryList(QDir::AllDirs | QDir::NoDotAndDotDot);
    if (core->value(scAllUsers, scFalse) == scTrue) {
        startMenuPath = core->value(scAllUsersStartMenuProgramsPath);
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

    connect(m_listWidget, &QListWidget::currentItemChanged, this,
        &StartMenuDirectoryPage::currentItemChanged);
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
    updatePageListTitle();

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

    connect(core, &PackageManagerCore::installerBinaryMarkerChanged,
            this, &ReadyForInstallationPage::updatePageListTitle);
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
        m_msgLabel->setText(tr("All required information is now available to begin removing %1 from your computer.<br>"
            "<font color=\"red\">The program directory %2 will be deleted completely</font>, "
            "including all content in that directory!")
            .arg(productName(),
                QDir::toNativeSeparators(QDir(packageManagerCore()->value(scTargetDir))
            .absolutePath())));
        setComplete(true);
        return;
    } else if (packageManagerCore()->isMaintainer()) {
        setButtonText(QWizard::CommitButton, tr("U&pdate"));
        setColoredTitle(tr("Ready to Update Packages"));
        m_msgLabel->setText(tr("All required information is now available to begin updating your installation."));
    } else if (packageManagerCore()->isOfflineGenerator()) {
        setButtonText(QWizard::CommitButton, tr("Create Offline Installer"));
        setColoredTitle(tr("Ready to Create Offline Installer"));
        m_msgLabel->setText(tr("All required information is now available to create an offline installer for selected components."));
    } else {
        Q_ASSERT(packageManagerCore()->isInstaller());
        setButtonText(QWizard::CommitButton, tr("&Install"));
        setColoredTitle(tr("Ready to Install"));
        m_msgLabel->setText(tr("All required information is now available to begin installing %1 on your computer.")
            .arg(productName()));
    }

    bool componentsOk = packageManagerCore()->recalculateAllComponents();
    const QString htmlOutput = packageManagerCore()->componentResolveReasons();

    qCDebug(QInstaller::lcInstallerInstallLog).noquote() << htmlToString(htmlOutput);
    m_taskDetailsBrowser->setHtml(htmlOutput);
    m_taskDetailsBrowser->setVisible(!componentsOk || LoggingHandler::instance().isVerbose());
    setComplete(componentsOk);

    if (packageManagerCore()->checkAvailableSpace()) {
        m_msgLabel->setText(QString::fromLatin1("%1 %2").arg(m_msgLabel->text(), packageManagerCore()->availableSpaceMessage()));
    } else {
        m_msgLabel->setText(packageManagerCore()->availableSpaceMessage());
        setComplete(false);
    }
}

/*!
    Called when end users leave the page and the PackageManagerGui:currentPageChanged()
    signal is triggered.
*/
void ReadyForInstallationPage::leaving()
{
    setButtonText(QWizard::CommitButton, gui()->defaultButtonText(QWizard::CommitButton));
}

/*!
    Updates page list title based on installer binary type.
*/
void ReadyForInstallationPage::updatePageListTitle()
{
    PackageManagerCore *core = packageManagerCore();
    if (core->isOfflineGenerator())
        setPageListTitle(tr("Ready to Create Offline Installer"));
    else if (core->isInstaller())
        setPageListTitle(tr("Ready to Install"));
    else if (core->isMaintainer())
        setPageListTitle(tr("Ready to Update"));
    else if (core->isUninstaller())
        setPageListTitle(tr("Ready to Uninstall"));
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
    \fn QInstaller::PerformInstallationPage::isInterruptible() const

    Returns \c true if the installation can be interrupted.
*/

/*!
    \fn QInstaller::PerformInstallationPage::setAutomatedPageSwitchEnabled(bool request)

    Enables automatic switching of pages when \a request is \c true.
*/

/*!
    Constructs a perform installation page with \a core as parent. The page
    contains a PerformInstallationForm that defines the UI for the page.
*/
PerformInstallationPage::PerformInstallationPage(PackageManagerCore *core)
    : PackageManagerPage(core)
    , m_performInstallationForm(new PerformInstallationForm(core, this))
{
    setPixmap(QWizard::WatermarkPixmap, QPixmap());
    setObjectName(QLatin1String("PerformInstallationPage"));
    updatePageListTitle();

    m_performInstallationForm->setupUi(this);
    m_imageChangeTimer.setInterval(10000);

    connect(ProgressCoordinator::instance(), &ProgressCoordinator::detailTextChanged,
        m_performInstallationForm, &PerformInstallationForm::appendProgressDetails);
    connect(ProgressCoordinator::instance(), &ProgressCoordinator::detailTextResetNeeded,
        m_performInstallationForm, &PerformInstallationForm::clearDetailsBrowser);
    connect(m_performInstallationForm, &PerformInstallationForm::showDetailsChanged,
            this, &PerformInstallationPage::toggleDetailsWereChanged);

    connect(core, &PackageManagerCore::installationStarted,
            this, &PerformInstallationPage::installationStarted);
    connect(core, &PackageManagerCore::installationFinished,
            this, &PerformInstallationPage::installationFinished);

    connect(core, &PackageManagerCore::offlineGenerationStarted,
            this, &PerformInstallationPage::installationStarted);
    connect(core, &PackageManagerCore::offlineGenerationFinished,
            this, &PerformInstallationPage::installationFinished);

    connect(core, &PackageManagerCore::uninstallationStarted,
            this, &PerformInstallationPage::uninstallationStarted);
    connect(core, &PackageManagerCore::uninstallationFinished,
            this, &PerformInstallationPage::uninstallationFinished);

    connect(core, &PackageManagerCore::titleMessageChanged,
            this, &PerformInstallationPage::setTitleMessage);
    connect(this, &PerformInstallationPage::setAutomatedPageSwitchEnabled,
            core, &PackageManagerCore::setAutomatedPageSwitchEnabled);

    connect(core, &PackageManagerCore::installerBinaryMarkerChanged,
            this, &PerformInstallationPage::updatePageListTitle);

    connect(&m_imageChangeTimer, &QTimer::timeout,
            this, &PerformInstallationPage::changeCurrentImage);

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

    m_performInstallationForm->enableDetails();
    emit setAutomatedPageSwitchEnabled(true);

    changeCurrentImage();
    // No need to start the timer if we only have one, or no images
    if (packageManagerCore()->settings().productImages().count() > 1)
        m_imageChangeTimer.start();

    if (LoggingHandler::instance().isVerbose()) {
        m_performInstallationForm->toggleDetails();
    }
    if (packageManagerCore()->isUninstaller()) {
        setButtonText(QWizard::CommitButton, tr("U&ninstall"));
        setColoredTitle(tr("Uninstalling %1").arg(productName()));

        QTimer::singleShot(30, packageManagerCore(), SLOT(runUninstaller()));
    } else if (packageManagerCore()->isMaintainer()) {
        setButtonText(QWizard::CommitButton, tr("&Update"));
        setColoredTitle(tr("Updating components of %1").arg(productName()));

        QTimer::singleShot(30, packageManagerCore(), SLOT(runPackageUpdater()));
    } else if (packageManagerCore()->isOfflineGenerator()) {
        setButtonText(QWizard::CommitButton, tr("&Create Offline Installer"));
        setColoredTitle(tr("Creating Offline Installer for %1").arg(productName()));

        QTimer::singleShot(30, packageManagerCore(), SLOT(runOfflineGenerator()));
    } else {
        setButtonText(QWizard::CommitButton, tr("&Install"));
        setColoredTitle(tr("Installing %1").arg(productName()));

        QTimer::singleShot(30, packageManagerCore(), SLOT(runInstaller()));
    }
}

/*!
    Called when end users leave the page and the PackageManagerGui:currentPageChanged()
    signal is triggered.
*/
void PerformInstallationPage::leaving()
{
    setButtonText(QWizard::CommitButton, gui()->defaultButtonText(QWizard::CommitButton));
    m_imageChangeTimer.stop();
}

/*!
    Updates page list title based on installer binary type.
*/
void PerformInstallationPage::updatePageListTitle()
{
    PackageManagerCore *core = packageManagerCore();
    if (core->isOfflineGenerator())
        setPageListTitle(tr("Creating Offline Installer"));
    else if (core->isInstaller())
        setPageListTitle(tr("Installing"));
    else if (core->isMaintainer())
        setPageListTitle(tr("Updating"));
    else if (core->isUninstaller())
        setPageListTitle(tr("Uninstalling"));
}

// -- public slots

/*!
    Sets \a title as the title of the perform installation page.
*/
void PerformInstallationPage::setTitleMessage(const QString &title)
{
    setColoredTitle(title);
}

/*!
    Changes the currently shown product image to the next available
    image from installer configuration.
*/
void PerformInstallationPage::changeCurrentImage()
{
    const QMap<QString, QVariant> productImages = packageManagerCore()->settings().productImages();
    if (productImages.isEmpty())
        return;

    QString nextImage;
    QString nextUrl;
    if (m_currentImage.isEmpty() || m_currentImage == productImages.lastKey()) {
        nextImage = productImages.firstKey();
        nextUrl = productImages.value(nextImage).toString();
    } else {
        QMap<QString, QVariant>::const_iterator i = productImages.constFind(m_currentImage);
        if (++i != productImages.end()) {
            nextImage = i.key();
            nextUrl = i.value().toString();
        }
    }

    // Do not update the pixmap if there was only one image available
    if (nextImage != m_currentImage) {
        m_performInstallationForm->setImageFromFileName(nextImage, nextUrl);
        m_currentImage = nextImage;
    }
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
    , m_commitButton(nullptr)
{
    setObjectName(QLatin1String("FinishedPage"));
    setColoredTitle(tr("Finished the %1 Setup").arg(productName()));
    setPageListTitle(tr("Finished"));

    m_msgLabel = new QLabel(this);
    m_msgLabel->setWordWrap(true);
    m_msgLabel->setObjectName(QLatin1String("MessageLabel"));

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
    m_msgLabel->setText(tr("Click %1 to exit the %2 Setup.")
                        .arg(gui()->defaultButtonText(QWizard::FinishButton).remove(QLatin1Char('&')))
                        .arg(productName()));

    if (m_commitButton) {
        disconnect(m_commitButton, &QAbstractButton::clicked, this, &FinishedPage::handleFinishClicked);
        m_commitButton = nullptr;
    }

    if (packageManagerCore()->isMaintainer()) {
#ifdef Q_OS_MACOS
        gui()->setOption(QWizard::NoCancelButton, false);
#endif
        if (QAbstractButton *cancel = gui()->button(QWizard::CancelButton)) {
            m_commitButton = cancel;
            cancel->setEnabled(true);
            cancel->setVisible(true);
            // we don't use the usual FinishButton so we need to connect the misused CancelButton
            connect(cancel, &QAbstractButton::clicked, gui(), &PackageManagerGui::finishButtonClicked);
            connect(cancel, &QAbstractButton::clicked, packageManagerCore(), &PackageManagerCore::finishButtonClicked);
            // for the moment we don't want the rejected signal connected
            disconnect(gui(), &QDialog::rejected, packageManagerCore(), &PackageManagerCore::setCanceled);

            connect(gui()->button(QWizard::CommitButton), &QAbstractButton::clicked,
                    this, &FinishedPage::cleanupChangedConnects);
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
        disconnect(m_commitButton, &QAbstractButton::clicked, this, &FinishedPage::handleFinishClicked);
        connect(m_commitButton, &QAbstractButton::clicked, this, &FinishedPage::handleFinishClicked);
    }

    if (packageManagerCore()->status() == PackageManagerCore::Success
            || packageManagerCore()->status() == PackageManagerCore::EssentialUpdated) {
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
        setColoredTitle(tr("The %1 Setup failed.").arg(productName()));
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
#ifdef Q_OS_MACOS
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
    if (m_runItCheckBox->isChecked())
        packageManagerCore()->runProgram();
}

/*!
    Removes changed connects from the page.
*/
void FinishedPage::cleanupChangedConnects()
{
    if (QAbstractButton *cancel = gui()->button(QWizard::CancelButton)) {
        // remove the workaround connect from entering page
        disconnect(cancel, &QAbstractButton::clicked, gui(), &PackageManagerGui::finishButtonClicked);
        disconnect(cancel, &QAbstractButton::clicked, packageManagerCore(), &PackageManagerCore::finishButtonClicked);
        connect(gui(), &QDialog::rejected, packageManagerCore(), &PackageManagerCore::setCanceled);

        disconnect(gui()->button(QWizard::CommitButton), &QAbstractButton::clicked,
                   this, &FinishedPage::cleanupChangedConnects);
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
    \fn QInstaller::RestartPage::restart()

    This signal is emitted when the installer is restarted.
*/

/*!
    Constructs a restart installation page with \a core as parent.
*/
RestartPage::RestartPage(PackageManagerCore *core)
    : PackageManagerPage(core)
{
    setObjectName(QLatin1String("RestartPage"));
    setColoredTitle(tr("Finished the %1 Setup").arg(productName()));

    // Never show this page on the page list
    setShowOnPageList(false);
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
