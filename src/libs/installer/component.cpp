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
#include "component.h"
#include "scriptengine.h"

#include "errors.h"
#include "fileutils.h"
#include "globals.h"
#include "lib7z_facade.h"
#include "messageboxhandler.h"
#include "packagemanagercore.h"
#include "remoteclient.h"
#include "settings.h"

#include <kdupdaterupdatesourcesinfo.h>
#include <kdupdaterupdateoperationfactory.h>

#include <productkeycheck.h>

#include <QtCore/QDirIterator>
#include <QtCore/QTranslator>

#include <QApplication>

#include <QtUiTools/QUiLoader>

#include <algorithm>

using namespace QInstaller;

static const QLatin1String scScriptTag("Script");
static const QLatin1String scDefault("Default");
static const QLatin1String scAutoDependOn("AutoDependOn");
static const QLatin1String scVirtual("Virtual");
static const QLatin1String scInstalled("Installed");
static const QLatin1String scUpdateText("UpdateText");
static const QLatin1String scUninstalled("Uninstalled");
static const QLatin1String scCurrentState("CurrentState");
static const QLatin1String scForcedInstallation("ForcedInstallation");

/*!
    \qmltype component
    \inqmlmodule scripting

    \brief The component type represents the current component that the Qt Script belongs to.

    A minimal valid script needs to contain a constructor, which can look like this:

    \code
    function Component()
    {
        // Access the global component's name property and log it to the debug console.
        console.log("component: " + component.displayName);
    }
    \endcode

    The \c Component class and the script engine both modify the script before it is evaluated.
    When the modifications are applied to the above snippet, the script changes to:

    \code
    (function() { [1]
        var component = installer.componentByName('Component.Name.This.Script.Belongs.To'); [2]
        function Component()
        {
            // Access the global component's name property and log it to the debug console.
            console.log("component: " + component.displayName);
        }

        if (typeof Component != undefined) [1]
            return new Component; [1]
        else [1]
            throw "Missing Component constructor. Please check your script." [1]
    })();

    [1] Changes done by the script engine.
    [2] Changes done by the Component class.
    \endcode

    \note The \e component (in lower case) is the global variable the C++ \c Component class
    introduced. The \e component variable represents the C++ \c Component object that the script
    belongs to. The \e Component (in upper case) is a JavaScript object that gets instantiated by
    the script engine.
*/

/*!
    \qmlproperty string component::name

    Returns the name of the component as set in the \c <Name> tag of the package
    information file.
*/

/*!
    \qmlproperty string component::displayName

    Returns the name of the component as shown in the user interface.
*/

/*!
    \qmlproperty boolean component::selected

    Indicates whether the component is currently selected.
*/

/*!
    \qmlproperty boolean component::autoCreateOperations

    Specifies whether some standard operations for the component should be
    automatically created when the installation starts. The default is \c true.
*/

/*!
    \qmlproperty stringlist component::archives

    Returns the list of archive URL's (prefixed with \c installer://) registered
    for the component.

    \sa addDownloadableArchive, removeDownloadableArchive
*/

/*!
    \qmlproperty stringlist component::dependencies

    This read-only property contains components this component depends on.
*/

/*!
    \qmlproperty stringlist component::autoDependencies

    Returns the value of the \c <AutoDependsOn> tag in the package information file.
*/

/*!
    \qmlproperty boolean component::fromOnlineRepository

    Returns whether this component has been loaded from an online repository.

    \sa isFromOnlineRepository
*/

/*!
    \qmlproperty url component::repositoryUrl

    Returns the repository URL the component is downloaded from.
    When this component is not downloaded from an online repository, returns an empty #QUrl.

*/

/*!
    \qmlproperty boolean component::default

    This read-only property indicates if the component is a default one.

    \note Always \c false for virtual components.

    \sa isDefault
*/

/*!
    \qmlproperty boolean component::installed

    This read-only property returns if the component is installed.

    \sa isInstalled
*/

/*!
    \qmlproperty boolean component::enabled

    Indicates whether the component is currently enabled. The property is both readable and writable.
*/

/*!
    \qmlsignal component::loaded()

    Emitted when the component has been loaded.
*/

/*!
    \qmlsignal component::selectedChanged(boolean isSelected)

    Emitted when the component selection has changed to \a isSelected.
*/

/*!
    \qmlsignal component::valueChanged(string key, string value)

    Emitted when the variable with name \a key has changed to \a value.

    \sa setValue
*/


/*!
    Constructor. Creates a new Component inside of \a installer.
*/
Component::Component(PackageManagerCore *core)
    : d(new ComponentPrivate(core, this))
{
    setPrivate(d);

    connect(this, SIGNAL(valueChanged(QString, QString)), this, SLOT(updateModelData(QString, QString)));
    qRegisterMetaType<QList<QInstaller::Component*> >("QList<QInstaller::Component*>");
}

/*!
    Destroys the Component.
*/
Component::~Component()
{
    if (parentComponent() != 0)
        d->m_parentComponent->d->m_allChildComponents.removeAll(this);

    //why can we delete all create operations if the component gets destroyed
    if (!d->m_newlyInstalled)
        qDeleteAll(d->m_operations);

    //we need to copy the list, because we are changing it with removeAll at top
    //(this made the iterators broken in the past)
    QList<Component*> copiedChildrenList = d->m_allChildComponents;
    copiedChildrenList.detach(); //this makes it a real copy

    qDeleteAll(copiedChildrenList);
    delete d;
    d = 0;
}

