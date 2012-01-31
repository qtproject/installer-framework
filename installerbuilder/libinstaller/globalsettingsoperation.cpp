/**************************************************************************
**
** This file is part of Qt SDK**
**
** Copyright (c) 2012 Nokia Corporation and/or its subsidiary(-ies).*
**
** Contact:  Nokia Corporation qt-info@nokia.com**
**
** No Commercial Usage
**
** This file contains pre-release code and may not be distributed.
** You may use this file in accordance with the terms and conditions
** contained in the Technology Preview License Agreement accompanying
** this package.
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
** rights. These rights are described in the Nokia Qt LGPL Exception version
** 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** If you are unsure which license is appropriate for your use, please contact
** (qt-info@nokia.com).
**
**************************************************************************/
#include "globalsettingsoperation.h"
#include "qsettingswrapper.h"

using namespace QInstaller;

GlobalSettingsOperation::GlobalSettingsOperation()
{
    setName(QLatin1String("GlobalConfig"));
}

void GlobalSettingsOperation::backup()
{
}

bool GlobalSettingsOperation::performOperation()
{
    QString key, value;
    QScopedPointer<QSettingsWrapper> settings(setup(&key, &value, arguments()));
    if (settings.isNull())
        return false;

    if (!settings->isWritable()) {
        setError(UserDefinedError);
        setErrorString(tr("Settings are not writable"));
        return false;
    }

    const QVariant oldValue = settings->value(key);
    settings->setValue(key, value);
    settings->sync();

    if (settings->status() != QSettingsWrapper::NoError) {
        setError(UserDefinedError);
        setErrorString(tr("Failed to write settings"));
        return false;
    }

    setValue(QLatin1String("oldvalue"), oldValue);
    return true;
}

bool GlobalSettingsOperation::undoOperation()
{
    QString key, val;
    QScopedPointer<QSettingsWrapper> settings(setup(&key, &val, arguments()));
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

Operation *GlobalSettingsOperation::clone() const
{
    return new GlobalSettingsOperation();
}

QSettingsWrapper *GlobalSettingsOperation::setup(QString *key, QString *value, const QStringList &arguments)
{
    if (arguments.count() == 4) {
        const QString &company = arguments.at(0);
        const QString &application = arguments.at(1);
        *key = arguments.at(2);
        *value = arguments.at(3);
        return new QSettingsWrapper(company, application);
    } else if (arguments.count() == 3) {
        const QString &filename = arguments.at(0);
        *key = arguments.at(1);
        *value = arguments.at(2);
        return new QSettingsWrapper(filename, QSettingsWrapper::NativeFormat);
    }

    setError(InvalidArguments);
    setErrorString(tr("Invalid arguments in 0%: %1 arguments given, at least 3 expected.")
        .arg(name()).arg(arguments.count()));
    return 0;
}
