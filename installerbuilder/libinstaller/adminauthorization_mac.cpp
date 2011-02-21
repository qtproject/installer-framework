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
#include "adminauthorization.h"

#include <Security/Authorization.h>
#include <Security/AuthorizationTags.h>

#include <unistd.h>

#include <QStringList>
#include <QVector>

class AdminAuthorization::Private
{
public:
    Private()
        : auth( 0 )
    {
    }

    AuthorizationRef auth;
};

AdminAuthorization::AdminAuthorization()
{
    AuthorizationCreate( 0, kAuthorizationEmptyEnvironment, kAuthorizationFlagDefaults, &d->auth );
}

AdminAuthorization::~AdminAuthorization()
{
    AuthorizationFree( d->auth, kAuthorizationFlagDestroyRights );
}

bool AdminAuthorization::authorize()
{
    if( geteuid() == 0 )
        setAuthorized();

    if( isAuthorized() )
        return true;

    AuthorizationItem item = { kAuthorizationRightExecute, 0, NULL, 0 };
    const AuthorizationRights rights = { 1, &item };
    
    const AuthorizationFlags flags = kAuthorizationFlagDefaults | kAuthorizationFlagInteractionAllowed | kAuthorizationFlagPreAuthorize | kAuthorizationFlagExtendRights;
    
    const int result = AuthorizationCopyRights( d->auth, &rights, 0, flags, 0 );
    if( result != 0 )
        return false;

    seteuid( 0 );
    setAuthorized();
    emit authorized();
    return true;
}
   
bool AdminAuthorization::execute( QWidget*, const QString& program, const QStringList& arguments )
{
    const QByteArray utf8Program = program.toUtf8();
    const char* const prog = utf8Program.data();
    QVector< QByteArray > utf8Args;
    QVector< char* > args;
    for( QStringList::const_iterator it = arguments.begin(); it != arguments.end(); ++it )
    {
        utf8Args.push_back( it->toUtf8() );
        args.push_back( utf8Args.last().data() );
    }
    args.push_back( 0 );

    const AuthorizationFlags flags = kAuthorizationFlagDefaults;
    
    const int result = AuthorizationExecuteWithPrivileges( d->auth, prog, flags, args.data(), 0 );
    return result == 0;
}
