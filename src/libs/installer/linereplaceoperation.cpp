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
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
**************************************************************************/

#include "linereplaceoperation.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QTextStream>

using namespace QInstaller;

LineReplaceOperation::LineReplaceOperation()
{
    setName(QLatin1String("LineReplace"));
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
    if (args.count() != 3) {
        setError(InvalidArguments);
        setErrorString(tr("Invalid arguments in %0: %1 arguments given, exactly 3 expected.").arg(name()).arg(args
            .count()));
        return false;
    }
    const QString fileName = args.at(0);
    const QString searchString = args.at(1);
    const QString replaceString = args.at(2);

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        setError(UserDefinedError);
        setErrorString(QObject::tr("Failed to open %1 for reading.").arg(fileName));
        return false;
    }

    QString replacement;
    QTextStream stream(&file);
    while (!stream.atEnd()) {
        const QString line = stream.readLine();
        if (line.trimmed().startsWith(searchString))
            replacement.append(replaceString + QLatin1String("\n"));
        else
            replacement.append(line + QLatin1String("\n"));
    }
    file.close();

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        setError(UserDefinedError);
        setErrorString(QObject::tr("Failed to open %1 for writing.").arg(fileName));
        return false;
    }

    stream.setDevice(&file);
    stream << replacement;
    file.close();

    return true;
}

bool LineReplaceOperation::undoOperation()
{
    return true;
}

bool LineReplaceOperation::testOperation()
{
    return true;
}

Operation *LineReplaceOperation::clone() const
{
    return new LineReplaceOperation();
}