void Component::loadDataFromPackage(const LocalPackage &package)
{
    setValue(scName, package.name);
    // pixmap ???
    setValue(scDisplayName, package.title);
    setValue(scDescription, package.description);
    setValue(scVersion, package.version);
    setValue(scInheritVersion, package.inheritVersionFrom);
    setValue(scInstalledVersion, package.version);
    setValue(QLatin1String("LastUpdateDate"), package.lastUpdateDate.toString());
    setValue(QLatin1String("InstallDate"), package.installDate.toString());
    setValue(scUncompressedSize, QString::number(package.uncompressedSize));

    QString dependstr;
    foreach (const QString &val, package.dependencies)
        dependstr += val + QLatin1String(",");

    if (package.dependencies.count() > 0)
        dependstr.chop(1);
    setValue(scDependencies, dependstr);

    setValue(scForcedInstallation, package.forcedInstallation ? scTrue : scFalse);
    if (package.forcedInstallation & !PackageManagerCore::noForceInstallation()) {
        setEnabled(false);
        setCheckable(false);
        setCheckState(Qt::Checked);
    }
    setValue(scVirtual, package.virtualComp ? scTrue : scFalse);
    setValue(scCurrentState, scInstalled);
}

void Component::loadDataFromPackage(const Package &package)
{
    Q_ASSERT(&package);

    setValue(scName, package.data(scName).toString());
    setValue(scDisplayName, package.data(scDisplayName).toString());
    setValue(scDescription, package.data(scDescription).toString());
    setValue(scDefault, package.data(scDefault).toString());
    setValue(scAutoDependOn, package.data(scAutoDependOn).toString());
    setValue(scCompressedSize, package.data(scCompressedSize).toString());
    setValue(scUncompressedSize, package.data(scUncompressedSize).toString());
    setValue(scRemoteVersion, package.data(scRemoteVersion).toString());
    setValue(scInheritVersion, package.data(scInheritVersion).toString());
    setValue(scDependencies, package.data(scDependencies).toString());
    setValue(scDownloadableArchives, package.data(scDownloadableArchives).toString());
    setValue(scVirtual, package.data(scVirtual).toString());
    setValue(scSortingPriority, package.data(scSortingPriority).toString());

    setValue(scEssential, package.data(scEssential).toString());
    setValue(scUpdateText, package.data(scUpdateText).toString());
    setValue(scNewComponent, package.data(scNewComponent).toString());
    setValue(scRequiresAdminRights, package.data(scRequiresAdminRights).toString());

    setValue(scScriptTag, package.data(scScriptTag).toString());
    setValue(scReplaces, package.data(scReplaces).toString());
    setValue(scReleaseDate, package.data(scReleaseDate).toString());

    QString forced = package.data(scForcedInstallation, scFalse).toString().toLower();
    if (PackageManagerCore::noForceInstallation())
        forced = scFalse;
    setValue(scForcedInstallation, forced);
    if (forced == scTrue) {
        setEnabled(false);
        setCheckable(false);
        setCheckState(Qt::Checked);
    }

    setLocalTempPath(QInstaller::pathFromUrl(package.sourceInfoUrl()));
    const QStringList uis = package.data(QLatin1String("UserInterfaces")).toString()
        .split(QInstaller::commaRegExp(), QString::SkipEmptyParts);
    if (!uis.isEmpty())
        loadUserInterfaces(QDir(QString::fromLatin1("%1/%2").arg(localTempPath(), name())), uis);

    const QStringList qms = package.data(QLatin1String("Translations")).toString()
        .split(QInstaller::commaRegExp(), QString::SkipEmptyParts);
    if (!qms.isEmpty())
        loadTranslations(QDir(QString::fromLatin1("%1/%2").arg(localTempPath(), name())), qms);

    QHash<QString, QVariant> licenseHash = package.data(QLatin1String("Licenses")).toHash();
    if (!licenseHash.isEmpty())
        loadLicenses(QString::fromLatin1("%1/%2/").arg(localTempPath(), name()), licenseHash);
}

quint64 Component::updateUncompressedSize()
{
    quint64 size = 0;

    if (installationRequested() || updateRequested())
        size = d->m_vars.value(scUncompressedSize).toLongLong();

    foreach (Component* comp, d->m_allChildComponents)
        size += comp->updateUncompressedSize();

    setValue(scUncompressedSizeSum, QString::number(size));
    setData(humanReadableSize(size), UncompressedSize);

    return size;
}

void Component::markAsPerformedInstallation()
{
    d->m_newlyInstalled = true;
}

/*
    Returns a key/value based hash of all variables set for this component.
*/
QHash<QString,QString> Component::variables() const
{
    return d->m_vars;
}

/*!
    \qmlmethod string component::value(string key, string value = "")

    Returns the value of variable name \a key. If \a key is not known yet, \a defaultValue is returned.
    Note: If a component is virtual and you ask for the component value with key "Default", it will always
    return \c false.
*/
QString Component::value(const QString &key, const QString &defaultValue) const
{
    if (key == scDefault)
        return isDefault() ? scTrue : scFalse;
    return d->m_vars.value(key, defaultValue);
}

/*!
    \qmlmethod void component::setValue(string key, string value)

    Sets the value of the variable with \a key to \a value.
*/
void Component::setValue(const QString &key, const QString &value)
{
    QString normalizedValue = d->m_core->replaceVariables(value);

    if (d->m_vars.value(key) == normalizedValue)
        return;

    if (key == scName)
        d->m_componentName = normalizedValue;

    d->m_vars[key] = normalizedValue;
    emit valueChanged(key, normalizedValue);
}

