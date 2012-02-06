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

#include "environmentvariablesoperation.h"
#include "qsettingswrapper.h"

#include <stdlib.h>

#include "environment.h"

#ifdef Q_WS_WIN
# include <windows.h>
#endif

using namespace QInstaller;
using namespace KDUpdater;

/*
TRANSLATOR QInstaller::EnvironmentVariablesOperation
*/

EnvironmentVariableOperation::EnvironmentVariableOperation()
{
    setName(QLatin1String("EnvironmentVariable"));
}

void EnvironmentVariableOperation::backup()
{
}

#ifdef Q_WS_WIN
static bool broadcastChange() {
    // Use SendMessageTimeout to Broadcast a message to the whole system to update settings of all
    // running applications. This is needed to activate the changes done above without logout+login.
    // Note that cmd.exe does not respond to any WM_SETTINGCHANGE messages...
    DWORD aResult = 0;
    LRESULT sendresult = SendMessageTimeout(HWND_BROADCAST, WM_SETTINGCHANGE,
        0, (LPARAM) "Environment", SMTO_BLOCK | SMTO_ABORTIFHUNG, 5000, &aResult);
    if (sendresult == 0 || aResult != 0) {
        qWarning("Failed to broadcast a WM_SETTINGCHANGE message\n");
        return false;
    }

    return true;
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
        *errorString = QObject::tr("Registry path %1 is not writable").arg(regPath);
        return UpdateOperation::UserDefinedError;
    }

    // remember old value for undo
    *oldValue = registry.value(name).toString();

    // set the new value
    registry.setValue(name, value);
    registry.sync();

    if (registry.status() != QSettingsWrapper::NoError) {
        *errorString = QObject::tr("Could not write to registry path %1").arg(regPath);
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
    if (arguments().count() < 2 || arguments().count() > 4) {
        setError(InvalidArguments);
        setErrorString(tr("Invalid arguments in %0: %1 arguments given, 2-3 expected.")
                        .arg(name()).arg(arguments().count()));
        return false;
    }

    const QString name = arguments().at(0);
    const QString value = arguments().at(1);
    bool isPersistent = false;

#ifdef Q_WS_WIN
    isPersistent = arguments().count() >= 3 ? arguments().at(2) == QLatin1String("true") : true;
    const bool isSystemWide = arguments().count() >= 4 ? arguments().at(3) == QLatin1String("true") : false;
    QString oldvalue;
    if (isPersistent) {
        const QString regPath = isSystemWide ? QLatin1String("HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet"
            "\\Control\\Session Manager\\Environment") : QLatin1String("HKEY_CURRENT_USER\\Environment");

        // write the name=value pair to the global environment
        QString errorString;

        Error err = NoError;

        err = isSystemWide
            ? writeSetting<QSettingsWrapper>(regPath, name, value, &errorString, &oldvalue)
            : writeSetting<QSettingsWrapper>(regPath, name, value, &errorString, &oldvalue);
        if (err != NoError) {
            setError(err);
            setErrorString(errorString);
            return false;
        }
        const bool bret = broadcastChange();
        Q_UNUSED(bret); // this is not critical, so fall-through
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

#ifdef Q_WS_WIN
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

#ifdef Q_WS_WIN
    const bool isSystemWide = arguments().count() >= 4 ? arguments().at(3) == QLatin1String("true") : false;

    const QString regPath = isSystemWide ? QLatin1String("HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\"
        "Control\\Session Manager\\Environment") : QLatin1String("HKEY_CURRENT_USER\\Environment");

    QString errorString;

    const Error err = isSystemWide
        ? undoSetting<QSettingsWrapper>(regPath, name, value, oldvalue, &errorString)
        : undoSetting<QSettingsWrapper>(regPath, name, value, oldvalue, &errorString);

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
