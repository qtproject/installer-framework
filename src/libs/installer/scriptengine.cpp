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
#include "scriptengine.h"

#include "messageboxhandler.h"
#include "errors.h"
#include "scriptengine_p.h"

#include <QMetaEnum>
#include <QQmlEngine>
#include <QUuid>
#include <QWizard>

namespace QInstaller {

/*!
    \class QInstaller::ScriptEngine
    \inmodule QtInstallerFramework
    \brief The ScriptEngine class is used to prepare and run the component scripts.
*/

/*!
    \qmltype console
    \inqmlmodule scripting
    \brief The console type provides methods for logging and debugging.
*/

/*!
    \qmlmethod void console::log(string value)
*/

/*!
    \qmltype QFileDialog
    \inqmlmodule scripting
*/

/*!
    \qmlmethod string QFileDialog::getExistingDirectory(string caption, string dir)
*/

/*!
    \qmlmethod string QFileDialog::getOpenFileName(string caption, string dir, string filter)
*/

/*!
    \qmltype print
    \inqmlmodule scripting
*/

/*!
    \qmltype buttons
    \inqmlmodule scripting
*/

/*!
    \qmlproperty enumeration buttons::QWizard

    \list
        \li buttons.BackButton
        \li buttons.NextButton
        \li buttons.CommitButton
        \li buttons.FinishButton
        \li buttons.CancelButton
        \li buttons.HelpButton
        \li buttons.CustomButton1
        \li buttons.CustomButton2
        \li buttons.CustomButton3
    \endlist
*/

/*!
    \qmltype QDesktopServices
    \inqmlmodule scripting
*/

/*!
    \qmlproperty enumeration QDesktopServices::QStandardPaths

    \list
        \li DesktopServices.DesktopLocation
        \li DesktopServices.DocumentsLocation
        \li DesktopServices.FontsLocation
        \li DesktopServices.ApplicationsLocation
        \li DesktopServices.MusicLocation
        \li DesktopServices.MoviesLocation
        \li DesktopServices.PicturesLocation
        \li DesktopServices.TempLocation
        \li DesktopServices.HomeLocation
        \li DesktopServices.DataLocation
        \li DesktopServices.CacheLocation
        \li DesktopServices.GenericDataLocation
        \li DesktopServices.RuntimeLocation
        \li DesktopServices.ConfigLocation
        \li DesktopServices.DownloadLocation
        \li DesktopServices.GenericCacheLocation
        \li DesktopServices.GenericConfigLocation
    \endlist
*/

/*!
    \qmlmethod boolean QDesktopServices::openUrl(string url)
*/

/*!
    \qmlmethod string QDesktopServices::displayName(int location)
*/

/*!
    \qmlmethod string QDesktopServices::storageLocation(int location)
*/

/*!
    \qmltype QInstaller
    \inqmlmodule scripting

    \brief The QInstaller type provides access to the installer status and pages from Qt Script.
*/

/*!
   \qmlproperty enumeration QInstaller::WizardPage

    The installer has various pre-defined pages that can be used to for example insert pages
    in a certain place:
    \list
        \li QInstaller.Introduction
        \li QInstaller.TargetDirectory
        \li QInstaller.ComponentSelection
        \li QInstaller.LicenseCheck
        \li QInstaller.StartMenuSelection
        \li QInstaller.ReadyForInstallation
        \li QInstaller.PerformInstallation
        \li QInstaller.InstallationFinished
        \li QInstaller.End
    \endlist
*/


/*!
    \qmlproperty enumeration QInstaller::status

    Status of the installer.

    Possible values are:
    \list
        \li QInstaller.Success (deprecated: QInstaller.InstallerSucceeded)
        \li QInstaller.Failure (deprecated: QInstaller.InstallerFailed)
        \li QInstaller.Running (deprecated: QInstaller.InstallerFailed)
        \li QInstaller.Canceled (deprecated: QInstaller.CanceledByUser)
        \li QInstaller.Unfinished (deprecated: QInstaller.InstallerUnfinished)
        \li QInstaller.ForceUpdate
    \endlist
*/

/*!
    \qmltype gui
    \inqmlmodule scripting
*/

/*!
    \qmlmethod object gui::pageById(int id)
*/

/*!
    \qmlmethod object gui::pageByObjectName(string name)
*/

/*!
    \qmlmethod object gui::currentPageWidget()
*/

/*!
    \qmlmethod object gui::pageWidgetByObjectName(string name)
*/

/*!
    \qmlmethod string gui::defaultButtonText(int wizardButton)
*/

/*!
    \qmlmethod void gui::clickButton(int wizardButton, int delayInMs)
*/

/*!
    \qmlmethod boolean gui::isButtonEnabled(int wizardButton)
*/

/*!
    \qmlmethod void gui::showSettingsButton(boolean show)
*/

/*!
    \qmlmethod void gui::setSettingsButtonEnabled(boolean enable)
*/

/*!
    \qmlsignal gui::interrupted()
*/

/*!
    \qmlsignal gui::languageChanged()
*/

/*!
    \qmlsignal gui::finishButtonClicked()
*/

/*!
    \qmlsignal gui::gotRestarted()
*/

/*!
    \qmlsignal gui::settingsButtonClicked();
*/

/*!
    Constructs a script engine with \a core as parent.
*/
ScriptEngine::ScriptEngine(PackageManagerCore *core)
    : QObject(core)
{
    QJSValue global = m_engine.globalObject();
    global.setProperty(QLatin1String("console"), m_engine.newQObject(new ConsoleProxy));
    global.setProperty(QLatin1String("QFileDialog"), m_engine.newQObject(new QFileDialogProxy));
    const QJSValue proxy = m_engine.newQObject(new InstallerProxy(&m_engine, core));
    global.setProperty(QLatin1String("InstallerProxy"), proxy);
    global.setProperty(QLatin1String("print"), m_engine.newQObject(new ConsoleProxy)
        .property(QLatin1String("log")));
#if QT_VERSION < 0x050400
    global.setProperty(QLatin1String("qsTr"), m_engine.newQObject(new QCoreApplicationProxy)
        .property(QStringLiteral("qsTr")));
#else
    m_engine.installTranslatorFunctions();
#endif
    global.setProperty(QLatin1String("QInstaller"), generateQInstallerObject());
    global.setProperty(QLatin1String("buttons"), generateWizardButtonsObject());
    global.setProperty(QLatin1String("QMessageBox"), generateMessageBoxObject());
    global.setProperty(QLatin1String("QDesktopServices"), generateDesktopServicesObject());

    if (core) {
        setGuiQObject(core->guiObject());
        QQmlEngine::setObjectOwnership(core, QQmlEngine::CppOwnership);
        global.setProperty(QLatin1String("installer"), m_engine.newQObject(core));
        connect(core, SIGNAL(guiObjectChanged(QObject*)), this, SLOT(setGuiQObject(QObject*)));

        const QList<Component*> all = core->components(PackageManagerCore::ComponentType::All);
        QJSValue scriptComponentsObject = m_engine.newArray(all.count());
        for (int i = 0; i < all.count(); ++i) {
            Component *const component = all.at(i);
            QQmlEngine::setObjectOwnership(component, QQmlEngine::CppOwnership);
            scriptComponentsObject.setProperty(i, newQObject(component));
        }
        global.property(QLatin1String("installer")).setProperty(QLatin1String("components"),
            scriptComponentsObject);
    } else {
        global.setProperty(QLatin1String("installer"), m_engine.newQObject(new QObject));
    }

    global.property(QLatin1String("installer")).setProperty(QLatin1String("componentByName"),
        proxy.property(QLatin1String("componentByName")));
}

QJSValue ScriptEngine::evaluate(const QString &program, const QString &fileName, int lineNumber)
{
    return m_engine.evaluate(program, fileName, lineNumber);
}

void ScriptEngine::addQObjectChildren(QObject *root)
{
    if ((!root) || root->objectName().isEmpty())
        return;

    const QObjectList children = root->children();
    QJSValue jsParent = newQObject(root);
    m_engine.globalObject().setProperty(root->objectName(), jsParent);

    foreach (QObject *const child, children) {
        if (child->objectName().isEmpty())
            continue;
        jsParent.setProperty(child->objectName(), m_engine.newQObject(child));
        addQObjectChildren(child);
    }
}

/*!
    Loads a script into the given \a context at \a fileName inside the ScriptEngine.

    The installer and all its components as well as other useful stuff are being exported into the
    script. Read \link componentscripting Component Scripting \endlink for details.
    Throws Error when either the script at \a fileName could not be opened, or the QScriptEngine
    couldn't evaluate the script.
*/
QJSValue ScriptEngine::loadInContext(const QString &context, const QString &fileName,
    const QString &scriptInjection)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        throw Error(tr("Could not open the requested script file at %1: %2.")
            .arg(fileName, file.errorString()));
    }

    // Create a closure. Put the content in the first line to keep line number order in case of an
    // exception. Script content will be added as the last argument to the command to prevent wrong
    // replacements of %1, %2 or %3 inside the javascript code.
    const QString scriptContent = QLatin1String("(function() {")
        + scriptInjection + QString::fromUtf8(file.readAll())
        + QString::fromLatin1(";"
        "    if (typeof %1 != \"undefined\")"
        "        return new %1;"
        "    else"
        "        throw \"Missing Component constructor. Please check your script.\";"
        "})();").arg(context);
    QJSValue scriptContext = evaluate(scriptContent, fileName);
    scriptContext.setProperty(QLatin1String("Uuid"), QUuid::createUuid().toString());
    if (scriptContext.isError()) {
        throw Error(tr("Exception while loading the component script '%1'. (%2)").arg(
            QFileInfo(file).absoluteFilePath(), scriptContext.toString().isEmpty() ?
            QString::fromLatin1("Unknown error.") : scriptContext.toString()));
    }
    return scriptContext;
}

