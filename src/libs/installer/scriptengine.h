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

#ifndef SCRIPTENGINE_H
#define SCRIPTENGINE_H

#include "qinstallerglobal.h"

#include <QtScript/QScriptEngine>

namespace QInstaller {

QString INSTALLER_EXPORT uncaughtExceptionString(const QScriptEngine *scriptEngine, const QString &context = QString());
QScriptValue INSTALLER_EXPORT qInstallerComponentByName(QScriptContext *context, QScriptEngine *engine);

QScriptValue INSTALLER_EXPORT qDesktopServicesOpenUrl(QScriptContext *context, QScriptEngine *engine);
QScriptValue INSTALLER_EXPORT qDesktopServicesDisplayName(QScriptContext *context, QScriptEngine *engine);
QScriptValue INSTALLER_EXPORT qDesktopServicesStorageLocation(QScriptContext *context, QScriptEngine *engine);

QScriptValue INSTALLER_EXPORT qFileDialogGetExistingDirectory(QScriptContext *context, QScriptEngine *engine);
QScriptValue INSTALLER_EXPORT qFileDialogGetOpenFileName(QScriptContext *context, QScriptEngine *engine);

QScriptValue INSTALLER_EXPORT checkArguments(QScriptContext *context, int minimalArgumentCount, int maximalArgumentCount);

class PackageManagerCore;

class INSTALLER_EXPORT ScriptEngine : public QScriptEngine
{
    Q_OBJECT
    Q_DISABLE_COPY(ScriptEngine)

public:
    explicit ScriptEngine(PackageManagerCore *core);
    ~ScriptEngine();
    void setGuiQObject(QObject *guiQObject);
    QScriptValue callScriptMethod(const QScriptValue &scriptContext, const QString &name,
        const QScriptValueList &parameters = QScriptValueList()) const;

    QScriptValue loadInConext(const QString &context, const QString &fileName, const QString &scriptInjection = QString());

private:
    QScriptValue generateMessageBoxObject();
    QScriptValue generateDesktopServicesObject();
    QScriptValue generateQInstallerObject();
    PackageManagerCore *m_core;
};
}

#endif // SCRIPTENGINE_H
