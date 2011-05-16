#ifndef QTCREATORPERSISTENTSETTINGS_H
#define QTCREATORPERSISTENTSETTINGS_H

#include "persistentsettings.h"

#include <QHash>
#include <QString>
#include <QVariantMap>

struct QtCreatorToolChain {
    QString key;
    QString type;
    QString displayName;
    QString abiString;
    QString compilerPath;
    QString debuggerPath;
    QString armVersion;
    QString force32Bit;
};

class QtCreatorPersistentSettings
{
public:
    QtCreatorPersistentSettings();
    bool init(const QString &fileName);
    bool addToolChain(const QtCreatorToolChain &toolChain);
    bool removeToolChain(const QtCreatorToolChain &toolChain);
    void addDefaultDebugger(const QString &abiString, const QString &debuggerPath);
    void removeDefaultDebugger(const QString &abiString);
    bool save();
    QHash<QString, QString> abiToDebuggerHash();

private:
    QHash<QString, QVariantMap> readValidToolChains();
    QHash<QString, QString> readAbiToDebuggerHash();
    bool versionCheck();

    QString m_fileName;
    QHash<QString, QVariantMap> m_toolChains;
    QHash<QString, QString> m_abiToDebuggerHash;
    ProjectExplorer::PersistentSettingsReader m_reader;
    ProjectExplorer::PersistentSettingsWriter m_writer;

};

#endif // QTCREATORPERSISTENTSETTINGS_H
