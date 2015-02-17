/**************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
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
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
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
#include "systeminfo.h"

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
    \brief Provides methods for logging and debugging.

    You can use the \c console object to print log information about installer
    functions to the console. The following example uses the \c console object
    \l{console::log()}{log} method and \l installer object
    \l{installer::isUpdater()}, \l{installer::isUninstaller()}, and
    \l{installer::isPackageManager()} methods to display a message that
    indicates whether the maintenance tool is currently being used to update,
    remove, or add components.

    \code
    onPackageManagerCoreTypeChanged = function()
    {
        console.log("Is Updater: " + installer.isUpdater());
        console.log("Is Uninstaller: " + installer.isUninstaller());
        console.log("Is Package Manager: " + installer.isPackageManager());
    }
    \endcode
*/

/*!
    \qmlmethod void console::log(string value)

    Prints the string specified by \a value to the console.
*/

/*!
    \qmltype QFileDialog
    \inqmlmodule scripting
    \brief Provides a dialog that allows users to select files or directories.

    Use the QFileDialog::getExistingDirectory() method to create a modal dialog
    that displays an existing directory selected by the user. Use the
    QFileDialog::getOpenFileName() method to create a dialog that displays
    matching files in the directory selected by the user.
*/

/*!
    \qmlmethod string QFileDialog::getExistingDirectory(string caption, string dir)

    Returns an existing directory selected by the user.

    The dialog's working directory is set to \a dir, and the caption is set to
    \a caption. Either of these may be an empty string, in which case the
    current directory and a default caption will be used, respectively.
*/

/*!
    \qmlmethod string QFileDialog::getOpenFileName(string caption, string dir, string filter)

    Returns an existing file selected by the user. If the user selects
    \uicontrol Cancel, returns a null string.

    The file dialog's caption is set to \a caption. If \c caption is not
    specified, a default caption is used.

    The file dialog's working directory is set to \a dir. If \c dir includes a
    file name, the file will be selected. Only files that match the specified
    \a filter are shown. Either of these may be an empty string.

    To specify multiple filters, separate them with two semicolons (;;). For
    example:

    \code
    "Images (*.png *.xpm *.jpg);;Text files (*.txt);;XML files (*.xml)"
    \endcode

    On Windows, and OS X, this static function will use the native file dialog
    and not a QFileDialog.
*/

/*!
    \qmltype print
    \inqmlmodule scripting
*/

/*!
    \qmltype buttons
    \inqmlmodule scripting

    \brief Provides buttons that can be used on installer pages.

    You can use a set of standard buttons and some custom buttons on the
    installer pages. For more information about the buttons used by default on
    each installer page, see \l {Controller Scripting}.
*/

/*!
    \qmlproperty enumeration buttons::QWizard

    Specifies the buttons on an installer page.

    \value  buttons.BackButton
            The \uicontrol Back button (\uicontrol {Go Back} on OS X.)
    \value  buttons.NextButton
            The \uicontrol Next button (\uicontrol Continue on OS X.)
    \value  buttons.CommitButton
            The \uicontrol Commit button.
    \value  buttons.FinishButton
            The \uicontrol Finish button (\uicontrol Done on OS X.)
    \value  buttons.CancelButton
            The \uicontrol Cancel button.
    \value  buttons.HelpButton
            The \uicontrol Help button.
    \value  buttons.CustomButton1
            A custom button.
    \value  buttons.CustomButton2
            A custom button.
    \value  buttons.CustomButton3
            A custom button.
*/

/*!
    \qmltype QDesktopServices
    \inqmlmodule scripting

    \brief Provides methods for accessing common desktop services.

    Many desktop environments provide services that can be used by applications
    to perform common tasks, such as opening a file, in a way that is both
    consistent and takes into account the user's application preferences.

    This object contains methods that provide simple interfaces to these
    services that indicate whether they succeeded or failed.

    The openUrl() method is used to open files located at arbitrary URLs in
    external applications. For URLs that correspond to resources on the local
    filing system (where the URL scheme is "file"), a suitable application is
    used to open the file.

    The displayName() and storageLocation() methods take one of the following
    enums as an argument:

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

    The enum values correspond to the values of the
    \l{QStandardPaths::StandardLocation} enum with the same names.
*/