/*!
    Returns the installer this component belongs to.
*/
PackageManagerCore *Component::packageManagerCore() const
{
    return d->m_core;
}

/*!
    Returns the parent of this component. If this component is org.qt-project.sdk.qt, its
    parent is org.qt-project.sdk, as far as this exists.
*/
Component *Component::parentComponent() const
{
    return d->m_parentComponent;
}

/*!
    Appends \a component as a child of this component. If \a component already has a parent,
    it is removed from the previous parent. If the \a component has as sorting priority set, the child list
    is sorted in case of multiple components (high goes on top).
*/
void Component::appendComponent(Component *component)
{
    if (d->m_core->isUpdater())
        throw Error(tr("Components cannot have children in updater mode."));

    if (!component->isVirtual()) {
        d->m_childComponents.append(component);
        std::sort(d->m_childComponents.begin(), d->m_childComponents.end(), SortingPriorityGreaterThan());
    } else {
        d->m_virtualChildComponents.append(component);
    }

    d->m_allChildComponents = d->m_childComponents + d->m_virtualChildComponents;
    if (Component *parent = component->parentComponent())
        parent->removeComponent(component);
    component->d->m_parentComponent = this;
    setTristate(d->m_childComponents.count() > 0);
}

/*!
    Removes \a component if it is a child of this component. The component object still exists after the
    function returns. It's up to the caller to delete the passed \a component.
*/
void Component::removeComponent(Component *component)
{
    if (component->parentComponent() == this) {
        component->d->m_parentComponent = 0;
        d->m_childComponents.removeAll(component);
        d->m_virtualChildComponents.removeAll(component);
        d->m_allChildComponents = d->m_childComponents + d->m_virtualChildComponents;
    }
}

/*!
    Returns a list of child components. If \a kind is set to DirectChildrenOnly, the returned list contains
    only the direct children, if set to Descendants it will also include all descendants of the components
    children. Note: The returned list does include ALL children, non virtual components as well as virtual
    components.
*/
QList<Component *> Component::childComponents(Kind kind) const
{
    if (d->m_core->isUpdater())
        return QList<Component*>();

    QList<Component *> result = d->m_allChildComponents;
    if (kind == DirectChildrenOnly)
        return result;

    foreach (Component *component, d->m_allChildComponents)
        result += component->childComponents(kind);
    return result;
}

/*!
    Contains this component's name (unique identifier).
*/
QString Component::name() const
{
    return d->m_componentName;
}

/*!
    Contains this component's display name (as visible to the user).
*/
QString Component::displayName() const
{
    return d->m_vars.value(scDisplayName);
}

void Component::loadComponentScript()
{
    const QString script = d->m_vars.value(scScriptTag);
    if (!localTempPath().isEmpty() && !script.isEmpty())
        loadComponentScript(QString::fromLatin1("%1/%2/%3").arg(localTempPath(), name(), script));
}

/*!
    Loads the script at \a fileName into ScriptEngine. The installer and all its
    components as well as other useful stuff are being exported into the script.
    Read \link componentscripting Component Scripting \endlink for details.
    Throws an error when either the script at \a fileName couldn't be opened, or the QScriptEngine
    couldn't evaluate the script.
*/
void Component::loadComponentScript(const QString &fileName)
{
    // introduce the component object as javascript value and call the name to check that it
    // was successful
    d->m_scriptContext = d->scriptEngine()->loadInContext(QLatin1String("Component"), fileName,
        QString::fromLatin1("var component = installer.componentByName('%1'); component.name;")
        .arg(name()));

    emit loaded();
    languageChanged();
}

/*!
    \internal
    Calls the script method \link retranslateUi() \endlink, if any. This is done whenever a
    QTranslator file is being loaded.
*/
void Component::languageChanged()
{
    d->scriptEngine()->callScriptMethod(d->m_scriptContext, QLatin1String("retranslateUi"));
}

/*!
    Loads the translations matching the name filters \a qms inside \a directory. Only translations
    with a \link QFileInfo::baseName() baseName \endlink matching the current locales \link
    QLocale::name() name \endlink are loaded.
    Read \l componenttranslation for details.
*/
void Component::loadTranslations(const QDir &directory, const QStringList &qms)
{
    QDirIterator it(directory.path(), qms, QDir::Files);
    const QStringList translations = d->m_core->settings().translations();
    const QString uiLanguage = QLocale().uiLanguages().value(0, QLatin1String("en_us"))
        .replace(QLatin1Char('-'), QLatin1Char('_'));
    while (it.hasNext()) {
        const QString filename = it.next();
        const QString basename = QFileInfo(filename).baseName();
        if (!uiLanguage.startsWith(QFileInfo(filename).baseName(), Qt::CaseInsensitive))
            continue; // do not load the file if it does not match the UI language

        if (!translations.isEmpty()) {
            bool found = false;
            foreach (const QString &translation, translations)
                found |= translation.startsWith(basename, Qt::CaseInsensitive);
            if (!found) // don't load the file if it does match the UI language but is not allowed to be used
                continue;
        }

        QScopedPointer<QTranslator> translator(new QTranslator(this));
        if (!translator->load(filename))
            throw Error(tr("Could not open the requested translation file '%1'.").arg(filename));
        qApp->installTranslator(translator.take());
    }
}

