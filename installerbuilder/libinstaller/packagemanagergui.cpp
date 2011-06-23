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
#include "packagemanagergui.h"

#include "component.h"
#include "componentmodel.h"
#include "messageboxhandler.h"
#include "packagemanagercore.h"
#include "qinstallerglobal.h"
#include "progresscoordinator.h"
#include "performinstallationform.h"
#include "settings.h"

#include "common/errors.h"
#include "common/utils.h"
#include "common/fileutils.h"

#include <KDToolsCore/KDByteSize>
#include <KDToolsCore/KDSysInfo>

#include <QtCore/QDir>
#include <QtCore/QDynamicPropertyChangeEvent>
#include <QtCore/QPair>
#include <QtCore/QProcess>
#include <QtCore/QRegExp>
#include <QtCore/QSettings>
#include <QtCore/QTimer>

#include <QtGui/QApplication>
#include <QtGui/QCheckBox>
#include <QtGui/QDesktopServices>
#include <QtGui/QFileDialog>
#include <QtGui/QGridLayout>
#include <QtGui/QFormLayout>
#include <QtGui/QHBoxLayout>
#include <QtGui/QHeaderView>
#include <QtGui/QLabel>
#include <QtGui/QLineEdit>
#include <QtGui/QListWidget>
#include <QtGui/QListWidgetItem>
#include <QtGui/QMessageBox>
#include <QtGui/QProgressBar>
#include <QtGui/QPushButton>
#include <QtGui/QRadioButton>
#include <QtGui/QTextBrowser>
#include <QtGui/QTreeWidget>
#include <QtGui/QTreeView>
#include <QtGui/QVBoxLayout>
#include <QtGui/QScrollBar>

#include <QtScript/QScriptEngine>

using namespace QInstaller;

/*
TRANSLATOR QInstaller::PackageManagerCore;
*/
/*
TRANSLATOR QInstaller::PackageManagerGui
*/
/*
TRANSLATOR QInstaller::PackageManagerPage
*/
/*
TRANSLATOR QInstaller::IntroductionPage
*/
/*
TRANSLATOR QInstaller::LicenseAgreementPage
*/
/*
TRANSLATOR QInstaller::ComponentSelectionPage
*/
/*
TRANSLATOR QInstaller::TargetDirectoryPage
*/
/*
TRANSLATOR QInstaller::StartMenuDirectoryPage
*/
/*
TRANSLATOR QInstaller::ReadyForInstallationPage
*/
/*
TRANSLATOR QInstaller::PerformInstallationPage
*/
/*
TRANSLATOR QInstaller::FinishedPage
*/


class DynamicInstallerPage : public PackageManagerPage
{
public:
    explicit DynamicInstallerPage(QWidget* widget, PackageManagerCore *core = 0)
        : PackageManagerPage(core)
        , m_widget(widget)
    {
        setObjectName(QLatin1String("Dynamic") + widget->objectName());
        setPixmap(QWizard::LogoPixmap, logoPixmap());
        setPixmap(QWizard::WatermarkPixmap, QPixmap());

        setLayout(new QVBoxLayout);
        setTitle(widget->windowTitle());
        m_widget->setProperty("complete", true);
        m_widget->setProperty("final", false);
        widget->installEventFilter(this);
        layout()->addWidget(widget);
    }

    QWidget* widget() const
    {
        return m_widget;
    }

    bool isComplete() const
    {
        return m_widget->property("complete").toBool();
    }

protected:
    bool eventFilter(QObject* obj, QEvent* event)
    {
        if (obj == m_widget) {
            switch(event->type()) {
            case QEvent::WindowTitleChange:
                setTitle(m_widget->windowTitle());
                break;

            case QEvent::DynamicPropertyChange:
                emit completeChanged();
                if (m_widget->property("final").toBool() != isFinalPage())
                    setFinalPage(m_widget->property("final").toBool());
                break;

            default:
                break;
            }
        }
        return PackageManagerPage::eventFilter(obj, event);
    }

private:
    QWidget *const m_widget;
};


// -- PackageManagerGui::Private

class PackageManagerGui::Private
{
public:
    Private()
        : m_modified(false)
        , m_autoSwitchPage(true)
    { }

    bool m_modified;
    bool m_autoSwitchPage;
    QMap<int, QWizardPage*> m_defaultPages;
    QMap<int, QString> m_defaultButtonText;

    QScriptValue m_controlScript;
    QScriptEngine m_controlScriptEngine;
};


// -- PackageManagerGui

QScriptEngine* PackageManagerGui::controlScriptEngine() const
{
    return &d->m_controlScriptEngine;
}

/*!
    \class QInstaller::PackageManagerGui
    Is the "gui" object in a none interactive installation
*/
PackageManagerGui::PackageManagerGui(PackageManagerCore *core, QWidget *parent)
    : QWizard(parent)
    , d(new Private)
    , m_core(core)
{
    if (m_core->isInstaller())
        setWindowTitle(tr("%1 Setup").arg(m_core->value(scTitle)));
    else
        setWindowTitle(tr("%1").arg(m_core->value(scMaintenanceTitle)));

#ifndef Q_WS_MAC
    setWindowIcon(QIcon(m_core->settings().icon()));
    setWizardStyle(QWizard::ModernStyle);
#else
    setPixmap(QWizard::BackgroundPixmap, m_core->settings().background());
#endif
    setOption(QWizard::NoBackButtonOnStartPage);
    setOption(QWizard::NoBackButtonOnLastPage);
    setLayout(new QVBoxLayout(this));

    connect(this, SIGNAL(rejected()), m_core, SLOT(setCanceled()));
    connect(this, SIGNAL(interrupted()), m_core, SLOT(interrupt()));

    // both queued to show the finished page once everything is done
    connect(m_core, SIGNAL(installationFinished()), this, SLOT(showFinishedPage()), Qt::QueuedConnection);
    connect(m_core, SIGNAL(uninstallationFinished()), this, SLOT(showFinishedPage()), Qt::QueuedConnection);

    connect(this, SIGNAL(currentIdChanged(int)), this, SLOT(slotCurrentPageChanged(int)));
    connect(this, SIGNAL(currentIdChanged(int)), m_core, SIGNAL(currentPageChanged(int)));
    connect(button(QWizard::FinishButton), SIGNAL(clicked()), this, SIGNAL(finishButtonClicked()));
    connect(button(QWizard::FinishButton), SIGNAL(clicked()), m_core, SIGNAL(finishButtonClicked()));

    // make sure the QUiLoader's retranslateUi is executed first, then the script
    connect(this, SIGNAL(languageChanged()), m_core, SLOT(languageChanged()), Qt::QueuedConnection);
    connect(this, SIGNAL(languageChanged()), this, SLOT(onLanguageChanged()), Qt::QueuedConnection);

    connect(m_core, SIGNAL(wizardPageInsertionRequested(QWidget*, QInstaller::PackageManagerCore::WizardPage)),
        this, SLOT(wizardPageInsertionRequested(QWidget*, QInstaller::PackageManagerCore::WizardPage)));
    connect(m_core, SIGNAL(wizardPageRemovalRequested(QWidget*)),this,
        SLOT(wizardPageRemovalRequested(QWidget*)));
    connect(m_core, SIGNAL(wizardWidgetInsertionRequested(QWidget*, QInstaller::PackageManagerCore::WizardPage)),
        this, SLOT(wizardWidgetInsertionRequested(QWidget*, QInstaller::PackageManagerCore::WizardPage)));
    connect(m_core, SIGNAL(wizardWidgetRemovalRequested(QWidget*)), this,
        SLOT(wizardWidgetRemovalRequested(QWidget*)));
    connect(m_core, SIGNAL(wizardPageVisibilityChangeRequested(bool, int)), this,
        SLOT(wizardPageVisibilityChangeRequested(bool, int)), Qt::QueuedConnection);

    connect(m_core, SIGNAL(setAutomatedPageSwitchEnabled(bool)), this,
        SLOT(setAutomatedPageSwitchEnabled(bool)));

    for (int i = QWizard::BackButton; i < QWizard::CustomButton1; ++i)
        d->m_defaultButtonText.insert(i, buttonText(QWizard::WizardButton(i)));

#ifdef Q_WS_MAC
    resize(sizeHint() * 1.25);
#else
    resize(sizeHint());
#endif
}

