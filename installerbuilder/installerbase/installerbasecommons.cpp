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
#include "installerbasecommons.h"

#include <component.h>
#include <messageboxhandler.h>
#include <packagemanagercore.h>
#include <settings.h>

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QTimer>

#include <QtGui/QLabel>
#include <QtGui/QProgressBar>
#include <QtGui/QRadioButton>
#include <QtGui/QStackedWidget>
#include <QtGui/QVBoxLayout>

using namespace QInstaller;


// -- IntroductionPageImpl

IntroductionPageImpl::IntroductionPageImpl(QInstaller::PackageManagerCore *core)
    : QInstaller::IntroductionPage(core)
{
    QWidget *widget = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(widget);

    m_packageManager = new QRadioButton(tr("Package manager"), this);
    layout->addWidget(m_packageManager);
    m_packageManager->setChecked(core->isPackageManager());
    connect(m_packageManager, SIGNAL(toggled(bool)), this, SLOT(setPackageManager(bool)));

    m_updateComponents = new QRadioButton(tr("Update components"), this);
    layout->addWidget(m_updateComponents);
    m_updateComponents->setChecked(core->isUpdater());
    connect(m_updateComponents, SIGNAL(toggled(bool)), this, SLOT(setUpdater(bool)));

    m_removeAllComponents = new QRadioButton(tr("Remove all components"), this);
    layout->addWidget(m_removeAllComponents);
    m_removeAllComponents->setChecked(core->isUninstaller());
    connect(m_removeAllComponents, SIGNAL(toggled(bool)), this, SLOT(setUninstaller(bool)));
    connect(m_removeAllComponents, SIGNAL(toggled(bool)), core, SLOT(setCompleteUninstallation(bool)));

    layout->addItem(new QSpacerItem(1, 1, QSizePolicy::Minimum, QSizePolicy::Expanding));

    m_label = new QLabel(this);
    m_label->setWordWrap(true);
    m_label->setText(tr("Retrieving information from remote installation sources..."));
    layout->addWidget(m_label);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 0);
    layout->addWidget(m_progressBar);

    layout->addItem(new QSpacerItem(1, 1, QSizePolicy::Minimum, QSizePolicy::Expanding));

    m_errorLabel = new QLabel(this);
    m_errorLabel->setWordWrap(true);
    layout->addWidget(m_errorLabel);

    widget->setLayout(layout);
    setWidget(widget);

    core->setCompleteUninstallation(core->isUninstaller());
}

int IntroductionPageImpl::nextId() const
{
    if (packageManagerCore()->isUninstaller())
        return PackageManagerCore::ReadyForInstallation;

    if (packageManagerCore()->isUpdater() || packageManagerCore()->isPackageManager())
        return PackageManagerCore::ComponentSelection;

    return QInstaller::IntroductionPage::nextId();
}

void IntroductionPageImpl::showAll()
{
    showWidgets(true);
}

void IntroductionPageImpl::hideAll()
{
    showWidgets(false);
}

void IntroductionPageImpl::showMetaInfoUdate()
{
    showWidgets(false);
    m_label->setVisible(true);
    m_progressBar->setVisible(true);
}

void IntroductionPageImpl::showMaintenanceTools()
{
    showWidgets(true);
    m_label->setVisible(false);
    m_progressBar->setVisible(false);
}

void IntroductionPageImpl::setMaintenanceToolsEnabled(bool enable)
{
    m_packageManager->setEnabled(enable);
    m_updateComponents->setEnabled(enable);
    m_removeAllComponents->setEnabled(enable);
}

void IntroductionPageImpl::setMessage(const QString &msg)
{
    m_label->setText(msg);
}

void IntroductionPageImpl::setErrorMessage(const QString &error)
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
}

void IntroductionPageImpl::setUpdater(bool value)
{
    if (value) {
        packageManagerCore()->setUpdater();
        emit initUpdater();
    }
}

void IntroductionPageImpl::setUninstaller(bool value)
{
    if (value) {
        packageManagerCore()->setUninstaller();
        emit initUninstaller();
    }
}