/*!
    Loads the user interface files matching the name filters \a uis inside \a directory. The loaded
    interface can be accessed via userInterfaces by using the class name set in the ui file.
    Read \l componentuserinterfaces for details.
*/
void Component::loadUserInterfaces(const QDir &directory, const QStringList &uis)
{
    if (qobject_cast<QApplication*> (qApp) == 0)
        return;

    QDirIterator it(directory.path(), uis, QDir::Files);
    while (it.hasNext()) {
        QFile file(it.next());
        if (!file.open(QIODevice::ReadOnly)) {
            throw Error(tr("Could not open the requested UI file '%1'. Error: %2").arg(it.fileName(),
                file.errorString()));
        }

        static QUiLoader loader;
        loader.setTranslationEnabled(true);
        loader.setLanguageChangeEnabled(true);
        QWidget *const widget = loader.load(&file, 0);
        if (!widget) {
            throw Error(tr("Could not load the requested UI file '%1'. Error: %2").arg(it.fileName(),
                loader.errorString()));
        }
        d->m_userInterfaces.insert(widget->objectName(), widget);
    }
}

/*!
  Loads the text of the Licenses contained in the licenseHash.
  This is saved into a new hash containing the filename and the text of that file.
*/
void Component::loadLicenses(const QString &directory, const QHash<QString, QVariant> &licenseHash)
{
    QHash<QString, QVariant>::const_iterator it;
    for (it = licenseHash.begin(); it != licenseHash.end(); ++it) {
        const QString &fileName = it.value().toString();

        if (!ProductKeyCheck::instance()->isValidLicenseTextFile(fileName))
            continue;

        QFileInfo fileInfo(fileName);
        QFile file(QString::fromLatin1("%1%2_%3.%4").arg(directory, fileInfo.baseName(),
            QLocale().name().toLower(), fileInfo.completeSuffix()));
        if (!file.open(QIODevice::ReadOnly)) {
            // No translated license, use untranslated file
            qDebug("Unable to open translated license file. Using untranslated fallback.");
            file.setFileName(directory + fileName);
            if (!file.open(QIODevice::ReadOnly)) {
                throw Error(tr("Could not open the requested license file '%1'. Error: %2").arg(fileName,
                    file.errorString()));
            }
        }
        QTextStream stream(&file);
        stream.setCodec("UTF-8");
        d->m_licenses.insert(it.key(), qMakePair(fileName, stream.readAll()));
    }
}


/*!
    \qmlproperty stringlist component::userInterfaces

    Returns a list of all user interface class names known to this component.
*/
QStringList Component::userInterfaces() const
{
    return d->m_userInterfaces.keys();
}

QHash<QString, QPair<QString, QString> > Component::licenses() const
{
    return d->m_licenses;
}

/*!
    \qmlmethod QWidget component::userInterface(string name)

    Returns the QWidget created for \a name or 0 if the widget already has been deleted or cannot be found.
*/
QWidget *Component::userInterface(const QString &name) const
{
    return d->m_userInterfaces.value(name).data();
}

/*!
    \qmlmethod void component::createOperationsForPath(string path)

    Creates all operations needed to install this component's \a path. \a path is a full qualified
    filename including the component's name. This method gets called from
    component::createOperationsForArchive. You can override it by providing a method with
    the same name in the component script.

    \note RSA signature files are omitted by this method.
    \note If you call this method from a script, it won't call the scripts method with the same name.

    The default implementation is recursively creating Copy and Mkdir operations for all files
    and folders within \a path.
*/
void Component::createOperationsForPath(const QString &path)
{
    const QFileInfo fi(path);

    // don't copy over a checksum file
    if (fi.suffix() == QLatin1String("sha1") && QFileInfo(fi.dir(), fi.completeBaseName()).exists())
        return;

    // the script can override this method
    if (!d->scriptEngine()->callScriptMethod(d->m_scriptContext,
        QLatin1String("createOperationsForPath"), QJSValueList() << path).isUndefined()) {
            return;
    }

    QString target;
    static const QString prefix = QString::fromLatin1("installer://");
    target = QString::fromLatin1("@TargetDir@%1").arg(path.mid(prefix.length() + name().length()));

    if (fi.isFile()) {
        static const QString copy = QString::fromLatin1("Copy");
        addOperation(copy, fi.filePath(), target);
    } else if (fi.isDir()) {
        qApp->processEvents();
        static const QString mkdir = QString::fromLatin1("Mkdir");
        addOperation(mkdir, target);

        QDirIterator it(fi.filePath());
        while (it.hasNext())
            createOperationsForPath(it.next());
    }
}

/*!
    \qmlmethod void component::createOperationsForArchive(string archive)

    Creates all operations needed to install this component's \a archive. This method gets called
    from component::createOperations. You can override this method by providing a method with the
    same name in the component script.

    \note If you call this method from a script, it won't call the scripts method with the same name.

    The default implementation calls createOperationsForPath for everything contained in the archive.
    If \a archive is a compressed archive known to the installer system, an Extract operation is
    created, instead.
*/
void Component::createOperationsForArchive(const QString &archive)
{
    const QFileInfo fi(archive);

    // don't do anything with sha1 files
    if (fi.suffix() == QLatin1String("sha1") && QFileInfo(fi.dir(), fi.completeBaseName()).exists())
        return;

    // the script can override this method
    if (!d->scriptEngine()->callScriptMethod(d->m_scriptContext,
        QLatin1String("createOperationsForArchive"), QJSValueList() << archive).isUndefined()) {
            return;
    }

    const bool isZip = Lib7z::isSupportedArchive(archive);

    if (isZip) {
        // archives get completely extracted per default (if the script isn't doing other stuff)
        addOperation(QLatin1String("Extract"), archive, QLatin1String("@TargetDir@"));
    } else {
        createOperationsForPath(archive);
    }
}