PackageManagerGui::~PackageManagerGui()
{
    delete d;
}

void PackageManagerGui::setAutomatedPageSwitchEnabled(bool request)
{
    d->m_autoSwitchPage = request;
}

QString PackageManagerGui::defaultButtonText(int wizardButton) const
{
    return d->m_defaultButtonText.value(wizardButton);
}

void PackageManagerGui::clickButton(int wb, int delay)
{
    if (QAbstractButton* b = button(static_cast<QWizard::WizardButton>(wb) )) {
        QTimer::singleShot(delay, b, SLOT(click()));
    } else {
        // TODO: we should probably abort immediately here (faulty test script)
        verbose() << "Button " << wb << " not found!" << std::endl;
    }
}

/*!
    Loads a script to perform the installation non-interactively.
    @throws QInstaller::Error if the script is not readable/cannot be parsed
*/
void PackageManagerGui::loadControlScript(const QString& scriptPath)
{
    QFile file(scriptPath);
    if (!file.open(QIODevice::ReadOnly)) {
        throw Error(QObject::tr("Could not open the requested script file at %1: %2")
            .arg(scriptPath, file.errorString()));
    }

    d->m_controlScriptEngine.globalObject().setProperty(QLatin1String("installer"),
        d->m_controlScriptEngine.newQObject(m_core));
    d->m_controlScriptEngine.globalObject().setProperty(QLatin1String("gui"),
        d->m_controlScriptEngine.newQObject(this));
    d->m_controlScriptEngine.globalObject().setProperty(QLatin1String("packagemanagergui"),
        d->m_controlScriptEngine.newQObject(this));
    registerMessageBox(&d->m_controlScriptEngine);

#undef REGISTER_BUTTON
#define REGISTER_BUTTON(x)    buttons.setProperty(QLatin1String(#x), \
    d->m_controlScriptEngine.newVariant(static_cast<int>(QWizard::x)));

    QScriptValue buttons = d->m_controlScriptEngine.newArray();
    REGISTER_BUTTON(BackButton)
    REGISTER_BUTTON(NextButton)
    REGISTER_BUTTON(CommitButton)
    REGISTER_BUTTON(FinishButton)
    REGISTER_BUTTON(CancelButton)
    REGISTER_BUTTON(HelpButton)
    REGISTER_BUTTON(CustomButton1)
    REGISTER_BUTTON(CustomButton2)
    REGISTER_BUTTON(CustomButton3)

#undef REGISTER_BUTTON

    d->m_controlScriptEngine.globalObject().setProperty(QLatin1String("buttons"), buttons);

    d->m_controlScriptEngine.evaluate(QLatin1String(file.readAll()), scriptPath);
    if (d->m_controlScriptEngine.hasUncaughtException()) {
        throw Error(QObject::tr("Exception while loading the control script %1")
            .arg(uncaughtExceptionString(&(d->m_controlScriptEngine)/*, scriptPath*/)));
    }

    QScriptValue comp = d->m_controlScriptEngine.evaluate(QLatin1String("Controller"));
    if (d->m_controlScriptEngine.hasUncaughtException()) {
        throw Error(QObject::tr("Exception while loading the control script %1")
            .arg(uncaughtExceptionString(&(d->m_controlScriptEngine)/*, scriptPath*/)));
    }

    d->m_controlScript = comp;
    d->m_controlScript.construct();

    verbose() << "Loaded control script " << qPrintable(scriptPath) << std::endl;
}

void PackageManagerGui::triggerControlScriptForCurrentPage()
{
    slotCurrentPageChanged(currentId());
}

void PackageManagerGui::slotCurrentPageChanged(int id)
{
    QMetaObject::invokeMethod(this, "delayedControlScriptExecution", Qt::QueuedConnection,
        Q_ARG(int, id));
}

void PackageManagerGui::callControlScriptMethod(const QString& methodName)
{
    QScriptValue method = d->m_controlScript.property(QLatin1String("prototype")).property(methodName);

    if (!method.isValid()) {
        verbose() << "Control script callback " << qPrintable(methodName) << " does not exist."
            << std::endl;
        return;
    }

    verbose() << "Calling control script callback " << qPrintable(methodName) << std::endl;

    method.call(d->m_controlScript);

    if (d->m_controlScriptEngine.hasUncaughtException()) {
        qCritical()
            << uncaughtExceptionString(&(d->m_controlScriptEngine) /*, QLatin1String("control script")*/);
        // TODO: handle error
    }
}

void PackageManagerGui::delayedControlScriptExecution(int id)
{
    if (PackageManagerPage *const p = qobject_cast<PackageManagerPage*> (page(id)))
        callControlScriptMethod(p->objectName() + QLatin1String("Callback"));
}

void PackageManagerGui::onLanguageChanged()
{
    d->m_defaultButtonText.clear();
    for (int i = QWizard::BackButton; i < QWizard::CustomButton1; ++i)
        d->m_defaultButtonText.insert(i, buttonText(QWizard::WizardButton(i)));
}

bool PackageManagerGui::event(QEvent* event)
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

void PackageManagerGui::wizardPageInsertionRequested(QWidget* widget,
    QInstaller::PackageManagerCore::WizardPage page)
{
    // just in case it was already in there...
    wizardPageRemovalRequested(widget);

    // now find a suitable ID lower than page
    int p = static_cast<int>(page) - 1;
    while (QWizard::page(p) != 0)
        --p;
    // add it
    setPage(p, new DynamicInstallerPage(widget, m_core));
}

