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
#include "registertoolchainoperation.h"

#include "persistentsettings.h"
#include "qinstaller.h"
#include "qtcreator_constants.h"

#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QSettings>
#include <QtCore/QString>

using namespace QInstaller;

using namespace ProjectExplorer;

RegisterToolChainOperation::RegisterToolChainOperation()
{
    setName(QLatin1String("RegisterToolChain"));
}

RegisterToolChainOperation::~RegisterToolChainOperation()
{
}

void RegisterToolChainOperation::backup()
{
}

bool RegisterToolChainOperation::performOperation()
{
    const QStringList args = arguments();

    if (args.count() < 4) {
        setError(InvalidArguments);
        setErrorString(tr("Invalid arguments in %0: %1 arguments given, minimum 4 expected.")
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

    int argCounter = 0;
    const QString &toolChainKey = args.at(argCounter++); //Qt SDK:gccPath
    const QString &toolChainType = args.at(argCounter++); //where this toolchain is defined in QtCreator
    const QString &displayName = args.at(argCounter++); //nice special Toolchain (Qt SDK)
    const QString &abiString = args.at(argCounter++); //x86-windows-msys-pe-32bit
    const QString &compilerPath = args.at(argCounter++); //gccPath
    QString debuggerPath;
    QString armVersion;
    QString force32Bit;
    if (args.count() > argCounter)
        debuggerPath = args.at(argCounter++);
    if (args.count() > argCounter)
        armVersion = args.at(argCounter++);
    if (args.count() > argCounter)
        force32Bit = args.at(argCounter++);

    PersistentSettingsReader reader;
    QHash<QString, QVariantMap> toolChainHash;
    QString toolChainsXmlFilePath = rootInstallPath + QLatin1String(ToolChainSettingsSuffixPath);
    if (reader.load(toolChainsXmlFilePath)) {
        QVariantMap data = reader.restoreValues();

        // Check version:
        int version = data.value(QLatin1String(TOOLCHAIN_FILE_VERSION_KEY), 0).toInt();
        if (version < 1) {
            setError(UserDefinedError);
            setErrorString(tr("Toolchain settings xml file %1 has not the right version.")
                .arg(toolChainsXmlFilePath));
            return false;
        }

        int count = data.value(QLatin1String(TOOLCHAIN_COUNT_KEY), 0).toInt();
        for (int i = 0; i < count; ++i) {
            const QString key = QLatin1String(TOOLCHAIN_DATA_KEY) + QString::number(i);

            const QVariantMap toolChainMap = data.value(key).toMap();
            //gets the path variable, hope ".Path" will stay in QtCreator
            QStringList pathContainingKeyList = QStringList(toolChainMap.keys())
                .filter(QLatin1String(".Path"));
            foreach(const QString& pathKey, pathContainingKeyList) {
                QString path = toolChainMap.value(pathKey).toString();
                if (!path.isEmpty() && QFile::exists(path)) {
                    toolChainHash.insert(path, toolChainMap);
                }
            }
        }
    }

    QVariantMap newToolChainVariantMap;

    newToolChainVariantMap.insert(QLatin1String(ID_KEY),
        QString(QLatin1String("%1:%2.%3")).arg(toolChainType, compilerPath, abiString));
    newToolChainVariantMap.insert(QLatin1String(DISPLAY_NAME_KEY), displayName);
    newToolChainVariantMap.insert(QString(QLatin1String("ProjectExplorer.%1.Path")).arg(toolChainKey),
        compilerPath);
    newToolChainVariantMap.insert(QString(QLatin1String("ProjectExplorer.%1.TargetAbi")).arg(toolChainKey),
        abiString);
    newToolChainVariantMap.insert(QString(QLatin1String("ProjectExplorer.%1.Debugger")).arg(toolChainKey),
        debuggerPath);

    toolChainHash.insert(compilerPath, newToolChainVariantMap);

    PersistentSettingsWriter writer;
    writer.saveValue(QLatin1String(TOOLCHAIN_FILE_VERSION_KEY), 1);

    int count = 0;

    foreach (const QVariantMap &toolChainMap, toolChainHash.values()) {
        if (toolChainMap.isEmpty())
            continue;
        writer.saveValue(QLatin1String(TOOLCHAIN_DATA_KEY) + QString::number(count++), toolChainMap);
    }
    writer.saveValue(QLatin1String(TOOLCHAIN_COUNT_KEY), count);
    writer.save(toolChainsXmlFilePath, QLatin1String("QtCreatorToolChains"));

    return true;
}

bool RegisterToolChainOperation::undoOperation()
{
    const QStringList args = arguments();

    if (args.count() < 4) {
        setError(InvalidArguments);
        setErrorString(tr("Invalid arguments in %0: %1 arguments given, minimum 4 expected.")
            .arg(name()).arg(args.count()));
        return false;
    }

    const Installer *const installer = qVariantValue< Installer*> (value(QLatin1String("installer")));
    const QString &rootInstallPath = installer->value(QLatin1String("TargetDir"));
    if (rootInstallPath.isEmpty() || !QDir(rootInstallPath).exists()) {
        setError(UserDefinedError);
        setErrorString(tr("The given TargetDir %1 is not a valid/existing dir.").arg(rootInstallPath));
        return false;
    }

    int argCounter = 0;
    const QString &toolChainKey = args.at(argCounter++); //Qt SDK:gccPath
    Q_UNUSED(toolChainKey)
    const QString &toolChainType = args.at(argCounter++); //where this toolchain is defined in QtCreator
    const QString &displayName = args.at(argCounter++); //nice special Toolchain (Qt SDK)
    Q_UNUSED(displayName)
    const QString &abiString = args.at(argCounter++); //x86-windows-msys-pe-32bit
    const QString &compilerPath = args.at(argCounter++); //gccPath
    QString debuggerPath;
    QString armVersion;
    QString force32Bit;
    if (args.count() > argCounter)
        debuggerPath = args.at(argCounter++);
    if (args.count() > argCounter)
        armVersion = args.at(argCounter++);
    if (args.count() > argCounter)
        force32Bit = args.at(argCounter++);

    PersistentSettingsReader reader;
    QHash<QString, QVariantMap> toolChainHash;
    QString toolChainsXmlFilePath = rootInstallPath + QLatin1String(ToolChainSettingsSuffixPath);
    if (reader.load(toolChainsXmlFilePath)) {
        QVariantMap data = reader.restoreValues();

        // Check version:
        int version = data.value(QLatin1String(TOOLCHAIN_FILE_VERSION_KEY), 0).toInt();
        if (version < 1) {
            setError(UserDefinedError);
            setErrorString(tr("Toolchain settings xml file %1 has not the right version.")
                .arg(toolChainsXmlFilePath));
            return false;
        }

        const int count = data.value(QLatin1String(TOOLCHAIN_COUNT_KEY), 0).toInt();
        const QString uniqueToolChainKey = QString(QLatin1String("%1:%2.%3")).arg(toolChainType,
            compilerPath, abiString);
        for (int i = 0; i < count; ++i) {
            const QString key = QLatin1String(TOOLCHAIN_DATA_KEY) + QString::number(i);

            const QVariantMap toolChainMap = data.value(key).toMap();
            //gets the path variable, hope ".Path" will stay in QtCreator
            QStringList pathContainingKeyList = QStringList(toolChainMap.keys())
                .filter(QLatin1String(".Path"));
            foreach (const QString& pathKey, pathContainingKeyList) {
                QString path = toolChainMap.value(pathKey).toString();
                QString currentUniqueToolChainKey = toolChainMap.value(QLatin1String(ID_KEY)).toString();
                Q_ASSERT(!currentUniqueToolChainKey.isEmpty());
                if (!path.isEmpty() && QFile::exists(path)
                        && uniqueToolChainKey != currentUniqueToolChainKey) {
                    toolChainHash.insert(path, toolChainMap);
                }
            }
        }
    }

    PersistentSettingsWriter writer;
    writer.saveValue(QLatin1String(TOOLCHAIN_FILE_VERSION_KEY), 1);

    int count = 0;
    foreach (const QVariantMap &toolChainMap, toolChainHash.values()) {
        if (toolChainMap.isEmpty())
            continue;
        writer.saveValue(QLatin1String(TOOLCHAIN_DATA_KEY) + QString::number(count++),
            toolChainMap);
    }
    writer.saveValue(QLatin1String(TOOLCHAIN_COUNT_KEY), count);
    writer.save(toolChainsXmlFilePath, QLatin1String("QtCreatorToolChains"));
    return true;
}

bool RegisterToolChainOperation::testOperation()
{
    return true;
}

KDUpdater::UpdateOperation* RegisterToolChainOperation::clone() const
{
    return new RegisterToolChainOperation();
}
