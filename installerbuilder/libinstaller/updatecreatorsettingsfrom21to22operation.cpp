/**************************************************************************
**
** This file is part of Qt SDK**
**
** Copyright (c) 2011 Nokia Corporation and/or its subsidiary(-ies).*
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
#include "updatecreatorsettingsfrom21to22operation.h"
#include "registerdefaultdebuggeroperation.h"
#include "registertoolchainoperation.h"
#include "qtcreatorpersistentsettings.h"
#include "qinstaller.h"
#include "qtcreator_constants.h"

#include <QString>
#include <QFileInfo>
#include <QDir>
#include <QSettings>
#include <QDebug>

QStringList getQmakePathesOfAllInstallerRegisteredQtVersions(const QSettings &settings)
{
    QStringList qmakePathes;

    QStringList oldNewQtVersions = settings.value(QLatin1String("NewQtVersions")).toString().split(
        QLatin1String(";"));

    foreach (const QString &qtVersion, oldNewQtVersions) {
        QStringList splitedQtConfiguration = qtVersion.split(QLatin1String("="));
        if (splitedQtConfiguration.count() > 1
            && splitedQtConfiguration.at(1).contains(QLatin1String("qmake"), Qt::CaseInsensitive)) {
                QString qmakePath = splitedQtConfiguration.at(1);
                qmakePathes.append(qmakePath);
        }
    }
    return qmakePathes;
}

void removeInstallerRegisteredQtVersions(QSettings &settings, const QStringList &qmakePathes)
{
    settings.beginGroup(QLatin1String(QtVersionsSectionName));
    int qtVersionSizeValue=settings.value(QLatin1String("size")).toInt();

    //read all settings for Qt Versions
    QHash<QString, QVariant> oldSettingsAsHash;
    foreach (const QString &key, settings.allKeys()) {
        oldSettingsAsHash.insert(key, settings.value(key));
    }

    //get the installer added Qt Version settings ids
    QList<int> toRemoveIds;
    QHashIterator<QString, QVariant> it(oldSettingsAsHash);
    while(it.hasNext()) {
        it.next();
        if(it.key().endsWith(QLatin1String("QMakePath")) && !it.value().toString().isEmpty()) {
            foreach(const QString &toRemoveQmakePath, qmakePathes) {
                if (QFileInfo(it.value().toString()) == QFileInfo(toRemoveQmakePath)) {
                    int firstNoDigitCharIndex = it.key().indexOf(QRegExp(QLatin1String("[^0-9]")));
                    QString numberAtTheBeginning = it.key().left(firstNoDigitCharIndex);
                    toRemoveIds << numberAtTheBeginning.toInt();
                }
            }
        }
    }

    //now write only the other Qt Versions to QtCreator settings
    it.toFront();
    QHash<int, int> qtVersionIdMapper; //old, new
    int newVersionId = 1;
    while(it.hasNext()) {
        it.next();
        settings.remove(it.key());
        int firstNoDigitCharIndex = it.key().indexOf(QRegExp(QLatin1String("[^0-9]")));
        QString numberAtTheBeginningAsString = it.key().left(firstNoDigitCharIndex);
        QString restOfTheKey = it.key().mid(firstNoDigitCharIndex);
        bool isNumber=false;
        //check that it is a nummer - for example "size" value of the settings array is not
        int numberAtTheBeginning = numberAtTheBeginningAsString.toInt(&isNumber);
        if (isNumber && !toRemoveIds.contains(numberAtTheBeginning)) {
            if (!qtVersionIdMapper.contains(numberAtTheBeginning)) {
                qtVersionIdMapper.insert(numberAtTheBeginning, newVersionId);
                newVersionId++;
            }
            QString newKey = QString::number(qtVersionIdMapper.value(numberAtTheBeginning)) + restOfTheKey;
            if (newKey.endsWith(QLatin1String("Id")))
                settings.setValue(newKey, qtVersionIdMapper.value(numberAtTheBeginning));
            else
                settings.setValue(newKey, it.value());
        }
    }

    Q_ASSERT(qtVersionIdMapper.count() == qtVersionSizeValue - toRemoveIds.count());
    settings.setValue(QLatin1String("size"), qtVersionIdMapper.count());
    settings.endGroup(); //QtVersionsSectionName
}

bool convertQtInstallerSettings(QSettings &settings, const QString &toolChainsXmlFilePath)
{
    QStringList oldNewQtVersions = settings.value(QLatin1String("NewQtVersions")).toString().split(
        QLatin1String(";"));

    QSet<QString> mingwToolChains;
    QSet<QString> gcceToolChains;
    QString newQtVersions;
    foreach (const QString &qtVersion, oldNewQtVersions) {
        QStringList splitedQtConfiguration = qtVersion.split(QLatin1String("="));
        if (splitedQtConfiguration.count() == 8) {
            int positionCounter = 0;
            const QString &versionName = splitedQtConfiguration.at(positionCounter++);
            const QString &qmakePath = splitedQtConfiguration.at(positionCounter++);
            mingwToolChains.insert(splitedQtConfiguration.at(positionCounter++));
            QString systemRoot = splitedQtConfiguration.at(positionCounter++);
            QString gccePath = splitedQtConfiguration.at(positionCounter++);
            gcceToolChains.insert(gccePath);
            QString carbidePath = splitedQtConfiguration.at(positionCounter++);
            Q_UNUSED(carbidePath)
            QString msvcPath = splitedQtConfiguration.at(positionCounter++);
            Q_UNUSED(msvcPath)
            QString sbsPath = splitedQtConfiguration.at(positionCounter++);

            QString addedQtVersion = versionName;

            addedQtVersion += QLatin1Char('=') + qmakePath;
            addedQtVersion += QLatin1Char('=') + systemRoot;
            addedQtVersion += QLatin1Char('=') + sbsPath;
            newQtVersions.append(addedQtVersion + QLatin1Char(';'));
        } else {
            //I am not sure that this can happen
            newQtVersions.append(qtVersion + QLatin1Char(';'));
        }
    }
    settings.setValue(QLatin1String("NewQtVersions"), newQtVersions);

    QtCreatorPersistentSettings creatorToolChainSettings;

    if (!creatorToolChainSettings.init(toolChainsXmlFilePath)) {
        return false;
    }

    foreach (const QString mingwPath, mingwToolChains) {
        if (mingwPath.isEmpty())
            continue;
        QInstaller::RegisterToolChainOperation operation;
        operation.setArguments(QStringList()
                               << QLatin1String("GccToolChain")
                               << QLatin1String("ProjectExplorer.ToolChain.Mingw")
                               << QLatin1String("Mingw as a GCC for Windows targets")
                               << QLatin1String("x86-windows-msys-pe-32bit")
                               << mingwPath + QLatin1String("\\bin\\g++.exe")
                               << creatorToolChainSettings.abiToDebuggerHash().value(QLatin1String
                                    ("x86-windows-msys-pe-32bit"))
                               );
        bool result = operation.performOperation();
        Q_ASSERT(result);
    }
    foreach (const QString gccePath, gcceToolChains) {
        if (gccePath.isEmpty())
            continue;
        QInstaller::RegisterToolChainOperation operation;
        operation.setArguments(QStringList()
                               << QLatin1String("GccToolChain")
                               << QLatin1String("Qt4ProjectManager.ToolChain.GCCE")
                               << QLatin1String("GCCE 4 for Symbian targets")
                               << QLatin1String("arm-symbian-device-elf-32bit")
                               << gccePath + QLatin1String("\\bin\\arm-none-symbianelf-g++.exe")
                               << creatorToolChainSettings.abiToDebuggerHash().value(QLatin1String(
                                    "arm-symbian-device-elf-32bit"))
                               );
        bool result = operation.performOperation();
        Q_ASSERT(result);
    }
    return true;
}

void convertDefaultGDBInstallerSettings(QSettings &settings)
{
    settings.beginGroup(QLatin1String("GdbBinaries21"));

    //read all settings for GDBs
    QHash<QString, QString> abiToDefaultDebuggerHash;
    foreach (const QString &key, settings.allKeys()) {
        QString oldValue = settings.value(key).toString();
        QString gdbBinaryPath = oldValue.left(oldValue.indexOf(QLatin1String(",")));

        QString gdbTypesAsCommaSeperatedString = oldValue.mid(oldValue.indexOf(QLatin1String(",")));
        QStringList gdbTypeList = gdbTypesAsCommaSeperatedString.split(QLatin1String(","));
        foreach(const QString &gdbType, gdbTypeList) {
            if (gdbType == QLatin1String("0")) {
                abiToDefaultDebuggerHash.insert(QLatin1String("x86-linux-generic-elf-64bit"), gdbBinaryPath);
                abiToDefaultDebuggerHash.insert(QLatin1String("x86-linux-generic-elf-32bit"), gdbBinaryPath);
            }
            if (gdbType == QLatin1String("2")) {
                abiToDefaultDebuggerHash.insert(QLatin1String("x86-windows-msys-pe-32bit"), gdbBinaryPath);
            }
            if (gdbType == QLatin1String("6")) {
                abiToDefaultDebuggerHash.insert(QLatin1String("arm-symbian-device-elf-32bit"), gdbBinaryPath);
            }
            if (gdbType == QLatin1String("9")) {
                abiToDefaultDebuggerHash.insert(QLatin1String("arm-linux-harmattan-elf-32bit"), gdbBinaryPath);
                abiToDefaultDebuggerHash.insert(QLatin1String("arm-linux-maemo-elf-32bit"), gdbBinaryPath);
                abiToDefaultDebuggerHash.insert(QLatin1String("arm-linux-meego-elf-32bit"), gdbBinaryPath);
            }
        }
    }
    QInstaller::RegisterDefaultDebuggerOperation operation;

    QHashIterator<QString, QString> it(abiToDefaultDebuggerHash);
    while(it.hasNext()) {
        it.next();
        operation.setArguments(QStringList() << it.key() << it.value());
        bool result = operation.performOperation();
        Q_ASSERT(result);
    }

    settings.endGroup(); //"GdbBinaries21"
}

using namespace QInstaller;

using namespace ProjectExplorer;

UpdateCreatorSettingsFrom21To22Operation::UpdateCreatorSettingsFrom21To22Operation()
{
    setName(QLatin1String("UpdateCreatorSettingsFrom21To22"));
}

UpdateCreatorSettingsFrom21To22Operation::~UpdateCreatorSettingsFrom21To22Operation()
{
}

void UpdateCreatorSettingsFrom21To22Operation::backup()
{
}

bool UpdateCreatorSettingsFrom21To22Operation::performOperation()
{
    const QStringList args = arguments();

    if (args.count() != 0) {
        setError(InvalidArguments);
        setErrorString(tr("Invalid arguments in %0: %1 arguments given, 0 expected.")
            .arg(name()).arg(args.count()));
        return false;
    }

    const Installer* const installer = qVariantValue<Installer*>(value(QLatin1String("installer")));
    if (!installer) {
        setError(UserDefinedError);
        setErrorString(tr("Needed installer object in \"%1\" operation is empty.").arg(name()));
        return false;
    }
    const QString &rootInstallPath = installer->value(QLatin1String("TargetDir"));

    QString toolChainsXmlFilePath = rootInstallPath + QLatin1String(ToolChainSettingsSuffixPath);

    QSettings sdkSettings(rootInstallPath + QLatin1String(QtCreatorSettingsSuffixPath),
        QSettings::IniFormat);

    convertDefaultGDBInstallerSettings(sdkSettings);

    QString userSettingsFileName = installer->value(QLatin1String("QtCreatorSettingsFile"));
    if (QFile::exists(userSettingsFileName)) {
        QSettings userSettings(userSettingsFileName, QSettings::IniFormat);
        QStringList qmakePathes = getQmakePathesOfAllInstallerRegisteredQtVersions(sdkSettings);
        removeInstallerRegisteredQtVersions(userSettings, qmakePathes);
    }

    return convertQtInstallerSettings(sdkSettings, toolChainsXmlFilePath);
}

bool UpdateCreatorSettingsFrom21To22Operation::undoOperation()
{
    return true;
}

bool UpdateCreatorSettingsFrom21To22Operation::testOperation()
{
    return true;
}

KDUpdater::UpdateOperation* UpdateCreatorSettingsFrom21To22Operation::clone() const
{
    return new UpdateCreatorSettingsFrom21To22Operation();
}