/*!
    \qmlmethod void component::beginInstallation()

    Starts the component installation.
    You can override this method by providing a method with the same name in the component script.

    \code
    Component.prototype.beginInstallation = function()
    {
        // call default implementation
        component.beginInstallation();
        // ...
    }
    \endcode

*/
void Component::beginInstallation()
{
    // the script can override this method
    d->scriptEngine()->callScriptMethod(d->m_scriptContext, QLatin1String("beginInstallation"));
}

/*!
    \qmlmethod void component::createOperations()

    Creates all operations needed to install this component.
    You can override this method by providing a method with the same name in the component script.

    \note If you call this method from a script, it won't call the scripts method with the same name.

    The default implementation calls createOperationsForArchive for all archives in this component.
*/
void Component::createOperations()
{
    // the script can override this method
    if (!d->scriptEngine()->callScriptMethod(d->m_scriptContext, QLatin1String("createOperations"))
        .isUndefined()) {
            d->m_operationsCreated = true;
            return;
    }

    foreach (const QString &archive, archives())
        createOperationsForArchive(archive);

    d->m_operationsCreated = true;
}

/*!
    \qmlmethod void component::registerPathForUninstallation(string path, boolean wipe = false)

    Registers the file or directory at \a path for being removed when this component gets uninstalled.
    In case of a directory, this will be recursive. If \a wipe is set to \c true, the directory will
    also be deleted if it contains changes done by the user after installation.
*/
void Component::registerPathForUninstallation(const QString &path, bool wipe)
{
    d->m_pathsForUninstallation.append(qMakePair(path, wipe));
}

/*!
    Returns the list of paths previously registered for uninstallation with
    #registerPathForUninstallation.
*/
QList<QPair<QString, bool> > Component::pathsForUninstallation() const
{
    return d->m_pathsForUninstallation;
}

/*!
    Contains the names of all archives known to this component. Even downloaded archives are mapped
    to the installer:// url throw the used QFileEngineHandler during the download process.
*/
QStringList Component::archives() const
{
    QString pathString = QString::fromLatin1("installer://%1/").arg(name());
    QStringList archivesNameList = QDir(pathString).entryList();
    //RegExp "^" means line beginning
    archivesNameList.replaceInStrings(QRegExp(QLatin1String("^")), pathString);
    return archivesNameList;
}

/*!
    \qmlmethod void component::addDownloadableArchive(string path)

    Adds the archive \a path to this component. This can only be called when this component was
    downloaded from an online repository. When adding \a path, it will be downloaded from the
    repository when the installation starts.

    \sa removeDownloadableArchive, fromOnlineRepository, archives
*/
void Component::addDownloadableArchive(const QString &path)
{
    Q_ASSERT(isFromOnlineRepository());

    qDebug() << "addDownloadable" << path;
    d->m_downloadableArchives.append(d->m_vars.value(scRemoteVersion) + path);
}

/*!
    \qmlmethod void component::removeDownloadableArchive(string path)

    Removes the archive \a path previously added via addDownloadableArchive from this component.
    This can only be called when this component was downloaded from an online repository.

    \sa addDownloadableArchive, fromOnlineRepository, archives
*/
void Component::removeDownloadableArchive(const QString &path)
{
    Q_ASSERT(isFromOnlineRepository());
    d->m_downloadableArchives.removeAll(path);
}

/*!
    Returns the archives to be downloaded from the online repository before installation.
*/
QStringList Component::downloadableArchives() const
{
    return d->m_downloadableArchives;
}

/*!
    \qmlmethod void component::addStopProcessForUpdateRequest(string process)

    Adds a request for quitting the process \a process before installing/updating/uninstalling the
    component.
*/
void Component::addStopProcessForUpdateRequest(const QString &process)
{
    d->m_stopProcessForUpdateRequests.append(process);
}

/*!
    \qmlmethod void component::removeStopProcessForUpdateRequest(string process)

    Removes the request for quitting the process \a process again.
*/
void Component::removeStopProcessForUpdateRequest(const QString &process)
{
    d->m_stopProcessForUpdateRequests.removeAll(process);
}

/*!
    \qmlmethod void component::setStopProcessForUpdateRequest(string process, boolean requested)

    Convenience: Add/remove request depending on \a requested (add if \c true, remove if \c false).
*/
void Component::setStopProcessForUpdateRequest(const QString &process, bool requested)
{
    if (requested)
        addStopProcessForUpdateRequest(process);
    else
        removeStopProcessForUpdateRequest(process);
}

/*!
    The list of processes this component needs to be closed before installing/updating/uninstalling
*/
QStringList Component::stopProcessForUpdateRequests() const
{
    return d->m_stopProcessForUpdateRequests;
}

/*!
    Returns the operations needed to install this component. If autoCreateOperations is \c true,
    createOperations is called, if no operations have been auto-created yet.
*/
OperationList Component::operations() const
{
    if (d->m_autoCreateOperations && !d->m_operationsCreated) {
        const_cast<Component*>(this)->createOperations();

        if (!d->m_minimumProgressOperation) {
            d->m_minimumProgressOperation = KDUpdater::UpdateOperationFactory::instance()
                .create(QLatin1String("MinimumProgress"));
            d->m_minimumProgressOperation->setValue(QLatin1String("component"), name());
            d->m_operations.append(d->m_minimumProgressOperation);
        }

        if (!d->m_licenses.isEmpty()) {
            d->m_licenseOperation = KDUpdater::UpdateOperationFactory::instance()
                .create(QLatin1String("License"));
            d->m_licenseOperation->setValue(QLatin1String("installer"), QVariant::fromValue(d->m_core));
            d->m_licenseOperation->setValue(QLatin1String("component"), name());

            QVariantMap licenses;
            const QList<QPair<QString, QString> > values = d->m_licenses.values();
            for (int i = 0; i < values.count(); ++i)
                licenses.insert(values.at(i).first, values.at(i).second);
            d->m_licenseOperation->setValue(QLatin1String("licenses"), licenses);
            d->m_operations.append(d->m_licenseOperation);
        }
    }
    return d->m_operations;
}

