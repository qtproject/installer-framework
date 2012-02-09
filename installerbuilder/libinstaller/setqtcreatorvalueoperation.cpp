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

#include "setqtcreatorvalueoperation.h"

#include "qtcreator_constants.h"
#include "updatecreatorsettingsfrom21to22operation.h"
#include "packagemanagercore.h"

#include <QtCore/QSettings>

using namespace QInstaller;

static QString groupName(const QString &groupName)
{
    return groupName == QLatin1String("General") ? QString() : groupName;
}

SetQtCreatorValueOperation::SetQtCreatorValueOperation()
{
    setName(QLatin1String("SetQtCreatorValue"));
}

void SetQtCreatorValueOperation::backup()
{
}

bool SetQtCreatorValueOperation::performOperation()
{
    const QStringList args = arguments();

    if (args.count() != 4) {
        setError(InvalidArguments);
        setErrorString(tr("Invalid arguments in %0: %1 arguments given, exactly 4 expected (rootInstallPath, "
            "group, key, value).").arg(name()).arg( arguments().count()));
        return false;
    }

    const QString &rootInstallPath = args.at(0); //for example "C:\\Nokia_SDK\\"

    const QString &group = groupName(args.at(1));
    const QString &key = args.at(2);
    const QString &settingsValue = args.at(3);
{
    QSettings settings(rootInstallPath + QLatin1String(QtCreatorSettingsSuffixPath), QSettings::IniFormat);
    if (!group.isEmpty())
        settings.beginGroup(group);

    if (settingsValue.contains(QLatin1String(",")))  // comma separated list of strings
        settings.setValue(key, settingsValue.split(QRegExp(QLatin1String("\\s*,\\s*")), QString::SkipEmptyParts));
    else
        settings.setValue(key, settingsValue);

    if (!group.isEmpty())
        settings.endGroup();

    settings.sync(); //be save ;)
} //destruct QSettings

    if (group == QLatin1String("GdbBinaries21")) {
        PackageManagerCore *const core = qVariantValue<PackageManagerCore*>(value(QLatin1String("installer")));
        if (!core) {
            setError(UserDefinedError);
            setErrorString(tr("Needed installer object in %1 operation is empty.").arg(name()));
            return false;
        }
        UpdateCreatorSettingsFrom21To22Operation updateCreatorSettingsOperation;
        updateCreatorSettingsOperation.setValue(QLatin1String("installer"), QVariant::fromValue(core));
        if (!updateCreatorSettingsOperation.performOperation()) {
            setError(updateCreatorSettingsOperation.error());
            setErrorString(updateCreatorSettingsOperation.errorString());
            return false;
        }
    }

    return true;
}

bool SetQtCreatorValueOperation::undoOperation()
{
    const QStringList args = arguments();

    const QString &rootInstallPath = args.at(0); //for example "C:\\Nokia_SDK\\"

    const QString &group = groupName(args.at(1));
    const QString &key = args.at(2);

    QSettings settings(rootInstallPath + QLatin1String(QtCreatorSettingsSuffixPath), QSettings::IniFormat);
    if (!group.isEmpty())
        settings.beginGroup(group);

    settings.remove(key);

    if (!group.isEmpty())
        settings.endGroup();

    return true;
}

bool SetQtCreatorValueOperation::testOperation()
{
    return true;
}

Operation *SetQtCreatorValueOperation::clone() const
{
    return new SetQtCreatorValueOperation();
}
