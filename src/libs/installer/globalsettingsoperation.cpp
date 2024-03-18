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

#include "globalsettingsoperation.h"
#include "qsettingswrapper.h"

using namespace QInstaller;

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::GlobalSettingsOperation
    \internal
*/

GlobalSettingsOperation::GlobalSettingsOperation(PackageManagerCore *core)
    : UpdateOperation(core)
{
    setName(QLatin1String("GlobalConfig"));
}

void GlobalSettingsOperation::backup()
{
}

bool GlobalSettingsOperation::performOperation()
{
    const QStringList args = parsePerformOperationArguments();
    QString key, value;
    QScopedPointer<QSettingsWrapper> settings(setup(&key, &value, args));
    if (settings.isNull())
        return false;

    if (!settings->isWritable()) {
        setError(UserDefinedError);
        setErrorString(tr("Settings are not writable."));
        return false;
    }

    const QVariant oldValue = settings->value(key);
    settings->setValue(key, value);
    settings->sync();

    if (settings->status() != QSettingsWrapper::NoError) {
        setError(UserDefinedError);
        setErrorString(tr("Failed to write settings."));
        return false;
    }

    setValue(QLatin1String("oldvalue"), oldValue);
    return true;
}

bool GlobalSettingsOperation::undoOperation()
{
    if (skipUndoOperation())
        return true;

    const QStringList args = parsePerformOperationArguments();
    QString key, val;
    QScopedPointer<QSettingsWrapper> settings(setup(&key, &val, args));
    if (settings.isNull())
        return false;

    // be sure it's still our value and nobody changed it in between
    const QVariant oldValue = value(QLatin1String("oldvalue"));
    if (settings->value(key) == val) {
        // restore the previous state
        if (oldValue.isNull())
            settings->remove(key);
        else
            settings->setValue(key, oldValue);
    }

    return true;
}

bool GlobalSettingsOperation::testOperation()
{
    return true;
}

QSettingsWrapper *GlobalSettingsOperation::setup(QString *key, QString *value, const QStringList &arguments)
{
    if (!checkArgumentCount(3, 5))
        return nullptr;

    if (arguments.count() == 5) {
        QSettingsWrapper::Scope scope = QSettingsWrapper::UserScope;
        if (arguments.at(0) == QLatin1String("SystemScope"))
            scope = QSettingsWrapper::SystemScope;
        const QString &company = arguments.at(1);
        const QString &application = arguments.at(2);
        *key = arguments.at(3);
        *value = arguments.at(4);
        return new QSettingsWrapper(scope, company, application);
    } else if (arguments.count() == 4) {
        const QString &company = arguments.at(0);
        const QString &application = arguments.at(1);
        *key = arguments.at(2);
        *value = arguments.at(3);
        return new QSettingsWrapper(company, application);
    } else if (arguments.count() == 3) {
        const QString &filename = arguments.at(0);
        *key = arguments.at(1);
        *value = arguments.at(2);
        return new QSettingsWrapper(filename, QSettings::NativeFormat);
    }

    return nullptr;
}
