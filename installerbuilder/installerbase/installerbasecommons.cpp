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

#include <messageboxhandler.h>
#include <qinstaller.h>
#include <qinstallercomponent.h>

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QTimer>

#include <QtGui/QLabel>
#include <QtGui/QVBoxLayout>

using namespace QInstaller;


// -- TargetDirectoryPageImpl

/*!
    A custom target directory selection based due to the no-space restriction...
*/
TargetDirectoryPageImpl::TargetDirectoryPageImpl(Installer *installer)
  : TargetDirectoryPage(installer)
{
    QPalette palette;
    palette.setColor(QPalette::WindowText, Qt::red);

    m_warningLabel = new QLabel(this);
    m_warningLabel->setPalette(palette);

    insertWidget(m_warningLabel, QLatin1String("MessageLabel"), 2);
}

QString TargetDirectoryPageImpl::targetDirWarning() const
{
    if (targetDir().contains(QLatin1Char(' ')))
        return TargetDirectoryPageImpl::tr("The installation path must not contain any space.");
    return QString();
}

bool TargetDirectoryPageImpl::isComplete() const
{
    const QString warning = targetDirWarning();
    m_warningLabel->setText(warning);

    return warning.isEmpty();
}

bool TargetDirectoryPageImpl::askQuestion(const QString &identifier, const QString &message)
{
    QMessageBox::StandardButton bt =
        MessageBoxHandler::warning(MessageBoxHandler::currentBestSuitParent(), identifier,
        TargetDirectoryPageImpl::tr("Warning"), message, QMessageBox::Yes | QMessageBox::No);
    QTimer::singleShot(100, wizard()->page(nextId()), SLOT(repaint()));
    return bt == QMessageBox::Yes;
}

bool TargetDirectoryPageImpl::failWithWarning(const QString &identifier, const QString &message)
{
    MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(), identifier,
        TargetDirectoryPageImpl::tr("Warning"), message);
    QTimer::singleShot(100, wizard()->page(nextId()), SLOT(repaint()));
    return false;
}

bool TargetDirectoryPageImpl::validatePage()
{
    if (!isVisible())
        return true;

    if (QFileInfo(targetDir()).isDir()) {
        QFileInfo fi2(targetDir() + QDir::separator() + installer()->uninstallerName());
        if (QDir(targetDir()) == QDir::root()) {
            MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(),
                QLatin1String("forbiddenTargetDirectory"), tr("Error"),
                tr("As the install directory is completely deleted installing in %1 is forbidden")
                .arg(QDir::rootPath()), QMessageBox::Ok);
            return false;
        }

        if (fi2.exists()) {
            return askQuestion(QLatin1String("overwriteTargetDirectory"),
                TargetDirectoryPageImpl::tr("The folder you selected exists already and "
                "contains an installation.\nDo you want to overwrite it?"));
        }

        return askQuestion(QLatin1String("overwriteTargetDirectory"),
            tr("You have selected an existing, non-empty folder for installation.\n"
            "Note that it will be completely wiped on uninstallation of this application.\n"
            "It is not advisable to install into this folder as installation might fail.\n"
            "Do you want to continue?"));
    }
    return true;
}


// -- QtInstallerGui

QtInstallerGui::QtInstallerGui(Installer *installer)
    : Gui(installer, 0)
{
    setPage(Installer::Introduction, new IntroductionPage(installer));
    setPage(Installer::TargetDirectory, new TargetDirectoryPageImpl(installer));
    setPage(Installer::ComponentSelection, new ComponentSelectionPage(m_installer));
    setPage(Installer::LicenseCheck, new LicenseAgreementPage(installer));
#ifdef Q_OS_WIN
    setPage(Installer::StartMenuSelection, new StartMenuDirectoryPage(installer));
#endif
    setPage(Installer::ReadyForInstallation, new ReadyForInstallationPage(installer));
    setPage(Installer::PerformInstallation, new PerformInstallationPage(installer));
    setPage(Installer::InstallationFinished, new FinishedPage(installer));

    bool ok = false;
    const int startPage = installer->value(QLatin1String("GuiStartPage")).toInt(&ok);
    if(ok)
        setStartId(startPage);
}

void QtInstallerGui::init()
{
    if(m_installer->components(true).count() == 1) {
        wizardPageVisibilityChangeRequested(false, Installer::ComponentSelection);

        Q_ASSERT(!m_installer->components().isEmpty());
        m_installer->components().first()->setSelected(true);
    }
}


// -- QtUninstallerGui

QtUninstallerGui::QtUninstallerGui(Installer *installer)
    : Gui(installer, 0)
{
    setPage(Installer::ComponentSelection, new ComponentSelectionPage(m_installer));
    setPage(Installer::LicenseCheck, new LicenseAgreementPage(installer));
    setPage(Installer::ReadyForInstallation, new ReadyForInstallationPage(installer));
    setPage(Installer::PerformInstallation, new PerformInstallationPage(installer));
    setPage(Installer::InstallationFinished, new FinishedPage(installer));

    if (installer->isPackageManager()) {
        RestartPage *p = new RestartPage(installer);
        connect(p, SIGNAL(restart()), this, SIGNAL(gotRestarted()));
        setPage(Installer::InstallationFinished + 1, p);
        setPage(Installer::InstallationFinished + 2, new Page(installer));
    }
}

void QtUninstallerGui::init()
{
    if(m_installer->components().isEmpty()) {
        wizardPageVisibilityChangeRequested(false, Installer::ComponentSelection);
        wizardPageVisibilityChangeRequested(false, Installer::LicenseCheck);
    }
}

int QtUninstallerGui::nextId() const
{
    const int next = QWizard::nextId();
    if (next == Installer::LicenseCheck) {
        const int nextNextId = pageIds().value(pageIds().indexOf(next)+ 1, -1);
        if (!m_installer->isPackageManager() && !m_installer->isUpdater())
            return nextNextId;

        QList<Component*> components = m_installer->componentsToInstall(true);
        if (components.isEmpty())
            return nextNextId;

        bool foundLicense = false;
        foreach (Component* component, components) {
            if (component->isInstalled())
                continue;
            foundLicense |= !component->licenses().isEmpty();
        }
        return foundLicense ? next : nextNextId;
    }
    return next;
}


// -- GetMetaInfoProgressWidget

GetMetaInfoProgressWidget::GetMetaInfoProgressWidget(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    setLayout(layout);
    m_label = new QLabel;
    m_label->setWordWrap(true);
    m_label->setText(tr("Retrieving information from remote installation sources..."));
    layout->addWidget(m_label);
}

QString GetMetaInfoProgressWidget::text() const
{
    return m_label->text();
}

void GetMetaInfoProgressWidget::message(KDJob *, const QString &msg)
{
    m_label->setText(msg);
}
