/**************************************************************************
**
** Copyright (C) 2022 The Qt Company Ltd.
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
#include "settingsoperation.h"
#include "packagemanagercore.h"
#include "updateoperations.h"
#include "qsettingswrapper.h"
#include "globals.h"

#include <QDir>
#include <QDebug>

using namespace QInstaller;

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::SettingsOperation
    \internal
*/

SettingsOperation::SettingsOperation(PackageManagerCore *core)
    : UpdateOperation(core)
{
    setName(QLatin1String("Settings"));
    setRequiresUnreplacedVariables(true);
}

void SettingsOperation::backup()
{
}

bool SettingsOperation::checkArguments()
{
    const QString path = argumentKeyValue(QLatin1String("path"));
    const QString method = argumentKeyValue(QLatin1String("method"));
    const QString key = argumentKeyValue(QLatin1String("key"));
    const QString aValue = argumentKeyValue(QLatin1String("value"));

    QStringList missingArguments;
    if (path.isEmpty())
        missingArguments << QLatin1String("path");
    if (method.isEmpty())
        missingArguments << QLatin1String("method");
    if (key.isEmpty())
        missingArguments << QLatin1String("key");
    if (method != QLatin1String("remove") && aValue.isEmpty())
        missingArguments << QLatin1String("value");

    if (!missingArguments.isEmpty()) {
        setError(InvalidArguments);
        setErrorString(tr("Missing argument(s) \"%1\" calling %2 with arguments \"%3\".").arg(
            missingArguments.join(QLatin1String("; ")), name(), arguments().join(QLatin1String("; "))));
        return false;
    }
    QStringList possibleMethodValues;
    possibleMethodValues << QLatin1String("set") << QLatin1String("remove") <<
        QLatin1String("add_array_value") << QLatin1String("remove_array_value");

    if (!possibleMethodValues.contains(method)) {
        setError(InvalidArguments);
        setErrorString(tr("Current method argument calling \"%1\" with arguments \"%2\" is not "
            "supported. Please use set, remove, add_array_value, or remove_array_value.").arg(name(),
            arguments().join(QLatin1String("; "))));
        return false;
    }
    return true;
}


bool SettingsOperation::performOperation()
{
    // Arguments:
    // 1. path=settings file path or registry path
    // 2. method=set|remove|add_array_value|remove_array_value
    // 3. key=can be prepended by a category name separated by slash
    // 4. value=just the value
    // optional arguments are
    // formate=native or ini TODO
    // backup=true or false (default is true) TODO
    // NOTE: remove and remove_array_value will do nothing at the undostep

    if (!checkArguments())
        return false;
    QString path = argumentKeyValue(QLatin1String("path"));
    const QString method = argumentKeyValue(QLatin1String("method"));
    const QString key = argumentKeyValue(QLatin1String("key"));
    QString aValue = argumentKeyValue(QLatin1String("value"));

    if (requiresUnreplacedVariables()) {
        if (PackageManagerCore *const core = packageManager()) {
            path = core->replaceVariables(path);
            aValue = core->replaceVariables(aValue);
        }
    }

    // use MkdirOperation to get the path so it can remove it with MkdirOperation::undoOperation later
    KDUpdater::MkdirOperation mkDirOperation;
    mkDirOperation.setArguments(QStringList() << QFileInfo(path).absolutePath());
    mkDirOperation.backup();
    if (!mkDirOperation.performOperation()) {
        setError(mkDirOperation.error());
        setErrorString(mkDirOperation.errorString());
        return false;
    }
    setValue(QLatin1String("createddir"), mkDirOperation.value(QLatin1String("createddir")));

    QSettingsWrapper settings(path, QSettings::IniFormat);
    if (method == QLatin1String("set"))
        settings.setValue(key, aValue);
    else if (method == QLatin1String("remove"))
        settings.remove(key);
    else if (method == QLatin1String("add_array_value")) {
        QVariant valueVariant = settings.value(key);
        if (valueVariant.canConvert<QStringList>()) {
            QStringList array = valueVariant.toStringList();
            array.append(aValue);
            settings.setValue(key, array);
        } else {
            settings.setValue(key, aValue);
        }
    } else if (method == QLatin1String("remove_array_value")) {
        QVariant valueVariant = settings.value(key);
        if (valueVariant.canConvert<QStringList>()) {
            QStringList array = valueVariant.toStringList();
            array.removeOne(aValue);
            settings.setValue(key, array);
        } else {
            settings.remove(key);
        }
    }

    return true;
}

bool SettingsOperation::undoOperation()
{
    if (!checkArguments())
        return false;
    QString path = argumentKeyValue(QLatin1String("path"));
    const QString method = argumentKeyValue(QLatin1String("method"));
    const QString key = argumentKeyValue(QLatin1String("key"));
    QString aValue = argumentKeyValue(QLatin1String("value"));

    if (method.startsWith(QLatin1String("remove")))
        return true;

    if (requiresUnreplacedVariables()) {
        if (PackageManagerCore *const core = packageManager()) {
            path = core->replaceVariables(path);
            aValue = core->replaceVariables(aValue);
            // Check is different settings file is wanted to be used.
            // Old settings file name should be set to variable <variable_name>_OLD,
            // and new path is then read from <variable_name> variable.
            variableReplacement(&path);
        }
    }

    bool cleanUp = false;
    { // kill the scope to kill settings object, else remove file will not work
        QSettingsWrapper settings(path, QSettings::IniFormat);
        if (method == QLatin1String("set")) {
            settings.remove(key);
        } else if (method == QLatin1String("add_array_value")) {
            QVariant valueVariant = settings.value(key);
            if (valueVariant.canConvert<QStringList>()) {
                QStringList array = valueVariant.toStringList();
                array.removeOne(aValue);
                if (array.isEmpty())
                    settings.remove(key);
                else
                    settings.setValue(key, array);
            } else {
                settings.setValue(key, aValue);
            }
        }
        settings.sync(); // be safe
        cleanUp = settings.allKeys().isEmpty();
    }

    if (cleanUp) {
        QFile settingsFile(path);
        if (!settingsFile.remove())
            qCWarning(QInstaller::lcInstallerInstallLog).noquote() << settingsFile.errorString();
        if (!value(QLatin1String("createddir")).toString().isEmpty()) {
            KDUpdater::MkdirOperation mkDirOperation(packageManager());
            mkDirOperation.setArguments(QStringList() << QFileInfo(path).absolutePath());
            mkDirOperation.setValue(QLatin1String("createddir"), value(QLatin1String("createddir")));

            if (!mkDirOperation.undoOperation()) {
                qCWarning(QInstaller::lcInstallerInstallLog).noquote() << mkDirOperation.errorString();
            }
        }
    }
    return true;
}

bool SettingsOperation::testOperation()
{
    return true;
}
