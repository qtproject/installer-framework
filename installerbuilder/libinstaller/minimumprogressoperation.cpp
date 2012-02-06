/**************************************************************************
**
** This file is part of Installer Framework
**
** Copyright (c) 2011-2012 Nokia Corporation and/or its subsidiary(-ies).
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

#include "minimumprogressoperation.h"

using namespace QInstaller;

MinimumProgressOperation::MinimumProgressOperation()
{
    //this shouldn't be call able by script, but we need a name for the binary format
    setName(QLatin1String("MinimumProgress"));
}

void MinimumProgressOperation::backup()
{
}

bool MinimumProgressOperation::performOperation()
{
    progressChanged(1);
    return true;
}

bool MinimumProgressOperation::undoOperation()
{
    progressChanged(1);
    return true;
}

bool MinimumProgressOperation::testOperation()
{
    return true;
}

Operation *MinimumProgressOperation::clone() const
{
    return new MinimumProgressOperation();
}

