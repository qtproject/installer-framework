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
#include "linereplaceoperation.h"
#include <packagemanagercore.h>
#include "common/utils.h"

#include <QFile>
#include <QDir>
#include <QDebug>
#include <QBuffer>

using namespace QInstaller;

LineReplaceOperation::LineReplaceOperation()
{
    setName(QLatin1String("LineReplace"));
}

LineReplaceOperation::~LineReplaceOperation()
{
}

void LineReplaceOperation::backup()
{
}

bool LineReplaceOperation::performOperation()
{
    const QStringList args = arguments();

    // Arguments:
    // 1. filename
    // 2. startsWith Search-String
    // 3. Replace-Line-String
    if ( args.count() != 3 ) {
        setError( InvalidArguments );
        setErrorString( tr("Invalid arguments in %0: %1 arguments given, 3 expected.")
                        .arg(name()).arg( args.count() ) );
        return false;
    }
    const QString currentFileName = args.at(0);
    const QString searchString = args.at(1);
    const QString replaceString = args.at(2);
    QString debugString( QLatin1String("Replacing lines with %1 to %2 in %3") );
    debugString = debugString.arg(searchString);
    debugString = debugString.arg(replaceString);
    debugString = debugString.arg(currentFileName);
    verbose() << debugString;

    QFile file(currentFileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        setError( UserDefinedError );
        setErrorString( QObject::tr( "Failed to open %1 for reading" ).arg( currentFileName ) );
        return false;
    }

    QByteArray memorybyteArray;

    while (!file.atEnd()) {
        QByteArray line = file.readLine();
        if (QString::fromLatin1(line).trimmed().startsWith(searchString)) {
            memorybyteArray.append(replaceString.toLatin1()).append("\n");
        } else {
            memorybyteArray.append(line);
        }
    }
    file.close();

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        setError( UserDefinedError );
        setErrorString( QObject::tr( "Failed to open %1 for writing" ).arg( currentFileName ) );
        return false;
    }

    file.write( memorybyteArray );
    file.close();

    return true;
}

bool LineReplaceOperation::undoOperation()
{
    // Need to remove settings again
    return true;
}

bool LineReplaceOperation::testOperation()
{
    return true;
}

KDUpdater::UpdateOperation* LineReplaceOperation::clone() const
{
    return new LineReplaceOperation();
}
