/**************************************************************************
**
** Copyright (C) 2024 The Qt Company Ltd.
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
#include "loggingutils.h"
#include "packagemanagergui.h"
#include "component.h"
#include "settings.h"

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
    \fn QInstaller::ScriptEngine::globalObject() const
    Returns a global object.
*/

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::QDesktopServicesProxy
    \internal
*/

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::QFileDialogProxy
    \internal
*/

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

/*!
    Automatically clicks the button specified by \a objectName after a delay
    in milliseconds specified by \a delayInMs.
*/
void GuiProxy::clickButton(const QString &objectName, int delayInMs) const
{
    if (m_gui)
        m_gui->clickButton(objectName, delayInMs);
}

bool GuiProxy::isButtonEnabled(int wizardButton)
{
    if (!m_gui)
        return false;
    return m_gui->isButtonEnabled(wizardButton);
}

void GuiProxy::setWizardPageButtonText(int pageId, int buttonId, const QString &buttonText)
{
    if (m_gui)
        m_gui->setWizardPageButtonText(pageId, buttonId, buttonText);
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

QFileDialogProxy::QFileDialogProxy(PackageManagerCore *core): m_core(core)
{
}

QString QFileDialogProxy::getExistingDirectory(const QString &caption,
                                               const QString &dir, const QString &identifier)
{
    if (m_core->isCommandLineInstance()) {
        return getExistingFileOrDirectory(caption, identifier, true);
    } else {
        return QFileDialog::getExistingDirectory(0, caption, dir);
    }
}

QString QFileDialogProxy::getOpenFileName(const QString &caption, const QString &dir,
                                          const QString &filter, const QString &identifier)
{
    if (m_core->isCommandLineInstance()) {
        return getExistingFileOrDirectory(caption, identifier, false);
    } else {
        return QFileDialog::getOpenFileName(0, caption, dir, filter);
    }
}

QString QFileDialogProxy::getExistingFileOrDirectory(const QString &caption,
                            const QString &identifier, bool isDirectory)
{
    QHash<QString, QString> autoAnswers = m_core->fileDialogAutomaticAnswers();
    QString selectedDirectoryOrFile;
    QString errorString;
    if (autoAnswers.contains(identifier)) {
        selectedDirectoryOrFile = autoAnswers.value(identifier);
        QFileInfo fileInfo(selectedDirectoryOrFile);
        if (isDirectory ? fileInfo.isDir() : fileInfo.isFile()) {
            qCDebug(QInstaller::lcInstallerInstallLog).nospace() << "Automatic answer for "<< identifier
                << ": " << selectedDirectoryOrFile;
        } else {
            if (isDirectory)
                errorString = QString::fromLatin1("Automatic answer for %1: Directory '%2' not found.")
                        .arg(identifier, selectedDirectoryOrFile);
            else
                errorString = QString::fromLatin1("Automatic answer for %1: File '%2' not found.")
                        .arg(identifier, selectedDirectoryOrFile);
            selectedDirectoryOrFile = QString();
        }
    } else if (!LoggingHandler::instance().outputRedirected()) {
        qDebug().nospace().noquote() << identifier << ": " << caption << ": ";
        QTextStream stream(stdin);
        stream.readLineInto(&selectedDirectoryOrFile);
        QFileInfo fileInfo(selectedDirectoryOrFile);
        if (isDirectory ? !fileInfo.isDir() : !fileInfo.isFile()) {
            if (isDirectory)
                errorString = QString::fromLatin1("Directory '%1' not found.")
                        .arg(selectedDirectoryOrFile);
            else
                errorString = QString::fromLatin1("File '%1' not found.")
                        .arg(selectedDirectoryOrFile);
            selectedDirectoryOrFile = QString();
        }
    } else {
        qCDebug(QInstaller::lcInstallerInstallLog).nospace()
            << "No answer available for "<< identifier << ": " << caption;

        throw Error(tr("User input is required but the output "
            "device is not associated with a terminal."));
    }
    if (!errorString.isEmpty())
        qCWarning(QInstaller::lcInstallerInstallLog).nospace() << errorString;
    return selectedDirectoryOrFile;
}


/*!
    Constructs a script engine with \a core as parent.
*/
ScriptEngine::ScriptEngine(PackageManagerCore *core) : QObject(core)
    , m_guiProxy(new GuiProxy(this, this))
    , m_core(core)
{
    m_engine.installExtensions(QJSEngine::TranslationExtension | QJSEngine::ConsoleExtension);
    QJSValue global = m_engine.globalObject();

    global.setProperty(QLatin1String("QFileDialog"), m_engine.newQObject(new QFileDialogProxy(core)));
    global.setProperty(QLatin1String("systemInfo"), m_engine.newQObject(new SystemInfo));

    global.setProperty(QLatin1String("QInstaller"), generateQInstallerObject());
    global.setProperty(QLatin1String("buttons"), generateWizardButtonsObject());
    global.setProperty(QLatin1String("QMessageBox"), generateMessageBoxObject());
    global.setProperty(QLatin1String("QDesktopServices"), generateDesktopServicesObject());
#ifdef Q_OS_WIN
    global.setProperty(QLatin1String("QSettings"), generateSettingsObject());
#endif

    if (core) {
        setGuiQObject(core->guiObject());
        QQmlEngine::setObjectOwnership(core, QQmlEngine::CppOwnership);
        global.setProperty(QLatin1String("installer"), m_engine.newQObject(core));
        connect(core, &PackageManagerCore::guiObjectChanged, this, &ScriptEngine::setGuiQObject);
    } else {
        global.setProperty(QLatin1String("installer"), m_engine.newQObject(new QObject));
    }
    global.setProperty(QLatin1String("gui"), m_engine.newQObject(m_guiProxy));
}

/*!
    Creates a JavaScript object that wraps the given QObject \a object.

    Signals and slots, properties and children of \a object are available as properties
    of the created QJSValue. If \a qtScriptCompat is set to \c true (default), some helper
    methods and properties from the legacy \c QtScript module are added:

    \list
        \li findChild(), findChildren() recursively search for child objects with the given
            object name.
        \li Direct child objects are made accessible as properties under their respective object
        names.
    \endlist
 */
QJSValue ScriptEngine::newQObject(QObject *object, bool qtScriptCompat)
{
    QJSValue jsValue = m_engine.newQObject(object);
    if (!jsValue.isQObject())
        return jsValue;

    QQmlEngine::setObjectOwnership(object, QQmlEngine::CppOwnership);

    if (!qtScriptCompat) // skip adding the extra properties
        return jsValue;

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
        + QString::fromLatin1("\n"
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
        QString errorString = method.toString().isEmpty() ? QString::fromLatin1("Unknown error.")
            : method.toString();

        throw Error(QString::fromLatin1("%1 \n%2 \"%3\"").arg(errorString, tr(scClearCacheHint), m_core->settings().localCachePath()));
    }

    const QJSValue result = method.call(arguments);
    if (result.isError()) {
        QString errorString = result.toString().isEmpty() ? QString::fromLatin1("Unknown error.")
            : result.toString();
        throw Error(QString::fromLatin1("%1 \n%2 \"%3\"").arg(errorString, tr(scClearCacheHint), m_core->settings().localCachePath()));
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
    SETPROPERTY(desktopServices, AppLocalDataLocation, QStandardPaths)
    SETPROPERTY(desktopServices, CacheLocation, QStandardPaths)
    SETPROPERTY(desktopServices, GenericCacheLocation, QStandardPaths)
    SETPROPERTY(desktopServices, GenericDataLocation, QStandardPaths)
    SETPROPERTY(desktopServices, RuntimeLocation, QStandardPaths)
    SETPROPERTY(desktopServices, ConfigLocation, QStandardPaths)
    SETPROPERTY(desktopServices, DownloadLocation, QStandardPaths)
    SETPROPERTY(desktopServices, GenericCacheLocation, QStandardPaths)
    SETPROPERTY(desktopServices, GenericConfigLocation, QStandardPaths)
    SETPROPERTY(desktopServices, AppDataLocation, QStandardPaths)
    SETPROPERTY(desktopServices, AppConfigLocation, QStandardPaths)
    SETPROPERTY(desktopServices, PublicShareLocation, QStandardPaths)
    SETPROPERTY(desktopServices, TemplatesLocation, QStandardPaths)

    QJSValue object = m_engine.newQObject(new QDesktopServicesProxy(this));
    object.setPrototype(desktopServices);   // attach the properties
    return object;
}

#ifdef Q_OS_WIN
QJSValue ScriptEngine::generateSettingsObject()
{
    QJSValue settingsObject = m_engine.newArray();
    SETPROPERTY(settingsObject, NativeFormat, QSettings)
    SETPROPERTY(settingsObject, IniFormat, QSettings)
    SETPROPERTY(settingsObject, Registry32Format, QSettings)
    SETPROPERTY(settingsObject, Registry64Format, QSettings)
    SETPROPERTY(settingsObject, InvalidFormat, QSettings)
    return settingsObject;
}
#endif

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
