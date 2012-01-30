/**************************************************************************
**
** This file is part of Qt SDK**
**
** Copyright (c) 2012 Nokia Corporation and/or its subsidiary(-ies).*
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

#include <kdsysinfo.h>

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
#include <QtGui/QShowEvent>

#include <QtScript/QScriptEngine>

using namespace KDUpdater;
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


static QString humanReadableSize(quint64 intSize)
{
    QString unit;
    double size;

    if (intSize < 1024 * 1024) {
        size = 1. + intSize / 1024.;
        unit = QObject::tr("kB");
    } else if (intSize < 1024 * 1024 * 1024) {
        size = 1. + intSize / 1024. / 1024.;
        unit = QObject::tr("MB");
    } else {
        size = 1. + intSize / 1024. / 1024. / 1024.;
        unit = QObject::tr("GB");
    }

    size = qRound(size * 10) / 10.0;
    return QString::fromLatin1("%L1 %2").arg(size, 0, 'g', 4).arg(unit);
}


class DynamicInstallerPage : public PackageManagerPage
{
public:
    explicit DynamicInstallerPage(QWidget *widget, PackageManagerCore *core = 0)
        : PackageManagerPage(core)
        , m_widget(widget)
    {
        setObjectName(QLatin1String("Dynamic") + widget->objectName());
        setPixmap(QWizard::LogoPixmap, logoPixmap());
        setPixmap(QWizard::WatermarkPixmap, QPixmap());

        setLayout(new QVBoxLayout);
        setSubTitle(QString());
        setTitle(widget->windowTitle());
        m_widget->setProperty("complete", true);
        m_widget->setProperty("final", false);
        widget->installEventFilter(this);
        layout()->addWidget(widget);
    }

    QWidget *widget() const
    {
        return m_widget;
    }

    bool isComplete() const
    {
        return m_widget->property("complete").toBool();
    }

protected:
    bool eventFilter(QObject *obj, QEvent *event)
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
        , m_showSettingsButton(false)
    { }

    bool m_modified;
    bool m_autoSwitchPage;
    bool m_showSettingsButton;
    QMap<int, QWizardPage*> m_defaultPages;
    QMap<int, QString> m_defaultButtonText;

    QScriptValue m_controlScript;
    QScriptEngine m_controlScriptEngine;
};


// -- PackageManagerGui

QScriptEngine *PackageManagerGui::controlScriptEngine() const
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
#else
    setPixmap(QWizard::BackgroundPixmap, m_core->settings().background());
#endif
#ifdef Q_OS_LINUX
    setWizardStyle(QWizard::ModernStyle);
    setSizeGripEnabled(true);
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

    connect(this, SIGNAL(customButtonClicked(int)), this, SLOT(customButtonClicked(int)));

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
    if (QAbstractButton *b = button(static_cast<QWizard::WizardButton>(wb) )) {
        QTimer::singleShot(delay, b, SLOT(click()));
    } else {
        // TODO: we should probably abort immediately here (faulty test script)
        qDebug() << "Button" << wb << "not found!";
    }
}

/*!
    Loads a script to perform the installation non-interactively.
    @throws QInstaller::Error if the script is not readable/cannot be parsed
*/
void PackageManagerGui::loadControlScript(const QString &scriptPath)
{
    QFile file(scriptPath);
    if (!file.open(QIODevice::ReadOnly)) {
        throw Error(QObject::tr("Could not open the requested script file at %1: %2")
            .arg(scriptPath, file.errorString()));
    }

    QScriptValue installerObject = d->m_controlScriptEngine.newQObject(m_core);
    installerObject.setProperty(QLatin1String("componentByName"), d->m_controlScriptEngine
        .newFunction(qInstallerComponentByName, 1));

    d->m_controlScriptEngine.globalObject().setProperty(QLatin1String("installer"),
        installerObject);
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

    qDebug() << "Loaded control script" << scriptPath;
}

void PackageManagerGui::slotCurrentPageChanged(int id)
{
    QMetaObject::invokeMethod(this, "delayedControlScriptExecution", Qt::QueuedConnection,
        Q_ARG(int, id));
}

