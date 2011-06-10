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

    QHash< QString, QVariantMap > toolChainHash;

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
        QString(QLatin1String("%1:%2.%3")).arg(toolChain.type, QFileInfo(toolChain.compilerPath
            ).absoluteFilePath(), toolChain.abiString));
    newToolChainVariantMap.insert(QLatin1String(DISPLAY_NAME_KEY), toolChain.displayName);
    newToolChainVariantMap.insert(QString(QLatin1String("ProjectExplorer.%1.Path")).arg(toolChain.key),
        QFileInfo(toolChain.compilerPath).absoluteFilePath());
    newToolChainVariantMap.insert(QString(QLatin1String("ProjectExplorer.%1.TargetAbi")).arg(toolChain.key),
        toolChain.abiString);
    newToolChainVariantMap.insert(QString(QLatin1String("ProjectExplorer.%1.Debugger")).arg(toolChain.key),
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
    m_abiToDebuggerHash.insert(abiString, QFileInfo(debuggerPath).absoluteFilePath());
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
        foreach (const QString &key, toolChainMap.keys()) {
            if (key.contains(QLatin1String(".TargetAbi"))) {
                abiString = toolChainMap.value(key).toString();
                break;
            }
        }
        foreach (const QString &key, toolChainMap.keys()) {
            if (key.contains(QLatin1String(".Debugger"))
                    && toolChainMap.value(key).toString().isEmpty()) {
                toolChainMap.insert(key, m_abiToDebuggerHash.value(abiString));
                break;
            }
        }

        m_writer.saveValue(QLatin1String(TOOLCHAIN_DATA_KEY) + QString::number(toolChainCounter++),
            toolChainMap);
    }

    m_writer.saveValue(QLatin1String(TOOLCHAIN_FILE_VERSION_KEY), 1);

    QDir pathToFile(m_fileName);
    pathToFile.cdUp();
    QDir().mkpath(pathToFile.absolutePath());
    return m_writer.save(m_fileName, QLatin1String("QtCreatorToolChains"));
}
