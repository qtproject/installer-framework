/**************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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
    \fn ScriptEngine::globalObject() const
    Returns a global object.
*/

QJSValue InstallerProxy::components() const
{
    if (m_core) {
        const QList<Component*> all = m_core->components(PackageManagerCore::ComponentType::All);
        QJSValue scriptComponentsObject = m_engine->newArray(all.count());
        for (int i = 0; i < all.count(); ++i) {
            Component *const component = all.at(i);
            QQmlEngine::setObjectOwnership(component, QQmlEngine::CppOwnership);
            scriptComponentsObject.setProperty(i, m_engine->newQObject(component));
        }
        return scriptComponentsObject;
    }
    return m_engine->newArray();
}

QJSValue InstallerProxy::componentByName(const QString &componentName)
{
    if (m_core)
        return m_engine->newQObject(m_core->componentByName(componentName));
    return QJSValue();
}

QJSValue QDesktopServicesProxy::findFiles(const QString &path, const QString &pattern)
{
    QStringList result;
    findRecursion(path, pattern, &result);

    QJSValue scriptComponentsObject = m_engine->newArray(result.count());
    for (int i = 0; i < result.count(); ++i) {
        scriptComponentsObject.setProperty(i, result.at(i));
    }
    return scriptComponentsObject;
}

void QDesktopServicesProxy::findRecursion(const QString &path, const QString &pattern, QStringList *result)
{
    QDir currentDir(path);
    const QString prefix = path + QLatin1Char('/');
    foreach (const QString &match, currentDir.entryList(QStringList(pattern), QDir::Files | QDir::NoSymLinks))
        result->append(prefix + match);
    foreach (const QString &dir, currentDir.entryList(QDir::Dirs | QDir::NoSymLinks | QDir::NoDotAndDotDot))
        findRecursion(prefix + dir, pattern, result);
}

GuiProxy::GuiProxy(ScriptEngine *engine, QObject *parent) :
    QObject(parent),
    m_engine(engine),
    m_gui(nullptr)
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
    Returns the current wizard page.
*/
QJSValue GuiProxy::currentPageWidget() const
{
    if (!m_gui)
        return QJSValue();
    return m_engine->newQObject(m_gui->currentPageWidget());
}

QJSValue GuiProxy::pageWidgetByObjectName(const QString &name) const
{
    if (!m_gui)
        return QJSValue();
    return m_engine->newQObject(m_gui->pageWidgetByObjectName(name));
}

QString GuiProxy::defaultButtonText(int wizardButton) const
{
    if (!m_gui)
        return QString();
    return m_gui->defaultButtonText(wizardButton);
}

/*!
    Automatically clicks the button specified by \a wizardButton after a delay
    in milliseconds specified by \a delayInMs.
*/
void GuiProxy::clickButton(int wizardButton, int delayInMs)
{
    if (m_gui)
        m_gui->clickButton(wizardButton, delayInMs);
}

bool GuiProxy::isButtonEnabled(int wizardButton)
{
    if (!m_gui)
        return false;
    return m_gui->isButtonEnabled(wizardButton);
}

void GuiProxy::showSettingsButton(bool show)
{
    if (m_gui)
        m_gui->showSettingsButton(show);
}

void GuiProxy::setSettingsButtonEnabled(bool enable)
{
    if (m_gui)
        m_gui->setSettingsButtonEnabled(enable);
}

/*!
    Returns the first descendant of \a parent that has \a objectName as name.

    \sa QObject::findChild
*/
QJSValue GuiProxy::findChild(QObject *parent, const QString &objectName)
{
    return m_engine->newQObject(parent->findChild<QObject*>(objectName));
}

/*!
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
    Hides the GUI when \a silent is \c true.
*/
void GuiProxy::setSilent(bool silent)
{
  if (m_gui)
      m_gui->setSilent(silent);
}

void GuiProxy::setTextItems(QObject *object, const QStringList &items)
{
    if (m_gui)
        m_gui->setTextItems(object, items);
}

void GuiProxy::cancelButtonClicked()
{
    if (m_gui)
        m_gui->cancelButtonClicked();
}

void GuiProxy::reject()
{
    if (m_gui)
        m_gui->reject();
}

void GuiProxy::rejectWithoutPrompt()
{
    if (m_gui)
        m_gui->rejectWithoutPrompt();
}

void GuiProxy::showFinishedPage()
{
    if (m_gui)
        m_gui->showFinishedPage();
}

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
        connect(core, &PackageManagerCore::guiObjectChanged, this, &ScriptEngine::setGuiQObject);
    } else {
        global.setProperty(QLatin1String("installer"), m_engine.newQObject(new QObject));
    }
    global.setProperty(QLatin1String("gui"), m_engine.newQObject(m_guiProxy));

    global.property(QLatin1String("installer")).setProperty(QLatin1String("components"),
        proxy.property(QLatin1String("components")));
    global.property(QLatin1String("installer")).setProperty(QLatin1String("componentByName"),
        proxy.property(QLatin1String("componentByName")));
}

/*!
    Creates a JavaScript object that wraps the given QObject \a object.

    Signals and slots, properties and children of \a object are
    available as properties of the created QJSValue. In addition some helper methods and properties
    are added:

    \list
        \li findChild(), findChildren() recursively search for child objects with the given
            object name.
        \li Direct child objects are made accessible as properties under their respective object
        names.
    \endlist
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

/*!
    Creates a JavaScript object of class Array with the specified \a length.
*/

QJSValue ScriptEngine::newArray(uint length)
{
  return m_engine.newArray(length);
}

/*!
    Evaluates \a program, using \a lineNumber as the base line number, and returns the results of
    the evaluation. \a fileName is used for error reporting.
*/
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

    TODO: document \a scriptInjection.
*/
QJSValue ScriptEngine::loadInContext(const QString &context, const QString &fileName,
    const QString &scriptInjection)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        throw Error(tr("Cannot open script file at %1: %2")
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
    QString copiedFileName = fileName;
#ifdef Q_OS_WIN
    // Workaround bug reported in QTBUG-70425 by appending "file://" when passing a filename to
    // QJSEngine::evaluate() to ensure it sees it as a valid URL when qsTr() is used.
    if (!copiedFileName.startsWith(QLatin1String("qrc:/")) &&
        !copiedFileName.startsWith(QLatin1String(":/"))) {
        copiedFileName = QLatin1String("file://") + fileName;
    }
#endif
    QJSValue scriptContext = evaluate(scriptContent, copiedFileName);
    scriptContext.setProperty(QLatin1String("Uuid"), QUuid::createUuid().toString());
    if (scriptContext.isError()) {
        throw Error(tr("Exception while loading the component script \"%1\": %2").arg(
                        QDir::toNativeSeparators(QFileInfo(file).absoluteFilePath()),
                        scriptContext.toString().isEmpty() ? tr("Unknown error.") : scriptContext.toString() +
                        QStringLiteral(" ") + tr("on line number: ") +
                        scriptContext.property(QStringLiteral("lineNumber")).toString()));
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

    QJSValue object = m_engine.newQObject(new QDesktopServicesProxy(this));
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
