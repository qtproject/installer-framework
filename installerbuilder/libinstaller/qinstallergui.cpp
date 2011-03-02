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
#include "qinstallergui.h"

#include "qinstaller.h"
#include "qinstallerglobal.h"
#include "qinstallercomponent.h"
#include "qinstallercomponentmodel.h"
#include "progresscoordinator.h"
#include "performinstallationform.h"

#include "common/errors.h"
#include "common/utils.h"
#include "common/installersettings.h"
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
#include <QtGui/QGroupBox>
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
TRANSLATOR QInstaller::Installer;
*/
/*
TRANSLATOR QInstaller::Gui
*/
/*
TRANSLATOR QInstaller::Page
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


class DynamicInstallerPage : public Page
{
public:
    explicit DynamicInstallerPage(QWidget* widget, Installer* parent = 0)
        : Page(parent)
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
        return Page::eventFilter(obj, event);
    }

private:
    QWidget* const m_widget;
};


// -- Gui::Private

class Gui::Private
{
public:
    Private()
        : autoSwitchPage(true)
        , modified(false) { }

    QScriptEngine controlScriptEngine;
    QScriptValue controlScript;
    bool autoSwitchPage;
    bool modified;
};


// -- Gui

QScriptEngine* Gui::controlScriptEngine() const
{
    return &d->controlScriptEngine;
}

/*!
    \class QInstaller::Gui
    Is the "gui" object in a none interactive installation
*/
Gui::Gui(Installer *installer, QWidget *parent)
    : QWizard(parent)
    , m_installer(installer)
    , d(new Private)
{
    if (installer->isInstaller())
        setWindowTitle(tr("%1 Setup").arg(m_installer->value(QLatin1String("Title"))));
    else
        setWindowTitle(tr("%1").arg(m_installer->value(QLatin1String("maintenanceTitle"))));

#ifndef Q_WS_MAC
    setWindowIcon(QIcon(m_installer->settings().icon()));
    setWizardStyle(QWizard::ModernStyle);
#else
    setPixmap(QWizard::BackgroundPixmap, m_installer->settings().background());
#endif
    setOption(QWizard::NoBackButtonOnStartPage);
    setOption(QWizard::NoBackButtonOnLastPage);
    setLayout(new QVBoxLayout(this));

    connect(this, SIGNAL(interrupted()), installer, SLOT(interrupt()));
    connect(this, SIGNAL(rejected()), installer, SLOT(setCanceled()));
    // both queued to show the finished page once everything is done
    connect(installer, SIGNAL(installationFinished()), this,
        SLOT(showFinishedPage()), Qt::QueuedConnection);
    connect(installer, SIGNAL(uninstallationFinished()), this,
        SLOT(showFinishedPage()), Qt::QueuedConnection);

    connect(this, SIGNAL(currentIdChanged(int)), this, SLOT(slotCurrentPageChanged(int)));
    connect(this, SIGNAL(currentIdChanged(int)), m_installer, SIGNAL(currentPageChanged(int)));
    connect(button(QWizard::FinishButton), SIGNAL(clicked()), this, SIGNAL(finishButtonClicked()));
    connect(this, SIGNAL(finishButtonClicked()), installer, SIGNAL(finishButtonClicked()));

    // make sure the QUiLoader's retranslateUi is executed first, then the script
    connect(this, SIGNAL(languageChanged()),
        installer, SLOT(languageChanged()), Qt::QueuedConnection);

    connect(installer, SIGNAL(wizardPageInsertionRequested(QWidget*, Installer::WizardPage)),
        this, SLOT(wizardPageInsertionRequested(QWidget*, Installer::WizardPage)));
    connect(installer, SIGNAL(wizardPageRemovalRequested(QWidget*)),
        this, SLOT(wizardPageRemovalRequested(QWidget*)));
    connect(installer, SIGNAL(wizardWidgetInsertionRequested(QWidget*, Installer::WizardPage)),
        this, SLOT(wizardWidgetInsertionRequested(QWidget*, Installer::WizardPage)));
    connect(installer, SIGNAL(wizardWidgetRemovalRequested(QWidget*)),
        this, SLOT(wizardWidgetRemovalRequested(QWidget*)));
    connect(installer, SIGNAL(wizardPageVisibilityChangeRequested(bool, int)),
        this, SLOT(wizardPageVisibilityChangeRequested(bool, int)), Qt::QueuedConnection);

    connect(installer, SIGNAL(setAutomatedPageSwitchEnabled(bool)),
        this, SLOT(setAutomatedPageSwitchEnabled(bool)));

#ifdef Q_WS_MAC
    setButtonText(QWizard::BackButton, tr("Go Back"));
    setButtonText(QWizard::NextButton, tr("Continue"));
    setButtonText(QWizard::FinishButton, tr("Done"));
#else
    setButtonText(QWizard::BackButton, tr("Back"));
    setButtonText(QWizard::NextButton, tr("Next"));
    setButtonText(QWizard::FinishButton, tr("Finish"));
#endif
    setButtonText(QWizard::CancelButton, tr("Cancel"));

    dynamic_cast<QPushButton*>(button(QWizard::NextButton))->setDefault(true);

#ifdef Q_WS_MAC
    resize(sizeHint() * 1.25);
#else
    resize(sizeHint());
#endif
}

Gui::~Gui()
{
    delete d;
}

void Gui::setAutomatedPageSwitchEnabled(bool request)
{
    d->autoSwitchPage = request;
}

