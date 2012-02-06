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

#include "qtcreatorpersistentsettings.h"
#include "qtcreator_constants.h"

#include <QDebug>
#include <QStringList>
#include <QFile>
#include <QDir>

QtCreatorPersistentSettings::QtCreatorPersistentSettings()
{
}

bool QtCreatorPersistentSettings::init(const QString &fileName)
{
    Q_ASSERT(!fileName.isEmpty());
    if (fileName.isEmpty())
        return false;
    m_fileName = fileName;
    if (m_reader.load(m_fileName) && !versionCheck())
        return false;

    m_toolChains = readValidToolChains();
    m_abiToDebuggerHash = readAbiToDebuggerHash();

    return true;
}

bool QtCreatorPersistentSettings::versionCheck()
{
    QVariantMap data = m_reader.restoreValues();

    // Check version:
    int version = data.value(QLatin1String(TOOLCHAIN_FILE_VERSION_KEY), 0).toInt();
    if (version < 1)
        return false;
    return true;
}


QHash<QString, QVariantMap> QtCreatorPersistentSettings::readValidToolChains()
{
    if (m_fileName.isEmpty())
        return QHash<QString, QVariantMap>();

    QHash<QString, QVariantMap> toolChainHash;

    QVariantMap data = m_reader.restoreValues();

    int count = data.value(QLatin1String(TOOLCHAIN_COUNT_KEY), 0).toInt();
    for (int i = 0; i < count; ++i) {
        const QString key = QLatin1String(TOOLCHAIN_DATA_KEY) + QString::number(i);

        const QVariantMap toolChainMap = data.value(key).toMap();

        //gets the path variable, hope ".Path" will stay in QtCreator
        QStringList pathContainingKeyList = QStringList(toolChainMap.keys()).filter(QLatin1String(
            ".Path"));
        foreach (const QString& pathKey, pathContainingKeyList) {
            QString path = toolChainMap.value(pathKey).toString();
            if (!path.isEmpty() && QFile::exists(path)) {
                toolChainHash.insert(path, toolChainMap);
            }
        }
    }
    return toolChainHash;
}

QHash<QString, QString> QtCreatorPersistentSettings::abiToDebuggerHash()
{
    return m_abiToDebuggerHash;
}

QHash<QString, QString> QtCreatorPersistentSettings::readAbiToDebuggerHash()
{
    if (m_fileName.isEmpty())
        return QHash<QString, QString>();

    QHash<QString, QString> abiToDebuggerHash;

    QVariantMap data = m_reader.restoreValues();

    // Read default debugger settings (if any)
    int count = data.value(QLatin1String(DEFAULT_DEBUGGER_COUNT_KEY)).toInt();
    for (int i = 0; i < count; ++i) {
        const QString abiKey = QString::fromLatin1(DEFAULT_DEBUGGER_ABI_KEY) + QString::number(i);
        if (!data.contains(abiKey))
            continue;
        const QString pathKey = QString::fromLatin1(DEFAULT_DEBUGGER_PATH_KEY) + QString::number(i);
        if (!data.contains(pathKey))
            continue;
        abiToDebuggerHash.insert(data.value(abiKey).toString(), data.value(pathKey).toString());
    }
    return abiToDebuggerHash;
}

bool QtCreatorPersistentSettings::addToolChain(const QtCreatorToolChain &toolChain)
{
    if (toolChain.key.isEmpty())
        return false;
    if (toolChain.type.isEmpty())
        return false;
    if (toolChain.displayName.isEmpty())
        return false;
    if (toolChain.abiString.isEmpty())
        return false;
    if (toolChain.compilerPath.isEmpty())
        return false;

    QVariantMap newToolChainVariantMap;

    newToolChainVariantMap.insert(QLatin1String(ID_KEY),
        QString::fromLatin1("%1:%2.%3").arg(toolChain.type, QFileInfo(toolChain.compilerPath
            ).absoluteFilePath(), toolChain.abiString));
    newToolChainVariantMap.insert(QLatin1String(DISPLAY_NAME_KEY), toolChain.displayName);
    newToolChainVariantMap.insert(QString::fromLatin1("ProjectExplorer.%1.Path").arg(toolChain.key),
        QFileInfo(toolChain.compilerPath).absoluteFilePath());
    newToolChainVariantMap.insert(QString::fromLatin1("ProjectExplorer.%1.TargetAbi").arg(toolChain.key),
        toolChain.abiString);
    newToolChainVariantMap.insert(QString::fromLatin1("ProjectExplorer.%1.Debugger").arg(toolChain.key),
        QFileInfo(toolChain.debuggerPath).absoluteFilePath());

    m_toolChains.insert(QFileInfo(toolChain.compilerPath).absoluteFilePath(), newToolChainVariantMap);
    return true;
}

bool QtCreatorPersistentSettings::removeToolChain(const QtCreatorToolChain &toolChain)
{
    m_toolChains.remove(QFileInfo(toolChain.compilerPath).absoluteFilePath());
    return true;
}

void QtCreatorPersistentSettings::addDefaultDebugger(const QString &abiString, const QString &debuggerPath)
{
    m_abiToDebuggerHash.insert(abiString, debuggerPath);
}

void QtCreatorPersistentSettings::removeDefaultDebugger(const QString &abiString)
{
    m_abiToDebuggerHash.remove(abiString);
}

bool QtCreatorPersistentSettings::save()
{
    if (m_fileName.isEmpty())
        return false;

    m_writer.saveValue(QLatin1String(DEFAULT_DEBUGGER_COUNT_KEY), m_abiToDebuggerHash.count());

    QHashIterator<QString, QString> it(m_abiToDebuggerHash);
    int debuggerCounter = 0;
    while (it.hasNext()) {
        it.next();
        const QString abiKey = QString::fromLatin1(DEFAULT_DEBUGGER_ABI_KEY) + QString::number(
            debuggerCounter);
        const QString pathKey = QString::fromLatin1(DEFAULT_DEBUGGER_PATH_KEY) + QString::number(
            debuggerCounter);
        m_writer.saveValue(abiKey, it.key());
        m_writer.saveValue(pathKey, it.value());
        debuggerCounter++;
    }

    m_writer.saveValue(QLatin1String(TOOLCHAIN_COUNT_KEY), m_toolChains.count());

    int toolChainCounter = 0;

    foreach (QVariantMap toolChainMap, m_toolChains.values()) {
        if (toolChainMap.isEmpty())
            continue;

        //if we added a new debugger we need to adjust the tool chains
        QString abiString;
        //find the abiString
        foreach (const QString &key, toolChainMap.keys()) {
            if (key.contains(QLatin1String(".TargetAbi"))) {
                abiString = toolChainMap.value(key).toString();
                break;
            }
        }
        //adjust debugger path
        foreach (const QString &key, toolChainMap.keys()) {
            if (key.contains(QLatin1String(".Debugger"))) {
                toolChainMap.insert(key, m_abiToDebuggerHash.value(abiString));
                break;
            }
        }

        m_writer.saveValue(QLatin1String(TOOLCHAIN_DATA_KEY) + QString::number(toolChainCounter++),
            toolChainMap);
    }

    m_writer.saveValue(QLatin1String(TOOLCHAIN_FILE_VERSION_KEY), 1);

    QDir().mkpath(QFileInfo(m_fileName).absolutePath());
    return m_writer.save(m_fileName, QLatin1String("QtCreatorToolChains"));
}
