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
#include "registerfiletypeoperation.h"

#include <QSettings>

// this makes us use QSettingsWrapper
#include "fsengineclient.h"

using namespace QInstaller;

RegisterFileTypeOperation::RegisterFileTypeOperation()
{
    setName( QLatin1String( "RegisterFileType" ) );
}

RegisterFileTypeOperation::~RegisterFileTypeOperation()
{
}

void RegisterFileTypeOperation::backup()
{
}

bool RegisterFileTypeOperation::performOperation()
{
    // extension
    // command
    // (description)
    // (content type)
    // (icon)
#ifdef Q_WS_WIN
    if (arguments().count() < 2 || arguments().count() > 5) {
        setError(InvalidArguments);
        setErrorString(tr("Invalid arguments in %0").arg(name()));
        return false;
    }
    const QString extension = arguments()[ 0 ];
    const QString command = arguments()[ 1 ];
    const QString description = arguments().count() > 2 ? arguments()[ 2 ] : QString(); // optional
    const QString contentType = arguments().count() > 3 ? arguments()[ 3 ] : QString(); // optional
    const QString icon = arguments().count() > 4 ? arguments()[ 4 ] : QString(); // optional
    const QString className = QString::fromLatin1( "%1_auto_file" ).arg( extension );
    const QString settingsPrefix = QString::fromLatin1( "Software/Classes/" );
    
    //QSettings settings( QLatin1String( "HKEY_CLASSES_ROOT" ), QSettings::NativeFormat );
    QSettings settings(QLatin1String("HKEY_CURRENT_USER"), QSettings::NativeFormat);
    // first backup the old values
    setValue(QLatin1String("oldClass"), settings.value(QString::fromLatin1("%1.%2/Default").arg(settingsPrefix, extension)));
    setValue(QLatin1String("oldContentType"), settings.value(QString::fromLatin1("%1.%2/Content Type").arg(settingsPrefix, extension)));

    // the file extension was not yet registered. Let's do so now
    settings.setValue(QString::fromLatin1("%1.%2/Default").arg(settingsPrefix, extension), className);

    // register the content type, if given
    if (!contentType.isEmpty())
        settings.setValue(QString::fromLatin1("%1.%2/Content Type").arg(settingsPrefix, extension), contentType);

    //const QString className = settings.value( QString::fromLatin1( ".%1/Default" ).arg( extension ) ).toString();
    //const QString oldClassName = value( QString::fromLatin1( ".%1/Default" ).arg( extension ) ).toString();
    setValue(QLatin1String("className"), className);

//    setValue( QLatin1String( "oldDescription" ), settings.value( QString::fromLatin1( "%1/Default" ).arg( oldClassName ) ) );
//    setValue( QLatin1String( "oldIcon" ), settings.value( QString::fromLatin1( "%1/DefaultIcon/Default" ).arg( oldClassName ) ) );
//    setValue( QLatin1String( "oldCommand" ), settings.value( QString::fromLatin1( "%1/shell/Open/Command/Default" ).arg( oldClassName ) ) );

    // register the description, if given
    if (!description.isEmpty())
        settings.setValue(QString::fromLatin1("%1%2/Default").arg(settingsPrefix, className), description);

    // register the icon, if given
    if (!icon.isEmpty()) {
        settings.setValue(QString::fromLatin1("%1%2/DefaultIcon/Default").arg(settingsPrefix,
            className), icon);
    }

    // register the command to open the file
    settings.setValue(QString::fromLatin1("%1%2/shell/Open/Command/Default").arg(settingsPrefix,
        className), command );

    return true;
#else
    setError(UserDefinedError);
    setErrorString(QObject::tr("Registering file types in only supported on Windows."));
    return false;
#endif
}

