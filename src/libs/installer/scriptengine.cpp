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
#include "scriptengine.h"

#include "component.h"
#include "packagemanagercore.h"
#include "messageboxhandler.h"
#include "errors.h"

#include <QDesktopServices>
#include <QFileDialog>
#include <QMetaEnum>
#include <QWizard>

using namespace QInstaller;

namespace QInstaller {

QString uncaughtExceptionString(const QScriptEngine *scriptEngine, const QString &context)
{
    QString error(QLatin1String("\n\n%1\n\nBacktrace:\n\t%2"));
    if (!context.isEmpty())
        error.prepend(context);

    return error.arg(scriptEngine->uncaughtException().toString(), scriptEngine->uncaughtExceptionBacktrace()
        .join(QLatin1String("\n\t")));
}

/*!
    Scriptable version of PackageManagerCore::componentByName(QString).
    \sa PackageManagerCore::componentByName
 */
QScriptValue qInstallerComponentByName(QScriptContext *context, QScriptEngine *engine)
{
    const QScriptValue check = checkArguments(context, 1, 1);
    if (check.isError())
        return check;

    // well... this is our "this" pointer
    PackageManagerCore *const core = qobject_cast<PackageManagerCore*>(engine->globalObject()
        .property(QLatin1String("installer")).toQObject());

    const QString name = context->argument(0).toString();
    return engine->newQObject(core->componentByName(name));
}

QScriptValue checkArguments(QScriptContext *context, int minimalArgumentCount, int maximalArgumentCount)
{
    if (context->argumentCount() < minimalArgumentCount || context->argumentCount() > maximalArgumentCount) {
        if (minimalArgumentCount != maximalArgumentCount) {
            return context->throwError(QObject::tr("Invalid arguments: %1 arguments given, %2 to "
                "%3 expected.").arg(QString::number(context->argumentCount()),
                QString::number(minimalArgumentCount), QString::number(maximalArgumentCount)));
        }
        return context->throwError(QObject::tr("Invalid arguments: %1 arguments given, %2 expected.")
            .arg(QString::number(context->argumentCount()), QString::number(minimalArgumentCount)));
    }
    return QScriptValue();
}

QScriptValue qDesktopServicesOpenUrl(QScriptContext *context, QScriptEngine *engine)
{
    Q_UNUSED(engine);
    const QScriptValue check = checkArguments(context, 1, 1);
    if (check.isError())
        return check;
    QString url = context->argument(0).toString();
    url.replace(QLatin1String("\\\\"), QLatin1String("/"));
    url.replace(QLatin1String("\\"), QLatin1String("/"));
    return QDesktopServices::openUrl(QUrl::fromUserInput(url));
}

QScriptValue qDesktopServicesDisplayName(QScriptContext *context, QScriptEngine *engine)
{
    Q_UNUSED(engine);
    const QScriptValue check = checkArguments(context, 1, 1);
    if (check.isError())
        return check;

#if QT_VERSION < 0x050000
    const QDesktopServices::StandardLocation location =
        static_cast< QDesktopServices::StandardLocation >(context->argument(0).toInt32());
    return QDesktopServices::displayName(location);
#else
    const QStandardPaths::StandardLocation location =
        static_cast< QStandardPaths::StandardLocation >(context->argument(0).toInt32());
    return QStandardPaths::displayName(location);
#endif
}

QScriptValue qDesktopServicesStorageLocation(QScriptContext *context, QScriptEngine *engine)
{
    Q_UNUSED(engine);
    const QScriptValue check = checkArguments(context, 1, 1);
    if (check.isError())
        return check;

#if QT_VERSION < 0x050000
    const QDesktopServices::StandardLocation location =
        static_cast< QDesktopServices::StandardLocation >(context->argument(0).toInt32());
    return QDesktopServices::storageLocation(location);
#else
    const QStandardPaths::StandardLocation location =
        static_cast< QStandardPaths::StandardLocation >(context->argument(0).toInt32());
    return QStandardPaths::writableLocation(location);
#endif
}

QScriptValue qFileDialogGetExistingDirectory(QScriptContext *context, QScriptEngine *engine)
{
    Q_UNUSED(engine);
    const QScriptValue check = checkArguments(context, 0, 2);
    if (check.isError())
        return check;
    QString caption;
    QString dir;
    if (context->argumentCount() > 0)
        caption = context->argument(0).toString();
    if (context->argumentCount() > 1)
        dir = context->argument(1).toString();
    return QFileDialog::getExistingDirectory(0, caption, dir);
}

QScriptValue qFileDialogGetOpenFileName(QScriptContext *context, QScriptEngine *engine)
{
    Q_UNUSED(engine);
    const QScriptValue check = checkArguments(context, 0, 3);
    if (check.isError())
        return check;
    QString caption;
    QString dir;
    QString fileNameFilter;
    if (context->argumentCount() > 0)
        caption = context->argument(0).toString();
    if (context->argumentCount() > 1)
        dir = context->argument(1).toString();
    if (context->argumentCount() > 2)
        fileNameFilter = context->argument(2).toString();
    return QFileDialog::getOpenFileName(0, caption, dir, fileNameFilter);
}

} //namespace QInstaller


