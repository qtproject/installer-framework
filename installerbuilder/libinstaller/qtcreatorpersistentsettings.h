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

#ifndef QTCREATORPERSISTENTSETTINGS_H
#define QTCREATORPERSISTENTSETTINGS_H

#include "persistentsettings.h"

#include <QHash>
#include <QString>
#include <QVariantMap>

struct QtCreatorToolChain
{
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