/*!
    \qmlproperty enumeration QDesktopServices::QStandardPaths
    \internal
*/

/*!
    \qmlmethod boolean QDesktopServices::openUrl(string url)

    Uses the URL scheme \c file to open the specified \a url with a suitable
    application.
*/

/*!
    \qmlmethod string QDesktopServices::displayName(int location)

    Returns a localized display name for the specified \a location or an empty
    QString if no relevant location can be found.
*/

/*!
    \qmlmethod string QDesktopServices::storageLocation(int location)

    Returns the specified \a location.
*/

/*!
    \qmltype QInstaller
    \inqmlmodule scripting

    \brief Provides access to the installer status and pages from Qt Script.

    For more information about using the \c QInstaller object in control
    scripts, see \l{Controller Scripting}.

    For examples of using the pages to support end user workflows, see
    \l{End User Workflows}.

*/

/*!
   \qmlproperty enumeration QInstaller::WizardPage

    The installer has various pre-defined pages that can be used to for example insert pages
    in a certain place:

    \value  QInstaller.Introduction
            \l{Introduction Page}
    \value  QInstaller.TargetDirectory
            \l{Target Directory Page}
    \value  QInstaller.ComponentSelection
            \l{Component Selection Page}
    \value  QInstaller.LicenseCheck
            \l{License Agreement Page}
    \value  QInstaller.StartMenuSelection
            \l{Start Menu Directory Page}
    \value  QInstaller.ReadyForInstallation
            \l{Ready for Installation Page}
    \value  QInstaller.PerformInstallation
            \l{Perform Installation Page}
    \value  QInstaller.InstallationFinished
            \l{Finished Page}

    \omitvalue  QInstaller.End
*/


/*!
    \qmlproperty enumeration QInstaller::status

    Status of the installer.

    Possible values are:

    \value  QInstaller.Success
            Installation was successful.
    \value  QInstaller.Failure
            Installation failed.
    \value  QInstaller.Running
            Installation is in progress.
    \value  QInstaller.Canceled
            Installation was canceled.
    \value  QInstaller.Unfinished
            Installation was not completed.
    \value  QInstaller.ForceUpdate
*/

/*!
    \qmltype gui
    \inqmlmodule scripting
    \brief Enables interaction with the installer UI.
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

QJSValue InstallerProxy::componentByName(const QString &componentName)
{
    if (m_core)
        return m_engine->newQObject(m_core->componentByName(componentName));
    return QJSValue();
}

GuiProxy::GuiProxy(ScriptEngine *engine, QObject *parent) :
    QObject(parent),
    m_engine(engine),
    m_gui(0)
{
}

void GuiProxy::setPackageManagerGui(PackageManagerGui *gui)
{
    if (m_gui) {
        disconnect(m_gui, &PackageManagerGui::interrupted, this, &GuiProxy::interrupted);
        disconnect(m_gui, &PackageManagerGui::languageChanged, this, &GuiProxy::languageChanged);
        disconnect(m_gui, &PackageManagerGui::finishButtonClicked, this, &GuiProxy::finishButtonClicked);
        disconnect(m_gui, &PackageManagerGui::gotRestarted, this, &GuiProxy::gotRestarted);
        disconnect(m_gui, &PackageManagerGui::settingsButtonClicked, this, &GuiProxy::settingsButtonClicked);
    }

    m_gui = gui;

    if (m_gui) {
        connect(m_gui, &PackageManagerGui::interrupted, this, &GuiProxy::interrupted);
        connect(m_gui, &PackageManagerGui::languageChanged, this, &GuiProxy::languageChanged);
        connect(m_gui, &PackageManagerGui::finishButtonClicked, this, &GuiProxy::finishButtonClicked);
        connect(m_gui, &PackageManagerGui::gotRestarted, this, &GuiProxy::gotRestarted);
        connect(m_gui, &PackageManagerGui::settingsButtonClicked, this, &GuiProxy::settingsButtonClicked);
    }
}

/*!
    \qmlmethod object gui::pageById(int id)

    Returns the installer page specified by \a id. The values of \c id for the
    available installer pages are provided by QInstaller::WizardPage.
*/
QJSValue GuiProxy::pageById(int id) const
{
    if (!m_gui)
        return QJSValue();
    return m_engine->newQObject(m_gui->pageById(id));
}

