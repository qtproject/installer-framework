/**************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
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
