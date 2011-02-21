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
#include "updaterplugin.h"

#include <QAbstractButton>
#include <QApplication>
#include <QFileInfo>
#include <QMessageBox>
#include <QProgressDialog>
#include <QStringList>
#include <QtPlugin>

#include "componentselectiondialog.h"
#include "updateagent.h"
#include "updatesettings.h"
#include "updatesettingsdialog.h"

#include "common/binaryformat.h"
#include "common/binaryformatenginehandler.h"
#include "common/errors.h"
#include "common/installersettings.h"
#include "init.h"
#include "qinstaller.h"
#include "updatersettingspage.h"

#include <coreplugin/actionmanager/actionmanager.h>
#include <coreplugin/coreconstants.h>
#include <coreplugin/icore.h>

#include <KDUpdater/Application>
#include <KDUpdater/PackagesInfo>

#include <KDToolsCore/KDAutoPointer>
#include <KDToolsCore/KDSelfRestarter>

using namespace Updater;
using namespace Updater::Internal;
using namespace QInstaller;

class UpdaterPlugin::Private
{
public:
    Private( UpdaterPlugin* qq )
        : q( qq ),
          agent( 0 )
    {
        QInstaller::init();
    }

private:
    UpdaterPlugin* const q;

public:
    void checkForUpdates()
    {
        std::auto_ptr< QInstallerCreator::BinaryFormatEngineHandler > handler( new QInstallerCreator::BinaryFormatEngineHandler( QInstallerCreator::ComponentIndex() ) );
        handler->setComponentIndex( QInstallerCreator::ComponentIndex() );
        
        UpdateSettings settings;
        
        try
        {
            settings.setLastCheck( QDateTime::currentDateTime() );
            installer.setRemoteRepositories( settings.repositories() );
        
            // no updates for us
            if( installer.components().isEmpty() )
            {
                QMessageBox::information( qApp->activeWindow(), tr( "Check for Updates" ), tr( "There are currently no updates available for you." ) );
                return;
            }

            // set the target directory to the actual one
            installer.setValue( QLatin1String( "TargetDir" ), QFileInfo( updaterapp.packagesInfo()->fileName() ).absolutePath() );

            // this will automatically mork components as to get installed
            ComponentSelectionDialog componentSelection( &installer );
            if( componentSelection.exec() == QDialog::Rejected )
                return;

            QProgressDialog dialog;
            dialog.setRange( 0, 100 );
            dialog.show();
            connect( &dialog, SIGNAL( canceled() ), &installer, SLOT( interrupt() ) );
            connect( &installer, SIGNAL( installationProgressTextChanged( QString ) ), &dialog, SLOT( setLabelText( QString ) ) );
            connect( &installer, SIGNAL( installationProgressChanged( int ) ), &dialog, SLOT( setValue( int ) ) );
            installer.installSelectedComponents();
            updatesInstalled();
        }
        catch( const QInstaller::Error& error )
        {
            QMessageBox::critical( qApp->activeWindow(), tr( "Check for Updates" ), tr( "Error while installing updates:\n%1" ).arg( error.what() ) );
            installer.rollBackInstallation();
            settings.setLastResult( tr( "Software Update failed." ) );
        }
        catch( ... )
        {
            QMessageBox::critical( qApp->activeWindow(), tr( "Check for Updates" ), tr( "Unknown error while installing updates." ) );
            installer.rollBackInstallation();
            settings.setLastResult( tr( "Software Update failed." ) );
        }
    }

    void updatesAvailable()
    {
        KDAutoPointer< QMessageBox > box( new QMessageBox( qApp->activeWindow() ) );
        box->setWindowTitle( tr( "Updates Available" ) );
        box->setText( tr( "Software updates are available for your computer. Do you want to install them?" ) );
        box->setStandardButtons( QMessageBox::Yes | QMessageBox::No );
        box->button( QMessageBox::Yes )->setText( tr( "Continue" ) );
        box->button( QMessageBox::No )->setText( tr( "Not Now" ) );
        box->exec();
        if ( !box )
            return;
        if ( box->clickedButton() == box->button( QMessageBox::Yes ) )
            checkForUpdates();
    }

    void updatesInstalled()
    {
        // only ask that dumb question if a SelfUpdateOperation was executed
        if( !KDSelfRestarter::restartOnQuit() )
        {
            QMessageBox::information( qApp->activeWindow(), tr( "Updates Installed" ), tr( "Installation complete." ) );
            return;
        }

        KDAutoPointer< QMessageBox > box( new QMessageBox( qApp->activeWindow() ) );
        box->setWindowTitle( tr( "Updates Installed" ) );
        box->setText( tr( "Installation complete, you need to restart the application for the changes to take effect." ) );
        box->setStandardButtons( QMessageBox::Yes | QMessageBox::No );
        box->button( QMessageBox::Yes )->setText( tr( "Restart Now" ) );
        box->button( QMessageBox::No )->setText( tr( "Restart Later" ) );
        box->exec();
        if ( !box )
            return;
        if ( box->clickedButton() == box->button( QMessageBox::Yes ) )
            QCoreApplication::quit();
        else
            KDSelfRestarter::setRestartOnQuit( false );
    }

    void configureUpdater()
    {
        UpdateSettingsDialog dialog( qApp->activeWindow() );
        connect( &dialog, SIGNAL( checkForUpdates() ), q, SLOT( checkForUpdates() ), Qt::QueuedConnection );
        dialog.exec();
    }

    KDUpdater::Application updaterapp;
    Installer installer;
    UpdateAgent* agent;
};

UpdaterPlugin::UpdaterPlugin()
    : d( new Private( this ) )
{
    if( !KDSelfRestarter::hasInstance() )
    {
        const KDSelfRestarter* const restarter = new KDSelfRestarter;
        Q_UNUSED( restarter )
    }
}

UpdaterPlugin::~UpdaterPlugin()
{
}

/*!
 \reimpl
*/
bool UpdaterPlugin::initialize( const QStringList& arguments, QString* error_message )
{
    Q_UNUSED( arguments )

    try
    {
        InstallerSettings::fromFileAndPrefix( QLatin1String( ":/metadata/installer-config/config.xml" ), QLatin1String( ":/metadata/installer-config/" ) );
    }
    catch ( const Error& e ) 
    {
        if( error_message )
            *error_message = e.message();
        return false;
    }


    Core::ActionManager* const am = Core::ICore::instance()->actionManager();
    Core::ActionContainer* const ac = am->actionContainer( Core::Constants::M_FILE );

    UpdateSettings::setSettingsSource( Core::ICore::instance()->settings() );

    d->agent = new UpdateAgent( this );

    QAction* const checkForUpdates = ac->menu()->addAction( tr( "Check for Updates" ), this, SLOT( checkForUpdates() ) );
    checkForUpdates->setMenuRole( QAction::ApplicationSpecificRole );

    QAction* const updateSettings = ac->menu()->addAction( tr( "Updater Settings" ), this, SLOT( configureUpdater() ) );
    updateSettings->setMenuRole( QAction::ApplicationSpecificRole );

    UpdaterSettingsPage* const page = new UpdaterSettingsPage;
    connect( page, SIGNAL( checkForUpdates() ), this, SLOT( checkForUpdates() ) );
    addAutoReleasedObject( page );

    if( error_message )
        error_message->clear();
    return true;
}

/*!
 \reimpl
*/
void UpdaterPlugin::shutdown()
{
}

/*!
 \reimpl
*/
void UpdaterPlugin::extensionsInitialized()
{
}

#include "moc_updaterplugin.cpp"

Q_EXPORT_PLUGIN( UpdaterPlugin )