void IntroductionPageImpl::setPackageManager(bool value)
{
    if (value) {
        packageManagerCore()->setPackageManager();
        emit initPackageManager();
    }
}

void IntroductionPageImpl::showWidgets(bool show)
{
    m_label->setVisible(show);
    m_progressBar->setVisible(show);
    m_packageManager->setVisible(show);
    m_updateComponents->setVisible(show);
    m_removeAllComponents->setVisible(show);
}


// -- TargetDirectoryPageImpl

/*!
    A custom target directory selection based due to the no-space restriction...
*/
TargetDirectoryPageImpl::TargetDirectoryPageImpl(PackageManagerCore *core)
  : TargetDirectoryPage(core)
{
    QPalette palette;
    palette.setColor(QPalette::WindowText, Qt::red);

    m_warningLabel = new QLabel(this);
    m_warningLabel->setPalette(palette);

    insertWidget(m_warningLabel, QLatin1String("MessageLabel"), 2);
}

QString TargetDirectoryPageImpl::targetDirWarning() const
{
    if (targetDir().isEmpty()) {
        return TargetDirectoryPageImpl::tr("The installation path cannot be empty, please specify a valid "
            "folder.");
    }

    if (QDir(targetDir()).isRelative()) {
        return TargetDirectoryPageImpl::tr("The installation path cannot be relative, please specify an "
            "absolute path.");
    }

    QString dir = targetDir();
#ifdef Q_OS_WIN
    // remove e.g. "c:"
    dir = dir.mid(2);
#endif
    // check if there are not allowed characters in the target path
    if (dir.contains(QRegExp(QLatin1String("[!@#$%^&*: ,;]")))) {
        return TargetDirectoryPageImpl::tr("The installation path must not contain !@#$%^&*:,; or spaces, "
            "please specify a valid folder.");
    }

    return QString();
}

bool TargetDirectoryPageImpl::isComplete() const
{
    m_warningLabel->setText(targetDirWarning());
    return m_warningLabel->text().isEmpty();
}

bool TargetDirectoryPageImpl::askQuestion(const QString &identifier, const QString &message)
{
    QMessageBox::StandardButton bt =
        MessageBoxHandler::warning(MessageBoxHandler::currentBestSuitParent(), identifier,
        TargetDirectoryPageImpl::tr("Warning"), message, QMessageBox::Yes | QMessageBox::No);
    QTimer::singleShot(100, wizard()->page(nextId()), SLOT(repaint()));

    return bt == QMessageBox::Yes;
}

bool TargetDirectoryPageImpl::failWithError(const QString &identifier, const QString &message)
{
    MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(), identifier,
        TargetDirectoryPageImpl::tr("Error"), message);
    QTimer::singleShot(100, wizard()->page(nextId()), SLOT(repaint()));

    return false;
}

