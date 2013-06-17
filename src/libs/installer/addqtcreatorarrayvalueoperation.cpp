/**************************************************************************
**
** Copyright (C) 2012-2013 Digia Plc and/or its subsidiary(-ies).
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

#include "addqtcreatorarrayvalueoperation.h"

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
        setErrorString(tr("Invalid arguments in %0: %1 arguments given, %2 expected%3.")
            .arg(name()).arg(arguments().count()).arg(tr("exactly 4"), tr(" (group, arrayname, key, value)")));
        return false;
    }

    PackageManagerCore *const core = value(QLatin1String("installer")).value<PackageManagerCore*>();
    if (!core) {
        setError(UserDefinedError);
        setErrorString(tr("Needed installer object in %1 operation is empty.").arg(name()));
        return false;
    }

    QString qtCreatorInstallerSettingsFileName = core->value(scQtCreatorInstallerSettingsFile);
    if (qtCreatorInstallerSettingsFileName.isEmpty()) {
        setError(UserDefinedError);
        setErrorString(tr("There is no value set for %1 on the installer object.").arg(
            scQtCreatorInstallerSettingsFile));
        return false;
    }

    QSettings settings(qtCreatorInstallerSettingsFileName, QSettings::IniFormat);

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
        //if it is already there we have nothing to do
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

    settings.sync(); //be safe ;)
    setValue(QLatin1String("ArrayValueSet"), true);
    return true;
}

bool AddQtCreatorArrayValueOperation::undoOperation()
{
    if (value(QLatin1String("ArrayValueSet")).isNull())
        return true;
    const QStringList args = arguments();

    PackageManagerCore *const core = value(QLatin1String("installer")).value<PackageManagerCore*>();
    if (!core) {
        setError(UserDefinedError);
        setErrorString(tr("Needed installer object in %1 operation is empty.").arg(name()));
        return false;
    }

    // default value is the old value to keep the possibility that old saved operations can run undo
#ifdef Q_OS_MAC
    QString qtCreatorInstallerSettingsFileName = core->value(scQtCreatorInstallerSettingsFile,
        QString::fromLatin1("%1/Qt Creator.app/Contents/Resources/QtProject/QtCreator.ini").arg(
        core->value(QLatin1String("TargetDir"))));
#else
    QString qtCreatorInstallerSettingsFileName = core->value(scQtCreatorInstallerSettingsFile,
        QString::fromLatin1("%1/QtCreator/share/qtcreator/QtProject/QtCreator.ini").arg(core->value(
        QLatin1String("TargetDir"))));
#endif

    QSettings settings(qtCreatorInstallerSettingsFileName, QSettings::IniFormat);

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

    settings.sync(); //be safe ;)
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