void PackageManagerGui::wizardPageRemovalRequested(QWidget* widget)
{
    const QList<int> pages = pageIds();
    for (QList<int>::const_iterator it = pages.begin(); it != pages.end(); ++it) {
        QWizardPage* const p = page(*it);
        DynamicInstallerPage* const dynamicPage = dynamic_cast<DynamicInstallerPage*>(p);
        if (dynamicPage == 0)
            continue;
        if (dynamicPage->widget() != widget)
            continue;
        removePage(*it);
    }
}

void PackageManagerGui::wizardWidgetInsertionRequested(QWidget* widget,
    QInstaller::PackageManagerCore::WizardPage page)
{
    Q_ASSERT(widget);
    if (QWizardPage* const p = QWizard::page(page))
        p->layout()->addWidget(widget);
}

void PackageManagerGui::wizardWidgetRemovalRequested(QWidget* widget)
{
    Q_ASSERT(widget);
    widget->setParent(0);
}

void PackageManagerGui::wizardPageVisibilityChangeRequested(bool visible, int p)
{
    if (visible && page(p) == 0) {
        setPage(p, d->m_defaultPages[p]);
    } else if (!visible && page(p) != 0) {
        d->m_defaultPages[p] = page(p);
        removePage(p);
    }
}

PackageManagerPage *PackageManagerGui::page(int pageId) const
{
    return qobject_cast<PackageManagerPage*> (QWizard::page(pageId));
}

QWidget* PackageManagerGui::pageWidgetByObjectName(const QString& name) const
{
    const QList<int> ids = pageIds();
    foreach (const int i, ids) {
        PackageManagerPage *const p = qobject_cast<PackageManagerPage*> (page(i));
        if (p && p->objectName() == name) {
            // For dynamic pages, return the contained widget (as read from the UI file), not the
            // wrapper page
            if (DynamicInstallerPage* dp = dynamic_cast<DynamicInstallerPage*>(p))
                return dp->widget();
            return p;
        }
    }
    verbose() << "No page found for object name " << name << std::endl;
    return 0;
}

QWidget* PackageManagerGui::currentPageWidget() const
{
    return currentPage();
}

void PackageManagerGui::cancelButtonClicked()
{
    if (currentId() != PackageManagerCore::InstallationFinished) {
        PackageManagerPage *const page = qobject_cast<PackageManagerPage*> (currentPage());
        if (page && page->isInterruptible()) {
            const QMessageBox::StandardButton bt =
                MessageBoxHandler::question(MessageBoxHandler::currentBestSuitParent(),
                QLatin1String("cancelInstallation"), tr("Question"),
                tr("Do you want to abort the %1 process?").arg(m_core->isUninstaller() ? tr("uninstallation")
                    : tr("installation")), QMessageBox::Yes | QMessageBox::No);
            if (bt == QMessageBox::Yes)
                emit interrupted();
        } else {
            QString app = tr("installer");
            if (m_core->isUninstaller())
                app = tr("uninstaller");
            if (m_core->isUpdater() || m_core->isPackageManager())
                app = tr("maintenance");

            const QMessageBox::StandardButton bt =
                MessageBoxHandler::question(MessageBoxHandler::currentBestSuitParent(),
                QLatin1String("cancelInstallation"), tr("Question"),
                tr("Do you want to abort the %1 application?").arg(app), QMessageBox::Yes | QMessageBox::No);
            if (bt == QMessageBox::Yes)
                QDialog::reject();
        }
    } else {
        QDialog::reject();
    }
}

void PackageManagerGui::rejectWithoutPrompt()
{
    m_core->setCanceled();
    QDialog::reject();
}

void PackageManagerGui::reject()
{
    cancelButtonClicked();
}

void PackageManagerGui::setModified(bool value)
{
    d->m_modified = value;
}

void PackageManagerGui::showFinishedPage()
{
    verbose() << "SHOW FINISHED PAGE" << std::endl;
    if (d->m_autoSwitchPage)
        next();
    else
        qobject_cast<QPushButton*>(button(QWizard::CancelButton))->setEnabled(false);
}


// -- PackageManagerPage

PackageManagerPage::PackageManagerPage(PackageManagerCore *core)
    : m_fresh(true)
    , m_complete(true)
    , m_core(core)
{
    // otherwise the colors will screw up
    setSubTitle(QLatin1String(" "));
}

PackageManagerCore *PackageManagerPage::packageManagerCore() const
{
    return m_core;
}

QPixmap PackageManagerPage::watermarkPixmap() const
{
    return QPixmap(m_core->value(QLatin1String("WatermarkPixmap")));
}

QPixmap PackageManagerPage::logoPixmap() const
{
    return QPixmap(m_core->value(QLatin1String("LogoPixmap")));
}

QString PackageManagerPage::productName() const
{
    return m_core->value(QLatin1String("ProductName"));
}

bool PackageManagerPage::isComplete() const
{
    return m_complete;
}

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

QWidget *PackageManagerPage::findWidget(const QString &objectName) const
{
    return findChild<QWidget*> (objectName);
}

/*!
    \reimp
    \Overwritten to support some kind of initializePage() in the case the wizard has been set
    to QWizard::IndependentPages. If that option has been set, initializePage() would be only called
    once. So we provide entering() and leaving() based on this overwritten function.
*/
void PackageManagerPage::setVisible(bool visible)
{
    QWizardPage::setVisible(visible);
    qApp->processEvents();

    if (m_fresh && !visible) {
        // this is only hit once when the page gets added to the wizard
        m_fresh = false;
        return;
    }

    if (visible)
        entering();
    else
        leaving();
}

int PackageManagerPage::nextId() const
{
    return QWizardPage::nextId();
}


// -- IntroductionPage

IntroductionPage::IntroductionPage(PackageManagerCore *core)
    : PackageManagerPage(core)
    , m_widget(0)
{
    setObjectName(QLatin1String("IntroductionPage"));
    setTitle(tr("Setup - %1").arg(productName()));
    setPixmap(QWizard::WatermarkPixmap, watermarkPixmap());
    setSubTitle(QString());

    m_msgLabel = new QLabel(this);
    m_msgLabel->setObjectName(QLatin1String("MessageLabel"));
    m_msgLabel->setWordWrap(true);
    m_msgLabel->setText(tr("Welcome to the %1 Setup Wizard.").arg(productName()));

    QVBoxLayout *layout = new QVBoxLayout(this);
    setLayout(layout);
    layout->addWidget(m_msgLabel);
    layout->addItem(new QSpacerItem(20, 20, QSizePolicy::Minimum, QSizePolicy::Expanding));
}

void IntroductionPage::setText(const QString &text)
{
    m_msgLabel->setText(text);
}

void IntroductionPage::setWidget(QWidget *w)
{
    if (m_widget) {
        layout()->removeWidget(m_widget);
        delete m_widget;
    }
    m_widget = w;
    if (m_widget)
        static_cast<QVBoxLayout*>(layout())->addWidget(m_widget, 1);
}


// -- LicenseAgreementPage::ClickForwarder

