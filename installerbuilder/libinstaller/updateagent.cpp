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
#include "updateagent.h"

#include "updatesettings.h"

#include <memory>

#include <QDateTime>
#include <QTimer>

#include <KDUpdater/Application>

#include "qinstaller.h"
#include "qinstallercomponent.h"

#include "common/binaryformatenginehandler.h"
#include "common/binaryformat.h"

using namespace QInstaller;

class UpdateAgent::Private
{
public:
    Private( UpdateAgent* qq )
        : q( qq )
    {
        connect( &checkTimer, SIGNAL( timeout() ), q, SLOT( maybeCheck() ) );
        checkTimer.start( 1000 );
    }

private:
    UpdateAgent* const q;

public:
    void maybeCheck()
    {
        checkTimer.stop();
            
        UpdateSettings settings;
        
        try
        {
            if( settings.updateInterval() > 0 && settings.lastCheck().secsTo( QDateTime::currentDateTime() ) >= settings.updateInterval() )
            {
                std::auto_ptr< QInstallerCreator::BinaryFormatEngineHandler > handler( new QInstallerCreator::BinaryFormatEngineHandler( QInstallerCreator::ComponentIndex() ) );
                handler->setComponentIndex( QInstallerCreator::ComponentIndex() );

                settings.setLastCheck( QDateTime::currentDateTime() );
                
                Installer installer;
                installer.setRemoteRepositories( settings.repositories() );
                QList< Component* > components = installer.components();
            
                // remove all unimportant updates
                if( settings.checkOnlyImportantUpdates() )
                {
                    for( int i = components.count() - 1; i >= 0; --i )
                    {
                        const Component* const comp = components[ i ];
                        if( comp->value( QLatin1String( "Important" ) ).toLower() != QLatin1String( "true" ) )
                            components.removeAt( i );
                    }
                }
            
                settings.setLastResult( tr( "Software Update run successfully." ) );

                // no updates available
                if( components.isEmpty() )
                    return;

                emit q->updatesAvailable();
            }
        }
        catch( ... )
        {
            settings.setLastResult( tr( "Software Update failed." ) );
        }

        checkTimer.start();
    }

    QTimer checkTimer;
};

UpdateAgent::UpdateAgent( QObject* parent )
    : QObject( parent ),
      d( new Private( this ) )
{
}

UpdateAgent::~UpdateAgent()
{
}

#include "moc_updateagent.cpp"