/*!
    \class QInstaller::ScriptEngine
    prepare and run the component scripts
*/
ScriptEngine::ScriptEngine(PackageManagerCore *core)
    : QScriptEngine(core)
    , m_core(core)
{
    // register translation stuff
    installTranslatorFunctions();

    globalObject().setProperty(QLatin1String("QMessageBox"), generateMessageBoxObject());
    globalObject().setProperty(QLatin1String("buttons"), generateWizardButtonsObject());
    globalObject().setProperty(QLatin1String("QDesktopServices"), generateDesktopServicesObject());
    globalObject().setProperty(QLatin1String("QInstaller"), generateQInstallerObject());

    QScriptValue installerObject = newQObject(m_core);
    installerObject.setProperty(QLatin1String("componentByName"), newFunction(qInstallerComponentByName, 1));

    globalObject().setProperty(QLatin1String("installer"), installerObject);

    QScriptValue fileDialog = newArray();
    fileDialog.setProperty(QLatin1String("getExistingDirectory"),
        newFunction(qFileDialogGetExistingDirectory));
    fileDialog.setProperty(QLatin1String("getOpenFileName"),
        newFunction(qFileDialogGetOpenFileName));
    globalObject().setProperty(QLatin1String("QFileDialog"), fileDialog);

    const QList<Component*> components = m_core->availableComponents();
    QScriptValue scriptComponentsObject = newArray(components.count());
    for (int i = 0; i < components.count(); ++i)
        scriptComponentsObject.setProperty(i, newQObject(components[i]));

    globalObject().property(QLatin1String("installer"))
        .setProperty(QLatin1String("components"), scriptComponentsObject);

    connect(this, SIGNAL(signalHandlerException(QScriptValue)), SLOT(handleException(QScriptValue)));

    connect(core, SIGNAL(guiObjectChanged(QObject*)), this, SLOT(setGuiQObject(QObject*)));
    setGuiQObject(core->guiObject());
}

ScriptEngine::~ScriptEngine()
{
}

void ScriptEngine::setGuiQObject(QObject *guiQObject)
{
    globalObject().setProperty(QLatin1String("gui"), newQObject(guiQObject));
}

/*!
    Loads a script into the given \a context at \a fileName inside the ScriptEngine.

    The installer and all its components as well as other useful stuff are being exported into the script.
    Read \link componentscripting Component Scripting \endlink for details.
    \throws Error when either the script at \a fileName couldn't be opened, or the QScriptEngine
    couldn't evaluate the script.
*/
QScriptValue ScriptEngine::loadInConext(const QString &context, const QString &fileName,
    const QString &scriptInjection)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        throw Error(tr("Could not open the requested script file at %1: %2.").arg(
            fileName, file.errorString()));
    }

    // create inside closure in one line to keep linenumber in the right order
    // which is used as a debug output in an exception case
    // scriptContent will be added as the last arg command to prevent wrong
    // replacements of %1, %2 or %3 inside the scriptContent
    QString scriptContent = QString::fromLatin1(
        "(function() { %1 %3 ; return new %2; })();");

    // merging everything together and as last we are adding the file content to prevent wrong
    // replacements of %1 %2 or %3 inside the javascript code
    scriptContent = scriptContent.arg(scriptInjection, context, QLatin1String(file.readAll()));
    QScriptValue scriptContext = evaluate(scriptContent, fileName);

    if (hasUncaughtException()) {
        throw Error(tr("Exception while loading the component script: '%1'").arg(
            uncaughtExceptionString(this, QFileInfo(file).absoluteFilePath())));
    }
    if (!scriptContext.isValid()) {
        throw Error(tr("Could not load the component script inside a script context: '%1'").arg(
            QFileInfo(file).absoluteFilePath()));
    }
    return scriptContext;
}

