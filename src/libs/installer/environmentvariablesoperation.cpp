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

#include "environmentvariablesoperation.h"
#include "qsettingswrapper.h"

#include <stdlib.h>

#include "environment.h"

#ifdef Q_OS_WIN
# include <windows.h>
#endif

using namespace QInstaller;
using namespace KDUpdater;

EnvironmentVariableOperation::EnvironmentVariableOperation()
{
    setName(QLatin1String("EnvironmentVariable"));
}

void EnvironmentVariableOperation::backup()
{
}

#ifdef Q_OS_WIN
static void broadcastEnvironmentChange()
{
    // Use SendMessageTimeout to Broadcast a message to the whole system to update settings of all
    // running applications. This is needed to activate the changes done above without logout+login.
    DWORD_PTR aResult = 0;
    LRESULT sendresult = SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE,
        0, (LPARAM) L"Environment", SMTO_BLOCK | SMTO_ABORTIFHUNG, 5000, &aResult);
    if (sendresult == 0 || aResult != 0)
        qWarning("Failed to broadcast the WM_SETTINGCHANGE message.");
}
#endif

namespace {

template <typename SettingsType>
UpdateOperation::Error writeSetting(const QString &regPath,
                                    const QString &name,
                                    const QString &value,
                                    QString *errorString,
                                    QString *oldValue)
{
    oldValue->clear();
    SettingsType registry(regPath, QSettingsWrapper::NativeFormat);
    if (!registry.isWritable()) {
        *errorString = UpdateOperation::tr("Registry path %1 is not writable").arg(regPath);
        return UpdateOperation::UserDefinedError;
    }

    // remember old value for undo
    *oldValue = registry.value(name).toString();

    // set the new value
    registry.setValue(name, value);
    registry.sync();

    if (registry.status() != QSettingsWrapper::NoError) {
        *errorString = UpdateOperation::tr("Could not write to registry path %1").arg(regPath);
        return UpdateOperation::UserDefinedError;
    }

    return UpdateOperation::NoError;
}

template <typename SettingsType>
UpdateOperation::Error undoSetting(const QString &regPath,
                                   const QString &name,
                                   const QString &value,
                                   const QString &oldValue,
                                   QString *errorString)
{
    QString actual;
    {
        SettingsType registry(regPath, QSettingsWrapper::NativeFormat);
        actual = registry.value(name).toString();
    }
    if (actual != value) //key changed, don't undo
        return UpdateOperation::UserDefinedError;
    QString dontcare;
    return writeSetting<SettingsType>(regPath, name, oldValue, errorString, &dontcare);
}

} // namespace

bool EnvironmentVariableOperation::performOperation()
{
    QStringList args = arguments();
    if (args.count() < 2 || args.count() > 4) {
        setError(InvalidArguments);
        setErrorString(tr("Invalid arguments in %0: %1 arguments given, %2 expected%3.")
            .arg(name()).arg(arguments().count()).arg(tr("2 to 4"), QLatin1String("")));
        return false;
    }

    const QString name = arguments().at(0);
    const QString value = arguments().at(1);
    bool isPersistent = false;

#ifdef Q_OS_WIN
    isPersistent = arguments().count() >= 3 ? arguments().at(2) == QLatin1String("true") : true;
    const bool isSystemWide = arguments().count() >= 4 ? arguments().at(3) == QLatin1String("true") : false;
    QString oldvalue;
    if (isPersistent) {
        const QString regPath = isSystemWide ? QLatin1String("HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet"
            "\\Control\\Session Manager\\Environment") : QLatin1String("HKEY_CURRENT_USER\\Environment");

        // write the name=value pair to the global environment
        QString errorString;

        Error err = NoError;

        err = writeSetting<QSettingsWrapper>(regPath, name, value, &errorString, &oldvalue);
        if (err != NoError) {
            setError(err);
            setErrorString(errorString);
            return false;
        }

        broadcastEnvironmentChange();

        setValue(QLatin1String("oldvalue"), oldvalue);
        return true;
    }
#endif
    Q_ASSERT(!isPersistent);
    Q_UNUSED(isPersistent)

    setValue(QLatin1String("oldvalue"), Environment::instance().value(name));
    Environment::instance().setTemporaryValue(name, value);

    return true;
}

bool EnvironmentVariableOperation::undoOperation()
{
    if (arguments().count() < 2 || arguments().count() > 4)
        return false;

    const QString name = arguments().at(0);
    const QString value = arguments().at(1);
    const QString oldvalue = this->value(QLatin1String("oldvalue")).toString();

#ifdef Q_OS_WIN
    const bool isPersistent = arguments().count() >= 3 ? arguments().at(2) == QLatin1String("true") : true;
#else
   const bool isPersistent = false;
#endif

    if (!isPersistent) {
        const QString actual = Environment::instance().value(name);
        const bool doUndo = actual == value;
        if (doUndo)
            Environment::instance().setTemporaryValue(name, oldvalue);
        return doUndo;
    }

#ifdef Q_OS_WIN
    const bool isSystemWide = arguments().count() >= 4 ? arguments().at(3) == QLatin1String("true") : false;

    const QString regPath = isSystemWide ? QLatin1String("HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\"
        "Control\\Session Manager\\Environment") : QLatin1String("HKEY_CURRENT_USER\\Environment");

    QString errorString;

    const Error err = undoSetting<QSettingsWrapper>(regPath, name, value, oldvalue, &errorString);

    if (err != NoError) {
        setError(err);
        setErrorString(errorString);
        return false;
    }
#endif

    return true;
}

bool EnvironmentVariableOperation::testOperation()
{
    return true;
}

Operation *EnvironmentVariableOperation::clone() const
{
    return new EnvironmentVariableOperation();
}