/*!
    Tries to call the method with \a name within the script and returns the result. If the method
    doesn't exist or is not callable, an undefined result is returned. If the call to the method
    succeeds and the return value is still undefined, a null value will be returned instead.
    If the method call has an exception, its string representation is thrown as an Error exception.

    \note The method is not called, if the current script context is the same method, to avoid
    infinite recursion.
*/
QJSValue ScriptEngine::callScriptMethod(const QJSValue &scriptContext, const QString &methodName,
    const QJSValueList &arguments)
{
    // don't allow a recursion
    const QString key = scriptContext.property(QLatin1String("Uuid")).toString();
    QStringList stack = m_callstack.value(key);
    if (m_callstack.contains(key) && stack.value(stack.size() - 1).startsWith(methodName))
        return QJSValue(QJSValue::UndefinedValue);

    stack.append(methodName);
    m_callstack.insert(key, stack);

    QJSValue method = scriptContext.property(methodName);
    if (!method.isCallable())
        return QJSValue(QJSValue::UndefinedValue);
    if (method.isError()) {
        throw Error(method.toString().isEmpty() ? QString::fromLatin1("Unknown error.")
            : method.toString());
    }

    const QJSValue result = method.call(arguments);
    if (result.isError()) {
        throw Error(result.toString().isEmpty() ? QString::fromLatin1("Unknown error.")
            : result.toString());
    }

    stack.removeLast();
    m_callstack.insert(key, stack);
    return result.isUndefined() ? QJSValue(QJSValue::NullValue) : result;
}


