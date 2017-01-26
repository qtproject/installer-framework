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
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
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