class LicenseAgreementPage::ClickForwarder : public QObject
{
    Q_OBJECT

public:
    explicit ClickForwarder(QAbstractButton* button)
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
    QAbstractButton* m_abstractButton;
};


// -- LicenseAgreementPage

LicenseAgreementPage::LicenseAgreementPage(PackageManagerCore *core)
    : PackageManagerPage(core)
{
    setTitle(tr("License Agreement"));
    setSubTitle(tr("Please read the following license agreement(s). You must accept the terms contained "
        "in these agreement(s) before continuing with the installation."));

    setPixmap(QWizard::LogoPixmap, logoPixmap());
    setPixmap(QWizard::WatermarkPixmap, QPixmap());
    setObjectName(QLatin1String("LicenseAgreementPage"));

    m_licenseListWidget = new QListWidget(this);
    connect(m_licenseListWidget, SIGNAL(currentItemChanged(QListWidgetItem *, QListWidgetItem *)),
        this, SLOT(currentItemChanged(QListWidgetItem *)));

    QSizePolicy sizePolicy(QSizePolicy::Maximum, QSizePolicy::Expanding);
    sizePolicy.setHeightForWidth(m_licenseListWidget->sizePolicy().hasHeightForWidth());
    m_licenseListWidget->setSizePolicy(sizePolicy);

    m_textBrowser = new QTextBrowser(this);
    m_textBrowser->setReadOnly(true);
    m_textBrowser->setOpenLinks(false);
    m_textBrowser->setOpenExternalLinks(true);
    connect(m_textBrowser, SIGNAL(anchorClicked(QUrl)), this, SLOT(openLicenseUrl(QUrl)));

    QSizePolicy sizePolicy2(QSizePolicy::Minimum, QSizePolicy::Expanding);
    sizePolicy2.setHeightForWidth(m_textBrowser->sizePolicy().hasHeightForWidth());
    m_textBrowser->setSizePolicy(sizePolicy2);

    QHBoxLayout *licenseBoxLayout = new QHBoxLayout();
    licenseBoxLayout->addWidget(m_licenseListWidget);
    licenseBoxLayout->addWidget(m_textBrowser);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addLayout(licenseBoxLayout);

    m_acceptRadioButton = new QRadioButton(this);
    m_acceptRadioButton->setObjectName(QString::fromUtf8("acceptLicenseRB"));

    m_acceptRadioButton->setShortcut(QKeySequence(tr("Alt+A", "agree license")));
    QLabel *acceptLabel = new QLabel(tr("I h<u>a</u>ve read and agree to the following terms contained in "
        "the license agreements accompanying the Qt SDK and additional items. I agree that my use of "
        "the Qt SDK is governed by the terms and conditions contained in these license agreements."));
    acceptLabel->setWordWrap(true);
    ClickForwarder* acceptClickForwarder = new ClickForwarder(m_acceptRadioButton);
    acceptLabel->installEventFilter(acceptClickForwarder);

    m_rejectRadioButton = new QRadioButton(this);
    m_rejectRadioButton->setObjectName(QString::fromUtf8("rejectLicenseRB"));

    m_rejectRadioButton->setShortcut(QKeySequence(tr("Alt+D", "do not agree license")));
    QLabel *rejectLabel = new QLabel(tr("I <u>d</u>o not accept the terms and conditions of the above "
        "listed license agreements. Please note by checking the box, you must cancel the "
        "installation or downloading the Qt SDK and must destroy all copies, or portions thereof, "
        "of the Qt SDK in your possessions."));
    rejectLabel->setWordWrap(true);
    ClickForwarder* rejectClickForwarder = new ClickForwarder(m_rejectRadioButton);
    rejectLabel->installEventFilter(rejectClickForwarder);

    QSizePolicy sizePolicy3(QSizePolicy::Preferred, QSizePolicy::Minimum);
    sizePolicy3.setHeightForWidth(rejectLabel->sizePolicy().hasHeightForWidth());
    sizePolicy3.setVerticalStretch(0);
    sizePolicy3.setHorizontalStretch(0);

    acceptLabel->setSizePolicy(sizePolicy3);
    rejectLabel->setSizePolicy(sizePolicy3);

#if defined(Q_WS_X11) || defined(Q_WS_MAC)
    QFont labelFont(font());
    labelFont.setPixelSize(9);

    acceptLabel->setFont(labelFont);
    rejectLabel->setFont(labelFont);
#endif

    QGridLayout *gridLayout = new QGridLayout;
    gridLayout->setColumnStretch(1, 1);
    gridLayout->addWidget(m_acceptRadioButton, 0, 0);
    gridLayout->addWidget(acceptLabel, 0, 1);
    gridLayout->addWidget(m_rejectRadioButton, 1, 0);
    gridLayout->addWidget(rejectLabel, 1, 1);
    layout->addLayout(gridLayout);

    connect(m_acceptRadioButton, SIGNAL(toggled(bool)), this, SIGNAL(completeChanged()));
    connect(m_rejectRadioButton, SIGNAL(toggled(bool)), this, SIGNAL(completeChanged()));

    m_rejectRadioButton->setChecked(true);
}

void LicenseAgreementPage::entering()
{
    m_licenseListWidget->clear();
    m_licenseListWidget->setVisible(false);
    m_textBrowser->setText(QLatin1String(""));

    RunMode runMode = packageManagerCore()->runMode();
    QList<QInstaller::Component*> components = packageManagerCore()->components(false, runMode);

    QInstaller::Component *rootComponent = 0;
    // TODO: this needs to be fixed once we support several root components
    foreach (QInstaller::Component* root, components) {
        if (root->isInstalled())
            continue;

        const QHash<QString, QPair<QString, QString> > &hash = root->licenses();
        if (!hash.isEmpty()) {
            addLicenseItem(hash);
            rootComponent = root;
        }
    }

    components = packageManagerCore()->componentsToInstall(runMode);
    foreach (QInstaller::Component* component, components) {
        if (rootComponent != component && !component->isInstalled())
            addLicenseItem(component->licenses());
    }

    const int licenseCount = m_licenseListWidget->count();
    if (licenseCount > 0) {
        m_licenseListWidget->setVisible(licenseCount > 1);
        m_licenseListWidget->setCurrentItem(m_licenseListWidget->item(0));
    }
}

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


// -- ComponentSelectionPage::Private