void Gui::clickButton(int wb, int delay)
{
    if (QAbstractButton* b = button(static_cast<QWizard::WizardButton>(wb) )) {
        QTimer::singleShot(delay, b, SLOT(click()));
    } else {
        //TODO we should probably abort immediately here (faulty test script)
        verbose() << "Button " << wb << " not found!" << std::endl;
    }
}

/*!
    Loads a script to perform the installation non-interactively.
    @throws QInstaller::Error if the script is not readable/cannot be parsed
*/
void Gui::loadControlScript(const QString& scriptPath)
{
    QFile file(scriptPath);
    if (!file.open(QIODevice::ReadOnly)) {
        throw Error(QObject::tr("Could not open the requested script file at %1: %2")
            .arg(scriptPath, file.errorString()));
    }

    d->controlScriptEngine.globalObject().setProperty(QLatin1String("installer"),
        d->controlScriptEngine.newQObject(m_installer));
    d->controlScriptEngine.globalObject().setProperty(QLatin1String("gui"),
        d->controlScriptEngine.newQObject(this));
    registerMessageBox(&d->controlScriptEngine);

#undef REGISTER_BUTTON
#define REGISTER_BUTTON(x)    buttons.setProperty(QLatin1String(#x), \
    d->controlScriptEngine.newVariant(static_cast<int>(QWizard::x)));

    QScriptValue buttons = d->controlScriptEngine.newArray();
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

    d->controlScriptEngine.globalObject().setProperty(QLatin1String("buttons"), buttons);

    d->controlScriptEngine.evaluate(QLatin1String(file.readAll()), scriptPath);
    if (d->controlScriptEngine.hasUncaughtException()) {
        throw Error(QObject::tr("Exception while loading the control script %1")
            .arg(uncaughtExceptionString(&(d->controlScriptEngine)/*, scriptPath*/)));
    }

    QScriptValue comp = d->controlScriptEngine.evaluate(QLatin1String("Controller"));
    if (d->controlScriptEngine.hasUncaughtException()) {
        throw Error(QObject::tr("Exception while loading the control script %1")
            .arg(uncaughtExceptionString(&(d->controlScriptEngine)/*, scriptPath*/)));
    }

    d->controlScript = comp;
    d->controlScript.construct();

    verbose() << "Loaded control script " << qPrintable(scriptPath) << std::endl;
}

void Gui::triggerControlScriptForCurrentPage()
{
    slotCurrentPageChanged(currentId());
}

void Gui::slotCurrentPageChanged(int id)
{
    QMetaObject::invokeMethod(this, "delayedControlScriptExecution", Qt::QueuedConnection,
        Q_ARG(int, id));
}

void Gui::callControlScriptMethod(const QString& methodName)
{
    QScriptValue method = d->controlScript.property(QLatin1String("prototype")).property(methodName);

    if (!method.isValid()) {
        verbose() << "Control script callback " << qPrintable(methodName) << " does not exist."
            << std::endl;
        return;
    }

    verbose() << "Calling control script callback " << qPrintable(methodName) << std::endl;

    method.call(d->controlScript);

    if (d->controlScriptEngine.hasUncaughtException()) {
        qCritical() << uncaughtExceptionString(&(d->controlScriptEngine)
            /*, QLatin1String("control script")*/);
        //TODO handle error
    }
}

void Gui::delayedControlScriptExecution(int id)
{
    if (Page* const p = qobject_cast<Page*>(page(id))) {
        const QString methodName = p->objectName() + QLatin1String("Callback");
        callControlScriptMethod(methodName);
    }
}

bool Gui::event(QEvent* event)
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

void Gui::wizardPageInsertionRequested(QWidget* widget, Installer::WizardPage page)
{
    // just in case it was already in there...
    wizardPageRemovalRequested(widget);

    // now find a suitable ID lower than page
    int p = static_cast<int>(page) - 1;
    while (QWizard::page(p) != 0)
        --p;
    // add it
    setPage(p, new DynamicInstallerPage(widget, m_installer));
}

void Gui::wizardPageRemovalRequested(QWidget* widget)
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

void Gui::wizardWidgetInsertionRequested(QWidget* widget, Installer::WizardPage page)
{
    Q_ASSERT(widget);
    if (QWizardPage* const p = QWizard::page(page))
        p->layout()->addWidget(widget);
}

void Gui::wizardWidgetRemovalRequested(QWidget* widget)
{
    Q_ASSERT(widget);
    widget->setParent(0);
}

void Gui::wizardPageVisibilityChangeRequested(bool visible, int p)
{
    if (visible && page(p) == 0) {
        setPage(p, defaultPages[p]);
    } else if (!visible && page(p) != 0) {
        defaultPages[p] = page(p);
        removePage(p);
    }
}

Page* Gui::page(int pageId) const
{
    return qobject_cast<Page*>(QWizard::page(pageId));
}