bool RegisterFileTypeOperation::undoOperation()
{
    // extension
    // command
    // (description)
    // (content type)
    // (icon)
#ifdef Q_WS_WIN
    if (arguments().count() < 2 || arguments().count() > 5) {
        setErrorString(tr("Register File Type: Invalid arguments"));
        return false;
    }
    const QString extension = arguments()[0];
    const QString command = arguments()[1];
    const QString description = arguments().count() > 2 ? arguments()[2] : QString(); // optional
    const QString contentType = arguments().count() > 3 ? arguments()[3] : QString(); // optional
    const QString icon = arguments().count() > 4 ? arguments()[4] : QString(); // optional
    const QString className = QString::fromLatin1("%1_auto_file").arg(extension);
    const QString settingsPrefix = QString::fromLatin1("Software/Classes/");
    
    QSettings settings(QLatin1String("HKEY_CURRENT_USER"), QSettings::NativeFormat);

    const QString restoredClassName = value(QLatin1String("oldClassName")).toString();
    // register the command to open the file
    if (settings.value(QString::fromLatin1("%1%2/shell/Open/Command/Default").arg(settingsPrefix, className)).toString() != command)
        return false;
    if (settings.value(QString::fromLatin1("%1%2/DefaultIcon/Default").arg(settingsPrefix, className)).toString() != icon)
        return false;
    if (settings.value(QString::fromLatin1("%1%2/Default").arg(settingsPrefix, className)).toString() != description)
        return false;
    if (settings.value(QString::fromLatin1("%1.%2/Content Type").arg(settingsPrefix, extension)).toString() != contentType)
        return false;
    if (settings.value(QString::fromLatin1("%1.%2/Default").arg(settingsPrefix, extension)).toString() !=  className)
        return false;

    const QVariant oldCommand = value(QLatin1String("oldCommand"));
    if (!oldCommand.isNull()) {
        settings.setValue(QString::fromLatin1("%1%2/shell/Open/Command/Default").arg(settingsPrefix,
            restoredClassName ), oldCommand);
    } else {
        settings.remove(QString::fromLatin1("%1%2/shell/Open/Command/Default").arg(settingsPrefix,
            className));
    }

    // register the icon, if given

    const QVariant oldIcon = value(QLatin1String("oldIcon"));
    if (!oldIcon.isNull()) {
        settings.setValue(QString::fromLatin1("%1%2/DefaultIcon/Default").arg( settingsPrefix,
            restoredClassName), oldIcon);
    } else {
        settings.remove(QString::fromLatin1("%1%2/DefaultIcon/Default").arg(settingsPrefix,
            className));
    }

    // register the description, if given

    const QVariant oldDescription = value(QLatin1String("oldDescription"));
    if (!oldDescription.isNull()) {
        settings.setValue(QString::fromLatin1("%1%2/Default").arg(settingsPrefix, restoredClassName),
            oldDescription );
    } else {
        settings.remove(QString::fromLatin1("%1%2/Default").arg(settingsPrefix, className));
    }

    // content type
    settings.remove(QString::fromLatin1("%1%2").arg(settingsPrefix, className));

    const QVariant oldContentType = value( QLatin1String( "oldContentType" ) );
    if( !oldContentType.isNull() )
        settings.setValue( QString::fromLatin1( "%1.%2/Content Type" ).arg( settingsPrefix, extension ), oldContentType );
    else
        settings.remove( QString::fromLatin1( "%1.%2/Content Type" ).arg( settingsPrefix, extension ) );
    
    // class
        
    const QVariant oldClass = value( QLatin1String( "oldClass" ) );
    if( !oldClass.isNull() )
        settings.setValue( QString::fromLatin1( "%1.%2/Default" ).arg( settingsPrefix, extension ), oldClass );
    else
        settings.remove( QString::fromLatin1( "%1.%2" ).arg( settingsPrefix, extension ) );

    return true;
#else
    setErrorString( QObject::tr( "Registering file types in only supported on Windows." ) );
    return false;
#endif
}

bool RegisterFileTypeOperation::testOperation()
{
    return true;
}

RegisterFileTypeOperation* RegisterFileTypeOperation::clone() const
{
    return new RegisterFileTypeOperation();
}