/*!
    Adds \a operation to the list of operations needed to install this component.
*/
void Component::addOperation(Operation *operation)
{
    d->m_operations.append(operation);
    if (RemoteClient::instance().isActive())
        operation->setValue(QLatin1String("admin"), true);
}

/*!
    Adds \a operation to the list of operations needed to install this component. \a operation
    is executed with elevated rights.
*/
void Component::addElevatedOperation(Operation *operation)
{
    if (value(scRequiresAdminRights, scFalse) != scTrue) {
        qWarning() << QString::fromLatin1("Component %1 uses addElevatedOperation in the script, "
                                          "but it does not have the needed RequiresAdminRights tag"
                                          ).arg(name());
    }
    addOperation(operation);
    operation->setValue(QLatin1String("admin"), true);
}

bool Component::operationsCreatedSuccessfully() const
{
    return d->m_operationsCreatedSuccessfully;
}

Operation *Component::createOperation(const QString &operationName, const QString &parameter1,
    const QString &parameter2, const QString &parameter3, const QString &parameter4, const QString &parameter5,
    const QString &parameter6, const QString &parameter7, const QString &parameter8, const QString &parameter9,
    const QString &parameter10)
{
    QStringList arguments;
    if (!parameter1.isNull())
        arguments.append(parameter1);
    if (!parameter2.isNull())
        arguments.append(parameter2);
    if (!parameter3.isNull())
        arguments.append(parameter3);
    if (!parameter4.isNull())
        arguments.append(parameter4);
    if (!parameter5.isNull())
        arguments.append(parameter5);
    if (!parameter6.isNull())
        arguments.append(parameter6);
    if (!parameter7.isNull())
        arguments.append(parameter7);
    if (!parameter8.isNull())
        arguments.append(parameter8);
    if (!parameter9.isNull())
        arguments.append(parameter9);
    if (!parameter10.isNull())
        arguments.append(parameter10);

    return createOperation(operationName, arguments);
}

Operation *Component::createOperation(const QString &operationName, const QStringList &parameters)
{
    Operation *operation = KDUpdater::UpdateOperationFactory::instance().create(operationName);
    if (operation == 0) {
        const QMessageBox::StandardButton button =
            MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(),
            QLatin1String("OperationDoesNotExistError"), tr("Error"), tr("Error: Operation %1 does not exist")
                .arg(operationName), QMessageBox::Abort | QMessageBox::Ignore);
        if (button == QMessageBox::Abort)
            d->m_operationsCreatedSuccessfully = false;
        return operation;
    }

    if (operation->name() == QLatin1String("Delete"))
        operation->setValue(QLatin1String("performUndo"), false);
    operation->setValue(QLatin1String("installer"), qVariantFromValue(d->m_core));

    operation->setArguments(d->m_core->replaceVariables(parameters));
    operation->setValue(QLatin1String("component"), name());
    return operation;
}

/*!
    \qmlmethod boolean component::addOperation(string operation, string parameter1 = "", string parameter2 = "", ..., string parameter10 = "")

    Convenience method for calling addOperation(string, stringlist) with up to 10 arguments.
*/
bool Component::addOperation(const QString &operation, const QString &parameter1, const QString &parameter2,
    const QString &parameter3, const QString &parameter4, const QString &parameter5, const QString &parameter6,
    const QString &parameter7, const QString &parameter8, const QString &parameter9, const QString &parameter10)
{
    if (Operation *op = createOperation(operation, parameter1, parameter2, parameter3, parameter4, parameter5,
        parameter6, parameter7, parameter8, parameter9, parameter10)) {
            addOperation(op);
            return true;
    }

    return false;
}

/*!
    \qmlmethod boolean component::addOperation(string operation, stringlist parameters)

    Creates and adds an installation operation for \a operation. Add any number of parameters.
    The contents of the parameters get variables like "@TargetDir@" replaced with their values,
    if contained.
*/
bool Component::addOperation(const QString &operation, const QStringList &parameters)
{
    if (Operation *op = createOperation(operation, parameters)) {
            addOperation(op);
            return true;
    }

    return false;
}

/*!
    \qmlmethod boolean component::addElevatedOperation(string operation, string parameter1 = "", string parameter2 = "", ..., string parameter10 = "")

    Convenience method for calling addElevatedOperation(string, stringlist) with up to 10 arguments.
*/
bool Component::addElevatedOperation(const QString &operation, const QString &parameter1,
    const QString &parameter2, const QString &parameter3, const QString &parameter4, const QString &parameter5,
    const QString &parameter6, const QString &parameter7, const QString &parameter8, const QString &parameter9,
    const QString &parameter10)
{
    if (Operation *op = createOperation(operation, parameter1, parameter2, parameter3, parameter4, parameter5,
        parameter6, parameter7, parameter8, parameter9, parameter10)) {
            addElevatedOperation(op);
            return true;
    }

    return false;
}

