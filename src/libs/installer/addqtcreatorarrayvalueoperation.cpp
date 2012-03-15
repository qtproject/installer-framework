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

#include "addqtcreatorarrayvalueoperation.h"

#include "constants.h"
#include "qtcreator_constants.h"
#include "packagemanagercore.h"

#include <QtCore/QSettings>
#include <QtCore/QSet>

using namespace QInstaller;

static QString groupName(const QString &groupName)
{
    return groupName == QLatin1String("General") ? QString() : groupName;
}

AddQtCreatorArrayValueOperation::AddQtCreatorArrayValueOperation()
{
    setName(QLatin1String("AddQtCreatorArrayValue"));
}

void AddQtCreatorArrayValueOperation::backup()
{
}

bool AddQtCreatorArrayValueOperation::performOperation()
{
    const QStringList args = arguments();

    if (args.count() != 4) {
        setError(InvalidArguments);
        setErrorString(tr("Invalid arguments in %0: %1 arguments given, exactly 4 expected (group, "
            "arrayname, key, value).").arg(name()).arg( arguments().count()));
        return false;
    }


    PackageManagerCore *const core = qVariantValue<PackageManagerCore*>(value(QLatin1String("installer")));
    if (!core) {
        setError(UserDefinedError);
        setErrorString(tr("Needed installer object in %1 operation is empty.").arg(name()));
        return false;
    }
    const QString &rootInstallPath = core->value(scTargetDir);

    QSettings settings(rootInstallPath + QLatin1String(QtCreatorSettingsSuffixPath),
        QSettings::IniFormat);

    const QString &group = groupName(args.at(0));
    const QString &arrayName = args.at(1);
    const QString &key = args.at(2);
    const QString &value = args.at(3);

    if (!group.isEmpty())
        settings.beginGroup(group);

    QList<QString> oldArrayValues;
    int arraySize = settings.beginReadArray(arrayName);
    for (int i = 0; i < arraySize; ++i) {
        settings.setArrayIndex(i);
        //if it is already there we have nothing todo
        if (settings.value(key).toString() == value)
            return true;
        oldArrayValues.append(settings.value(key).toString());
    }
    settings.endArray();


    settings.beginWriteArray(arrayName);

    for (int i = 0; i < oldArrayValues.size(); ++i) {
        settings.setArrayIndex(i);
        settings.setValue(key, oldArrayValues.value(i));
    }
    settings.setArrayIndex(oldArrayValues.size()); //means next index after the last insert
    settings.setValue(key, value);
    settings.endArray();

    settings.sync(); //be save ;)
    setValue(QLatin1String("ArrayValueSet"), true);
    return true;
}

bool AddQtCreatorArrayValueOperation::undoOperation()
{
    if (value(QLatin1String("ArrayValueSet")).isNull())
        return true;
    const QStringList args = arguments();

    PackageManagerCore *const core = qVariantValue<PackageManagerCore*>(value(QLatin1String("installer")));
    if (!core) {
        setError(UserDefinedError);
        setErrorString(tr("Needed installer object in %1 operation is empty.").arg(name()));
        return false;
    }
    const QString &rootInstallPath = core->value(scTargetDir);

    QSettings settings(rootInstallPath + QLatin1String(QtCreatorSettingsSuffixPath),
        QSettings::IniFormat);

    const QString &group = groupName(args.at(0));
    const QString &arrayName = args.at(1);
    const QString &key = args.at(2);
    const QString &value = args.at(3);

    if (!group.isEmpty())
        settings.beginGroup(group);

    QList<QString> oldArrayValues;
    int arraySize = settings.beginReadArray(arrayName);
    for (int i = 0; i < arraySize; ++i) {
        settings.setArrayIndex(i);
        if (settings.value(key).toString() != value)
            oldArrayValues.append(settings.value(key).toString());
    }
    settings.endArray();


    settings.beginWriteArray(arrayName);

    for (int i = 0; i < oldArrayValues.size(); ++i) {
        settings.setArrayIndex(i);
        settings.setValue(key, oldArrayValues.value(i));
    }
    settings.endArray();

    settings.sync(); //be save ;)
    return true;
}

bool AddQtCreatorArrayValueOperation::testOperation()
{
    return true;
}

Operation *AddQtCreatorArrayValueOperation::clone() const
{
    return new AddQtCreatorArrayValueOperation();
}