// -- private slots

void ScriptEngine::setGuiQObject(QObject *guiQObject)
{
    QQmlEngine::setObjectOwnership(guiQObject, QQmlEngine::CppOwnership);
    m_engine.globalObject().setProperty(QLatin1String("gui"), m_engine.newQObject(guiQObject));
}


// -- private

#undef SETPROPERTY
#define SETPROPERTY(a, x, t) a.setProperty(QLatin1String(#x), QJSValue(t::x));

QJSValue ScriptEngine::generateWizardButtonsObject()
{
    QJSValue buttons = m_engine.newArray();
    SETPROPERTY(buttons, BackButton, QWizard)
    SETPROPERTY(buttons, NextButton, QWizard)
    SETPROPERTY(buttons, CommitButton, QWizard)
    SETPROPERTY(buttons, FinishButton, QWizard)
    SETPROPERTY(buttons, CancelButton, QWizard)
    SETPROPERTY(buttons, HelpButton, QWizard)
    SETPROPERTY(buttons, CustomButton1, QWizard)
    SETPROPERTY(buttons, CustomButton2, QWizard)
    SETPROPERTY(buttons, CustomButton3, QWizard)
    return buttons;
}

/*!
    \internal
    generates QMessageBox::StandardButton enum as an QScriptValue array
*/
QJSValue ScriptEngine::generateMessageBoxObject()
{
    QJSValue messageBox = m_engine.newQObject(MessageBoxHandler::instance());

    const QMetaObject &messageBoxMetaObject = QMessageBox::staticMetaObject;
    const int index = messageBoxMetaObject.indexOfEnumerator("StandardButtons");

    QJSValue value = m_engine.newArray();
    value.setProperty(QLatin1String("NoButton"), QJSValue(QMessageBox::NoButton));

    const QMetaEnum metaEnum = messageBoxMetaObject.enumerator(index);
    for (int i = 0; i < metaEnum.keyCount(); i++) {
        const int enumValue = metaEnum.value(i);
        if (enumValue >= QMessageBox::FirstButton)
            value.setProperty(QLatin1String(metaEnum.valueToKey(enumValue)), QJSValue(enumValue));
        if (enumValue == QMessageBox::LastButton)
            break;
    }

    messageBox.setPrototype(value);
    return messageBox;
}