/*!
    \qmlmethod boolean component::addElevatedOperation(string operation, stringlist parameters)

    Creates and adds an installation operation for \a operation. Add any number of parameters.
    The contents of the parameters get variables like "@TargetDir@" replaced with their values,
    if contained. \a operation is executed with elevated rights.

*/
bool Component::addElevatedOperation(const QString &operation, const QStringList &parameters)
{
    if (Operation *op = createOperation(operation, parameters)) {
            addElevatedOperation(op);
            return true;
    }

    return false;
}

/*!
    Specifies whether operations should be automatically created when the installation starts. This
    would be done by calling #createOperations. If you set this to \c false, it is completely up to the
    component's script to create all operations.
*/
bool Component::autoCreateOperations() const
{
    return d->m_autoCreateOperations;
}

/*!
    \qmlmethod void component::setAutoCreateOperations(boolean autoCreateOperations)

    Setter for the \l autoCreateOperations property.
 */
void Component::setAutoCreateOperations(bool autoCreateOperations)
{
    d->m_autoCreateOperations = autoCreateOperations;
}

bool Component::isVirtual() const
{
    return d->m_vars.value(scVirtual, scFalse).toLower() == scTrue;
}

/*!
    Specifies whether this component is selected for installation.
*/
bool Component::isSelected() const
{
    return checkState() != Qt::Unchecked;
}

bool Component::forcedInstallation() const
{
    return d->m_vars.value(scForcedInstallation, scFalse).toLower() == scTrue;
}

void Component::setValidatorCallbackName(const QString &name)
{
    validatorCallbackName = name;
}

bool Component::validatePage()
{
    if (!validatorCallbackName.isEmpty())
        return d->scriptEngine()->callScriptMethod(d->m_scriptContext, validatorCallbackName).toBool();
    return true;
}

/*!
    \qmlmethod void component::setSelected(boolean selected)

    Marks the component for installation. Emits the selectedChanged() signal if the check state changes.

    \note This method does not do anything and is deprecated since 1.3.
*/
void Component::setSelected(bool selected)
{
    Q_UNUSED(selected)
    qDebug() << Q_FUNC_INFO << QString::fromLatin1("on '%1' is deprecated.").arg(d->m_componentName);
}

/*!
    \qmlmethod void component::addDependency(string newDependency)

    Adds a new component \a newDependency to the list of dependencies.

    \sa dependencies
*/

void Component::addDependency(const QString &newDependency)
{
    QString oldDependencies = d->m_vars.value(scDependencies);
    if (oldDependencies.isEmpty())
        setValue(scDependencies, newDependency);
    else
        setValue(scDependencies, oldDependencies + QLatin1String(", ") + newDependency);
}


/*!
    Contains this component dependencies.
    Read \l componentdependencies for details.
*/
QStringList Component::dependencies() const
{
    return d->m_vars.value(scDependencies).split(QInstaller::commaRegExp(), QString::SkipEmptyParts);
}

QStringList Component::autoDependencies() const
{
    QStringList autoDependencyStringList =
        d->m_vars.value(scAutoDependOn).split(QInstaller::commaRegExp(), QString::SkipEmptyParts);
    autoDependencyStringList.removeAll(scScriptTag);
    return autoDependencyStringList;
}

/*!
    \qmlmethod void component::setInstalled()

    Set's the components state to installed.
*/
void Component::setInstalled()
{
    setValue(scCurrentState, scInstalled);
}

/*!
    \qmlmethod boolean component::isAutoDependOn(QSet<string> componentsToInstall)

    Determines if the component comes as an auto dependency. Returns \c true if the component needs
    to be installed.
*/
bool Component::isAutoDependOn(const QSet<QString> &componentsToInstall) const
{
    // If there is no auto depend on value or the value is empty, we have nothing todo. The component does
    // not need to be installed as an auto dependency.
    QStringList autoDependOnList = autoDependencies();
    if (autoDependOnList.isEmpty())
        return false;

    // The script can override this method and determines if the component needs to be installed.
    if (autoDependOnList.first().compare(scScriptTag, Qt::CaseInsensitive) == 0) {
        QJSValue valueFromScript;
        try {
            valueFromScript = d->scriptEngine()->callScriptMethod(d->m_scriptContext,
                QLatin1String("isAutoDependOn"));
        } catch (const Error &error) {
            MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(),
                QLatin1String("isAutoDependOnError"), tr("Cannot resolve isAutoDependOn in %1"
                    ).arg(name()), error.message());
            return false;
        }

        if (!valueFromScript.isError())
            return valueFromScript.toBool();
        qDebug() << "Value from script is not valid." << (valueFromScript.toString().isEmpty()
            ? QString::fromLatin1("Unknown error.") : valueFromScript.toString());
        return false;
    }

    QSet<QString> components = componentsToInstall;
    const QStringList installedPackages = d->m_core->localInstalledPackages().keys();
    foreach (const QString &name, installedPackages)
        components.insert(name);

    foreach (const QString &component, components) {
        autoDependOnList.removeAll(component);
        if (autoDependOnList.isEmpty()) {
            // If all components in the isAutoDependOn field are already installed or selected for
            // installation, this component needs to be installed as well.
            return true;
        }
    }

    return false;
}

