/**************************************************************************
**
** This file is part of Installer Framework
**
** Copyright (c) 2012 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (qt-info@nokia.com)
**
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
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** Other Usage
**
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
** If you have questions regarding the use of this file, please contact
** Nokia at qt-info@nokia.com.
**
**************************************************************************/
#include "createlinkoperation.h"

#include "link.h"

#include <QFileInfo>

using namespace QInstaller;

/*
TRANSLATOR QInstaller::CreateLinkOperation
*/

CreateLinkOperation::CreateLinkOperation()
{
    setName(QLatin1String("CreateLink"));
}

void CreateLinkOperation::backup()
{
}

bool CreateLinkOperation::performOperation()
{
    QStringList args = arguments();

    if (args.count() != 2) {
        setError(InvalidArguments);
        setErrorString(QObject::tr("Invalid arguments: %1 arguments given, 2 expected").arg(
            args.count()));
        return false;
    }

    const QString& linkPath = args.at(0);
    const QString& targetPath = args.at(1);
    Link link = Link::create(linkPath, targetPath);

    if (!link.isValid()) {
        setError(UserDefinedError);
        setErrorString(QObject::tr("Could not create link from %1 to %2.").arg(linkPath, targetPath));
        return false;
    }

    return true;
}

bool CreateLinkOperation::undoOperation()
{
    QStringList args = arguments();

    const QString& linkPath = args.at(0);
    const QString& targetPath = args.at(1);

    if (!QFileInfo(linkPath).exists()) {
        return true;
    }
    Link link = Link(linkPath);
    if (!link.remove()) {
        setError(UserDefinedError);
        setErrorString(QObject::tr("Could not remove link from %1 to %2.").arg(linkPath, targetPath));
        return false;
    }

    return !QFileInfo(linkPath).exists();
}

bool CreateLinkOperation::testOperation()
{
    return true;
}

Operation *CreateLinkOperation::clone() const
{
    return new CreateLinkOperation();
}