class ComponentSelectionPage::Private : public QObject
{
    Q_OBJECT

public:
    Private(ComponentSelectionPage *qq, PackageManagerCore *core)
        : q(qq)
        , m_core(core)
        , m_treeView(new QTreeView(q))
        , m_allModel(new ComponentModel(4, m_core))
        , m_updaterModel(new ComponentModel(4, m_core))
    {
        m_treeView->setObjectName(QLatin1String("ComponentTreeView"));

        int i = 0;
        m_currentModel = m_allModel;
        ComponentModel *list[] = { m_allModel, m_updaterModel, 0 };
        while (ComponentModel *model = list[i++]) {
            connect(model, SIGNAL(defaultCheckStateChanged(bool)), q, SLOT(setModified(bool)));
            model->setHeaderData(ComponentModelHelper::NameColumn, Qt::Horizontal, tr("Name"));
            model->setHeaderData(ComponentModelHelper::InstalledVersionColumn, Qt::Horizontal,
                tr("Installed Version"));
            model->setHeaderData(ComponentModelHelper::NewVersionColumn, Qt::Horizontal, tr("New Version"));
            model->setHeaderData(ComponentModelHelper::UncompressedSizeColumn, Qt::Horizontal, tr("Size"));
        }

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

        QPushButton *button;
        hlayout = new QHBoxLayout;
        if (m_core->isInstaller()) {
            hlayout->addWidget(button = new QPushButton(tr("Default")));
            connect(button, SIGNAL(clicked()), this, SLOT(selectDefault()));
        }
        hlayout->addWidget(button = new QPushButton(tr("Select All")));
        connect(button, SIGNAL(clicked()), this, SLOT(selectAll()));
        hlayout->addWidget(button = new QPushButton(tr("Deselect All")));
        connect(button, SIGNAL(clicked()), this, SLOT(deselectAll()));
        hlayout->addSpacerItem(new QSpacerItem(1, 1, QSizePolicy::MinimumExpanding,
            QSizePolicy::MinimumExpanding));
        layout->addLayout(hlayout);

        connect(m_core, SIGNAL(finishAllComponentsReset()), this, SLOT(allComponentsChanged()),
            Qt::QueuedConnection);
        connect(m_core, SIGNAL(finishUpdaterComponentsReset()), this, SLOT(updaterComponentsChanged()),
            Qt::QueuedConnection);
    }

    void updateTreeView()
    {
        m_currentModel = m_core->isUpdater() ? m_updaterModel : m_allModel;
        m_treeView->setModel(m_currentModel);

        if (m_core->isInstaller()) {
            m_treeView->setHeaderHidden(true);
            for (int i = 1; i < m_currentModel->columnCount(); ++i)
                m_treeView->hideColumn(i);
        } else {
            m_treeView->header()->setStretchLastSection(true);
            for (int i = 0; i < m_currentModel->columnCount(); ++i)
                m_treeView->resizeColumnToContents(i);
        }

        bool hasChildren = false;
        const int rowCount = m_currentModel->rowCount();
        for (int row = 0; row < rowCount && !hasChildren; ++row)
            hasChildren = m_currentModel->hasChildren(m_currentModel->index(row, 0));
        m_treeView->setRootIsDecorated(hasChildren);

        disconnect(m_treeView->selectionModel(), SIGNAL(currentChanged(QModelIndex, QModelIndex)),
            this, SLOT(currentChanged(QModelIndex)));
        connect(m_treeView->selectionModel(), SIGNAL(currentChanged(QModelIndex, QModelIndex)),
            this, SLOT(currentChanged(QModelIndex)));

        m_treeView->setCurrentIndex(m_currentModel->index(0, 0));
        m_treeView->setExpanded(m_currentModel->index(0, 0), true);
    }

public slots:
    void currentChanged(const QModelIndex &current)
    {
        m_sizeLabel->clear();
        m_descriptionLabel->clear();

        if (current.isValid()) {
            m_descriptionLabel->setText(m_currentModel->data(m_currentModel->index(current.row(),
                ComponentModelHelper::NameColumn, current.parent()), Qt::ToolTipRole).toString());
            if (!m_core->isUninstaller()) {
                m_sizeLabel->setText(tr("This component will occupy approximately %1 on your "
                    "hard disk.").arg(m_currentModel->data(m_currentModel->index(current.row(),
                    ComponentModelHelper::UncompressedSizeColumn, current.parent())).toString()));
            }
        }
    }

    void selectAll()
    {
        m_currentModel->selectAll();
    }

    void deselectAll()
    {
        m_currentModel->deselectAll();
    }

    void selectDefault()
    {
        m_currentModel->selectDefault();
    }

private slots:
    void allComponentsChanged()
    {
        m_allModel->setRootComponents(m_core->components(false, AllMode));
    }

    void updaterComponentsChanged()
    {
        m_updaterModel->setRootComponents(m_core->components(false, UpdaterMode));
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
};


// -- ComponentSelectionPage

/*!
    \class QInstaller::ComponentSelectionPage
    On this page the user can select and deselect what he wants to be installed.
*/
ComponentSelectionPage::ComponentSelectionPage(PackageManagerCore *core)
    : PackageManagerPage(core)
    , d(new Private(this, core))
{
    setTitle(tr("Select Components"));
    setPixmap(QWizard::LogoPixmap, logoPixmap());
    setPixmap(QWizard::WatermarkPixmap, QPixmap());
    setObjectName(QLatin1String("ComponentSelectionPage"));

    if (core->isInstaller())
        setSubTitle(tr("Please select the components you want to install."));
    if (core->isUninstaller())
        setSubTitle(tr("Please select the components you want to uninstall."));

    if (core->isUpdater())
        setSubTitle(tr("Please select the components you want to update."));
    if (core->isPackageManager())
        setSubTitle(tr("Please (de)select the components you want to (un)install."));
}

ComponentSelectionPage::~ComponentSelectionPage()
{
    delete d;
}

void ComponentSelectionPage::entering()
{
    d->updateTreeView();
    setModified(isComplete());
}

void ComponentSelectionPage::selectAll()
{
    d->selectAll();
}

void ComponentSelectionPage::deselectAll()
{
    d->deselectAll();
}

void ComponentSelectionPage::selectDefault()
{
    if (packageManagerCore()->isInstaller())
        d->selectDefault();
}

/*!
    Selects the component with /a id in the component tree.
*/
void ComponentSelectionPage::selectComponent(const QString& id)
{
    const QModelIndex &idx = d->m_currentModel->indexFromComponentName(id);
    if (idx.isValid())
        d->m_currentModel->setData(idx, Qt::Checked, Qt::CheckStateRole);
}

/*!
    Deselects the component with /a id in the component tree.
*/
void ComponentSelectionPage::deselectComponent(const QString& id)
{
    const QModelIndex &idx = d->m_currentModel->indexFromComponentName(id);
    if (idx.isValid())
        d->m_currentModel->setData(idx, Qt::Unchecked, Qt::CheckStateRole);
}

void ComponentSelectionPage::setModified(bool modified)
{
    setComplete(modified);
}

bool ComponentSelectionPage::isComplete() const
{
    if (packageManagerCore()->isInstaller() || packageManagerCore()->isUpdater())
        return d->m_currentModel->hasCheckedComponents();
    return !d->m_currentModel->defaultCheckState();
}


// -- TargetDirectoryPage

TargetDirectoryPage::TargetDirectoryPage(PackageManagerCore *core)
    : PackageManagerPage(core)
{
    setObjectName(QLatin1String("TargetDirectoryPage"));
    setTitle(tr("Installation Folder"));
    setPixmap(QWizard::LogoPixmap, logoPixmap());
    setPixmap(QWizard::WatermarkPixmap, QPixmap());

    QLabel *msgLabel = new QLabel(this);
    msgLabel->setText(tr("Please specify the folder where %1 will be installed.").arg(productName()));
    msgLabel->setWordWrap(true);
    msgLabel->setObjectName(QLatin1String("MessageLabel"));

    m_lineEdit = new QLineEdit(this);
    m_lineEdit->setObjectName(QLatin1String("targetDirectoryLE"));

    QPushButton *browseButton = new QPushButton(this);
    browseButton->setObjectName(QLatin1String("BrowseButton"));
    browseButton->setText(tr("Browse..."));

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(msgLabel);
    QHBoxLayout *hlayout = new QHBoxLayout;
    hlayout->addWidget(m_lineEdit);
    hlayout->addWidget(browseButton);
    layout->addLayout(hlayout);
    setLayout(layout);

    connect(browseButton, SIGNAL(clicked()), this, SLOT(dirRequested()));
    connect(m_lineEdit, SIGNAL(textChanged(QString)), this, SIGNAL(completeChanged()));
}

QString TargetDirectoryPage::targetDir() const
{
    return m_lineEdit->text();
}

void TargetDirectoryPage::setTargetDir(const QString &dirName)
{
    m_lineEdit->setText(dirName);
}

void TargetDirectoryPage::initializePage()
{
    QString targetDir = packageManagerCore()->value(scTargetDir);
    if (targetDir.isEmpty()) {
        targetDir = QDir::homePath() + QDir::separator();
        // prevent spaces in the default target directory
        if (targetDir.contains(QLatin1Char(' ')))
            targetDir = QDir::rootPath();
        targetDir += productName().remove(QLatin1Char(' '));
    }
    m_lineEdit->setText(QDir::toNativeSeparators(QDir(targetDir).absolutePath()));

    PackageManagerPage::initializePage();
}

bool TargetDirectoryPage::validatePage()
{
    if (targetDir().isEmpty()) {
        MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(),
            QLatin1String("forbiddenTargetDirectory"), tr("Error"),
            tr("The install directory cannot be empty, please specify a valid folder"), QMessageBox::Ok);
        return false;
    }

