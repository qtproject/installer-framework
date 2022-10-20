/**************************************************************************
**
** Copyright (C) 2020 The Qt Company Ltd.
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
#include "fileutils.h"
#include "globals.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QTextStream>

using namespace QInstaller;

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::LicenseOperation
    \internal
*/

LicenseOperation::LicenseOperation(PackageManagerCore *core)
    : UpdateOperation(core)
{
    setName(QLatin1String("License"));
}

void LicenseOperation::backup()
{
}

bool LicenseOperation::performOperation()
{
    QVariantMap licenses = value(scLicensesValue).toMap();
    if (licenses.isEmpty()) {
        setError(UserDefinedError);
        setErrorString(tr("No license files found to copy."));
        return false;
    }

    PackageManagerCore *const core = packageManager();
    if (!core) {
        setError( UserDefinedError );
        setErrorString(tr("Needed installer object in %1 operation is empty.").arg(name()));
        return false;
    }

    QString targetDir = QString::fromLatin1("%1%2%3").arg(core->value(scTargetDir),
        QDir::separator(), QLatin1String("Licenses"));

    QDir dir;
    dir.mkpath(targetDir);
    setDefaultFilePermissions(targetDir, DefaultFilePermissions::Executable);
    setArguments(QStringList(targetDir));

    for (QVariantMap::const_iterator it = licenses.constBegin(); it != licenses.constEnd(); ++it) {
        QFile file(targetDir + QLatin1Char('/') + it.key());
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            setError(UserDefinedError);
            setErrorString(tr("Can not write license file \"%1\".").arg(QDir::toNativeSeparators(file.fileName())));
            return false;
        }

        QString outString;
        QTextStream stream(&outString);
        stream << it.value().toString();

        file.write(outString.toUtf8());
    }

    return true;
}

bool LicenseOperation::undoOperation()
{
    const QVariantMap licenses = value(scLicensesValue).toMap();
    if (licenses.isEmpty()) {
        qCWarning(QInstaller::lcInstallerInstallLog) << "No license files found to delete.";
        return true;
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
