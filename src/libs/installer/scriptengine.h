/**************************************************************************
**
** Copyright (C) 2023 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
**************************************************************************/

#ifndef SCRIPTENGINE_H
#define SCRIPTENGINE_H

#include "installer_global.h"

#include <QJSValue>
#include <QJSEngine>

namespace QInstaller {

class PackageManagerCore;
class GuiProxy;

class INSTALLER_EXPORT ScriptEngine : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(ScriptEngine)

public:
    explicit ScriptEngine(PackageManagerCore *core = 0);

    QJSValue globalObject() const { return m_engine.globalObject(); }
    QJSValue newQObject(QObject *object, bool qtScriptCompat = true);
    QJSValue newArray(uint length = 0);
    QJSValue evaluate(const QString &program, const QString &fileName = QString(),
        int lineNumber = 1);

    void addToGlobalObject(QObject *object);
    void removeFromGlobalObject(QObject *object);

    QJSValue loadInContext(const QString &context, const QString &fileName,
        const QString &scriptInjection = QString());
    QJSValue callScriptMethod(const QJSValue &context, const QString &methodName,
        const QJSValueList &arguments = QJSValueList());

private slots:
    void setGuiQObject(QObject *guiQObject);

private:
    QJSValue generateMessageBoxObject();
    QJSValue generateQInstallerObject();
    QJSValue generateWizardButtonsObject();
    QJSValue generateDesktopServicesObject();
#ifdef Q_OS_WIN
    QJSValue generateSettingsObject();
#endif

private:
    QJSEngine m_engine;
    QHash<QString, QStringList> m_callstack;
    GuiProxy *m_guiProxy;
    PackageManagerCore *m_core;
};

}
Q_DECLARE_METATYPE(QInstaller::ScriptEngine*)

#endif // SCRIPTENGINE_H
