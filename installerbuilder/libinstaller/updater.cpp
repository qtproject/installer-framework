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
#include "updater.h"

#include "common/binaryformat.h"
#include "common/errors.h"
#include "common/utils.h"
#include "componentselectiondialog.h"
#include "init.h"
#include "installationprogressdialog.h"
#include "messageboxhandler.h"
#include "qinstaller.h"
#include "qinstallercomponent.h"
#include "qinstallercomponentmodel.h"
#include "updatesettings.h"

#include <KDUpdater/Application>
#include <KDUpdater/PackagesInfo>

#include <QtCore/QDateTime>

#include <QtGui/QProgressDialog>

#include <QtXml/QDomDocument>

using namespace QInstaller;
using namespace QInstallerCreator;


// -- Updater::Private

class Updater::Private
{
public:
    Private()
        : dialog(0) {}

    Installer *installer_shared;
    ComponentSelectionDialog *dialog;
    QSharedPointer<QInstallerCreator::BinaryFormatEngineHandler> handler;
    QSharedPointer<UpdateSettings> settings;
    QList<QInstaller::Component*> components;
};


// -- Updater

Updater::Updater(QObject* parent)
    : QObject(parent)
    , d(new Private)
{
}

Updater::~Updater()
{
    delete d;
}

void Updater::init()
{
    d->settings = QSharedPointer<UpdateSettings> (new UpdateSettings());
    d->handler =
        QSharedPointer<BinaryFormatEngineHandler> (new BinaryFormatEngineHandler(ComponentIndex()));
}

void Updater::setVerbose(bool verbose)
{
    QInstaller::setVerbose(verbose);
}

void Updater::setInstaller(QInstaller::Installer *installer)
{
    d->installer_shared = installer;
}

bool Updater::checkForUpdates(bool checkonly)
{
    QInstaller::init();

    BinaryContent content = BinaryContent::readFromApplicationFile();
    content.registerEmbeddedQResources();

    if (content.magicmaker == MagicInstallerMarker) {
        QMessageBox::information(0, tr("Check for Updates"),
            tr("Impossible to use an installer to check for updates!"));
        return false;
    }

    Installer installer(content.magicmaker, content.performedOperations);
    installer.setUpdater();
    installer.setLinearComponentList(true);

    KDUpdater::Application updaterapp;
    installer.setUpdaterApplication(&updaterapp);

    QSharedPointer<UpdateSettings> settings = QSharedPointer<UpdateSettings> (new UpdateSettings());;
    QScopedPointer<ComponentSelectionDialog> dialog(checkonly
        ? 0 : new ComponentSelectionDialog(&installer));
    if (!checkonly) {
        d->settings = settings;
        setInstaller(&installer);
        setUpdaterGui(dialog.data());
        dialog->show();
    }

    ComponentModel::setVirtualComponentsVisible(true);

    try {
        QScopedPointer<QProgressDialog> progress(checkonly ? 0 : new QProgressDialog(dialog.data()));
        if (!checkonly) {
            progress->setLabelText(tr("Checking for updates..."));
            progress->setRange(0, 0);
            progress->show();
        }

        settings->setLastCheck(QDateTime::currentDateTime());
        installer.setRemoteRepositories(settings->repositories());
        if (installer.status() == QInstaller::Installer::Failure)
            return false;
        installer.setValue(QLatin1String("TargetDir"),
            QFileInfo(updaterapp.packagesInfo()->fileName()).absolutePath());
    } catch (const Error& error) {
        QMessageBox::critical(dialog.data(), tr("Check for Updates"),
            tr("Error while checking for updates:\n%1").arg(QString::fromStdString(error.what())));
        settings->setLastResult(tr("Software Update failed."));
        return false;
    } catch (...) {
        QMessageBox::critical(dialog.data(), tr("Check for Updates"),
            tr("Unknown error while checking for updates."));
        settings->setLastResult(tr("Software Update failed."));
        return false;
    }

    const QList<QInstaller::Component*> components = installer.components(true, UpdaterMode);

    // no updates for us
    if (components.isEmpty()) {
        QMessageBox::information(dialog.data(), tr("Check for Updates"),
            tr("There are currently no updates available."));
        return false;
    }

    if (checkonly) {
        QDomDocument doc;
        QDomElement root = doc.createElement(QLatin1String("updates"));
        doc.appendChild(root);

        QList< QInstaller::Component* >::const_iterator it;
        for (it = components.begin(); it != components.end(); ++it) {
            QDomElement update = doc.createElement(QLatin1String("update"));
            update.setAttribute(QLatin1String("name"), (*it)->value(QLatin1String("DisplayName")));
            update.setAttribute(QLatin1String("version"), (*it)->value(QLatin1String("Version")));
            update.setAttribute(QLatin1String("size"), (*it)->value(QLatin1String("UncompressedSize")));
            root.appendChild(update);
        }
        QMessageBox::information(dialog.data(), tr("Check for Updates"), doc.toString(4));
        return true;
    }

    if (dialog->exec() == QDialog::Rejected)
        return false;

    try {
        InstallationProgressDialog* dialog = new InstallationProgressDialog();
        dialog->setModal(true);
        dialog->setAttribute(Qt::WA_DeleteOnClose, true);
        dialog->setFixedSize(480, 360);
        dialog->show();

        connect(dialog, SIGNAL(canceled()), &installer, SLOT(interrupt()));
        connect(&installer, SIGNAL(installationProgressTextChanged(QString)), dialog,
            SLOT(changeInstallationProgressText(QString)));
        connect(&installer, SIGNAL(installationProgressChanged(int)), dialog,
            SLOT(changeInstallationProgress(int)));
        connect(&installer, SIGNAL(updateFinished()), dialog, SLOT(finished()));
        installer.installSelectedComponents();

        QEventLoop loop;
        connect(dialog, SIGNAL(destroyed()), &loop, SLOT(quit()));
        loop.exec();
    } catch (const Error& error) {
        //if the user clicked the confirm cancel dialog he don't want to see another messagebox
        if (installer.status() != Installer::Canceled) {
            QMessageBox::critical(dialog.data(), tr("Check for Updates"),
                tr("Error while installing updates:\n%1").arg(QString::fromStdString(error.what())));
        }
        installer.rollBackInstallation();
        settings->setLastResult(tr("Software Update failed."));
        return false;
    } catch (...) {
        QMessageBox::critical(dialog.data(), tr("Check for Updates"),
            tr("Unknown error while installing updates."));
        installer.rollBackInstallation();
        settings->setLastResult(tr("Software Update failed."));
        return false;
    }

    return true;
}