QJSValue ScriptEngine::generateDesktopServicesObject()
{
    QJSValue desktopServices = m_engine.newArray();
    SETPROPERTY(desktopServices, DesktopLocation, QStandardPaths)
    SETPROPERTY(desktopServices, DocumentsLocation, QStandardPaths)
    SETPROPERTY(desktopServices, FontsLocation, QStandardPaths)
    SETPROPERTY(desktopServices, ApplicationsLocation, QStandardPaths)
    SETPROPERTY(desktopServices, MusicLocation, QStandardPaths)
    SETPROPERTY(desktopServices, MoviesLocation, QStandardPaths)
    SETPROPERTY(desktopServices, PicturesLocation, QStandardPaths)
    SETPROPERTY(desktopServices, TempLocation, QStandardPaths)
    SETPROPERTY(desktopServices, HomeLocation, QStandardPaths)
    SETPROPERTY(desktopServices, DataLocation, QStandardPaths)
    SETPROPERTY(desktopServices, CacheLocation, QStandardPaths)
    SETPROPERTY(desktopServices, GenericDataLocation, QStandardPaths)
    SETPROPERTY(desktopServices, RuntimeLocation, QStandardPaths)
    SETPROPERTY(desktopServices, ConfigLocation, QStandardPaths)
    SETPROPERTY(desktopServices, DownloadLocation, QStandardPaths)
    SETPROPERTY(desktopServices, GenericCacheLocation, QStandardPaths)
    SETPROPERTY(desktopServices, GenericConfigLocation, QStandardPaths)

    QJSValue object = m_engine.newQObject(new QDesktopServicesProxy);
    object.setPrototype(desktopServices);   // attach the properties
    return object;
}

QJSValue ScriptEngine::generateQInstallerObject()
{
    // register ::WizardPage enum in the script connection
    QJSValue qinstaller = m_engine.newArray();
    SETPROPERTY(qinstaller, Introduction, PackageManagerCore)
    SETPROPERTY(qinstaller, LicenseCheck, PackageManagerCore)
    SETPROPERTY(qinstaller, TargetDirectory, PackageManagerCore)
    SETPROPERTY(qinstaller, ComponentSelection, PackageManagerCore)
    SETPROPERTY(qinstaller, StartMenuSelection, PackageManagerCore)
    SETPROPERTY(qinstaller, ReadyForInstallation, PackageManagerCore)
    SETPROPERTY(qinstaller, PerformInstallation, PackageManagerCore)
    SETPROPERTY(qinstaller, InstallationFinished, PackageManagerCore)
    SETPROPERTY(qinstaller, End, PackageManagerCore)

    // register ::Status enum in the script connection
    SETPROPERTY(qinstaller, Success, PackageManagerCore)
    SETPROPERTY(qinstaller, Failure, PackageManagerCore)
    SETPROPERTY(qinstaller, Running, PackageManagerCore)
    SETPROPERTY(qinstaller, Canceled, PackageManagerCore)
    SETPROPERTY(qinstaller, Unfinished, PackageManagerCore)
    SETPROPERTY(qinstaller, ForceUpdate, PackageManagerCore)
    return qinstaller;
}

#undef SETPROPERTY

}   // namespace QInstaller