void ScriptEngine::handleException(const QScriptValue &value)
{
    if (!value.engine())
        return;
    throw Error(uncaughtExceptionString(this, tr("Fatal error while evaluating a script.")));
}

/*!
    Tries to call the method with \a name within the script and returns the result. If the method
    doesn't exist, an invalid result is returned. If the method has an uncaught exception, its
    string representation is thrown as an Error exception.

    \note The method is not called, if the current script context is the same method, to avoid
    infinite recursion.
*/
QScriptValue ScriptEngine::callScriptMethod(const QScriptValue &scriptContext,
    const QString &methodName, const QScriptValueList &arguments) const
{
    // don't allow such a recursion
    if (currentContext()->backtrace().first().startsWith(methodName))
        return QScriptValue();

    QScriptValue method = scriptContext.property(methodName);
    // this marks the method to be called not any longer
    if (!method.isValid())
        return QScriptValue();

    const QScriptValue result = method.call(scriptContext, arguments);
    if (!result.isValid())
        return result;

    if (hasUncaughtException())
        throw Error(uncaughtExceptionString(this));

    return result;
}


QScriptValue ScriptEngine::generateWizardButtonsObject()
{
#undef REGISTER_BUTTON
#define REGISTER_BUTTON(x)    buttons.setProperty(QLatin1String(#x), \
    newVariant(static_cast<int>(QWizard::x)));

    QScriptValue buttons = newArray();
    REGISTER_BUTTON(BackButton)
    REGISTER_BUTTON(NextButton)
    REGISTER_BUTTON(CommitButton)
    REGISTER_BUTTON(FinishButton)
    REGISTER_BUTTON(CancelButton)
    REGISTER_BUTTON(HelpButton)
    REGISTER_BUTTON(CustomButton1)
    REGISTER_BUTTON(CustomButton2)
    REGISTER_BUTTON(CustomButton3)

#undef REGISTER_BUTTON
    return buttons;
}

/*!
    generates QMessageBox::StandardButton enum as an QScriptValue array
*/
QScriptValue ScriptEngine::generateMessageBoxObject()
{
    // register QMessageBox::StandardButton enum in the script connection
    QScriptValue messageBox = newQObject(MessageBoxHandler::instance());

    const QMetaObject &messageBoxMetaObject = QMessageBox::staticMetaObject;
    int index = messageBoxMetaObject.indexOfEnumerator("StandardButtons");

    QMetaEnum metaEnum = messageBoxMetaObject.enumerator(index);
    for (int i = 0; i < metaEnum.keyCount(); i++) {
        int enumValue = metaEnum.value(i);
        if (enumValue < QMessageBox::FirstButton)
            continue;
        messageBox.setProperty(QLatin1String(metaEnum.valueToKey(metaEnum.value(i))), newVariant(enumValue));
        if (enumValue == QMessageBox::LastButton)
            break;
    }

    return messageBox;
}