bool TargetDirectoryPageImpl::validatePage()
{
    if (!isVisible())
        return true;

    const QString remove = packageManagerCore()->value(QLatin1String("RemoveTargetDir"));
    if (!QVariant(remove).toBool())
        return true;

    const QDir dir(targetDir());
    // the directory exists and is empty...
    if (dir.exists() && dir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty())
        return true;

    const QFileInfo fi(targetDir());
    if (fi.isDir()) {
        if (dir == QDir::root() || dir == QDir::home()) {
            return failWithError(QLatin1String("ForbiddenTargetDirectory"), tr("As the install directory "
                "is completely deleted installing in %1 is forbidden.").arg(QDir::rootPath()));
        }

        QString fileName = packageManagerCore()->settings().uninstallerName();
#if defined(Q_WS_MAC)
        if (QFileInfo(QCoreApplication::applicationDirPath() + QLatin1String("/../..")).isBundle())
            fileName += QLatin1String(".app/Contents/MacOS/") + fileName;
#elif defined(Q_OS_WIN)
        fileName += QLatin1String(".exe");
#endif

        QFileInfo fi2(targetDir() + QDir::separator() + fileName);
        if (fi2.exists()) {
            return askQuestion(QLatin1String("OverwriteTargetDirectory"),
                TargetDirectoryPageImpl::tr("The folder you selected exists already and contains an "
                "installation.\nDo you want to overwrite it?"));
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


// -- InstallerGui

InstallerGui::InstallerGui(PackageManagerCore *core)
    : PackageManagerGui(core, 0)
{
    setPage(PackageManagerCore::Introduction, new IntroductionPageImpl(core));
    setPage(PackageManagerCore::TargetDirectory, new TargetDirectoryPageImpl(core));
    setPage(PackageManagerCore::ComponentSelection, new ComponentSelectionPage(core));
    setPage(PackageManagerCore::LicenseCheck, new LicenseAgreementPage(core));
#ifdef Q_OS_WIN
    setPage(PackageManagerCore::StartMenuSelection, new StartMenuDirectoryPage(core));
#endif
    setPage(PackageManagerCore::ReadyForInstallation, new ReadyForInstallationPage(core));
    setPage(PackageManagerCore::PerformInstallation, new PerformInstallationPage(core));
    setPage(PackageManagerCore::InstallationFinished, new FinishedPage(core));

    bool ok = false;
    const int startPage = core->value(QLatin1String("GuiStartPage")).toInt(&ok);
    if(ok)
        setStartId(startPage);
}

void InstallerGui::init()
{
}

int InstallerGui::nextId() const
{
    const int next = QWizard::nextId();
    if (next == PackageManagerCore::LicenseCheck) {
        PackageManagerCore *const core = packageManagerCore();
        const int nextNextId = pageIds().value(pageIds().indexOf(next)+ 1, -1);
        if (!core->isInstaller())
            return nextNextId;

        core->calculateComponentsToInstall();
        foreach (Component* component, core->orderedComponentsToInstall()) {
            if (!component->licenses().isEmpty())
                return next;
        }
        return nextNextId;
    }
    return next;
}


// -- MaintenanceGui

MaintenanceGui::MaintenanceGui(PackageManagerCore *core)
    : PackageManagerGui(core, 0)
{
    IntroductionPageImpl *intro = new IntroductionPageImpl(core);
    connect(intro, SIGNAL(initUpdater()), this, SLOT(updateRestartPage()));
    connect(intro, SIGNAL(initUninstaller()), this, SLOT(updateRestartPage()));
    connect(intro, SIGNAL(initPackageManager()), this, SLOT(updateRestartPage()));

    setPage(PackageManagerCore::Introduction, intro);
    setPage(PackageManagerCore::ComponentSelection, new ComponentSelectionPage(core));
    setPage(PackageManagerCore::LicenseCheck, new LicenseAgreementPage(core));
    setPage(PackageManagerCore::ReadyForInstallation, new ReadyForInstallationPage(core));
    setPage(PackageManagerCore::PerformInstallation, new PerformInstallationPage(core));
    setPage(PackageManagerCore::InstallationFinished, new FinishedPage(core));

    RestartPage *p = new RestartPage(core);
    connect(p, SIGNAL(restart()), this, SIGNAL(gotRestarted()));
    setPage(PackageManagerCore::InstallationFinished + 1, p);

    if (core->isUninstaller())
        wizardPageVisibilityChangeRequested(false, PackageManagerCore::InstallationFinished + 1);
}

void MaintenanceGui::init()
{
}

int MaintenanceGui::nextId() const
{
    const int next = QWizard::nextId();
    if (next == PackageManagerCore::LicenseCheck) {
        PackageManagerCore *const core = packageManagerCore();
        const int nextNextId = pageIds().value(pageIds().indexOf(next)+ 1, -1);
        if (!core->isPackageManager() && !core->isUpdater())
            return nextNextId;

        core->calculateComponentsToInstall();
        foreach (Component* component, core->orderedComponentsToInstall()) {
            if (component->isInstalled())
                continue;
            if (!component->licenses().isEmpty())
                return next;
        }
        return nextNextId;
    }
    return next;
}

void MaintenanceGui::updateRestartPage()
{
    wizardPageVisibilityChangeRequested((packageManagerCore()->isUninstaller() ? false : true),
        PackageManagerCore::InstallationFinished + 1);
}