    const QDir dir(targetDir());
    // it exists, but is empty (might be created by the Browse button (getExistingDirectory)
    if (dir.exists() && dir.entryList(QDir::NoDotAndDotDot).isEmpty()) {
        return true;
    } else if (dir.exists() && dir.isReadable()) {
        // it exists, but is not empty
        if (dir == QDir::root()) {
            MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(),
                QLatin1String("forbiddenTargetDirectory"), tr("Error"),
                tr("As the install directory is completely deleted installing in %1 is forbidden")
                .arg(QDir::rootPath()), QMessageBox::Ok);
            return false;
        }

        if (!QVariant(packageManagerCore()->value(scRemoveTargetDir)).toBool())
            return true;

        return MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(),
            QLatin1String("overwriteTargetDirectory"), tr("Warning"),
            tr("You have selected an existing, non-empty folder for installation.\n"
            "Note that it will be completely wiped on uninstallation of this application.\n"
            "It is not advisable to install into this folder as installation might fail.\n"
            "Do you want to continue?"), QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes;
    }
    return true;
}

void TargetDirectoryPage::entering()
{
}

void TargetDirectoryPage::leaving()
{
    packageManagerCore()->setValue(scTargetDir, targetDir());
}

void TargetDirectoryPage::targetDirSelected()
{
}

void TargetDirectoryPage::dirRequested()
{
    QString newDirName = QFileDialog::getExistingDirectory(this, tr("Select Installation Folder"),
        targetDir());
    if (newDirName.isEmpty() || newDirName == targetDir())
        return;
    m_lineEdit->setText(QDir::toNativeSeparators(newDirName));
}


// -- StartMenuDirectoryPage

StartMenuDirectoryPage::StartMenuDirectoryPage(PackageManagerCore *core)
    : PackageManagerPage(core)
{
    setTitle(tr("Start Menu shortcuts"));
    setPixmap(QWizard::LogoPixmap, logoPixmap());
    setPixmap(QWizard::WatermarkPixmap, QPixmap());
    setObjectName(QLatin1String("StartMenuDirectoryPage"));
    setSubTitle(tr("Select the Start Menu in which you would like to create the program's shortcuts. You can "
        "also enter a name to create a new folder."));

    m_lineEdit = new QLineEdit(this);
    m_lineEdit->setObjectName(QLatin1String("LineEdit"));

    QString startMenuDir = core->value(scStartMenuDir);
    if (startMenuDir.isEmpty())
        startMenuDir = productName();
    m_lineEdit->setText(startMenuDir);

    // grab existing start menu folders
    QSettings user(QLatin1String("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\"
        "Explorer\\User Shell Folders"), QSettings::NativeFormat);
    // User Shell Folders uses %USERPROFILE%
    startMenuPath = replaceWindowsEnvironmentVariables(user.value(QLatin1String("Programs"),
        QString()).toString());
    core->setValue(QLatin1String("DesktopDir"), replaceWindowsEnvironmentVariables(user
        .value(QLatin1String("Desktop")).toString()));

    QDir dir(startMenuPath); // user only dirs
    QStringList dirs = dir.entryList(QDir::AllDirs | QDir::NoDotAndDotDot);

    if (core->value(QLatin1String("AllUsers")) == QLatin1String("true")) {
        verbose() << "AllUsers set. Using HKEY_LOCAL_MACHINE" << std::endl;
        QSettings system(QLatin1String("HKEY_LOCAL_MACHINE\\Software\\Microsoft\\Windows\\"
            "CurrentVersion\\Explorer\\Shell Folders"), QSettings::NativeFormat);
        startMenuPath = system.value(QLatin1String("Common Programs"), QString()).toString();
        core->setValue(QLatin1String("DesktopDir"),system.value(QLatin1String("Desktop")).toString());

        dir.setPath(startMenuPath); // system only dirs
        dirs += dir.entryList(QDir::AllDirs | QDir::NoDotAndDotDot);
    }

    verbose() << "StartMenuPath: " << startMenuPath;
    verbose() << "DesktopDir:" << core->value(QLatin1String("DesktopDir")) << std::endl;

    m_listWidget = new QListWidget(this);
    if (!dirs.isEmpty()) {
        dirs.removeDuplicates();
        foreach (const QString &dir, dirs)
            new QListWidgetItem(dir, m_listWidget);
    }

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(m_lineEdit);
    layout->addWidget(m_listWidget);

    setLayout(layout);

    connect(m_listWidget, SIGNAL(currentItemChanged(QListWidgetItem*, QListWidgetItem*)), this,
        SLOT(currentItemChanged(QListWidgetItem*)));
}

