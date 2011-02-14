/**************************************************************************
**
** This file is part of Qt SDK**
**
** Copyright (c) 2010 Nokia Corporation and/or its subsidiary(-ies).*
**
** Contact:  Nokia Corporation qt-info@nokia.com**
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
#ifndef QINSTALLER_GLOBAL_H
#define QINSTALLER_GLOBAL_H

#include <installer_global.h>

#include <QtGlobal>
#include <qnamespace.h>

class QIODevice;
class QFile;
template <typename T> class QList;
class QScriptContext;
class QScriptEngine;
class QScriptValue;

namespace QInstaller {

class Component;

#if 0
// Faster or not?
static void appendFileData(QIODevice *out, const QString &fileName)
{
    QFile file(fileName);
    openForRead(file);
    qint64 size = file.size();
    QInstaller::appendInt(out, size);
    if (size == 0)
        return;
    uchar *data = file.map(0, size);
    if (!data)
        throw Error(QInstaller::tr("Cannot map file %1").arg(file.fileName()));
    rawWrite(out, (const char *)data, size);
    if (!file.unmap(data))
        throw Error(QInstaller::tr("Cannot unmap file %1").arg(file.fileName()));
}
#endif

    QScriptValue qDesktopServicesOpenUrl( QScriptContext* context, QScriptEngine* engine );
    QScriptValue qDesktopServicesDisplayName( QScriptContext* context, QScriptEngine* engine );
    QScriptValue qDesktopServicesStorageLocation( QScriptContext* context, QScriptEngine* engine );
    
    QScriptValue qInstallerComponentByName( QScriptContext* context, QScriptEngine* engine );

    Qt::CheckState componentCheckState( const Component* component, RunModes runMode = InstallerMode );
    QString uncaughtExceptionString(QScriptEngine *scriptEngine/*, const QString &context*/);
}

#endif // QINSTALLER_GLOBAL_H
