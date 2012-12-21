/**************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
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

#include "licenseoperation.h"

#include "packagemanagercore.h"
#include "settings.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QTextStream>

using namespace QInstaller;

LicenseOperation::LicenseOperation()
{
    setName(QLatin1String("License"));
}

void LicenseOperation::backup()
{
}

bool LicenseOperation::performOperation()
{
    QVariantMap licenses = value(QLatin1String("licenses")).toMap();
    if (licenses.isEmpty()) {
        setError(UserDefinedError);
        setErrorString(tr("No license files found to copy."));
        return false;
    }

    PackageManagerCore *const core = value(QLatin1String("installer")).value<PackageManagerCore*>();
    if (!core) {
        setError( UserDefinedError );
        setErrorString(tr("Needed installer object in %1 operation is empty.").arg(name()));
        return false;
    }

    QString targetDir = QString::fromLatin1("%1/%2").arg(core->value(scTargetDir),
        QLatin1String("Licenses"));

    QDir dir;
    dir.mkpath(targetDir);
    setArguments(QStringList(targetDir));

    for (QVariantMap::const_iterator it = licenses.begin(); it != licenses.end(); ++it) {
        QFile file(targetDir + QDir::separator() + it.key());
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            setError(UserDefinedError);
            setErrorString(tr("Can not write license file: %1.").arg(targetDir + QDir::separator()
                + it.key()));
            return false;
        }

        QTextStream stream(&file);
        stream << it.value().toString();
    }

    return true;
}

bool LicenseOperation::undoOperation()
{
    QVariantMap licenses = value(QLatin1String("licenses")).toMap();
    if (licenses.isEmpty()) {
        setError(UserDefinedError);
        setErrorString(tr("No license files found to delete."));
        return false;
    }

    QString targetDir = arguments().value(0);
    for (QVariantMap::const_iterator it = licenses.begin(); it != licenses.end(); ++it)
        QFile::remove(targetDir + QDir::separator() + it.key());

    QDir dir;
    dir.rmdir(targetDir);

    return true;
}

bool LicenseOperation::testOperation()
{
    return true;
}

Operation *LicenseOperation::clone() const
{
    return new LicenseOperation();
}
