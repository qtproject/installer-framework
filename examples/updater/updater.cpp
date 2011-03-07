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

#include <QDateTime>
#include <QDomDocument>
#include <QProgressDialog>

#include "common/binaryformat.h"
#include "common/binaryformatenginehandler.h"
#include "common/errors.h"

#include "componentselectiondialog.h"
#include "qinstaller.h"
#include "qinstallercomponent.h"
#include "qinstallercomponentmodel.h"
#include "updatesettings.h"
#include "init.h"

#include <KDUpdater/Application>
#include <KDUpdater/PackagesInfo>

#include <KDToolsCore/KDAutoPointer>

#include <iostream>

using namespace QInstaller;

class Updater::Private
{
public:
    KDUpdater::Application updaterapp;
    Installer installer;
};

Updater::Updater( QObject* parent )
    : QObject( parent )
{
    QInstaller::init();
}

Updater::~Updater()
{
}

bool Updater::checkForUpdates( bool checkonly )
{
    d->installer.setLinearComponentList( true );
    d->installer.setUpdaterApplication(&d->updaterapp);

    std::auto_ptr< QInstallerCreator::BinaryFormatEngineHandler > handler( new QInstallerCreator::BinaryFormatEngineHandler( QInstallerCreator::ComponentIndex() ) );
    handler->setComponentIndex( QInstallerCreator::ComponentIndex() );

    UpdateSettings settings;
    KDAutoPointer< ComponentSelectionDialog > dialog( checkonly ? 0 : new ComponentSelectionDialog( &d->installer ) );
    if( !checkonly )
        dialog->show();

    ComponentModel::setVirtualComponentsVisible( true );

    try
    {
        KDAutoPointer< QProgressDialog > progress( checkonly ? 0 : new QProgressDialog( dialog.get() ) );
        if( !checkonly )
        {
            progress->setLabelText( tr( "Checking for updates..." ) );
            progress->setRange( 0, 0 );
            progress->show();
        }

        settings.setLastCheck( QDateTime::currentDateTime() );
        d->installer.setRemoteRepositories( settings.repositories() );
        d->installer.setValue( QLatin1String( "TargetDir" ), QFileInfo( d->updaterapp.packagesInfo()->fileName() ).absolutePath() );
    }
    catch( const Error& error )
    {
        if( !checkonly )
            QMessageBox::critical( dialog.get(), tr( "Check for Updates" ), tr( "Error while checking for updates:\n%1" ).arg( error.what() ) );
        settings.setLastResult( tr( "Software Update failed." ) );
        return false;
    }
    catch( ... )
    {
        if( !checkonly )
            QMessageBox::critical( dialog.get(), tr( "Check for Updates" ), tr( "Unknown error while checking for updates." ) );
        settings.setLastResult( tr( "Software Update failed." ) );
        return false;
    }

    const QList< Component* > components = d->installer.components( true );

    // no updates for us
    if( components.isEmpty() && !checkonly )
    {
        QMessageBox::information( dialog.get(), tr( "Check for Updates" ), tr( "There are currently no updates available for you." ) );
        return false;
    }

    if( checkonly )
    {
        QDomDocument doc;
        QDomElement root = doc.createElement( QLatin1String( "updates" ) );
        doc.appendChild( root );
        for( QList< Component* >::const_iterator it = components.begin(); it != components.end(); ++it )
        {
            QDomElement update = doc.createElement( QLatin1String( "update" ) );
            update.setAttribute( QLatin1String( "name" ), (*it)->value( QLatin1String( "DisplayName" ) ) );
            update.setAttribute( QLatin1String( "version" ), (*it)->value( QLatin1String( "Version" ) ) );
            update.setAttribute( QLatin1String( "size" ), (*it)->value( QLatin1String( "UncompressedSize" ) ) );
            root.appendChild( update );
        }
        std::cout << doc.toString( 4 ).toStdString() << std::endl;
        return true;
    }

    if( dialog->exec() == QDialog::Rejected )
        return false;

    try
    {
        QProgressDialog dialog;
        dialog.setRange( 0, 100 );
        dialog.show();
        connect( &dialog, SIGNAL( canceled() ), &d->installer, SLOT( interrupt() ) );
        connect( &d->installer, SIGNAL( installationProgressTextChanged( QString ) ), &dialog, SLOT( setLabelText( QString ) ) );
        connect( &d->installer, SIGNAL( installationProgressChanged( int ) ), &dialog, SLOT( setValue( int ) ) );
        d->installer.installSelectedComponents();
    }
    catch( const Error& error )
    {
        QMessageBox::critical( dialog.get(), tr( "Check for Updates" ), tr( "Error while installing updates:\n%1" ).arg( error.what() ) );
        d->installer.rollBackInstallation();
        settings.setLastResult( tr( "Software Update failed." ) );
        return false;
    }
    catch( ... )
    {
        QMessageBox::critical( dialog.get(), tr( "Check for Updates" ), tr( "Unknown error while installing updates." ) );
        d->installer.rollBackInstallation();
        settings.setLastResult( tr( "Software Update failed." ) );
        return false;
    }

    return true;
}
