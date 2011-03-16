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
#include "mainwindow.h"

#include <memory>

#include <QAbstractButton>
#include <QApplication>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QProgressDialog>
#include <QStringList>

#include <KDToolsCore/KDAutoPointer>
#include <KDToolsCore/KDSelfRestarter>

#include <KDUpdater/Application>
#include <KDUpdater/PackagesInfo>
#include <KDUpdater/UpdateFinder>
#include <KDUpdater/UpdateSourcesInfo>

#include "common/binaryformatenginehandler.h"
#include "common/binaryformat.h"
#include "componentselectiondialog.h"
#include "getrepositorymetainfojob.h"
#include "common/errors.h"
#include "common/fileutils.h"
#include "init.h"
#include "updateagent.h"

#include "updatesettings.h"
#include "updatesettingsdialog.h"

using namespace QInstaller;

MainWindow::MainWindow( const QStringList& args, QWidget* parent ) : QMainWindow( parent ) {

    QInstaller::init();

    QMenuBar* mbar = menuBar();
    QMenu* fm = mbar->addMenu( QObject::tr("File") );
    fm->addAction( QObject::tr("Check for Updates"), this, SLOT(checkForUpdates()), QKeySequence( QLatin1String("Ctrl+U") ) );
    fm->addAction( QObject::tr("Update Settings"), this, SLOT(editUpdateSettings()) );
    fm->addAction( QObject::tr("Quit"), QApplication::instance(), SLOT(quit()), QKeySequence( QLatin1String("Ctrl+Q") ) );
    QLabel* label = new QLabel( this );
    label->setAlignment( Qt::AlignCenter );
    setCentralWidget( label );    
    label->setText( QString::fromLatin1("Version: %1\n").arg( m_installer.settings().applicationVersion() ) + args.join( QLatin1String(" ") ) );
    updaterapp.packagesInfo()->setApplicationName( m_settings.applicationName() );
    updaterapp.packagesInfo()->setApplicationVersion( m_settings.applicationVersion() );

    UpdateAgent* const agent = new UpdateAgent( this );
    connect( agent, SIGNAL( updatesAvailable() ), this, SLOT( updatesAvailable() ) );
}

void MainWindow::editUpdateSettings()
{
    UpdateSettingsDialog dialog( this );
    connect( &dialog, SIGNAL( checkForUpdates() ), this, SLOT( checkForUpdates() ) );
    dialog.exec();
}

void MainWindow::checkForUpdates() {
    std::auto_ptr< QInstallerCreator::BinaryFormatEngineHandler > handler( new QInstallerCreator::BinaryFormatEngineHandler( QInstallerCreator::ComponentIndex() ) );
    handler->setComponentIndex( QInstallerCreator::ComponentIndex() );
        
    UpdateSettings settings;
        
    try
    {
        m_installer.setRemoteRepositories( settings.repositories() );
        settings.setLastCheck( QDateTime::currentDateTime() );
        
        // no updates for us
        if( m_installer.components(false, UpdaterMode).isEmpty() )
        {
            QMessageBox::information( this, tr( "Check for Updates" ), tr( "There are currently no updates available for you." ) );
            return;
        }

        // set the target directory to the actual one
        m_installer.setValue( QLatin1String( "TargetDir" ), QFileInfo( updaterapp.packagesInfo()->fileName() ).absolutePath() );

        // this will automatically mork components as to get installed
        ComponentSelectionDialog componentSelection( &m_installer, this );
        if( componentSelection.exec() == QDialog::Rejected )
            return;

        QProgressDialog dialog( this );
        dialog.setRange( 0, 100 );
        dialog.show();
        connect( &dialog, SIGNAL( canceled() ), &m_installer, SLOT( interrupt() ) );
        connect( &m_installer, SIGNAL( installationProgressTextChanged( QString ) ), &dialog, SLOT( setLabelText( QString ) ) );
        connect( &m_installer, SIGNAL( installationProgressChanged( int ) ), &dialog, SLOT( setValue( int ) ) );
        m_installer.installSelectedComponents();
        updatesInstalled();
    }
    catch( const QInstaller::Error& error )
    {
        QMessageBox::critical( this, tr( "Check for Updates" ), tr( "Error while installing updates:\n%1" ).arg( error.what() ) );
        m_installer.rollBackInstallation();
        settings.setLastResult( tr( "Software Update failed." ) );
    }
    catch( ... )
    {
        QMessageBox::critical( this, tr( "Check for Updates" ), tr( "Unknown error while installing updates." ) );
        m_installer.rollBackInstallation();
        settings.setLastResult( tr( "Software Update failed." ) );
    }
}

void MainWindow::updatesAvailable()
{
    KDAutoPointer<QMessageBox> box( new QMessageBox( this ) );
    box->setWindowTitle( tr("Updates Available") );
    box->setText( tr("Software updates are available for your computer. Do you want to install them?") );
    box->setStandardButtons( QMessageBox::Yes|QMessageBox::No );
    box->button( QMessageBox::Yes )->setText( tr( "Continue" ) );
    box->button( QMessageBox::No )->setText( tr( "Not Now" ) );
    box->exec();
    if ( !box )
        return;
    if ( box->clickedButton() == box->button( QMessageBox::Yes ) )
        checkForUpdates();
}

void MainWindow::updatesInstalled()
{
    // only ask that dumb question if a SelfUpdateOperation was executed
    if( !KDSelfRestarter::restartOnQuit() )
    {
        QMessageBox::information( this, tr( "Updates Installed" ), tr( "Installation complete." ) );
        return;
    }

    KDAutoPointer<QMessageBox> box( new QMessageBox( this ) );
    box->setWindowTitle( tr("Updates Installed" ) );
    box->setText( tr("Installation complete, you need to restart the application for the changes to take effect.") );
    box->setStandardButtons( QMessageBox::Yes|QMessageBox::No );
    box->button( QMessageBox::Yes )->setText( tr("Restart Now") );
    box->button( QMessageBox::No )->setText( tr("Restart Later") );
    box->exec();
    if ( !box )
        return;
    if ( box->clickedButton() == box->button( QMessageBox::Yes ) )
        QCoreApplication::quit();
    else
        KDSelfRestarter::setRestartOnQuit( false );
}

