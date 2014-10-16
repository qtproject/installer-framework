/**************************************************************************
**
** Copyright (C) 2012-2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
**
** $QT_END_LICENSE$
**
**************************************************************************/

#include "replaceoperation.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QTextStream>

using namespace QInstaller;

ReplaceOperation::ReplaceOperation()
{
    setName(QLatin1String("Replace"));
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
        setErrorString(tr("Invalid arguments in %0: %1 arguments given, %2 expected%3.")
            .arg(name()).arg(arguments().count()).arg(tr("exactly 3"), QLatin1String("")));
        return false;
    }
    const QString fileName = args.at(0);
    const QString before = args.at(1);
    const QString after = args.at(2);

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(UserDefinedError);
        setErrorString(tr("Failed to open %1 for reading").arg(fileName));
        return false;
    }

    QTextStream stream(&file);
    QString replacedFileContent = stream.readAll();
    file.close();

    if (!file.open(QIODevice::WriteOnly)) {
        setError(UserDefinedError);
        setErrorString(tr("Failed to open %1 for writing").arg(fileName));
        return false;
    }

    stream.setDevice(&file);
    stream << replacedFileContent.replace(before, after);
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

Operation *ReplaceOperation::clone() const
{
    return new ReplaceOperation();
}