QString StartMenuDirectoryPage::startMenuDir() const
{
    return m_lineEdit->text();
}

void StartMenuDirectoryPage::setStartMenuDir(const QString &startMenuDir)
{
    m_lineEdit->setText(startMenuDir);
}

void StartMenuDirectoryPage::leaving()
{
    packageManagerCore()->setValue(scStartMenuDir, startMenuPath + QDir::separator() + startMenuDir());
}

void StartMenuDirectoryPage::currentItemChanged(QListWidgetItem* current)
{
    if (current) {
        QString dir = current->data(Qt::DisplayRole).toString();
        if (!dir.isEmpty())
            dir += QDir::separator();
        setStartMenuDir(dir + packageManagerCore()->value(scStartMenuDir));
    }
}


// -- ReadyForInstallationPage

inline QString unitSizeText(const qint64 size)
{
    if (size < 10000)
        return QString::number(size) + QLatin1Char(' ') + ReadyForInstallationPage::tr("Bytes");

    if (size < 1024 * 10000)
        return QString::number(size / 1024) + QLatin1Char(' ') + ReadyForInstallationPage::tr("kBytes");

    return QString::number(size / 1024 / 1024) + QLatin1Char(' ') + ReadyForInstallationPage::tr("MBytes");
}

ReadyForInstallationPage::ReadyForInstallationPage(PackageManagerCore *core)
    : PackageManagerPage(core)
    , msgLabel(new QLabel)
{
    setPixmap(QWizard::LogoPixmap, logoPixmap());
    setPixmap(QWizard::WatermarkPixmap, QPixmap());

    setObjectName(QLatin1String("ReadyForInstallationPage"));

    msgLabel->setWordWrap(true);
    msgLabel->setObjectName(QLatin1String("MessageLabel"));

    QLayout *layout = new QVBoxLayout(this);
    layout->addWidget(msgLabel);
    setLayout(layout);
}

/*!
    \reimp
*/
void ReadyForInstallationPage::entering()
{
    setCommitPage(true);
    const QString target = packageManagerCore()->value(scTargetDir);

    if (packageManagerCore()->isUninstaller()) {
        setTitle(tr("Ready to Uninstall"));
        setButtonText(QWizard::CommitButton, tr("U&ninstall"));
        msgLabel->setText(tr("Setup is now ready to begin removing %1 from your computer.\nThe "
            "program dir %2 will be deleted completely, including all content in that directory!")
            .arg(productName(), QDir::toNativeSeparators(QDir(target).absolutePath())));
        return;
    } else if (packageManagerCore()->isPackageManager() || packageManagerCore()->isUpdater()) {
        setTitle(tr("Ready to Update Packages"));
        setButtonText(QWizard::CommitButton, tr("U&pdate"));
        msgLabel->setText(tr("Setup is now ready to begin updating your installation."));
    } else {
        Q_ASSERT(packageManagerCore()->isInstaller());
        setTitle(tr("Ready to Install"));
        setButtonText(QWizard::CommitButton, tr("&Install"));
        msgLabel->setText(tr("Setup is now ready to begin installing %1 on your computer.")
            .arg(productName()));
    }

    const KDSysInfo::Volume vol = KDSysInfo::Volume::fromPath(target);
    const KDSysInfo::Volume tempVolume =
        KDSysInfo::Volume::fromPath(QInstaller::generateTemporaryFileName());
    const bool tempOnSameVolume = vol == tempVolume;

    // there is no better way atm to check this
    if (vol.size().size() == 0 && vol.availableSpace().size() == 0) {
        verbose() << "Could not determine available space on device " << target
            << ". Continue silently." << std::endl;
        return;
    }

    const KDByteSize required(packageManagerCore()->requiredDiskSpace());
    const KDByteSize tempRequired(packageManagerCore()->requiredTemporaryDiskSpace());

    const KDByteSize available = vol.availableSpace();
    const KDByteSize tempAvailable = tempVolume.availableSpace();
    const KDByteSize realRequiredTempSpace = KDByteSize(0.1 * tempRequired + tempRequired);
    const KDByteSize realRequiredSpace = KDByteSize(2 * required);

    const bool tempInstFailure = tempOnSameVolume && available < realRequiredSpace
        + realRequiredTempSpace;

    verbose() << "Disk space check on " << target << ": required=" << required.size()
        << " available=" << available.size() << " size=" << vol.size().size() << std::endl;

    QString tempString;
    if (tempAvailable <  realRequiredTempSpace || tempInstFailure) {
        if (tempOnSameVolume) {
            tempString = tr("Not enough disk space to store temporary files and the installation, "
                "at least %1 are required").arg(unitSizeText(realRequiredTempSpace + realRequiredSpace));
        } else {
            tempString = tr("Not enough disk space to store temporary files, at least %1 are required")
                .arg(unitSizeText(realRequiredTempSpace.size()));
        }
    }

    // error on not enough space
    if (available < required || tempInstFailure) {
        if (tempOnSameVolume) {
            msgLabel->setText(tempString);
        } else {
            msgLabel->setText(tr("The volume you selected for installation has insufficient space "
                "for the selected components.\n\nThe installation requires approximately %1.")
                .arg(required.toString()) + tempString);
        }
        setCommitPage(false);
    } else if (available - required < 0.01 * vol.size()) {
        // warn for less than 1% of the volume's space being free
        msgLabel->setText(tr("The volume you selected for installation seems to have sufficient space for "
            "installation,\nbut there will be less than 1% of the volume's space available afterwards.\n\n%1")
            .arg(msgLabel->text()));
    } else if (available - required < 100*1024*1024LL) {
        // warn for less than 100MB being free
        msgLabel->setText(tr("The volume you selected for installation seems to have sufficient space for "
            "installation,\nbut there will be less than 100 MB available afterwards.\n\n%1")
            .arg(msgLabel->text()));
    }
}

void ReadyForInstallationPage::leaving()
{
    setButtonText(QWizard::CommitButton, gui()->defaultButtonText(QWizard::CommitButton));
}

/*!
    \reimp
*/
bool ReadyForInstallationPage::isComplete() const
{
    return isCommitPage();
}


// -- PerformInstallationPage