/*!
    \qmlmethod object gui::pageByObjectName(string name)

    Returns the installer page specified by \a name. The value of \c name is the
    object name set in the UI file that defines the installer page.
*/
QJSValue GuiProxy::pageByObjectName(const QString &name) const
{
    if (!m_gui)
        return QJSValue();
    return m_engine->newQObject(m_gui->pageByObjectName(name));
}

/*!
    \qmlmethod object gui::currentPageWidget()

    Returns the current wizard page.
*/
QJSValue GuiProxy::currentPageWidget() const
{
    if (!m_gui)
        return QJSValue();
    return m_engine->newQObject(m_gui->currentPageWidget());
}

/*!
    \qmlmethod object gui::pageWidgetByObjectName(string name)
*/
QJSValue GuiProxy::pageWidgetByObjectName(const QString &name) const
{
    if (!m_gui)
        return QJSValue();
    return m_engine->newQObject(m_gui->pageWidgetByObjectName(name));
}

/*!
    \qmlmethod string gui::defaultButtonText(int wizardButton)
*/
QString GuiProxy::defaultButtonText(int wizardButton) const
{
    if (!m_gui)
        return QString();
    return m_gui->defaultButtonText(wizardButton);
}

/*!
    \qmlmethod void gui::clickButton(int wizardButton, int delayInMs)

    Automatically clicks the button specified by \a wizardButton after a delay
    in milliseconds specified by \a delayInMs.
*/
void GuiProxy::clickButton(int wizardButton, int delayInMs)
{
    if (m_gui)
        m_gui->clickButton(wizardButton, delayInMs);
}

/*!
    \qmlmethod boolean gui::isButtonEnabled(int wizardButton)
*/
bool GuiProxy::isButtonEnabled(int wizardButton)
{
    if (!m_gui)
        return false;
    return m_gui->isButtonEnabled(wizardButton);
}

/*!
    \qmlmethod void gui::showSettingsButton(boolean show)
*/
void GuiProxy::showSettingsButton(bool show)
{
    if (m_gui)
        m_gui->showSettingsButton(show);
}

/*!
    \qmlmethod void gui::setSettingsButtonEnabled(boolean enable)
*/
void GuiProxy::setSettingsButtonEnabled(bool enable)
{
    if (m_gui)
        m_gui->setSettingsButtonEnabled(enable);
}

/*!
    \qmlmethod object gui::findChild(object parent, string objectName)

    Returns the first descendant of \a parent that has \a objectName as name.

    \sa QObject::findChild
*/
QJSValue GuiProxy::findChild(QObject *parent, const QString &objectName)
{
    return m_engine->newQObject(parent->findChild<QObject*>(objectName));
}

/*!
    \qmlmethod object[] gui::findChildren(object parent, string objectName)

    Returns all descendants of \a parent that have \a objectName as name.

    \sa QObject::findChildren
*/
QList<QJSValue> GuiProxy::findChildren(QObject *parent, const QString &objectName)
{
    QList<QJSValue> children;
    foreach (QObject *child, parent->findChildren<QObject*>(objectName))
        children.append(m_engine->newQObject(child));
    return children;
}

/*!
    \qmlmethod void gui::cancelButtonClicked()
*/
void GuiProxy::cancelButtonClicked()
{
    if (m_gui)
        m_gui->cancelButtonClicked();
}

/*!
    \qmlmethod void gui::reject()
*/
void GuiProxy::reject()
{
    if (m_gui)
        m_gui->reject();
}

/*!
    \qmlmethod void gui::rejectWithoutPrompt()
*/
void GuiProxy::rejectWithoutPrompt()
{
    if (m_gui)
        m_gui->rejectWithoutPrompt();
}

/*!
    \qmlmethod void gui::showFinishedPage()
*/
void GuiProxy::showFinishedPage()
{
    if (m_gui)
        m_gui->showFinishedPage();
}

/*!
    \qmlmethod void gui::setModified(boolean value)
*/
void GuiProxy::setModified(bool value)
{
    if (m_gui)
        m_gui->setModified(value);
}