QScriptValue ScriptEngine::generateDesktopServicesObject()
{
    QScriptValue desktopServices = newArray();
#if QT_VERSION < 0x050000
    desktopServices.setProperty(QLatin1String("DesktopLocation"), QDesktopServices::DesktopLocation);
    desktopServices.setProperty(QLatin1String("DesktopLocation"), QDesktopServices::DesktopLocation);
    desktopServices.setProperty(QLatin1String("DocumentsLocation"), QDesktopServices::DocumentsLocation);
    desktopServices.setProperty(QLatin1String("FontsLocation"), QDesktopServices::FontsLocation);
    desktopServices.setProperty(QLatin1String("ApplicationsLocation"), QDesktopServices::ApplicationsLocation);
    desktopServices.setProperty(QLatin1String("MusicLocation"), QDesktopServices::MusicLocation);
    desktopServices.setProperty(QLatin1String("MoviesLocation"), QDesktopServices::MoviesLocation);
    desktopServices.setProperty(QLatin1String("PicturesLocation"), QDesktopServices::PicturesLocation);
    desktopServices.setProperty(QLatin1String("TempLocation"), QDesktopServices::TempLocation);
    desktopServices.setProperty(QLatin1String("HomeLocation"), QDesktopServices::HomeLocation);
    desktopServices.setProperty(QLatin1String("DataLocation"), QDesktopServices::DataLocation);
    desktopServices.setProperty(QLatin1String("CacheLocation"), QDesktopServices::CacheLocation);
#else
    desktopServices.setProperty(QLatin1String("DesktopLocation"), QStandardPaths::DesktopLocation);
    desktopServices.setProperty(QLatin1String("DesktopLocation"), QStandardPaths::DesktopLocation);
    desktopServices.setProperty(QLatin1String("DocumentsLocation"), QStandardPaths::DocumentsLocation);
    desktopServices.setProperty(QLatin1String("FontsLocation"), QStandardPaths::FontsLocation);
    desktopServices.setProperty(QLatin1String("ApplicationsLocation"), QStandardPaths::ApplicationsLocation);
    desktopServices.setProperty(QLatin1String("MusicLocation"), QStandardPaths::MusicLocation);
    desktopServices.setProperty(QLatin1String("MoviesLocation"), QStandardPaths::MoviesLocation);
    desktopServices.setProperty(QLatin1String("PicturesLocation"), QStandardPaths::PicturesLocation);
    desktopServices.setProperty(QLatin1String("TempLocation"), QStandardPaths::TempLocation);
    desktopServices.setProperty(QLatin1String("HomeLocation"), QStandardPaths::HomeLocation);
    desktopServices.setProperty(QLatin1String("DataLocation"), QStandardPaths::DataLocation);
    desktopServices.setProperty(QLatin1String("CacheLocation"), QStandardPaths::CacheLocation);
#endif

    desktopServices.setProperty(QLatin1String("openUrl"),
        newFunction(qDesktopServicesOpenUrl));
    desktopServices.setProperty(QLatin1String("displayName"),
        newFunction(qDesktopServicesDisplayName));
    desktopServices.setProperty(QLatin1String("storageLocation"),
        newFunction(qDesktopServicesStorageLocation));
    return desktopServices;
}

QScriptValue ScriptEngine::generateQInstallerObject()
{
    // register ::WizardPage enum in the script connection
    QScriptValue qinstaller = newArray();
    qinstaller.setProperty(QLatin1String("Introduction"), PackageManagerCore::Introduction);
    qinstaller.setProperty(QLatin1String("LicenseCheck"), PackageManagerCore::LicenseCheck);
    qinstaller.setProperty(QLatin1String("TargetDirectory"), PackageManagerCore::TargetDirectory);
    qinstaller.setProperty(QLatin1String("ComponentSelection"), PackageManagerCore::ComponentSelection);
    qinstaller.setProperty(QLatin1String("StartMenuSelection"), PackageManagerCore::StartMenuSelection);
    qinstaller.setProperty(QLatin1String("ReadyForInstallation"), PackageManagerCore::ReadyForInstallation);
    qinstaller.setProperty(QLatin1String("PerformInstallation"), PackageManagerCore::PerformInstallation);
    qinstaller.setProperty(QLatin1String("InstallationFinished"), PackageManagerCore::InstallationFinished);
    qinstaller.setProperty(QLatin1String("End"), PackageManagerCore::End);

    // register ::Status enum in the script connection
    qinstaller.setProperty(QLatin1String("Success"), PackageManagerCore::Success);
    qinstaller.setProperty(QLatin1String("Failure"), PackageManagerCore::Failure);
    qinstaller.setProperty(QLatin1String("Running"), PackageManagerCore::Running);
    qinstaller.setProperty(QLatin1String("Canceled"), PackageManagerCore::Canceled);
    qinstaller.setProperty(QLatin1String("Unfinished"), PackageManagerCore::Unfinished);
    qinstaller.setProperty(QLatin1String("ForceUpdate"), PackageManagerCore::ForceUpdate);

    return qinstaller;
}