/*!
    \qmlmethod boolean component::isDefault()

    Indicates if the component is a default one.

    \note Always returns \c false for virtual components.

    \sa default
*/
bool Component::isDefault() const
{
     if (isVirtual())
         return false;

    // the script can override this method
    if (d->m_vars.value(scDefault).compare(scScriptTag, Qt::CaseInsensitive) == 0) {
        QJSValue valueFromScript;
        try {
            valueFromScript = d->scriptEngine()->callScriptMethod(d->m_scriptContext,
                QLatin1String("isDefault"));
        } catch (const Error &error) {
            MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(),
                QLatin1String("isDefaultError"), tr("Cannot resolve isDefault in %1").arg(name()),
                    error.message());
            return false;
        }
        if (!valueFromScript.isError())
            return valueFromScript.toBool();
        qDebug() << "Value from script is not valid." << (valueFromScript.toString().isEmpty()
            ? QString::fromLatin1("Unknown error.") : valueFromScript.toString());
        return false;
    }

    return d->m_vars.value(scDefault).compare(scTrue, Qt::CaseInsensitive) == 0;
}

/*!
    \qmlmethod boolean component::isInstalled()

    Determines if the component is installed.
*/
bool Component::isInstalled() const
{
    return scInstalled == d->m_vars.value(scCurrentState);
}

/*!
    \qmlmethod boolean component::installationRequested()

    Determines if the user wants to install the component
*/
bool Component::installationRequested() const
{
    return !isInstalled() && isSelected();
}

/*!
    \qmlmethod void component::setUpdateAvailable(boolean isUpdateAvailable)

    Sets a flag that the core found an update
*/
void Component::setUpdateAvailable(bool isUpdateAvailable)
{
    d->m_updateIsAvailable = isUpdateAvailable;
}

/*!
    \qmlmethod boolean component::updateRequested()

    Determines if the user wants to install the update for this component
*/
bool Component::updateRequested()
{
    return d->m_updateIsAvailable && isSelected();
}

/*!
    \qmlmethod boolean component::componentChangeRequested()

    Returns \c true if that component will be changed (update/installation/uninstallation).
*/
bool Component::componentChangeRequested()
{
    return updateRequested() || installationRequested() || uninstallationRequested();
}


/*!
    \qmlmethod void component::setUninstalled()

    Sets the component state to uninstalled.
*/
void Component::setUninstalled()
{
    setValue(scCurrentState, scUninstalled);
}

/*!
    \qmlmethod boolean component::isUninstalled()

    Determines if the component is uninstalled.
*/
bool Component::isUninstalled() const
{
    return scUninstalled == d->m_vars.value(scCurrentState);
}

/*!
    \qmlmethod boolean component::uninstallationRequested()

    Determines if the user wants to uninstall the component.
*/
bool Component::uninstallationRequested() const
{
    if (packageManagerCore()->isUpdater())
        return false;
    return isInstalled() && !isSelected();
}

/*!
    \qmlmethod boolean component::isFromOnlineRepository()

    Determines whether this component has been loaded from an online repository.

    \sa addDownloadableArchive, fromOnlineRepository
*/
bool Component::isFromOnlineRepository() const
{
    return !repositoryUrl().isEmpty();
}

/*!
    Contains the repository Url this component is downloaded from.
    When this component is not downloaded from an online repository, returns an empty #QUrl.
*/
QUrl Component::repositoryUrl() const
{
    return d->m_repositoryUrl;
}

/*!
    Sets this components #repositoryUrl.
*/
void Component::setRepositoryUrl(const QUrl &url)
{
    d->m_repositoryUrl = url;
}


QString Component::localTempPath() const
{
    return d->m_localTempPath;
}

void Component::setLocalTempPath(const QString &tempLocalPath)
{
    d->m_localTempPath = tempLocalPath;
}

void Component::updateModelData(const QString &key, const QString &data)
{
    if (key == scVirtual) {
        if (data.toLower() == scTrue)
            setData(d->m_core->virtualComponentsFont(), Qt::FontRole);
        if (Component *const parent = parentComponent()) {
            parent->removeComponent(this);
            parent->appendComponent(this);
        }
        emit virtualStateChanged();
    }

    if (key == scRemoteDisplayVersion)
        setData(data, RemoteDisplayVersion);

    if (key == scDisplayName)
        setData(data, Qt::DisplayRole);

    if (key == scDisplayVersion)
        setData(data, LocalDisplayVersion);

    if (key == scReleaseDate)
        setData(data, ReleaseDate);

    if (key == scUncompressedSize) {
        quint64 size = d->m_vars.value(scUncompressedSizeSum).toLongLong();
        setData(humanReadableSize(size), UncompressedSize);
    }

    const QString &updateInfo = d->m_vars.value(scUpdateText);
    if (!d->m_core->isUpdater() || updateInfo.isEmpty()) {
        const QString tooltipText
                = QString::fromLatin1("<html><body>%1</body></html>").arg(d->m_vars.value(scDescription));
        setData(tooltipText, Qt::ToolTipRole);
    } else {
        const QString tooltipText
                = d->m_vars.value(scDescription) + QLatin1String("<br><br>")
                + tr("Update Info: ") + updateInfo;

        setData(tooltipText, Qt::ToolTipRole);
    }
}


QDebug QInstaller::operator<<(QDebug dbg, Component *component)
{
    dbg << "component: " << component->name() << "\n";
    dbg << "\tisSelected: \t" << component->isSelected() << "\n";
    dbg << "\tisInstalled: \t" << component->isInstalled() << "\n";
    dbg << "\tisUninstalled: \t" << component->isUninstalled() << "\n";
    dbg << "\tupdateRequested: \t" << component->updateRequested() << "\n";
    dbg << "\tinstallationRequested: \t" << component->installationRequested() << "\n";
    dbg << "\tuninstallationRequested: \t" << component->uninstallationRequested() << "\n";
    return dbg;
}