/*!
    \class QInstaller::PerformInstallationPage
    On this page the user can see on a progress bar how far the current installation is.
*/
PerformInstallationPage::PerformInstallationPage(PackageManagerCore *core)
    : PackageManagerPage(core)
    , m_performInstallationForm(new PerformInstallationForm(this))
{
    setPixmap(QWizard::LogoPixmap, logoPixmap());
    setPixmap(QWizard::WatermarkPixmap, QPixmap());

    setObjectName(QLatin1String("PerformInstallationPage"));

    m_performInstallationForm->setupUi(this);

    connect(ProgressCoordninator::instance(), SIGNAL(detailTextChanged(QString)),
        m_performInstallationForm, SLOT(appendProgressDetails(QString)));
    connect(ProgressCoordninator::instance(), SIGNAL(detailTextResetNeeded()),
        m_performInstallationForm, SLOT(clearDetailsBrowser()));

    connect(m_performInstallationForm, SIGNAL(showDetailsChanged()), this,
        SLOT(toggleDetailsWereChanged()));

    connect(core, SIGNAL(installationStarted()), this, SLOT(installationStarted()));
    connect(core, SIGNAL(uninstallationStarted()), this, SLOT(installationStarted()));
    connect(core, SIGNAL(installationFinished()), this, SLOT(installationFinished()));
    connect(core, SIGNAL(uninstallationFinished()), this, SLOT(installationFinished()));
    connect(core, SIGNAL(titleMessageChanged(QString)), this, SLOT(setTitleMessage(QString)));
    connect(this, SIGNAL(setAutomatedPageSwitchEnabled(bool)), core,
        SIGNAL(setAutomatedPageSwitchEnabled(bool)));
}

PerformInstallationPage::~PerformInstallationPage()
{
    delete m_performInstallationForm;
}

bool PerformInstallationPage::isAutoSwitching() const
{
    return !m_performInstallationForm->isShowingDetails();
}

// -- protected

void PerformInstallationPage::entering()
{
    setComplete(false);
    setCommitPage(true);

    const QString productName = packageManagerCore()->value(QLatin1String("ProductName"));
    if (packageManagerCore()->isUninstaller()) {
        setTitle(tr("Uninstalling %1").arg(productName));
        setButtonText(QWizard::CommitButton, tr("&Uninstall"));
        m_performInstallationForm->setDetailsWidgetVisible(false);
        QTimer::singleShot(30, packageManagerCore(), SLOT(runUninstaller()));
    } else if (packageManagerCore()->isPackageManager() || packageManagerCore()->isUpdater()) {
        setButtonText(QWizard::CommitButton, tr("&Update"));
        m_performInstallationForm->setDetailsWidgetVisible(true);
        setTitle(tr("Updating components of %1").arg(productName));
        QTimer::singleShot(30, packageManagerCore(), SLOT(runPackageUpdater()));
    } else {
        setTitle(tr("Installing %1").arg(productName));
        setButtonText(QWizard::CommitButton, tr("&Install"));
        m_performInstallationForm->setDetailsWidgetVisible(true);
        QTimer::singleShot(30, packageManagerCore(), SLOT(runInstaller()));
    }

    m_performInstallationForm->enableDetails();
    emit setAutomatedPageSwitchEnabled(true);
}

void PerformInstallationPage::leaving()
{
    setButtonText(QWizard::CommitButton, gui()->defaultButtonText(QWizard::CommitButton));
}

// -- public slots

void PerformInstallationPage::setTitleMessage(const QString& title)
{
    setTitle(title);
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

void PerformInstallationPage::toggleDetailsWereChanged()
{
    emit setAutomatedPageSwitchEnabled(isAutoSwitching());
}


// -- FinishedPage

FinishedPage::FinishedPage(PackageManagerCore *core)
    : PackageManagerPage(core)
{
    setObjectName(QLatin1String("FinishedPage"));
    setTitle(tr("Completing the %1 Wizard").arg(productName()));
    setPixmap(QWizard::WatermarkPixmap, watermarkPixmap());
    setSubTitle(QString());

    m_msgLabel = new QLabel(this);
    m_msgLabel->setWordWrap(true);
    m_msgLabel->setObjectName(QLatin1String("MessageLabel"));

#ifdef Q_WS_MAC
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
}

void FinishedPage::entering()
{
    verbose() << "FINISHED ENTERING: " << std::endl;

    setCommitPage(true);
    if (packageManagerCore()->isUpdater() || packageManagerCore()->isPackageManager()) {
#ifdef Q_WS_MAC
        wizard()->setOption(QWizard::NoCancelButton, false);
#endif
        gui()->button(QWizard::CancelButton)->setEnabled(true);
        setButtonText(QWizard::CommitButton, gui()->defaultButtonText(QWizard::NextButton));
        setButtonText(QWizard::CancelButton, gui()->defaultButtonText(QWizard::FinishButton));
    } else {
        wizard()->setOption(QWizard::NoCancelButton, true);
    }

    if (QAbstractButton *commit = wizard()->button(QWizard::CommitButton)) {
        disconnect(commit, SIGNAL(clicked()), this, SLOT(handleFinishClicked()));
        connect(commit, SIGNAL(clicked()), this, SLOT(handleFinishClicked()));
    }

    if (packageManagerCore()->status() == PackageManagerCore::Success) {
        const QString finishedtext = packageManagerCore()->value(QLatin1String("FinishedText"));
        if (!finishedtext.isEmpty())
            m_msgLabel->setText(finishedtext);

        if (!packageManagerCore()->value(scRunProgram).isEmpty()) {
            m_runItCheckBox->show();
            m_runItCheckBox->setText(packageManagerCore()->value(scRunProgramDescription, tr("Run %1 now.")
                .arg(productName())));
            return; // job done
        }
    } else {
        setTitle(tr("The %1 Wizard failed.").arg(productName()));
    }

    m_runItCheckBox->hide();
    m_runItCheckBox->setChecked(false);
}

void FinishedPage::leaving()
{
#ifdef Q_WS_MAC
    wizard()->setOption(QWizard::NoCancelButton, true);
#endif

    setButtonText(QWizard::CommitButton, gui()->defaultButtonText(QWizard::CommitButton));
    setButtonText(QWizard::CancelButton, gui()->defaultButtonText(QWizard::CancelButton));

    if (QAbstractButton *commit = wizard()->button(QWizard::CommitButton))
        disconnect(commit, SIGNAL(clicked()), this, SLOT(handleFinishClicked()));
    disconnect(wizard(), SIGNAL(customButtonClicked(int)), this, SLOT(handleFinishClicked()));
}

void FinishedPage::handleFinishClicked()
{
    QString program = packageManagerCore()->value(scRunProgram);
    if (!m_runItCheckBox->isChecked() || program.isEmpty())
        return;
    program = packageManagerCore()->replaceVariables(program);
    verbose() << "STARTING " << program << std::endl;
    QProcess::startDetached(program);
}


// -- RestartPage

RestartPage::RestartPage(PackageManagerCore *core)
    : PackageManagerPage(core)
{
    setObjectName(QLatin1String("RestartPage"));
    setTitle(tr("Completing the %1 Setup Wizard").arg(productName()));
    setPixmap(QWizard::WatermarkPixmap, watermarkPixmap());
    setSubTitle(QString());

    setFinalPage(false);
    setCommitPage(false);
}

int RestartPage::nextId() const
{
    return PackageManagerCore::Introduction;
}

void RestartPage::entering()
{
    if (QAbstractButton *finish = wizard()->button(QWizard::FinishButton))
        finish->setVisible(false);

    wizard()->restart();
    emit restart();
}

void RestartPage::leaving()
{
}

#include "packagemanagergui.moc"
#include "moc_packagemanagergui.cpp"