QWidget* Gui::pageWidgetByObjectName(const QString& name) const
{
    const QList<int> ids = pageIds();
    foreach (const int i, ids) {
        Page* const p = qobject_cast<Page*>(page(i));
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

QWidget* Gui::currentPageWidget() const
{
    return currentPage();
}

void Gui::cancelButtonClicked()
{
    const bool doIgnore = m_installer->isInstaller()
        ? m_installer->status() == Installer::InstallerCanceledByUser
            || m_installer->status() == Installer::InstallerSucceeded
        : m_installer->status() == Installer::InstallerCanceledByUser;

    //if the user canceled (all modes) or the installation is finished (installer mode only), ignore
    //clicks on cancel.
    if (doIgnore)
        return;

    //close the manager without asking if nothing was modified, always ask for the installer
    if (!m_installer->isInstaller() && !d->modified) {
        QDialog::reject();
        return;
    }

    Page* const page = qobject_cast<Page*>(currentPage());
    verbose() << "CANCEL CLICKED" << currentPage() << page << std::endl;
    if (page && page->isInterruptible()) {
        const QMessageBox::StandardButton bt =
            MessageBoxHandler::warning(MessageBoxHandler::currentBestSuitParent(),
            QLatin1String("cancelInstallation"), tr("Warning"),
            tr("Do you want to abort the %1 process?").arg(m_installer->isUninstaller()
                ? tr("uninstallation") : tr("installation")), QMessageBox::Yes | QMessageBox::No);
        if (bt == QMessageBox::Yes)
            emit interrupted();
    } else {
        const QMessageBox::StandardButton bt =
            MessageBoxHandler::warning(MessageBoxHandler::currentBestSuitParent(),
            QLatin1String("cancelInstallation"), tr("Warning"),
            tr("Do you want to abort the %1 application?").arg(m_installer->isUninstaller()
                ? tr("uninstaller") : tr("installer")), QMessageBox::Yes | QMessageBox::No);
        if (bt == QMessageBox::Yes)
            QDialog::reject();
    }
}

void Gui::rejectWithoutPrompt()
{
    m_installer->setCanceled();
    QDialog::reject();
}

void Gui::reject()
{
    cancelButtonClicked();
}

void Gui::setModified(bool value)
{
    d->modified = value;
}

void Gui::showFinishedPage()
{
    verbose() << "SHOW FINISHED PAGE" << std::endl;
    if (d->autoSwitchPage)
        next();
    else
        dynamic_cast< QPushButton*>(button(QWizard::CancelButton))->setEnabled(false);
}


// -- Page

Page::Page(Installer *installer)
    : m_installer(installer)
    , m_fresh(true)
    , m_complete(true)
{
    // otherwise the colors will screw up
    setSubTitle(QLatin1String(" "));
}

Installer *Page::installer() const
{
    return m_installer;
}

QPixmap Page::watermarkPixmap() const
{
    return QPixmap(m_installer->value(QLatin1String("WatermarkPixmap")));
}

QPixmap Page::logoPixmap() const
{
    return QPixmap(m_installer->value(QLatin1String("LogoPixmap")));
}

QString Page::productName() const
{
    return m_installer->value(QLatin1String("ProductName"));
}

bool Page::isComplete() const
{
    return m_complete;
}

void Page::setComplete(bool complete)
{
    m_complete = complete;
    emit completeChanged();
    if (wizard() == 0)
        return;
    if (wizard()->button(QWizard::CancelButton) == 0)
        return;
    if (wizard()->button(QWizard::NextButton) == 0)
        return;
    if (wizard()->button(QWizard::CancelButton)->hasFocus())
        wizard()->button(QWizard::NextButton)->setFocus();
}

void Page::insertWidget(QWidget *widget, const QString &siblingName, int offset)
{
    QWidget *sibling = findChild<QWidget *>(siblingName);
    QWidget *parent = sibling ? sibling->parentWidget() : 0;
    QLayout *layout = parent ? parent->layout() : 0;
    QBoxLayout *blayout = qobject_cast<QBoxLayout *>(layout);

    if (blayout) {
        int index = blayout->indexOf(sibling) + offset;
        blayout->insertWidget(index, widget);
    }
}

QWidget *Page::findWidget(const QString &objectName) const
{
    return findChild<QWidget *>(objectName);
}

void Page::setVisible(bool visible)
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

int Page::nextId() const
{
    return QWizardPage::nextId();
}


// -- QInstallerIntroductionPage

IntroductionPage::IntroductionPage(Installer *inst)
    : Page(inst)
    , m_widget(0)
{
    setObjectName(QLatin1String("IntroductionPage"));
    setTitle(tr("Setup - %1").arg(productName()));
    setPixmap(QWizard::WatermarkPixmap, watermarkPixmap());
    setSubTitle(QString());

    m_msgLabel = new QLabel(this);
    m_msgLabel->setObjectName(QLatin1String("MessageLabel"));
    m_msgLabel->setWordWrap(true);
    m_msgLabel->setText(tr("Welcome to the %1 Setup Wizard.")
        .arg(productName()));

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


// -- LicenseAgreementPage

LicenseAgreementPage::LicenseAgreementPage(Installer *inst)
    : Page(inst)
{
    setTitle(tr("License Agreement"));
    setPixmap(QWizard::LogoPixmap, logoPixmap());
    setPixmap(QWizard::WatermarkPixmap, QPixmap());
    setObjectName(QLatin1String("LicenseAgreementPage"));

    QLabel *msgLabel = new QLabel(tr("Please read the following license agreement(s). You must "
        "accept the terms contained in these agreement(s) before continuing with the installation."));
    msgLabel->setWordWrap(true);

    QGroupBox *licenseBox = new QGroupBox(this);
    licenseBox->setObjectName(QString::fromUtf8("licenseBox"));

    m_licenseListWidget = new QListWidget(licenseBox);
    connect(m_licenseListWidget, SIGNAL(currentItemChanged(QListWidgetItem *, QListWidgetItem *)),
        this, SLOT(currentItemChanged(QListWidgetItem *)));

    QSizePolicy sizePolicy(QSizePolicy::Maximum, QSizePolicy::Expanding);
    sizePolicy.setHeightForWidth(m_licenseListWidget->sizePolicy().hasHeightForWidth());
    m_licenseListWidget->setSizePolicy(sizePolicy);

    m_textBrowser = new QTextBrowser(licenseBox);
    m_textBrowser->setReadOnly(true);
    m_textBrowser->setOpenExternalLinks(true);
    m_textBrowser->setOpenLinks(false);
    connect(m_textBrowser, SIGNAL(anchorClicked(QUrl)), this, SLOT(openLicenseUrl(QUrl)));

    QSizePolicy sizePolicy2(QSizePolicy::Minimum, QSizePolicy::Expanding);
    sizePolicy2.setHeightForWidth(m_textBrowser->sizePolicy().hasHeightForWidth());
    m_textBrowser->setSizePolicy(sizePolicy2);

    QHBoxLayout *licenseBoxLayout = new QHBoxLayout(licenseBox);
    licenseBoxLayout->addWidget(m_licenseListWidget);
    licenseBoxLayout->addWidget(m_textBrowser);

    m_acceptRadioButton = new QRadioButton(this);
    QLabel *acceptLabel = new QLabel(tr("I have read and agree to the following terms contained in "
        "the license agreements accompanying the Qt SDK and additional items. I agree that my use of "
        "the Qt SDK is governed by the terms and conditions contained in these license agreements."));
    acceptLabel->setWordWrap(true);

    m_rejectRadioButton = new QRadioButton(this);
    QLabel *rejectLabel = new QLabel(tr("I do not accept the terms and conditions of the above "
        "listed license agreements. Please note by checking the box, you must cancel the "
        "installation or downloading the Qt SDK and must destroy all copies, or portions thereof, "
        "of the Qt SDK in your possessions."));
    rejectLabel->setWordWrap(true);

    QSizePolicy sizePolicy3(QSizePolicy::Preferred, QSizePolicy::Minimum);
    sizePolicy3.setHeightForWidth(rejectLabel->sizePolicy().hasHeightForWidth());
    sizePolicy3.setVerticalStretch(0);
    sizePolicy3.setHorizontalStretch(0);

    acceptLabel->setSizePolicy(sizePolicy3);
    rejectLabel->setSizePolicy(sizePolicy3);

#if defined(Q_WS_X11) || defined(Q_WS_MAC)
    QFont labelFont(font());
    labelFont.setPixelSize(9);

    msgLabel->setFont(labelFont);
    acceptLabel->setFont(labelFont);
    rejectLabel->setFont(labelFont);
#endif

    QGridLayout *gridLayout = new QGridLayout;
    gridLayout->setColumnStretch(1, 1);
    gridLayout->addWidget(m_acceptRadioButton, 0, 0);
    gridLayout->addWidget(acceptLabel, 0, 1);
    gridLayout->addWidget(m_rejectRadioButton, 1, 0);
    gridLayout->addWidget(rejectLabel, 1, 1);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(msgLabel);
    layout->addWidget(licenseBox);
    layout->addLayout(gridLayout);

    connect(m_acceptRadioButton, SIGNAL(toggled(bool)), this,
        SIGNAL(completeChanged()));
    connect(m_rejectRadioButton, SIGNAL(toggled(bool)), this,
        SIGNAL(completeChanged()));

    m_rejectRadioButton->setChecked(true);
}

void LicenseAgreementPage::initializePage()
{
    m_licenseListWidget->clear();
    m_textBrowser->setText(QLatin1String(""));

    // TODO: this needs to be fixed once we support several root components
    QInstaller::Component *rootComponent = 0;
    foreach (QInstaller::Component* root, installer()->components()) {
        if (root->isInstalled())
            continue;

        const QHash<QString, QPair<QString, QString> > &hash = root->licenses();
        if (!hash.isEmpty()) {
            addLicenseItem(hash);
            rootComponent = root;
        }
    }

    QList<QInstaller::Component*> components = installer()->componentsToInstall(true);
    foreach (QInstaller::Component* component, components) {
        if (rootComponent != component && !component->isInstalled())
            addLicenseItem(component->licenses());
    }

    if (m_licenseListWidget->count() > 0)
        m_licenseListWidget->setCurrentItem(m_licenseListWidget->item(0));
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


// -- ComponentSelectionPage

inline QString unitSizeText(const qint64 size)
{
    if (size < 10000)
        return QString::number(size) + QLatin1Char(' ') + ComponentSelectionPage::tr("Bytes");

    if (size < 1024 * 10000)
        return QString::number(size / 1024) + QLatin1Char(' ') + ComponentSelectionPage::tr("kBytes");

    return QString::number(size / 1024 / 1024) + QLatin1Char(' ') + ComponentSelectionPage::tr("MBytes");
}

static QString niceSizeText(const QString &str)
{
    const qint64 size = str.toLongLong();
    if (size == 0)
        return QString();

    QString msg = ComponentSelectionPage::tr(
        "This component will occupy approximately %1 on your harddisk.");
    return msg.arg(unitSizeText(size));
}


// -- ComponentSelectionPage::Private

class ComponentSelectionPage::Private : public QObject
{
    Q_OBJECT

public:
    Private(ComponentSelectionPage *qq, Installer *installer)
        : q(qq),
        wasshown(false),
        modified(false),
        connected(false),
        m_model(new ComponentModel(installer)),
        m_installer(installer),
        m_treeView(new QTreeView(q))
    {
        m_treeView->setModel(m_model);
        m_treeView->setHeaderHidden(true);
        m_treeView->setMouseTracking(true);
        m_treeView->setObjectName(QLatin1String("ComponentTreeView"));

        for (int i = 1; i < m_treeView->model()->columnCount(); ++i)
            m_treeView->setColumnHidden(i, true);

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
        layout->addLayout(hlayout);

        connect(m_treeView->model(), SIGNAL(dataChanged(QModelIndex, QModelIndex)), q,
            SIGNAL(completeChanged()), Qt::QueuedConnection);
        connect(m_treeView->selectionModel(), SIGNAL(currentChanged(QModelIndex, QModelIndex)),
            this, SLOT(selectionChanged()));
    }

protected Q_SLOTS:
    void selectionChanged()
    {
        m_sizeLabel->clear();
        m_descriptionLabel->clear();

        const QModelIndex index = m_treeView->currentIndex();
        if (index.isValid()) {
            m_descriptionLabel->setText(index.data(Qt::ToolTipRole).toString());
            if (!m_installer->isUninstaller()) {
                m_sizeLabel->setText(niceSizeText(qVariantValue<Component*>(index
                    .data(Qt::UserRole))->value(QString::fromLatin1("UncompressedSize"))));
            }
        }
    }

public:
    ComponentSelectionPage* const q;

    bool wasshown;
    bool modified;
    bool connected;
    ComponentModel* const m_model;
    Installer* const m_installer;
    QTreeView* const m_treeView;
    QLabel *m_descriptionLabel;
    QLabel *m_sizeLabel;
};


// -- ComponentSelectionPage

/*!
    \class QInstaller::ComponentSelectionPage
    On this page the user can select and deselect what he wants to be installed.
*/
ComponentSelectionPage::ComponentSelectionPage(Installer* installer)
    : Page(installer),
    d(new Private(this, installer))
{
    setTitle(tr("Select Components"));
    setPixmap(QWizard::LogoPixmap, logoPixmap());
    setPixmap(QWizard::WatermarkPixmap, QPixmap());
    setObjectName(QLatin1String("ComponentSelectionPage"));

    if (installer->isInstaller())
        setSubTitle(tr("Please select the components you want to install."));
    if (installer->isUninstaller())
        setSubTitle(tr("Please select the components you want to uninstall."));
    if (installer->isPackageManager())
        setSubTitle(tr("Please (de)select the components you want to (un)install."));
}

ComponentSelectionPage::~ComponentSelectionPage()
{
    delete d;
}

void ComponentSelectionPage::entering()
{
    wizard()->button(QWizard::CancelButton)->setEnabled(true);
    if (!d->connected) {
        if (Gui* par = dynamic_cast<Gui*> (wizard())) {
            d->connected = true;
            connect(d->m_model, SIGNAL(workRequested(bool)), par, SLOT(setModified(bool)),
                Qt::QueuedConnection);
            connect(d->m_model, SIGNAL(workRequested(bool)), this, SLOT(setModified(bool)),
                Qt::QueuedConnection);
            connect(d->m_model, SIGNAL(modelReset()), this, SLOT(modelWasReseted()),
                Qt::QueuedConnection);
        }
        if (!d->m_installer->isInstaller())
            setButtonText(QWizard::CancelButton, tr("Close"));
    }
}

void ComponentSelectionPage::modelWasReseted()
{
    if (d->m_treeView->model()->rowCount() == 1) {
        d->wasshown = true;
        d->m_treeView->setExpanded(d->m_treeView->model()->index(0, 0), true);
    }
}

void ComponentSelectionPage::showEvent(QShowEvent* event)
{
    if (!d->wasshown)
        modelWasReseted();
    Page::showEvent(event);
}

/*!
    Selects the component with /a id in the component tree.
*/
void ComponentSelectionPage::selectComponent(const QString& id)
{
    const QModelIndex idx = d->m_model->findComponent(id);
    if (!idx.isValid())
        return;
    d->m_model->setData(idx, Qt::Checked);
}

/*!
    Deselects the component with /a id in the component tree.
*/
void ComponentSelectionPage::deselectComponent(const QString& id)
{
    const QModelIndex idx = d->m_model->findComponent(id);
    if (!idx.isValid())
        return;
    d->m_model->setData(idx, Qt::Unchecked);
}

void ComponentSelectionPage::setModified(bool value)
{
    d->modified = value;
    if (d->m_installer->isInstaller() || value)
        setButtonText(QWizard::CancelButton, tr("Cancel"));
    else
        setButtonText(QWizard::CancelButton, tr("Close"));
}

bool ComponentSelectionPage::isComplete() const
{
    if (d->m_installer->isPackageManager()) {
        // at least component should be different from its previous state
        if (d->modified)
            return true;
        return false;
    }

    // at least one component needs to be selected for (un)installation
    const QList<Component*> components = d->m_installer->components(true);
    for (QList<Component*>::const_iterator it = components.begin(); it != components.end(); ++it) {
        if ((*it)->isSelected() == d->m_installer->isInstaller())
            return true;
    }
    return false;
}


// -- TargetDirectoryPage

TargetDirectoryPage::TargetDirectoryPage(Installer *installer)
    : Page(installer)
{
    setObjectName(QLatin1String("TargetDirectoryPage"));
    setTitle(tr("Installation Folder"));
    setPixmap(QWizard::LogoPixmap, logoPixmap());
    setPixmap(QWizard::WatermarkPixmap, QPixmap());

    QLabel *msgLabel = new QLabel(this);
    msgLabel->setText(tr("Please specify the folder where %1 "
        "will be installed.").arg(productName()));
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
    QString targetDir = installer()->value(QLatin1String("TargetDir"));
    if (targetDir.isEmpty()) {
        targetDir = QDir::homePath() + QDir::separator();
        // prevent spaces in the default target directory
        if (targetDir.contains(QLatin1Char(' ')))
            targetDir = QDir::rootPath();
        targetDir += productName().remove(QLatin1Char(' '));
    }
    m_lineEdit->setText(QDir::toNativeSeparators(QDir(targetDir).absolutePath()));

    Page::initializePage();
}

bool TargetDirectoryPage::validatePage()
{
    if (targetDir().isEmpty()) {
        MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(),
                                    QLatin1String("forbiddenTargetDirectory"), tr("Error"),
                                    tr( "The install directory cannot be empty, please specify a valid folder"),
                                    QMessageBox::Ok);
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
    installer()->setValue(QLatin1String("TargetDir"), targetDir());
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

StartMenuDirectoryPage::StartMenuDirectoryPage(Installer *installer)
    : Page(installer)
{
    setObjectName(QLatin1String("StartMenuDirectoryPage"));
    setTitle(tr("Start Menu shortcuts"));

    setPixmap(QWizard::LogoPixmap, logoPixmap());
    setPixmap(QWizard::WatermarkPixmap, QPixmap());

    QString msg = tr("Select the Start Menu in which you would like to create the program's "
        "shortcuts. You can also enter a name to create a new folder.");

    QLabel *msgLabel = new QLabel(this);
    msgLabel->setText(msg);
    msgLabel->setWordWrap(true);
    msgLabel->setObjectName(QLatin1String("MessageLabel"));

    m_lineEdit = new QLineEdit(this);
    m_lineEdit->setObjectName(QLatin1String("LineEdit"));

    QString startMenuDir = installer->value(QLatin1String("StartMenuDir"));
    if (startMenuDir.isEmpty())
        startMenuDir = productName();
    m_lineEdit->setText(startMenuDir);

    // grab existing start menu folders
    QSettings user(QLatin1String("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\"
        "Explorer\\User Shell Folders"), QSettings::NativeFormat);
    //User Shell Folders uses %USERPROFILE%
    startMenuPath = replaceWindowsEnvironmentVariables(user.value(QLatin1String("Programs"),
        QString()).toString());
    installer->setValue(QLatin1String("DesktopDir"),
        replaceWindowsEnvironmentVariables(user.value(QLatin1String("Desktop")).toString()));

    QDir dir(startMenuPath); // user only dirs
    QStringList dirs = dir.entryList(QDir::AllDirs | QDir::NoDotAndDotDot);

    if (installer->value(QLatin1String("AllUsers")) == QLatin1String("true")) {
        verbose() << "AllUsers set. Using HKEY_LOCAL_MACHINE" << std::endl;
        QSettings system(QLatin1String("HKEY_LOCAL_MACHINE\\Software\\Microsoft\\Windows\\"
            "CurrentVersion\\Explorer\\Shell Folders"), QSettings::NativeFormat);
        startMenuPath = system.value(QLatin1String("Common Programs"), QString()).toString();
        installer->setValue(QLatin1String("DesktopDir"),
            system.value(QLatin1String("Desktop")).toString());

        dir.setPath(startMenuPath); // system only dirs
        dirs += dir.entryList(QDir::AllDirs | QDir::NoDotAndDotDot);
    }

    verbose() << "StartMenuPath: " << startMenuPath;
    verbose() << "DesktopDir:" << installer->value(QLatin1String("DesktopDir")) << std::endl;

    m_listWidget = new QListWidget(this);
    if (!dirs.isEmpty()) {
        dirs.removeDuplicates();
        foreach (const QString &dir, dirs)
            new QListWidgetItem(dir, m_listWidget);
    }

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(msgLabel);
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
    installer()->setValue(QLatin1String("StartMenuDir"), startMenuPath + QDir::separator()
        + startMenuDir());
}

void StartMenuDirectoryPage::currentItemChanged(QListWidgetItem* current)
{
    if (current) {
        QString dir = current->data(Qt::DisplayRole).toString();
        if (!dir.isEmpty())
            dir += QDir::separator();
        setStartMenuDir(dir + installer()->value(QLatin1String("StartMenuDir")));
    }
}


// -- ReadyForInstallationPage

ReadyForInstallationPage::ReadyForInstallationPage(Installer* installer)
    : Page(installer)
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
void ReadyForInstallationPage::initializePage()
{
    setCommitPage(true);

    if (installer()->isUninstaller()) {
        setTitle(tr("Ready to Uninstall"));
        setButtonText(QWizard::CommitButton, tr("Uninstall"));
        msgLabel->setText(tr("Setup is now ready to begin removing %1 from your computer.\nThe "
            "program dir %2 will be deleted completely, including all content in that directory!")
            .arg(productName(),
            QDir::toNativeSeparators(QDir(installer()->value(QLatin1String("TargetDir")))
            .absolutePath())));
    } else if (installer()->isPackageManager()) {
        setTitle(tr("Ready to Update Packages"));
        setButtonText(QWizard::CommitButton, tr("Update"));
        msgLabel->setText(tr("Setup is now ready to begin updating your installation."));
    } else {
        Q_ASSERT(installer()->isInstaller());
        setTitle(tr("Ready to Install"));
        setButtonText(QWizard::CommitButton, tr("Install"));
        msgLabel->setText(tr("Setup is now ready to begin installing %1 on your computer.")
            .arg(productName()));
    }
}

/*!
    \reimp
*/
void ReadyForInstallationPage::entering()
{
    initializePage();

    if (installer()->isUninstaller())
        return;

    const QString target = installer()->value(QLatin1String("TargetDir"));

    const KDSysInfo::Volume vol = KDSysInfo::Volume::fromPath(target);
    const KDSysInfo::Volume tempVolume =
        KDSysInfo::Volume::fromPath(QInstaller::generateTemporaryFileName());
    const bool tempOnSameVolume = vol == tempVolume;

    //there is no better way atm to check this
    if (vol.size().size() == 0 && vol.availableSpace().size() == 0) {
        verbose() << "Could not determine available space on device " << target
            << ". Continue silently." << std::endl;
        return;
    }

    const KDByteSize required(installer()->requiredDiskSpace());
    const KDByteSize tempRequired(installer()->requiredTemporaryDiskSpace());

    const KDByteSize available = vol.availableSpace();
    const KDByteSize tempAvailable = tempVolume.availableSpace();
    const KDByteSize realRequiredTempSpace = KDByteSize(0.1 * tempRequired + tempRequired);
    const KDByteSize realRequiredSpace = KDByteSize(0.5 * required + required);

    const bool tempInstFailure = tempOnSameVolume && available < realRequiredSpace
        + realRequiredTempSpace;

    verbose() << "Disk space check on " << target << ": required=" << required.size()
        << " available=" << available.size() << " size=" << vol.size().size() << std::endl;

    QString tempString;
    if (tempAvailable <  realRequiredTempSpace || tempInstFailure) {
        if (tempOnSameVolume) {
            tempString = tr("Not enough diskspace to store temporary files and the installation, "
                "at least %1 are required").arg(unitSizeText(realRequiredTempSpace + realRequiredSpace));
        } else {
            tempString = tr("Not enough diskspace to store temporary files, at least %1 are required")
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
        msgLabel->setText(tr("The volume you selected for installation seems to have sufficient "
            "space for installation,\nbut there will not more than 1% of the volume's space "
            "available afterwards.\n\n%1").arg(msgLabel->text()));
    } else if (available - required < 100*1024*1024LL) {
        // warn for less than 100MB being free
        msgLabel->setText(tr("The volume you selected for installation seems to have sufficient "
            "space for installation,\nbut there will not more than 100 MB available afterwards.\n\n%1")
            .arg(msgLabel->text()));
    }
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
    On this page the user can see on a progressbar how far the current installation is.
*/
PerformInstallationPage::PerformInstallationPage(Installer *gui)
    : Page(gui)
    , m_performInstallationForm(new PerformInstallationForm(this))
{
    setPixmap(QWizard::LogoPixmap, logoPixmap());
    setPixmap(QWizard::WatermarkPixmap, QPixmap());

    setObjectName(QLatin1String("PerformInstallationPage"));
    setCommitPage(true);

    m_performInstallationForm->setupUi(this);

    m_performInstallationForm->setDetailsWidgetVisible(installer()->isInstaller()
        || installer()->isPackageManager());
    connect(ProgressCoordninator::instance(), SIGNAL(detailTextChanged(QString)),
        m_performInstallationForm, SLOT(appendProgressDetails(QString)));
    connect(ProgressCoordninator::instance(), SIGNAL(detailTextResetNeeded()),
        m_performInstallationForm, SLOT(clearDetailsBrowser()));

    connect(m_performInstallationForm, SIGNAL(showDetailsChanged()), this,
        SLOT(toggleDetailsWereChanged()));

    connect(installer(), SIGNAL(installationStarted()), this, SLOT(installationStarted()));
    connect(installer(), SIGNAL(uninstallationStarted()), this, SLOT(installationStarted()));
    connect(installer(), SIGNAL(installationFinished()), this, SLOT(installationFinished()));
    connect(installer(), SIGNAL(uninstallationFinished()), this, SLOT(installationFinished()));
    connect(installer(), SIGNAL(titleMessageChanged(QString)), this, SLOT(setTitleMessage(QString)));
    connect(this, SIGNAL(setAutomatedPageSwitchEnabled(bool)), installer(),
        SIGNAL(setAutomatedPageSwitchEnabled(bool)));
}

PerformInstallationPage::~PerformInstallationPage()
{
    delete m_performInstallationForm;
}

void PerformInstallationPage::entering()
{
    setComplete(false);

    if (installer()->isUninstaller()) {
        setTitle(tr("Uninstalling %1").arg(installer()->value(QLatin1String("ProductName"))));
        setButtonText(QWizard::CommitButton, tr("Uninstall"));
    } else if (installer()->isPackageManager()) {
        setTitle(tr("Updating component configuration of %1")
            .arg(installer()->value(QLatin1String("ProductName"))));
        setButtonText(QWizard::CommitButton, tr("Update"));
    } else {
        Q_ASSERT(installer()->isInstaller());
        setTitle(tr("Installing %1").arg(installer()->value(QLatin1String("ProductName"))));
        setButtonText(QWizard::CommitButton, tr("Install"));
    }
    m_performInstallationForm->enableDetails();
    emit setAutomatedPageSwitchEnabled(true);
}

void PerformInstallationPage::initializePage()
{
    QWizardPage::initializePage();
    if (installer()->isUninstaller()) {
        wizard()->removePage(QInstaller::Installer::InstallationFinished + 1);
        wizard()->removePage(QInstaller::Installer::InstallationFinished + 2);
        setTitle(tr("Uninstalling %1").arg(installer()->value(QLatin1String("ProductName"))));
        m_commitBtnText = tr("Uninstall");
        setButtonText(QWizard::CommitButton, m_commitBtnText);
        QTimer::singleShot(30, installer(), SLOT(runUninstaller()));
    } else if (installer()->isPackageManager()) {
        setTitle(tr("Updating components of %1").arg(installer()->value(QLatin1String("ProductName"))));
        m_commitBtnText = tr ("Update");
        setButtonText(QWizard::CommitButton, m_commitBtnText);
        QTimer::singleShot(30, installer(), SLOT(runPackageUpdater()));
    } else {
        Q_ASSERT(installer()->isInstaller());
        setTitle(tr("Installing %1").arg(installer()->value(QLatin1String("ProductName"))));
        m_commitBtnText = tr("Install");
        setButtonText(QWizard::CommitButton, m_commitBtnText);
        QTimer::singleShot(30, installer(), SLOT(runInstaller()));
    }
}

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
        setButtonText(QWizard::NextButton, tr("Next"));
        setButtonText(QWizard::CommitButton, tr("Next"));
        setComplete(true);
    }
}

bool PerformInstallationPage::isAutoSwitching() const
{
    return !m_performInstallationForm->isShowingDetails();
}

void PerformInstallationPage::toggleDetailsWereChanged()
{
    const bool needAutoSwitching = isAutoSwitching();
    setButtonText(QWizard::CommitButton, needAutoSwitching ? m_commitBtnText : tr("Next"));
    emit setAutomatedPageSwitchEnabled(needAutoSwitching);
}

void PerformInstallationPage::setTitleMessage(const QString& title)
{
    setTitle(title);
}


// -- FinishedPage

FinishedPage::FinishedPage(Installer *installer)
    : Page(installer)
{
    setObjectName(QLatin1String("FinishedPage"));
    setTitle(tr("Completing the %1 Setup Wizard").arg(productName()));
    setPixmap(QWizard::WatermarkPixmap, watermarkPixmap());
    setSubTitle(QString());

    m_msgLabel = new QLabel(this);
    m_msgLabel->setObjectName(QLatin1String("MessageLabel"));
    m_msgLabel->setWordWrap(true);
#ifdef Q_WS_MAC
    m_msgLabel->setText(tr("Click Done to exit the Setup Wizard"));
#else
    m_msgLabel->setText(tr("Click Finish to exit the Setup Wizard"));
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
    if (wizard()->button(QWizard::BackButton))
        wizard()->button(QWizard::BackButton)->setVisible(false);

    if (installer()->isPackageManager()) {
        setButtonText(QWizard::CommitButton, QLatin1String("Finish"));
        wizard()->button(QWizard::CommitButton)->disconnect(this);
        connect(wizard()->button(QWizard::CommitButton), SIGNAL(clicked()), this,
            SLOT(handleFinishClicked()));
        if (Gui* par = dynamic_cast<Gui*> (wizard()))
            connect(this, SIGNAL(finishClicked()), par, SIGNAL(finishButtonClicked()));
    }

    verbose() << "FINISHED ENTERING: " << std::endl;

    connect(wizard()->button(QWizard::FinishButton), SIGNAL(clicked()), this,
        SLOT(handleFinishClicked()));
    wizard()->setOption(QWizard::NoCancelButton, true);

    if (installer()->status() == Installer::InstallerSucceeded) {
        const QString finishedtext = installer()->value(QLatin1String("FinishedText"));
        if (!finishedtext.isEmpty())
            m_msgLabel->setText(finishedtext);

        if (!installer()->value(QLatin1String("RunProgram")).isEmpty()) {
            m_runItCheckBox->show();
            m_runItCheckBox->setText(installer()->value(QLatin1String("RunProgramDescription"),
                tr("Run %1 now.").arg(productName())));
            return; // job done
        }
    } else {
        setTitle(tr("The %1 Setup Wizard failed").arg(productName()));
    }
    m_runItCheckBox->hide();
    m_runItCheckBox->setChecked(false);
}

void FinishedPage::initializePage()
{
    setCommitPage(true);
    if (wizard()->button(QWizard::BackButton))
        wizard()->button(QWizard::BackButton)->setVisible(false);

    if (installer()->isPackageManager()) {
        setButtonText(QWizard::CommitButton, QLatin1String("Finish"));
        setButtonText(QWizard::NextButton, QLatin1String("Finish"));
    }
}

void FinishedPage::leaving()
{
    disconnect(wizard(), SIGNAL(customButtonClicked(int)), this, SLOT(handleFinishClicked()));
    if (installer()->isPackageManager()) {
        disconnect(wizard()->button(QWizard::CommitButton), SIGNAL(clicked()), this,
            SLOT(handleFinishClicked()));
        if (Gui* par = dynamic_cast<Gui*> (wizard()))
            disconnect(this, SIGNAL(finishClicked()), par, SIGNAL(finishButtonClicked()));
    }
}

void FinishedPage::handleFinishClicked()
{
    QString program = installer()->value(QLatin1String("RunProgram"));
    if (!m_runItCheckBox->isChecked() || program.isEmpty())
        return;
    program = installer()->replaceVariables(program);
    verbose() << "STARTING " << program << std::endl;
    QProcess::startDetached(program);
}


// -- RestartPage

RestartPage::RestartPage(Installer *installer)
    : Page(installer)
{
    setObjectName(QLatin1String("RestartPage"));
    setTitle(tr("Completing the %1 Setup Wizard").arg(productName()));
    setPixmap(QWizard::WatermarkPixmap, watermarkPixmap());
    setSubTitle(QString());
    setFinalPage(false);
}

void RestartPage::entering()
{
    emit restart();
    wizard()->restart();
    if (wizard()->button(QWizard::FinishButton))
        wizard()->button(QWizard::FinishButton)->setVisible(false);
    wizard()->setOption(QWizard::NoCancelButton, false);
}

void RestartPage::leaving()
{
}

#include "qinstallergui.moc"
