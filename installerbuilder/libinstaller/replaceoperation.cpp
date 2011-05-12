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
#include "replaceoperation.h"
#include <qinstaller.h>
#include "common/utils.h"

#include <QFile>
#include <QDir>
#include <QDebug>

using namespace QInstaller;

ReplaceOperation::ReplaceOperation()
{
    setName(QLatin1String("Replace"));
}

ReplaceOperation::~ReplaceOperation()
{
}

void ReplaceOperation::backup()
{
}

bool ReplaceOperation::performOperation()
{
    const QStringList args = arguments();

    // Arguments:
    // 1. filename
    // 2. Source-String
    // 3. Replace-String
    if (args.count() != 3) {
        setError(InvalidArguments);
        setErrorString(tr("Invalid arguments in %0: %1 arguments given, 3 expected.")
                        .arg(name()).arg( args.count() ) );
        return false;
    }
    const QString currentFileName = args.at(0);
    const QString beforeString = args.at(1);
    const QString afterString = args.at(2);
//    QString debugString( QLatin1String("Replacing %1 to %2 in %3") );
//    debugString = debugString.arg(beforeString);
//    debugString = debugString.arg(afterString);
//    debugString = debugString.arg(currentFileName);
//    verbose() << debugString;

    QFile file(currentFileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        setError(UserDefinedError);
        setErrorString(QObject::tr("Failed to open %1 for reading").arg(currentFileName));
        return false;
    }

    QString replacedFileContent = QString::fromLocal8Bit(file.readAll());
    replacedFileContent.replace(beforeString, afterString);
    file.close();

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        setError(UserDefinedError);
        setErrorString(QObject::tr("Failed to open %1 for writing").arg(currentFileName));
        return false;
    }

    //TODO: check that toAscii is right(?)
    file.write(replacedFileContent.toLocal8Bit());
    file.close();

    return true;
}

bool ReplaceOperation::undoOperation()
{
    // Need to remove settings again
    return true;
}

bool ReplaceOperation::testOperation()
{
    return true;
}

KDUpdater::UpdateOperation* ReplaceOperation::clone() const
{
    return new ReplaceOperation();
}