ComponentSelectionDialog* Updater::updaterGui() const
{
    return d->dialog;
}

void Updater::setUpdaterGui(QInstaller::ComponentSelectionDialog *gui)
{
    if (d->dialog)
        disconnect(d->dialog, SIGNAL(requestUpdate()), this, SLOT(update()));

    d->dialog = gui;
    connect(gui, SIGNAL(requestUpdate()), this, SLOT(update()));
}

bool Updater::update()
{
    try {
        InstallationProgressDialog* dialog = new InstallationProgressDialog(d->dialog);
        dialog->setAttribute(Qt::WA_DeleteOnClose, true);
        dialog->setModal(true);
        dialog->setFixedSize(480, 360);
        dialog->show();

        connect(dialog, SIGNAL(canceled()), d->installer_shared, SLOT(interrupt()));
        connect(d->installer_shared, SIGNAL(updateFinished()), dialog, SLOT(finished()));
        d->installer_shared->installSelectedComponents();

        QEventLoop loop;
        connect(dialog, SIGNAL(destroyed()), &loop, SLOT(quit()));
        loop.exec();
    } catch (const Error& error) {
        //if the user clicked the confirm cancel dialog he don't want to see another messagebox
        if (d->installer_shared->status() != Installer::Canceled) {
            MessageBoxHandler::critical(d->dialog, QLatin1String("updaterCriticalInstallError"),
                tr("Check for Updates"), tr("Error while installing updates:\n%1").arg(error.message()));
        }
        d->installer_shared->rollBackInstallation();
        d->settings->setLastResult(tr("Software Update failed."));
        emit updateFinished(true);
        return false;
    } catch (...) {
        MessageBoxHandler::critical(d->dialog, QLatin1String("updaterCriticalUnknownError"),
            tr("Check for Updates"), tr("Unknown error while installing updates."));
        d->installer_shared->rollBackInstallation();
        d->settings->setLastResult(tr("Software Update failed."));
        emit updateFinished(true);
        return false;
    }
    emit updateFinished(false);
    return true;
}

bool Updater::searchForUpdates()
{
    try {
        QSharedPointer<QProgressDialog> progress(new QProgressDialog(d->dialog));
        progress->setLabelText(tr("Checking for updates..."));
        progress->setRange(0, 0);
        connect(progress.data(), SIGNAL(canceled()), d->dialog, SLOT(reject()));
        progress->show();

        d->settings->setLastCheck(QDateTime::currentDateTime());
        d->installer_shared->setRemoteRepositories(d->settings->repositories());
        d->installer_shared->setValue(QLatin1String("TargetDir"),
            QFileInfo(d->installer_shared->updaterApplication().packagesInfo()->fileName())
            .absolutePath());
    } catch (const Error& error) {
        QMessageBox::critical(d->dialog, tr("Check for Updates"),
            tr("Error while checking for updates:\n%1").arg(QString::fromStdString(error.what())));
        d->settings->setLastResult(tr("Software Update failed."));
        return false;
    } catch(...) {
        QMessageBox::critical(d->dialog, tr("Check for Updates"),
            tr("Unknown error while checking for updates."));
        d->settings->setLastResult(tr("Software Update failed."));
        return false;
    }

    d->components = d->installer_shared->components(true, UpdaterMode);

    // no updates for us
    if (d->components.isEmpty()) {
        QMessageBox::information(d->dialog, tr("Check for Updates"),
            tr("There are currently no updates available."));
        return false;
    }
    return true;
}