void PackageManagerGui::callControlScriptMethod(const QString &methodName)
{
    if (!d->m_controlScript.isValid())
        return;

    QScriptValue method = d->m_controlScript.property(QLatin1String("prototype")).property(methodName);

    if (!method.isValid()) {
        qDebug() << "Control script callback" << methodName << "does not exist.";
        return;
    }

    qDebug() << "Calling control script callback" << methodName;

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

void PackageManagerGui::showEvent(QShowEvent *event)
{
#ifndef Q_OS_LINUX
    if (!event->spontaneous()) {
        foreach (int id, pageIds()) {
            const QString subTitle = page(id)->subTitle();
            if (subTitle.isEmpty()) {
                const QWizard::WizardStyle style = wizardStyle();
                if ((style == QWizard::ClassicStyle || style == QWizard::ModernStyle))
                    page(id)->setSubTitle(QLatin1String(" "));    // otherwise the colors might screw up
            }
        }
    }
#endif
    QWizard::showEvent(event);
}

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

void PackageManagerGui::wizardPageRemovalRequested(QWidget *widget)
{
    foreach (int pageId, pageIds()) {
        DynamicInstallerPage *const dynamicPage = dynamic_cast<DynamicInstallerPage*>(page(pageId));
        if (dynamicPage == 0)
            continue;
        if (dynamicPage->widget() != widget)
            continue;
        removePage(pageId);
        d->m_defaultPages.remove(pageId);
    }
}

void PackageManagerGui::wizardWidgetInsertionRequested(QWidget *widget,
    QInstaller::PackageManagerCore::WizardPage page)
{
    Q_ASSERT(widget);
    if (QWizardPage *const p = QWizard::page(page))
        p->layout()->addWidget(widget);
}

void PackageManagerGui::wizardWidgetRemovalRequested(QWidget *widget)
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

QWidget *PackageManagerGui::pageWidgetByObjectName(const QString &name) const
{
    const QList<int> ids = pageIds();
    foreach (const int i, ids) {
        PackageManagerPage *const p = qobject_cast<PackageManagerPage*> (page(i));
        if (p && p->objectName() == name) {
            // For dynamic pages, return the contained widget (as read from the UI file), not the
            // wrapper page
            if (DynamicInstallerPage *dp = dynamic_cast<DynamicInstallerPage*>(p))
                return dp->widget();
            return p;
        }
    }
    qDebug() << "No page found for object name" << name;
    return 0;
}

QWidget *PackageManagerGui::currentPageWidget() const
{
    return currentPage();
}

void PackageManagerGui::cancelButtonClicked()
{
    if (currentId() != PackageManagerCore::InstallationFinished) {
        PackageManagerPage *const page = qobject_cast<PackageManagerPage*> (currentPage());
        if (page && page->isInterruptible() && m_core->status() != PackageManagerCore::Canceled
            && m_core->status() != PackageManagerCore::Failure) {
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
    qDebug() << "SHOW FINISHED PAGE";
    if (d->m_autoSwitchPage)
        next();
    else
        qobject_cast<QPushButton*>(button(QWizard::CancelButton))->setEnabled(false);
}

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
    Force an update of our own button layout, needs to be called whenever a button option has been set.
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

void PackageManagerGui::setSettingsButtonEnabled(bool enabled)
{
    if (QAbstractButton *btn = button(QWizard::CustomButton1))
        btn->setEnabled(enabled);
}

void PackageManagerGui::customButtonClicked(int which)
{
    if (QWizard::WizardButton(which) == QWizard::CustomButton1 && d->m_showSettingsButton)
        emit settingsButtonClicked();
}


// -- PackageManagerPage

PackageManagerPage::PackageManagerPage(PackageManagerCore *core)
    : m_fresh(true)
    , m_complete(true)
    , m_core(core)
{
}

PackageManagerCore *PackageManagerPage::packageManagerCore() const
{
    return m_core;
}

QVariantHash PackageManagerPage::elementsForPage(const QString &pageName) const
{
    const QVariant variant = m_core->settings().value(pageName);

    QVariantHash hash;
    if (variant.canConvert<QVariantHash>())
        hash = variant.value<QVariantHash>();
    return hash;
}

QString PackageManagerPage::titleForPage(const QString &pageName, const QString &value) const
{
    return tr("%1").arg(titleFromHash(m_core->settings().titlesForPage(pageName), value));
}

QString PackageManagerPage::subTitleForPage(const QString &pageName, const QString &value) const
{
    return tr("%1").arg(titleFromHash(m_core->settings().subTitlesForPage(pageName), value));
}

QString PackageManagerPage::titleFromHash(const QVariantHash &hash, const QString &value) const
{
    QString defaultValue = hash.value(QLatin1String("Default")).toString();
    if (defaultValue.isEmpty())
        defaultValue = value;

    if (m_core->isUpdater())
        return hash.value(QLatin1String("Updater"), defaultValue).toString();
    if (m_core->isInstaller())
        return hash.value(QLatin1String("Installer"), defaultValue).toString();
    if (m_core->isPackageManager())
        return hash.value(QLatin1String("PackageManager"), defaultValue).toString();
    return hash.value(QLatin1String("Uninstaller"), defaultValue).toString();
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
    setPixmap(QWizard::WatermarkPixmap, watermarkPixmap());
    setSubTitle(subTitleForPage(QLatin1String("IntroductionPage")));
    setTitle(titleForPage(QLatin1String("IntroductionPage"), tr("Setup - %1")).arg(productName()));

    m_msgLabel = new QLabel(this);
    m_msgLabel->setWordWrap(true);
    m_msgLabel->setObjectName(QLatin1String("MessageLabel"));
    const QVariantHash hash = elementsForPage(QLatin1String("IntroductionPage"));
    m_msgLabel->setText(tr("%1").arg(hash.value(QLatin1String("MessageLabel"), tr("Welcome to the %1 "
        "Setup Wizard.")).toString().arg(productName())));

    QVBoxLayout *layout = new QVBoxLayout(this);
    setLayout(layout);
    layout->addWidget(m_msgLabel);
    layout->addItem(new QSpacerItem(20, 20, QSizePolicy::Minimum, QSizePolicy::Expanding));
}

void IntroductionPage::setWidget(QWidget *widget)
{
    if (m_widget) {
        layout()->removeWidget(m_widget);
        delete m_widget;
    }
    m_widget = widget;
    if (m_widget)
        static_cast<QVBoxLayout*>(layout())->addWidget(m_widget, 1);
}

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

LicenseAgreementPage::LicenseAgreementPage(PackageManagerCore *core)
    : PackageManagerPage(core)
{
    setPixmap(QWizard::LogoPixmap, logoPixmap());
    setPixmap(QWizard::WatermarkPixmap, QPixmap());
    setObjectName(QLatin1String("LicenseAgreementPage"));
    setTitle(titleForPage(QLatin1String("LicenseAgreementPage"), tr("License Agreement")));
    setSubTitle(subTitleForPage(QLatin1String("LicenseAgreementPage"), tr("Please read the following license "
        "agreement(s). You must accept the terms contained in these agreement(s) before continuing with the "
        "installation.")));

    m_licenseListWidget = new QListWidget(this);
    m_licenseListWidget->setObjectName(QLatin1String("LicenseListWidget"));
    m_licenseListWidget->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Expanding);
    connect(m_licenseListWidget, SIGNAL(currentItemChanged(QListWidgetItem *, QListWidgetItem *)),
        this, SLOT(currentItemChanged(QListWidgetItem *)));

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

    QLabel *acceptLabel = new QLabel;
    acceptLabel->setWordWrap(true);
    acceptLabel->installEventFilter(acceptClickForwarder);
    acceptLabel->setObjectName(QLatin1String("AcceptLicenseLabel"));
    acceptLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    const QVariantHash hash = elementsForPage(QLatin1String("LicenseAgreementPage"));
    acceptLabel->setText(tr("%1").arg(hash.value(QLatin1String("AcceptLicenseLabel"), tr("I h<u>a</u>ve read "
        "and agree to the following terms contained in the license agreements accompanying the Qt SDK and "
        "additional items. I agree that my use of the Qt SDK is governed by the terms and conditions "
        "contained in these license agreements.")).toString()));

    m_rejectRadioButton = new QRadioButton(this);
    ClickForwarder *rejectClickForwarder = new ClickForwarder(m_rejectRadioButton);
    m_rejectRadioButton->setObjectName(QString::fromUtf8("RejectLicenseRadioButton"));
    m_rejectRadioButton->setShortcut(QKeySequence(tr("Alt+D", "do not agree license")));

    QLabel *rejectLabel = new QLabel;
    rejectLabel->setWordWrap(true);
    rejectLabel->installEventFilter(rejectClickForwarder);
    rejectLabel->setObjectName(QLatin1String("RejectLicenseLabel"));
    rejectLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    rejectLabel->setText(tr("%1").arg(hash.value(QLatin1String("RejectLicenseLabel"), tr("I <u>d</u>o not "
        "accept the terms and conditions of the above listed license agreements. Please note by checking the "
        "box, you must cancel the installation or downloading the Qt SDK and must destroy all copies, or "
        "portions thereof, of the Qt SDK in your possessions.")).toString()));

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
    m_textBrowser->setText(QString());
    m_licenseListWidget->setVisible(false);

    packageManagerCore()->calculateComponentsToInstall();
    foreach (QInstaller::Component *component, packageManagerCore()->orderedComponentsToInstall())
        addLicenseItem(component->licenses());

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
        m_treeView->setObjectName(QLatin1String("ComponentsTreeView"));
        m_allModel->setObjectName(QLatin1String("AllComponentsModel"));
        m_updaterModel->setObjectName(QLatin1String("UpdaterComponentsModel"));

        int i = 0;
        m_currentModel = m_allModel;
        ComponentModel *list[] = { m_allModel, m_updaterModel, 0 };
        while (ComponentModel *model = list[i++]) {
            connect(model, SIGNAL(defaultCheckStateChanged(bool)), q, SLOT(setModified(bool)));
            connect(model, SIGNAL(defaultCheckStateChanged(bool)), m_core,
                SLOT(componentsToInstallNeedsRecalculation()));

            model->setHeaderData(ComponentModelHelper::NameColumn, Qt::Horizontal, tr("Component Name"));
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

        m_checkDefault = new QPushButton;
        connect(m_checkDefault, SIGNAL(clicked()), this, SLOT(selectDefault()));
        connect(m_allModel, SIGNAL(defaultCheckStateChanged(bool)), m_checkDefault, SLOT(setEnabled(bool)));
        const QVariantHash hash = q->elementsForPage(QLatin1String("ComponentSelectionPage"));
        if (m_core->isInstaller()) {
            m_checkDefault->setObjectName(QLatin1String("SelectDefaultComponentsButton"));
            m_checkDefault->setShortcut(QKeySequence(tr("Alt+A", "select default components")));
            m_checkDefault->setText(hash.value(QLatin1String("SelectDefaultComponentsButton"), tr("Def&ault"))
                .toString());
        } else {
            m_checkDefault->setEnabled(false);
            m_checkDefault->setObjectName(QLatin1String("ResetComponentsButton"));
            m_checkDefault->setShortcut(QKeySequence(tr("Alt+R", "reset to already installed components")));
            m_checkDefault->setText(hash.value(QLatin1String("ResetComponentsButton"), tr("&Reset")).toString());
        }
        hlayout = new QHBoxLayout;
        hlayout->addWidget(m_checkDefault);

        m_checkAll = new QPushButton;
        hlayout->addWidget(m_checkAll);
        connect(m_checkAll, SIGNAL(clicked()), this, SLOT(selectAll()));
        m_checkAll->setObjectName(QLatin1String("SelectAllComponentsButton"));
        m_checkAll->setShortcut(QKeySequence(tr("Alt+S", "select all components")));
        m_checkAll->setText(hash.value(QLatin1String("SelectAllComponentsButton"), tr("&Select All")).toString());

        m_uncheckAll = new QPushButton;
        hlayout->addWidget(m_uncheckAll);
        connect(m_uncheckAll, SIGNAL(clicked()), this, SLOT(deselectAll()));
        m_uncheckAll->setObjectName(QLatin1String("DeselectAllComponentsButton"));
        m_uncheckAll->setShortcut(QKeySequence(tr("Alt+D", "deselect all components")));
        m_uncheckAll->setText(hash.value(QLatin1String("DeselectAllComponentsButton"), tr("&Deselect All"))
            .toString());

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
        m_checkDefault->setVisible(m_core->isInstaller() || m_core->isPackageManager());
        if (m_treeView->selectionModel()) {
            disconnect(m_treeView->selectionModel(), SIGNAL(currentChanged(QModelIndex, QModelIndex)),
                this, SLOT(currentChanged(QModelIndex)));
            disconnect(m_currentModel, SIGNAL(checkStateChanged(QModelIndex)), this,
                SLOT(currentChanged(QModelIndex)));
        }

        m_currentModel = m_core->isUpdater() ? m_updaterModel : m_allModel;
        m_treeView->setModel(m_currentModel);
        m_treeView->setExpanded(m_currentModel->index(0, 0), true);

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

        connect(m_treeView->selectionModel(), SIGNAL(currentChanged(QModelIndex, QModelIndex)),
            this, SLOT(currentChanged(QModelIndex)));
        connect(m_currentModel, SIGNAL(checkStateChanged(QModelIndex)), this,
            SLOT(currentChanged(QModelIndex)));

        m_treeView->setCurrentIndex(m_currentModel->index(0, 0));
    }

public slots:
    void currentChanged(const QModelIndex &current)
    {
        // if there is not selection or the current selected node didn't change, return
        if (!current.isValid() || current != m_treeView->selectionModel()->currentIndex())
            return;

        m_descriptionLabel->setText(m_currentModel->data(m_currentModel->index(current.row(),
            ComponentModelHelper::NameColumn, current.parent()), Qt::ToolTipRole).toString());

        m_sizeLabel->clear();
        if (!m_core->isUninstaller()) {
            Component *component = m_currentModel->componentFromIndex(current);
            if (component && component->updateUncompressedSize() > 0) {
                const QVariantHash hash = q->elementsForPage(QLatin1String("ComponentSelectionPage"));
                m_sizeLabel->setText(tr("%1").arg(hash.value(QLatin1String("ComponentSizeLabel"),
                    tr("This component will occupy approximately %1 on your hard disk drive.")).toString()
                    .arg(m_currentModel->data(m_currentModel->index(current.row(),
                    ComponentModelHelper::UncompressedSizeColumn, current.parent())).toString())));
            }
        }
    }

    void selectAll()
    {
        m_currentModel->selectAll();

        m_checkAll->setEnabled(!m_currentModel->hasCheckedComponents());
        m_uncheckAll->setEnabled(m_currentModel->hasCheckedComponents());
    }

    void deselectAll()
    {
        m_currentModel->deselectAll();

        m_checkAll->setEnabled(m_currentModel->hasCheckedComponents());
        m_uncheckAll->setEnabled(!m_currentModel->hasCheckedComponents());
    }

    void selectDefault()
    {
        m_currentModel->selectDefault();

        // Do not apply special magic here to keep the enabled/ disabled state in sync with the checked
        // components. We would need to implement the counter in the model, which has an unnecessary impact
        // on the complexity and amount of code compared to what we gain in functionality.
        m_checkAll->setEnabled(true);
        m_uncheckAll->setEnabled(true);
    }

private slots:
    void allComponentsChanged()
    {
        m_allModel->setRootComponents(m_core->rootComponents());
    }

    void updaterComponentsChanged()
    {
        m_updaterModel->setRootComponents(m_core->updaterComponents());
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
    On this page the user can select and deselect what he wants to be installed.
*/
ComponentSelectionPage::ComponentSelectionPage(PackageManagerCore *core)
    : PackageManagerPage(core)
    , d(new Private(this, core))
{
    setPixmap(QWizard::LogoPixmap, logoPixmap());
    setPixmap(QWizard::WatermarkPixmap, QPixmap());
    setObjectName(QLatin1String("ComponentSelectionPage"));
    setTitle(titleForPage(QLatin1String("ComponentSelectionPage"), tr("Select Components")));
}

ComponentSelectionPage::~ComponentSelectionPage()
{
    delete d;
}

void ComponentSelectionPage::entering()
{
    static const char *strings[] = {
        QT_TR_NOOP("Please select the components you want to update."),
        QT_TR_NOOP("Please select the components you want to install."),
        QT_TR_NOOP("Please select the components you want to uninstall."),
        QT_TR_NOOP("Select the components to install. Deselect installed components to unistall them.")
     };

    int index = 0;
    PackageManagerCore *core = packageManagerCore();
    if (core->isInstaller()) index = 1;
    if (core->isUninstaller()) index = 2;
    if (core->isPackageManager()) index = 3;
    setSubTitle(subTitleForPage(QLatin1String("ComponentSelectionPage"), tr(strings[index])));

    d->updateTreeView();
    setModified(isComplete());
}

void ComponentSelectionPage::showEvent(QShowEvent *event)
{
    // remove once we deprecate isSelected, setSelected etc...
    if (!event->spontaneous())
        packageManagerCore()->resetComponentsToUserCheckedState();
    QWizardPage::showEvent(event);
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
void ComponentSelectionPage::selectComponent(const QString &id)
{
    const QModelIndex &idx = d->m_currentModel->indexFromComponentName(id);
    if (idx.isValid())
        d->m_currentModel->setData(idx, Qt::Checked, Qt::CheckStateRole);
}

/*!
    Deselects the component with /a id in the component tree.
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
    setPixmap(QWizard::LogoPixmap, logoPixmap());
    setPixmap(QWizard::WatermarkPixmap, QPixmap());
    setObjectName(QLatin1String("TargetDirectoryPage"));
    setSubTitle(subTitleForPage(QLatin1String("TargetDirectoryPage")));
    setTitle(titleForPage(QLatin1String("TargetDirectoryPage"), tr("Installation Folder")));

    QVBoxLayout *layout = new QVBoxLayout(this);

    QLabel *msgLabel = new QLabel(this);
    msgLabel->setWordWrap(true);
    msgLabel->setObjectName(QLatin1String("MessageLabel"));
    const QVariantHash hash = elementsForPage(QLatin1String("TargetDirectoryPage"));
    msgLabel->setText(tr("%1").arg(hash.value(QLatin1String("MessageLabel"), tr("Please specify the folder "
        "where %1 will be installed.")).toString().arg(productName())));
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
    browseButton->setText(tr("%1").arg(hash.value(QLatin1String("BrowseDirectoryButton"), tr("B&rowse..."))
        .toString()));
    hlayout->addWidget(browseButton);

    layout->addLayout(hlayout);
    setLayout(layout);
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
    const QVariantHash hash = elementsForPage(QLatin1String("TargetDirectoryPage"));
    if (targetDir().isEmpty()) {
        MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(),
            QLatin1String("EmptyTargetDirectoryMessage"), tr("Error"), tr("%1").arg(hash
            .value(QLatin1String("EmptyTargetDirectoryMessage"), tr("The install directory cannot be "
            "empty, please specify a valid folder.")).toString()), QMessageBox::Ok);
        return false;
    }

    const QDir dir(targetDir());
    // it exists, but is empty (might be created by the Browse button (getExistingDirectory)
    if (dir.exists() && dir.entryList(QDir::NoDotAndDotDot).isEmpty())
        return true;

    if (dir.exists() && dir.isReadable()) {
        // it exists, but is not empty
        if (dir == QDir::root()) {
            MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(),
                QLatin1String("ForbiddenTargetDirectoryMessage"), tr("Error"), tr("%1").arg(hash
                .value(QLatin1String("ForbiddenTargetDirectoryMessage"), tr("As the install directory is "
                "completely deleted, installing in %1 is forbidden.")).toString().arg(QDir::rootPath())),
                QMessageBox::Ok);
            return false;
        }

        if (!QVariant(packageManagerCore()->value(scRemoveTargetDir)).toBool())
            return true;

        return MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(),
            QLatin1String("OverwriteTargetDirectoryMessage"), tr("Warning"), tr("%1").arg(hash
            .value(QLatin1String("OverwriteTargetDirectoryMessage"), tr("You have selected an existing, "
            "non-empty folder for installation. Note that it will be completely wiped on uninstallation of "
            "this application. It is not advisable to install into this folder as installation might fail. "
            "Do you want to continue?")).toString()), QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes;
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
    const QVariantHash hash = elementsForPage(QLatin1String("TargetDirectoryPage"));
    const QString newDirName = QFileDialog::getExistingDirectory(this, tr("%1").arg(hash
        .value(QLatin1String("SelectInstallationFolderCaption"), tr("Select Installation Folder")).toString()),
        targetDir());
    if (newDirName.isEmpty() || newDirName == targetDir())
        return;
    m_lineEdit->setText(QDir::toNativeSeparators(newDirName));
}


// -- StartMenuDirectoryPage

StartMenuDirectoryPage::StartMenuDirectoryPage(PackageManagerCore *core)
    : PackageManagerPage(core)
{
    setPixmap(QWizard::LogoPixmap, logoPixmap());
    setPixmap(QWizard::WatermarkPixmap, QPixmap());
    setObjectName(QLatin1String("StartMenuDirectoryPage"));
    setTitle(titleForPage(QLatin1String("StartMenuDirectoryPage"), tr("Start Menu shortcuts")));
    setSubTitle(subTitleForPage(QLatin1String("StartMenuDirectoryPage"), tr("Select the Start Menu in which "
        "you would like to create the program's shortcuts. You can also enter a name to create a new folder.")));

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
    startMenuPath = replaceWindowsEnvironmentVariables(user.value(QLatin1String("Programs"), QString())
        .toString());
    core->setValue(QLatin1String("DesktopDir"), replaceWindowsEnvironmentVariables(user
        .value(QLatin1String("Desktop")).toString()));

    QDir dir(startMenuPath); // user only dirs
    QStringList dirs = dir.entryList(QDir::AllDirs | QDir::NoDotAndDotDot);

    if (core->value(QLatin1String("AllUsers")) == QLatin1String("true")) {
        qDebug() << "AllUsers set. Using HKEY_LOCAL_MACHINE";
        QSettings system(QLatin1String("HKEY_LOCAL_MACHINE\\Software\\Microsoft\\Windows\\CurrentVersion\\"
            "Explorer\\Shell Folders"), QSettings::NativeFormat);
        startMenuPath = system.value(QLatin1String("Common Programs"), QString()).toString();
        core->setValue(QLatin1String("DesktopDir"),system.value(QLatin1String("Desktop")).toString());

        dir.setPath(startMenuPath); // system only dirs
        dirs += dir.entryList(QDir::AllDirs | QDir::NoDotAndDotDot);
    }

    qDebug() << "StartMenuPath: \t" << startMenuPath;
    qDebug() << "DesktopDir: \t" << core->value(QLatin1String("DesktopDir"));

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

void StartMenuDirectoryPage::currentItemChanged(QListWidgetItem *current)
{
    if (current) {
        QString dir = current->data(Qt::DisplayRole).toString();
        if (!dir.isEmpty())
            dir += QDir::separator();
        setStartMenuDir(dir + packageManagerCore()->value(scStartMenuDir));
    }
}


// -- ReadyForInstallationPage

ReadyForInstallationPage::ReadyForInstallationPage(PackageManagerCore *core)
    : PackageManagerPage(core)
    , m_msgLabel(new QLabel)
{
    setPixmap(QWizard::LogoPixmap, logoPixmap());
    setPixmap(QWizard::WatermarkPixmap, QPixmap());
    setObjectName(QLatin1String("ReadyForInstallationPage"));
    setSubTitle(subTitleForPage(QLatin1String("ReadyForInstallationPage")));

    QVBoxLayout *baseLayout = new QVBoxLayout();
    baseLayout->setObjectName(QLatin1String("BaseLayout"));

    QVBoxLayout *topLayout = new QVBoxLayout();
    topLayout->setObjectName(QLatin1String("TopLayout"));

    m_msgLabel->setWordWrap(true);
    m_msgLabel->setObjectName(QLatin1String("MessageLabel"));
    m_msgLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    topLayout->addWidget(m_msgLabel);
    baseLayout->addLayout(topLayout);

    m_taskDetailsButton = new QPushButton(tr("&Show Details"), this);
    m_taskDetailsButton->setObjectName(QLatin1String("TaskDetailsButton"));
    m_taskDetailsButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    connect(m_taskDetailsButton, SIGNAL(clicked()), this, SLOT(toggleDetails()));
    topLayout->addWidget(m_taskDetailsButton);

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

    setLayout(baseLayout);
}


/*!
    \reimp
*/
void ReadyForInstallationPage::entering()
{
    setCommitPage(true);
    const QString target = packageManagerCore()->value(scTargetDir);

    if (packageManagerCore()->isUninstaller()) {
        m_taskDetailsButton->setVisible(false);
        m_taskDetailsBrowser->setVisible(false);
        setButtonText(QWizard::CommitButton, tr("U&ninstall"));
        setTitle(titleForPage(objectName(), tr("Ready to Uninstall")));
        m_msgLabel->setText(tr("Setup is now ready to begin removing %1 from your computer.<br>"
            "<font color=\"red\">The program dir %2 will be deleted completely</font>, "
            "including all content in that directory!")
            .arg(productName(), QDir::toNativeSeparators(QDir(target).absolutePath())));
        return;
    } else if (packageManagerCore()->isPackageManager() || packageManagerCore()->isUpdater()) {
        setButtonText(QWizard::CommitButton, tr("U&pdate"));
        setTitle(titleForPage(objectName(), tr("Ready to Update Packages")));
        m_msgLabel->setText(tr("Setup is now ready to begin updating your installation."));
    } else {
        Q_ASSERT(packageManagerCore()->isInstaller());
        setButtonText(QWizard::CommitButton, tr("&Install"));
        setTitle(titleForPage(objectName(), tr("Ready to Install")));
        m_msgLabel->setText(tr("Setup is now ready to begin installing %1 on your computer.")
            .arg(productName()));
    }

    refreshTaskDetailsBrowser();

    const VolumeInfo vol = VolumeInfo::fromPath(target);
    const VolumeInfo tempVolume = VolumeInfo::fromPath(QDir::tempPath());
    const bool tempOnSameVolume = (vol == tempVolume);

    // there is no better way atm to check this
    if (vol.size() == 0 && vol.availableSpace() == 0) {
        qDebug() << QString::fromLatin1("Could not determine available space on device %1. Continue silently."
            ).arg(target);
        return;
    }

    const quint64 required(packageManagerCore()->requiredDiskSpace());
    const quint64 tempRequired(packageManagerCore()->requiredTemporaryDiskSpace());

    const quint64 available = vol.availableSpace();
    const quint64 tempAvailable = tempVolume.availableSpace();
    const quint64 realRequiredTempSpace = quint64(0.1 * tempRequired + tempRequired);
    const quint64 realRequiredSpace = quint64(2 * required);

    const bool tempInstFailure = tempOnSameVolume && (available < realRequiredSpace
        + realRequiredTempSpace);

    qDebug() << QString::fromLatin1("Disk space check on %1: required: %2, available: %3, size: %4").arg(
        target, QString::number(required), QString::number(available), QString::number(vol.size()));

    QString tempString;
    if (tempAvailable < realRequiredTempSpace || tempInstFailure) {
        if (tempOnSameVolume) {
            tempString = tr("Not enough disk space to store temporary files and the installation, "
                "at least %1 are required").arg(humanReadableSize(realRequiredTempSpace + realRequiredSpace));
        } else {
            tempString = tr("Not enough disk space to store temporary files, at least %1 are required.")
                .arg(humanReadableSize(realRequiredTempSpace));
            setCommitPage(false);
            m_msgLabel->setText(tempString);
        }
    }

    // error on not enough space
    if (available < required || tempInstFailure) {
        if (tempOnSameVolume) {
            m_msgLabel->setText(tempString);
        } else {
            m_msgLabel->setText(tr("The volume you selected for installation has insufficient space "
                "for the selected components. The installation requires approximately %1.")
                .arg(humanReadableSize(required)) + tempString);
        }
        setCommitPage(false);
    } else if (available - required < 0.01 * vol.size()) {
        // warn for less than 1% of the volume's space being free
        m_msgLabel->setText(tr("The volume you selected for installation seems to have sufficient space for "
            "installation, but there will be less than 1% of the volume's space available afterwards. %1")
            .arg(m_msgLabel->text()));
    } else if (available - required < 100 * 1024 * 1024LL) {
        // warn for less than 100MB being free
        m_msgLabel->setText(tr("The volume you selected for installation seems to have sufficient space for "
            "installation, but there will be less than 100 MB available afterwards. %1")
            .arg(m_msgLabel->text()));
    }
}

void ReadyForInstallationPage::refreshTaskDetailsBrowser()
{
    QString htmlOutput;
    QString lastInstallReason;
    if (!packageManagerCore()->calculateComponentsToUninstall() ||
        !packageManagerCore()->calculateComponentsToInstall()) {
            htmlOutput.append(QString::fromLatin1("<h2><font color=\"red\">%1</font></h2><ul>")
                .arg(tr("Can not resolve all dependencies!")));
            //if we have a missing dependency or a recursion we can display it
            if (!packageManagerCore()->componentsToInstallError().isEmpty()) {
                htmlOutput.append(QString::fromLatin1("<li> %1 </li>").arg(
                    packageManagerCore()->componentsToInstallError()));
            }
            htmlOutput.append(QLatin1String("</ul>"));
            m_taskDetailsBrowser->setHtml(htmlOutput);
            if (!m_taskDetailsBrowser->isVisible())
                toggleDetails();
            setCommitPage(false);
            return;
    }

    // In case of updater mode we don't uninstall components.
    if (!packageManagerCore()->isUpdater()) {
        QList<Component*> componentsToRemove = packageManagerCore()->componentsToUninstall();
        if (!componentsToRemove.isEmpty()) {
            htmlOutput.append(QString::fromLatin1("<h3>%1</h3><ul>").arg(tr("Components about to be removed.")));
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
    m_taskDetailsBrowser->setHtml(htmlOutput);
}

void ReadyForInstallationPage::toggleDetails()
{
    const bool visible = !m_taskDetailsBrowser->isVisible();
    m_taskDetailsBrowser->setVisible(visible);
    m_taskDetailsButton->setText(visible ? tr("&Hide Details") : tr("&Show Details"));
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
    setSubTitle(subTitleForPage(QLatin1String("PerformInstallationPage")));

    m_performInstallationForm->setupUi(this);

    connect(ProgressCoordinator::instance(), SIGNAL(detailTextChanged(QString)), m_performInstallationForm,
        SLOT(appendProgressDetails(QString)));
    connect(ProgressCoordinator::instance(), SIGNAL(detailTextResetNeeded()), m_performInstallationForm,
        SLOT(clearDetailsBrowser()));
    connect(m_performInstallationForm, SIGNAL(showDetailsChanged()), this, SLOT(toggleDetailsWereChanged()));

    connect(core, SIGNAL(installationStarted()), this, SLOT(installationStarted()));
    connect(core, SIGNAL(uninstallationStarted()), this, SLOT(installationStarted()));
    connect(core, SIGNAL(installationFinished()), this, SLOT(installationFinished()));
    connect(core, SIGNAL(uninstallationFinished()), this, SLOT(installationFinished()));
    connect(core, SIGNAL(titleMessageChanged(QString)), this, SLOT(setTitleMessage(QString)));
    connect(this, SIGNAL(setAutomatedPageSwitchEnabled(bool)), core,
        SIGNAL(setAutomatedPageSwitchEnabled(bool)));

    m_performInstallationForm->setDetailsWidgetVisible(true);
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
        setButtonText(QWizard::CommitButton, tr("&Uninstall"));
        setTitle(titleForPage(objectName(), tr("Uninstalling %1")).arg(productName));

        QTimer::singleShot(30, packageManagerCore(), SLOT(runUninstaller()));
    } else if (packageManagerCore()->isPackageManager() || packageManagerCore()->isUpdater()) {
        setButtonText(QWizard::CommitButton, tr("&Update"));
        setTitle(titleForPage(objectName(), tr("Updating components of %1")).arg(productName));

        QTimer::singleShot(30, packageManagerCore(), SLOT(runPackageUpdater()));
    } else {
        setButtonText(QWizard::CommitButton, tr("&Install"));
        setTitle(titleForPage(objectName(), tr("Installing %1")).arg(productName));

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

void PerformInstallationPage::setTitleMessage(const QString &title)
{
    setTitle(tr("%1").arg(title));
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
    , m_commitButton(0)
{
    setObjectName(QLatin1String("FinishedPage"));
    setPixmap(QWizard::WatermarkPixmap, watermarkPixmap());
    setSubTitle(subTitleForPage(QLatin1String("FinishedPage")));
    setTitle(titleForPage(QLatin1String("FinishedPage"), tr("Completing the %1 Wizard")).arg(productName()));

    m_msgLabel = new QLabel(this);
    m_msgLabel->setWordWrap(true);
    m_msgLabel->setObjectName(QLatin1String("MessageLabel"));

    const QVariantHash hash = elementsForPage(QLatin1String("FinishedPage"));
#ifdef Q_WS_MAC
    m_msgLabel->setText(tr("%1").arg(hash.value(QLatin1String("MessageLabel"), tr("Click Done to exit the %1 "
        "Wizard.")).toString().arg(productName())));
#else
    m_msgLabel->setText(tr("%1").arg(hash.value(QLatin1String("MessageLabel"), tr("Click Finish to exit the "
        "%1 Wizard.")).toString().arg(productName())));
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
    if (m_commitButton) {
        disconnect(m_commitButton, SIGNAL(clicked()), this, SLOT(handleFinishClicked()));
        m_commitButton = 0;
    }

    setCommitPage(true);
    if (packageManagerCore()->isUpdater() || packageManagerCore()->isPackageManager()) {
#ifdef Q_WS_MAC
        gui()->setOption(QWizard::NoCancelButton, false);
#endif
        if (QAbstractButton *cancel = gui()->button(QWizard::CancelButton)) {
            m_commitButton = cancel;
            cancel->setEnabled(true);
            cancel->setVisible(true);
        }
        setButtonText(QWizard::CommitButton, tr("Restart"));
        setButtonText(QWizard::CancelButton, gui()->defaultButtonText(QWizard::FinishButton));
    } else {
        if (packageManagerCore()->isInstaller())
            m_commitButton = wizard()->button(QWizard::FinishButton);

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
            m_msgLabel->setText(tr("%1").arg(finishedText));

        if (!packageManagerCore()->value(scRunProgram).isEmpty()) {
            m_runItCheckBox->show();
            m_runItCheckBox->setText(packageManagerCore()->value(scRunProgramDescription, tr("Run %1 now."))
                .arg(productName()));
            return; // job done
        }
    } else {
        // TODO: how to handle this using the config.xml
        setTitle(tr("The %1 Wizard failed.").arg(productName()));
    }

    m_runItCheckBox->hide();
    m_runItCheckBox->setChecked(false);
}

void FinishedPage::leaving()
{
#ifdef Q_WS_MAC
    gui()->setOption(QWizard::NoCancelButton, true);
#endif

    if (QAbstractButton *cancel = gui()->button(QWizard::CancelButton))
        cancel->setVisible(false);
    gui()->updateButtonLayout();

    setButtonText(QWizard::CommitButton, gui()->defaultButtonText(QWizard::CommitButton));
    setButtonText(QWizard::CancelButton, gui()->defaultButtonText(QWizard::CancelButton));
}

void FinishedPage::handleFinishClicked()
{
    const QString program = packageManagerCore()->replaceVariables(packageManagerCore()->value(scRunProgram));
    if (!m_runItCheckBox->isChecked() || program.isEmpty())
        return;

    qDebug() << "starting" << program;
    QProcess::startDetached(program);
}


// -- RestartPage

RestartPage::RestartPage(PackageManagerCore *core)
    : PackageManagerPage(core)
{
    setObjectName(QLatin1String("RestartPage"));
    setPixmap(QWizard::WatermarkPixmap, watermarkPixmap());
    setSubTitle(subTitleForPage(QLatin1String("RestartPage")));
    setTitle(titleForPage(QLatin1String("RestartPage"), tr("Completing the %1 Setup Wizard"))
        .arg(productName()));

    setFinalPage(false);
    setCommitPage(false);
}

int RestartPage::nextId() const
{
    return PackageManagerCore::Introduction;
}

void RestartPage::entering()
{
    if (!packageManagerCore()->needsRestart()) {
        if (QAbstractButton *finish = wizard()->button(QWizard::FinishButton))
            finish->setVisible(false);
        QMetaObject::invokeMethod(this, "restart", Qt::QueuedConnection);
    } else {
        gui()->accept();
    }
}

void RestartPage::leaving()
{
}

#include "packagemanagergui.moc"
#include "moc_packagemanagergui.cpp"
