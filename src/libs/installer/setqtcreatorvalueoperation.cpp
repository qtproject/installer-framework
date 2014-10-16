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
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
**
** $QT_END_LICENSE$
**
**************************************************************************/

#include "setqtcreatorvalueoperation.h"

#include "qtcreator_constants.h"
#include "packagemanagercore.h"

#include <QtCore/QSettings>
#include <QDebug>

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
        setErrorString(tr("Invalid arguments in %0: %1 arguments given, %2 expected%3.")
            .arg(name()).arg(arguments().count()).arg(tr("exactly 4"), tr(" (rootInstallPath, group, key, value)")));
        return false;
    }

    PackageManagerCore *const core = value(QLatin1String("installer")).value<PackageManagerCore*>();
    if (!core) {
        setError(UserDefinedError);
        setErrorString(tr("Needed installer object in \"%1\" operation is empty.").arg(name()));
        return false;
    }

    const QString &rootInstallPath = args.at(0); //for example "C:\\Nokia_SDK\\"
    if (!rootInstallPath.isEmpty()) {
        qWarning() << QString::fromLatin1("Because of internal changes the first argument '%1' on '%2' "\
            "operation is just ignored, please be aware of that.").arg(rootInstallPath, name());
    }

    const QString &group = groupName(args.at(1));
    const QString &key = args.at(2);
    const QString &settingsValue = args.at(3);

    QString qtCreatorInstallerSettingsFileName = core->value(scQtCreatorInstallerSettingsFile);
    if (qtCreatorInstallerSettingsFileName.isEmpty()) {
        setError(UserDefinedError);
        setErrorString(tr("There is no value set for '%1' on the installer object.").arg(
            scQtCreatorInstallerSettingsFile));
        return false;
    }
    QSettings settings(qtCreatorInstallerSettingsFileName, QSettings::IniFormat);
    if (!group.isEmpty())
        settings.beginGroup(group);

    if (settingsValue.contains(QLatin1String(",")))  // comma separated list of strings
        settings.setValue(key, settingsValue.split(QRegExp(QLatin1String("\\s*,\\s*")), QString::SkipEmptyParts));
    else
        settings.setValue(key, settingsValue);

    if (!group.isEmpty())
        settings.endGroup();

    settings.sync(); //be safe ;)

    return true;
}

bool SetQtCreatorValueOperation::undoOperation()
{
    const QStringList args = arguments();

    const QString &rootInstallPath = args.at(0); //for example "C:\\Nokia_SDK\\"
    if (!rootInstallPath.isEmpty()) {
        qWarning() << QString::fromLatin1("Because of internal changes the first argument '%1' on '%2' "\
            "operation is just ignored, please be aware of that.").arg(rootInstallPath, name());
    }

    const QString &group = groupName(args.at(1));
    const QString &key = args.at(2);

    PackageManagerCore *const core = value(QLatin1String("installer")).value<PackageManagerCore*>();
    if (!core) {
        setError(UserDefinedError);
        setErrorString(tr("Needed installer object in '%1' operation is empty.").arg(name()));
        return false;
    }

    // default value is the old value to keep the possibility that old saved operations can run undo
#ifdef Q_OS_OSX
    QString qtCreatorInstallerSettingsFileName = core->value(scQtCreatorInstallerSettingsFile,
        QString::fromLatin1("%1/Qt Creator.app/Contents/Resources/QtProject/QtCreator.ini").arg(
        core->value(QLatin1String("TargetDir"))));
#else
    QString qtCreatorInstallerSettingsFileName = core->value(scQtCreatorInstallerSettingsFile,
        QString::fromLatin1("%1/QtCreator/share/qtcreator/QtProject/QtCreator.ini").arg(core->value(
        QLatin1String("TargetDir"))));
#endif

    QSettings settings(qtCreatorInstallerSettingsFileName, QSettings::IniFormat);
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