/*!
    Constructs a script engine with \a core as parent.
*/
ScriptEngine::ScriptEngine(PackageManagerCore *core) :
    QObject(core),
    m_guiProxy(new GuiProxy(this, this))
{
    QJSValue global = m_engine.globalObject();
    global.setProperty(QLatin1String("console"), m_engine.newQObject(new ConsoleProxy));
    global.setProperty(QLatin1String("QFileDialog"), m_engine.newQObject(new QFileDialogProxy));
    const QJSValue proxy = m_engine.newQObject(new InstallerProxy(this, core));
    global.setProperty(QLatin1String("InstallerProxy"), proxy);
    global.setProperty(QLatin1String("print"), m_engine.newQObject(new ConsoleProxy)
        .property(QLatin1String("log")));
#if QT_VERSION < 0x050400
    global.setProperty(QLatin1String("qsTr"), m_engine.newQObject(new QCoreApplicationProxy)
        .property(QStringLiteral("qsTr")));
#else
    m_engine.installTranslatorFunctions();
#endif
    global.setProperty(QLatin1String("systemInfo"), m_engine.newQObject(new SystemInfo));

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
    global.setProperty(QLatin1String("gui"), m_engine.newQObject(m_guiProxy));

    global.property(QLatin1String("installer")).setProperty(QLatin1String("componentByName"),
        proxy.property(QLatin1String("componentByName")));
}

/*!
    Creates a JavaScript object that wraps the given QObject \a object.

    Signals and slots, properties and children of \a object are
    available as properties of the created QJSValue. In addition some helper methods and properties
    are added:

    findChild(), findChildren() recursively search for child objects with the given object name.

    Direct child objects are made accessible as properties under their respective object names.
 */
QJSValue ScriptEngine::newQObject(QObject *object)
{
    QJSValue jsValue = m_engine.newQObject(object);
    if (!jsValue.isQObject())
        return jsValue;

    QQmlEngine::setObjectOwnership(object, QQmlEngine::CppOwnership);

    // add findChild(), findChildren() methods known from QtScript
    QJSValue findChild = m_engine.evaluate(
                QLatin1String("(function() { return gui.findChild(this, arguments[0]); })"));
    QJSValue findChildren = m_engine.evaluate(
                QLatin1String("(function() { return gui.findChildren(this, arguments[0]); })"));
    jsValue.setProperty(QLatin1String("findChild"), findChild);
    jsValue.setProperty(QLatin1String("findChildren"), findChildren);

    // add all named children as properties
    foreach (QObject *const child, object->children()) {
        if (child->objectName().isEmpty())
            continue;
        jsValue.setProperty(child->objectName(), m_engine.newQObject(child));
        newQObject(child);
    }

    return jsValue;
}

QJSValue ScriptEngine::evaluate(const QString &program, const QString &fileName, int lineNumber)
{
    return m_engine.evaluate(program, fileName, lineNumber);
}

/*!
    Registers QObject \a object in the engine, and makes it globally accessible under its object name.
 */
void ScriptEngine::addToGlobalObject(QObject *object)
{
    if (!object || object->objectName().isEmpty())
        return;

    QJSValue value = newQObject(object);
    globalObject().setProperty(object->objectName(), value);
}

/*!
    Removes the \a object name from the global object.
 */
void ScriptEngine::removeFromGlobalObject(QObject *object)
{
    globalObject().deleteProperty(object->objectName());
}

/*!
    Loads a script into the given \a context at \a fileName inside the ScriptEngine.

    The installer and all its components as well as other useful stuff are being exported into the
    script. For more information, see \l {Component Scripting}.
    Throws Error when either the script at \a fileName could not be opened, or the QScriptEngine
    could not evaluate the script.
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
    Tries to call the method specified by \a methodName with the arguments specified by
    \a arguments within the script and returns the result. If the method does not exist or
    is not callable, an undefined result is returned. If the call to the method
    succeeds and the return value is still undefined, a null value will be returned instead.
    If the method call has an exception, its string representation is thrown as an Error exception.

    \note The method is not called if \a scriptContext is the same method, to avoid
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
    m_guiProxy->setPackageManagerGui(qobject_cast<PackageManagerGui*>(guiQObject));
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
