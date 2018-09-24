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
#include "utils.h"

#include "updateoperationfactory.h"

#include <productkeycheck.h>

#include <QtCore/QDirIterator>
#include <QtCore/QRegExp>
#include <QtCore/QTranslator>

#include <QApplication>

#include <QtUiTools/QUiLoader>

#include <private/qv8engine_p.h>
#include <private/qv4scopedvalue_p.h>
#include <private/qv4object_p.h>

#include <algorithm>

using namespace QInstaller;

static const QLatin1String scScriptTag("Script");
static const QLatin1String scVirtual("Virtual");
static const QLatin1String scInstalled("Installed");
static const QLatin1String scUpdateText("UpdateText");
static const QLatin1String scUninstalled("Uninstalled");
static const QLatin1String scCurrentState("CurrentState");
static const QLatin1String scForcedInstallation("ForcedInstallation");
static const QLatin1String scCheckable("Checkable");
static const QLatin1String scExpandedByDefault("ExpandedByDefault");
static const QLatin1String scUnstable("Unstable");

/*!
    \inmodule QtInstallerFramework
    \class Component::SortingPriorityLessThan
    \brief The SortingPriorityLessThan class sets an increasing sorting order for child components.

    If the component contains several children and has this sorting priority set, the child list
    is sorted so that the child component with the lowest priority is placed on top.
*/

/*!
    \fn Component::SortingPriorityLessThan::operator() (const Component *lhs, const Component *rhs) const

    Returns \c true if \a lhs is less than \a rhs; otherwise returns \c false.
*/

/*!
    \inmodule QtInstallerFramework
    \class Component::SortingPriorityGreaterThan
    \brief The SortingPriorityGreaterThan class sets a decreasing sorting priority for child
    components.

    If the component contains several children and has this sorting priority set, the child list
    is sorted so that the child component with the highest priority is placed on top.
*/

/*!
    \fn Component::SortingPriorityGreaterThan::operator() (const Component *lhs, const Component *rhs) const

    Returns \c true if \a lhs is greater than \a rhs; otherwise returns \c false.
*/

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::Component
    \brief The Component class represents the current component.
*/

/*!
    \property Component::name

    \brief The name of the component as set in the \c <Name> tag of the package
    information file.
*/

/*!
    \property Component::displayName

    \brief The name of the component as shown in the user interface.
*/

/*!
    \property Component::autoCreateOperations

    \brief Whether some standard operations for the component should be
    automatically created when the installation starts.

    The default is \c true.
*/

/*!
    \property Component::archives

    \brief The list of archive URLs registered for the component.

    The URLs are prefixed with \c {installer://}.

    \sa addDownloadableArchive(), removeDownloadableArchive()
*/

/*!
    \property Component::dependencies

    \brief The components this component depends on. This is a read-only property.
*/

/*!
    \property Component::autoDependencies

    \brief The value of the \c <AutoDependOn> element in the package information file.
*/

/*!
    \property Component::fromOnlineRepository

    \brief Whether this component has been loaded from an online repository.
*/

/*!
    \property Component::repositoryUrl

    \brief The repository URL the component is downloaded from.

    If this component is not downloaded from an online repository, returns an empty QUrl.
*/

/*!
    \property Component::default

    \brief Whether the component is a default one. This is a read-only property.

    \note Always \c false for virtual components.
*/

/*!
    \property Component::installed

    \brief Whether the component is installed.  This is a read-only property.
*/

/*!
    \property Component::enabled

    \brief Whether the component is currently enabled. The property is both readable and writable.
*/

/*!
    \fn Component::loaded()

    \sa {component::loaded}{component.loaded}
*/

/*!
    \fn Component::valueChanged(const QString &key, const QString &value)

    Emitted when the value of the variable with the name \a key changes to \a value.

    \sa {component::valueChanged}{component.valueChanged}, setValue()
*/

/*!
    \fn Component::virtualStateChanged()

    \sa {component::virtualStateChanged}{component.virtualStateChanged}
*/


/*!
    Creates a new component in the package manager specified by \a core.
*/
Component::Component(PackageManagerCore *core)
    : d(new ComponentPrivate(core, this))
{
    setPrivate(d);

    connect(this, &Component::valueChanged, this, &Component::updateModelData);
    qRegisterMetaType<QList<QInstaller::Component*> >("QList<QInstaller::Component*>");
}

/*!
    Destroys the component.
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

/*!
    Sets variables according to the values set in the package.xml file of a local \a package.
*/
void Component::loadDataFromPackage(const KDUpdater::LocalPackage &package)
{
    setValue(scName, package.name);
    setValue(scDisplayName, package.title);
    setValue(scDescription, package.description);
    setValue(scVersion, package.version);
    setValue(scInheritVersion, package.inheritVersionFrom);
    setValue(scInstalledVersion, package.version);
    setValue(QLatin1String("LastUpdateDate"), package.lastUpdateDate.toString());
    setValue(QLatin1String("InstallDate"), package.installDate.toString());
    setValue(scUncompressedSize, QString::number(package.uncompressedSize));
    setValue(scDependencies, package.dependencies.join(QLatin1String(",")));
    setValue(scAutoDependOn, package.autoDependencies.join(QLatin1String(",")));

    setValue(scForcedInstallation, package.forcedInstallation ? scTrue : scFalse);
    if (package.forcedInstallation & !PackageManagerCore::noForceInstallation()) {
        setCheckable(false);
        setCheckState(Qt::Checked);
    }
    setValue(scVirtual, package.virtualComp ? scTrue : scFalse);
    setValue(scCurrentState, scInstalled);
    setValue(scCheckable, package.checkable ? scTrue : scFalse);
    setValue(scExpandedByDefault, package.expandedByDefault ? scTrue : scFalse);
}

/*!
    Sets variables according to the values set in the package.xml file of \a package.
    Also loads UI files, licenses and translations if they are referenced in the package.xml.
*/
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
    setValue(scVersion, package.data(scVersion).toString());
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
    setValue(scCheckable, package.data(scCheckable).toString());
    setValue(scExpandedByDefault, package.data(scExpandedByDefault).toString());

    QString forced = package.data(scForcedInstallation, scFalse).toString().toLower();
    if (PackageManagerCore::noForceInstallation())
        forced = scFalse;
    setValue(scForcedInstallation, forced);
    if (forced == scTrue) {
        setCheckable(false);
        setCheckState(Qt::Checked);
    }

    setLocalTempPath(QInstaller::pathFromUrl(package.packageSource().url));
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

/*!
    Returns the size of a compressed archive.
*/
quint64 Component::updateUncompressedSize()
{
    quint64 size = 0;

    if (installAction() == ComponentModelHelper::Install
            || installAction() == ComponentModelHelper::KeepInstalled) {
        size = d->m_vars.value(scUncompressedSize).toLongLong();
    }

    foreach (Component* comp, d->m_allChildComponents)
        size += comp->updateUncompressedSize();

    setValue(scUncompressedSizeSum, QString::number(size));
    setData(humanReadableSize(size), UncompressedSize);

    return size;
}

/*!
    Marks the component as installed.
*/
void Component::markAsPerformedInstallation()
{
    d->m_newlyInstalled = true;
}

/*!
    Returns a key and value based hash of all variables set for this component.
*/
QHash<QString,QString> Component::variables() const
{
    return d->m_vars;
}

/*!
    Returns the value of variable name \a key. If \a key is not known yet, \a defaultValue is
    returned.

    \note If a component is virtual and you ask for the component value with the key \c Default,
    it will always return \c false.
*/
QString Component::value(const QString &key, const QString &defaultValue) const
{
    if (key == scDefault)
        return isDefault() ? scTrue : scFalse;
    return d->m_vars.value(key, defaultValue);
}

/*!
    Sets the value of the variable with \a key to \a value.

    \sa {component::setValue}{component.setValue}
*/
void Component::setValue(const QString &key, const QString &value)
{
    QString normalizedValue = d->m_core->replaceVariables(value);

    if (d->m_vars.value(key) == normalizedValue)
        return;

    if (key == scName)
        d->m_componentName = normalizedValue;
    if (key == scCheckable)
        this->setCheckable(normalizedValue.toLower() == scTrue);
    if (key == scExpandedByDefault)
        this->setExpandedByDefault(normalizedValue.toLower() == scTrue);

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
    Returns the parent of this component. For example, the parent of a component called
    \c org.qt-project.sdk.qt is \c org.qt-project.sdk if it exists.
*/
Component *Component::parentComponent() const
{
    return d->m_parentComponent;
}

/*!
    Appends \a component as a child of this component. If \a component already has a parent,
    it is removed from the previous parent. If the \a component contains several children and
    has the SortingPriorityGreaterThan() sorting priority set, the child list is sorted so that the
    child component with the highest priority is placed on top.
*/
void Component::appendComponent(Component *component)
{
    if (d->m_core->isUpdater())
        throw Error(tr("Components cannot have children in updater mode."));

    if (!component->isVirtual()) {
        const QList<Component *> virtualChildComponents = d->m_allChildComponents.mid(d->m_childComponents.count());
        d->m_childComponents.append(component);
        std::sort(d->m_childComponents.begin(), d->m_childComponents.end(), SortingPriorityGreaterThan());
        d->m_allChildComponents = d->m_childComponents + virtualChildComponents;
    } else {
        d->m_allChildComponents.append(component);
    }

    if (Component *parent = component->parentComponent())
        parent->removeComponent(component);
    component->d->m_parentComponent = this;
    setTristate(d->m_childComponents.count() > 0);
}

/*!
    Removes \a component if it is a child of this component. The component object still exists after
    the function returns. It is up to the caller to delete the passed \a component.
*/
void Component::removeComponent(Component *component)
{
    if (component->parentComponent() == this) {
        component->d->m_parentComponent = 0;
        d->m_childComponents.removeAll(component);
        d->m_allChildComponents.removeAll(component);
    }
}

/*!
    Returns a list of child components, including all descendants of the component's
    children.

    \note The returned list does include all children; non-virtual components as well as virtual
    components.
*/
QList<Component *> Component::descendantComponents() const
{
    if (d->m_core->isUpdater())
        return QList<Component*>();

    QList<Component *> result = d->m_allChildComponents;

    foreach (Component *component, d->m_allChildComponents)
        result += component->descendantComponents();
    return result;
}

/*!
    Contains the unique identifier of this component.
*/
QString Component::name() const
{
    return d->m_componentName;
}

/*!
    Contains this component's display name as visible to the user.
*/
QString Component::displayName() const
{
    return d->m_vars.value(scDisplayName);
}

/*!
    Loads the component script into the script engine.
*/
void Component::loadComponentScript()
{
    const QString script = d->m_vars.value(scScriptTag);
    if (!localTempPath().isEmpty() && !script.isEmpty())
        loadComponentScript(QString::fromLatin1("%1/%2/%3").arg(localTempPath(), name(), script));
}

/*!
    Loads the script at \a fileName into the script engine. The installer and all its
    components as well as other useful things are being exported into the script.
    For more information, see \l{Component Scripting}.

    Throws an error when either the script at \a fileName could not be opened, or QScriptEngine
    could not evaluate the script.
*/
void Component::loadComponentScript(const QString &fileName)
{
    // introduce the component object as javascript value and call the name to check that it
    // was successful
    try {
        d->m_scriptContext = d->scriptEngine()->loadInContext(QLatin1String("Component"), fileName,
            QString::fromLatin1("var component = installer.componentByName('%1'); component.name;")
            .arg(name()));
        if (packageManagerCore()->settings().allowUnstableComponents()) {
            // Check if component has dependency to a broken component. Dependencies to broken
            // components are checked if error is thrown but if dependency to a broken
            // component is added in script, the script might not be loaded yet
            foreach (QString dependency, dependencies()) {
                Component *dependencyComponent = packageManagerCore()->componentByName
                        (PackageManagerCore::checkableName(dependency));
                if (dependencyComponent && dependencyComponent->isUnstable())
                    setUnstable(PackageManagerCore::UnstableError::DepencyToUnstable, QLatin1String("Dependent on unstable component"));
            }
        }
    } catch (const Error &error) {
        if (packageManagerCore()->settings().allowUnstableComponents()) {
            setUnstable(PackageManagerCore::UnstableError::ScriptLoadingFailed, error.message());
            qWarning() << error.message();
        } else {
            throw error;
        }
    }

    emit loaded();
    languageChanged();
}

/*!
    \internal
    Calls the script method retranslateUi(), if any. This is done whenever a
    QTranslator file is being loaded.
*/
void Component::languageChanged()
{
    d->scriptEngine()->callScriptMethod(d->m_scriptContext, QLatin1String("retranslateUi"));
}

/*!
    Loads the translations matching the name filters \a qms inside \a directory. Only translations
    with a base name matching the current locale's name are loaded. For more information, see
    \l{Translating Pages}.
*/
void Component::loadTranslations(const QDir &directory, const QStringList &qms)
{
    QDirIterator it(directory.path(), qms, QDir::Files);
    const QStringList translations = d->m_core->settings().translations();
    const QString uiLanguage = QLocale().uiLanguages().value(0, QLatin1String("en"));
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
        if (translator->load(filename)) {
            // Do not throw if translator returns false as it may just be an intentionally
            // empty file. See also QTBUG-31031
            qApp->installTranslator(translator.take());
        }
    }
}

/*!
    Loads the user interface files matching the name filters \a uis inside \a directory. The loaded
    interface can be accessed via userInterfaces() by using the class name set in the UI file.
*/
void Component::loadUserInterfaces(const QDir &directory, const QStringList &uis)
{
    if (qobject_cast<QApplication*> (qApp) == 0)
        return;

    QDirIterator it(directory.path(), uis, QDir::Files);
    while (it.hasNext()) {
        QFile file(it.next());
        if (!file.open(QIODevice::ReadOnly)) {
            throw Error(tr("Cannot open the requested UI file \"%1\": %2").arg(
                            it.fileName(), file.errorString()));
        }

        static QUiLoader loader;
        loader.setTranslationEnabled(true);
        loader.setLanguageChangeEnabled(true);
        QWidget *const widget = loader.load(&file, 0);
        if (!widget) {
            throw Error(tr("Cannot load the requested UI file \"%1\": %2").arg(
                            it.fileName(), loader.errorString()));
        }
        d->scriptEngine()->newQObject(widget);
        d->m_userInterfaces.insert(widget->objectName(), widget);
    }
}

/*!
  Loads the text of the licenses contained in \a licenseHash from \a directory.
  This is saved into a new hash containing the filename and the text of that file.
*/
void Component::loadLicenses(const QString &directory, const QHash<QString, QVariant> &licenseHash)
{
    QHash<QString, QVariant>::const_iterator it;
    for (it = licenseHash.begin(); it != licenseHash.end(); ++it) {
        const QString &fileName = it.value().toString();

        if (!ProductKeyCheck::instance()->isValidLicenseTextFile(fileName))
            continue;

        QFileInfo fileInfo(directory, fileName);
        foreach (const QString &lang, QLocale().uiLanguages()) {
            if (QLocale(lang).language() == QLocale::English) // we assume English is the default language
                break;

            QList<QFileInfo> fileCandidates;
            foreach (const QString &locale, QInstaller::localeCandidates(lang.toLower())) {
                fileCandidates << QFileInfo(QString::fromLatin1("%1%2_%3.%4").arg(
                                                directory, fileInfo.baseName(), locale,
                                                fileInfo.completeSuffix()));
            }

            auto fInfo = std::find_if(fileCandidates.constBegin(), fileCandidates.constEnd(),
                                      [](const QFileInfo &file) {
                                           return file.exists();
                                       });
            if (fInfo != fileCandidates.constEnd()) {
                fileInfo = *fInfo;
                break;
            }
        }

        QFile file(fileInfo.filePath());
        if (!file.open(QIODevice::ReadOnly)) {
            throw Error(tr("Cannot open the requested license file \"%1\": %2").arg(
                            file.fileName(), file.errorString()));
        }
        QTextStream stream(&file);
        stream.setCodec("UTF-8");
        d->m_licenses.insert(it.key(), qMakePair(fileName, stream.readAll()));
    }
}


/*!
    \property Component::userInterfaces

    \brief A list of all user interface class names known to this component.
*/
QStringList Component::userInterfaces() const
{
    return d->m_userInterfaces.keys();
}

/*!
    Returns a hash that contains the file names and text of license files for the component.
*/
QHash<QString, QPair<QString, QString> > Component::licenses() const
{
    return d->m_licenses;
}

/*!
    Returns the QWidget created for \a name or \c 0 if the widget has been deleted or cannot
    be found.

    \sa {component::userInterface}{component.userInterface}
*/
QWidget *Component::userInterface(const QString &name) const
{
    return d->m_userInterfaces.value(name).data();
}

/*!
    Creates all operations needed to install this component's \a path. \a path is a fully qualified
    filename including the component's name. This method gets called from
    createOperationsForArchive. You can override it by providing a method with
    the same name in the component script.

    \note RSA signature files are omitted by this method.
    \note If you call this method from a script, it will not call the script's method with the same
    name.

    The default implementation is recursively creating Copy and Mkdir operations for all files
    and folders within \a path.

    \sa {component::createOperationsForPath}{component.createOperationsForPath}
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
        addOperation(copy, QStringList() << fi.filePath() << target);
    } else if (fi.isDir()) {
        qApp->processEvents();
        static const QString mkdir = QString::fromLatin1("Mkdir");
        addOperation(mkdir, QStringList(target));

        QDirIterator it(fi.filePath());
        while (it.hasNext())
            createOperationsForPath(it.next());
    }
}

/*!
    Creates all operations needed to install this component's \a archive. This method gets called
    from createOperations. You can override this method by providing a method with the
    same name in the component script.

    \note If you call this method from a script, it will not call the script's method with the same
    name.

    The default implementation calls createOperationsForPath for everything contained in the archive.
    If \a archive is a compressed archive known to the installer system, an Extract operation is
    created, instead.

    \sa {component::createOperationsForArchive}{component.createOperationsForArchive}
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
        addOperation(QLatin1String("Extract"), QStringList() << archive << QLatin1String("@TargetDir@"));
    } else {
        createOperationsForPath(archive);
    }
}

/*!
    \sa {component::beginInstallation}{component.beginInstallation}
*/
void Component::beginInstallation()
{
    // the script can override this method
    d->scriptEngine()->callScriptMethod(d->m_scriptContext, QLatin1String("beginInstallation"));
}

/*!
    \sa {component::createOperations}{component.createOperations}
    \sa createOperationsForArchive()
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
    Registers the file or directory at \a path for being removed when this component gets uninstalled.
    In case of a directory, this will be recursive. If \a wipe is set to \c true, the directory will
    also be deleted if it contains changes made by the user after installation.

    \sa {component::registerPathForUninstallation}{component.registerPathForUninstallation}
*/
void Component::registerPathForUninstallation(const QString &path, bool wipe)
{
    d->m_pathsForUninstallation.append(qMakePair(path, wipe));
}

/*!
    Returns the list of paths previously registered for uninstallation with
    registerPathForUninstallation().
*/
QList<QPair<QString, bool> > Component::pathsForUninstallation() const
{
    return d->m_pathsForUninstallation;
}

/*!
    Contains the names of all archives known to this component. Even downloaded archives are mapped
    to the \c{installer://} URL through the used QFileEngineHandler during the download process.
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
    Adds the archive \a path to this component. This can only be called if this component was
    downloaded from an online repository. When adding \a path, it will be downloaded from the
    repository when the installation starts.

    \sa {component::addDownloadableArchive}{component.addDownloadableArchive}
    \sa removeDownloadableArchive(), fromOnlineRepository, archives
*/
void Component::addDownloadableArchive(const QString &path)
{
    Q_ASSERT(isFromOnlineRepository());

    qDebug() << "addDownloadable" << path;
    d->m_downloadableArchives.append(d->m_vars.value(scVersion) + path);
}

/*!
    Removes the archive \a path previously added via addDownloadableArchive() from this component.
    This can only be called if this component was downloaded from an online repository.

    \sa {component::removeDownloadableArchive}{component.removeDownloadableArchive}
    \sa addDownloadableArchive(), fromOnlineRepository, archives
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
    Adds a request for quitting the process \a process before installing, updating, or uninstalling
    the component.

    \sa {component::addStopProcessForUpdateRequest}{component.addStopProcessForUpdateRequest}
*/
void Component::addStopProcessForUpdateRequest(const QString &process)
{
    d->m_stopProcessForUpdateRequests.append(process);
}

/*!
    Removes the request for quitting the process \a process again.

    \sa {component::removeStopProcessForUpdateRequest}{component.removeStopProcessForUpdateRequest}
*/
void Component::removeStopProcessForUpdateRequest(const QString &process)
{
    d->m_stopProcessForUpdateRequests.removeAll(process);
}

/*!
    A convenience function for adding or removing the request for stopping \a process depending on
    whether \a requested is \c true (add) or \c false (remove).

    \sa {component::setStopProcessForUpdateRequest}{component.addStopProcessForUpdateReques}
*/
void Component::setStopProcessForUpdateRequest(const QString &process, bool requested)
{
    if (requested)
        addStopProcessForUpdateRequest(process);
    else
        removeStopProcessForUpdateRequest(process);
}

/*!
    The list of processes that need to be closed before installing, updating, or uninstalling this
    component.
*/
QStringList Component::stopProcessForUpdateRequests() const
{
    return d->m_stopProcessForUpdateRequests;
}

/*!
    Returns the operations needed to install this component. If autoCreateOperations() is \c true,
    createOperations() is called if no operations have been automatically created yet.
*/
OperationList Component::operations() const
{
    if (d->m_autoCreateOperations && !d->m_operationsCreated) {
        const_cast<Component*>(this)->createOperations();

        if (!d->m_minimumProgressOperation) {
            d->m_minimumProgressOperation = KDUpdater::UpdateOperationFactory::instance()
                .create(QLatin1String("MinimumProgress"), d->m_core);
            d->m_minimumProgressOperation->setValue(QLatin1String("component"), name());
            d->m_operations.append(d->m_minimumProgressOperation);
        }

        if (!d->m_licenses.isEmpty()) {
            d->m_licenseOperation = KDUpdater::UpdateOperationFactory::instance()
                .create(QLatin1String("License"), d->m_core);
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
    addOperation(operation);
    operation->setValue(QLatin1String("admin"), true);
}

/*!
    Returns whether the operations needed to install this component were created successfully.
*/
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
    Operation *operation = KDUpdater::UpdateOperationFactory::instance().create(operationName,
        d->m_core);
    if (operation == 0) {
        const QMessageBox::StandardButton button =
            MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(),
            QLatin1String("OperationDoesNotExistError"), tr("Error"), tr("Error: Operation %1 does not exist.")
                .arg(operationName), QMessageBox::Abort | QMessageBox::Ignore);
        if (button == QMessageBox::Abort)
            d->m_operationsCreatedSuccessfully = false;
        return operation;
    }

    if (operation->name() == QLatin1String("Delete"))
        operation->setValue(QLatin1String("performUndo"), false);

    operation->setArguments(d->m_core->replaceVariables(parameters));
    operation->setValue(QLatin1String("component"), name());
    return operation;
}

void Component::markComponentUnstable()
{
    setValue(scDefault, scFalse);
    // Mark unstable component unchecked if:
    // 1. Installer, so the unstable component won't be installed
    // 2. Maintenancetool, when component is not installed.
    // 3. Updater, we don't want to update unstable components
    // Mark unstable component checked if:
    // 1. Maintenancetool, if component is installed and
    //    unstable so it won't get uninstalled.
    if (d->m_core->isInstaller() || !isInstalled() || d->m_core->isUpdater())
        setCheckState(Qt::Unchecked);
    setValue(scUnstable, scTrue);
}

namespace {

inline bool convert(QQmlV4Function *func, QStringList *toArgs)
{
    if (func->length() < 2)
        return false;

    QV4::Scope scope(func->v4engine());
    QV4::ScopedValue val(scope);
    val = (*func)[0];

    *toArgs << val->toQString();
    for (int i = 1; i < func->length(); i++) {
        val = (*func)[i];
        if (val->isObject() && val->as<QV4::Object>()->isArrayObject()) {
            QV4::ScopedValue valtmp(scope);
            QV4::Object *array = val->as<QV4::Object>();
            uint length = array->getLength();
            for (uint ii = 0; ii < length; ++ii) {
                valtmp = array->getIndexed(ii);
                *toArgs << valtmp->toQStringNoThrow();
            }
        } else {
            *toArgs << val->toQString();
        }
    }
    return true;
}

}
/*!
    \internal
*/
bool Component::addOperation(QQmlV4Function *func)
{
    QStringList args;
    if (convert(func, &args))
        return addOperation(args[0], args.mid(1));
    return false;
}

/*!
    Creates and adds an installation operation for \a operation. Add any number of \a parameters.
    The variables that the parameters contain, such as \c @TargetDir@, are replaced with their
    values.

    \sa {component::addOperation}{component.addOperation}
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
    \internal
*/
bool Component::addElevatedOperation(QQmlV4Function *func)
{
    QStringList args;
    if (convert(func, &args))
        return addElevatedOperation(args[0], args.mid(1));
    return false;
}

/*!
    Creates and adds the installation operation \a operation. Add any number of \a parameters.
    The variables that the parameters contain, such as \c @TargetDir@, are replaced with their
    values. The operation is executed with elevated rights.

    \sa {component::addElevatedOperation}{component.addElevatedOperation}
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
    would be done by calling createOperations(). If you set this to \c false, it is completely up
    to the component's script to create all operations.

    \sa {component::autoCreateOperations}{component.autoCreateOperations}
*/
bool Component::autoCreateOperations() const
{
    return d->m_autoCreateOperations;
}

void Component::setAutoCreateOperations(bool autoCreateOperations)
{
    d->m_autoCreateOperations = autoCreateOperations;
}

/*!
    Returns whether this component is virtual.
*/
bool Component::isVirtual() const
{
    return d->m_vars.value(scVirtual, scFalse).toLower() == scTrue;
}

/*!
   Returns whether the component is selected.
*/
bool Component::isSelected() const
{
    return checkState() != Qt::Unchecked;
}

/*!
    Returns whether this component should always be installed.
*/
bool Component::forcedInstallation() const
{
    return d->m_vars.value(scForcedInstallation, scFalse).toLower() == scTrue;
}

/*!
    Sets the validator callback name to \a name.
*/
void Component::setValidatorCallbackName(const QString &name)
{
    validatorCallbackName = name;
}

/*!
    Calls the script method with the validator callback name. Returns \c true if the method returns
    \c true. Always returns \c true if the validator callback name is empty.
*/
bool Component::validatePage()
{
    if (!validatorCallbackName.isEmpty())
        return d->scriptEngine()->callScriptMethod(d->m_scriptContext, validatorCallbackName).toBool();
    return true;
}

/*!
    Adds the component specified by \a newDependency to the list of dependencies.

    \sa {component::addDependency}{component.addDependency}
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

QStringList Component::dependencies() const
{
    return d->m_vars.value(scDependencies).split(QInstaller::commaRegExp(), QString::SkipEmptyParts);
}

/*!
    Adds the component specified by \a newDependOn to the automatic depend-on list.

    \sa {component::addAutoDependOn}{component.addAutoDependOn}
    \sa dependencies
*/

void Component::addAutoDependOn(const QString &newDependOn)
{
    QString oldDependOn = d->m_vars.value(scAutoDependOn);
    if (oldDependOn.isEmpty())
        setValue(scAutoDependOn, newDependOn);
    else
        setValue(scAutoDependOn, oldDependOn + QLatin1String(", ") + newDependOn);
}

QStringList Component::autoDependencies() const
{
    return d->m_vars.value(scAutoDependOn).split(QInstaller::commaRegExp(), QString::SkipEmptyParts);
}

/*!
    \sa {component::setInstalled}{component.setInstalled}
*/
void Component::setInstalled()
{
    setValue(scCurrentState, scInstalled);
}

/*!
    Determines whether the component comes as an auto dependency. Returns \c true if all components
    in \a componentsToInstall are already installed or selected for installation and this component
    thus needs to be installed as well.

    \sa {component::isAutoDependOn}{component.isAutoDependOn}
*/
bool Component::isAutoDependOn(const QSet<QString> &componentsToInstall) const
{
    // If there is no auto depend on value or the value is empty, we have nothing todo. The component does
    // not need to be installed as an auto dependency.
    QStringList autoDependOnList = autoDependencies();
    if (autoDependOnList.isEmpty())
        return false;

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

bool Component::isDefault() const
{
     if (isVirtual())
         return false;

    // the script can override this method
    if (d->m_vars.value(scDefault).compare(scScript, Qt::CaseInsensitive) == 0) {
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

bool Component::isInstalled(const QString version) const
{
    if (version.isEmpty()) {
        return scInstalled == d->m_vars.value(scCurrentState);
    } else {
        return d->m_vars.value(scInstalledVersion) == version;
    }
}

/*!
   Returns whether the user wants to install the component.

   \sa {component::installationRequested}{component.installationRequested}
*/
bool Component::installationRequested() const
{
    return installAction() == Install;
}

/*!
   Returns whether the component is selected for installation.
*/
bool Component::isSelectedForInstallation() const
{
    return !isInstalled() && isSelected();
}

/*!
    Sets the \a isUpdateAvailable flag to \c true to indicate that the core found an update.

   \sa {component::setUpdateAvailable}{component.setUpdateAvailable}
*/
void Component::setUpdateAvailable(bool isUpdateAvailable)
{
    d->m_updateIsAvailable = isUpdateAvailable;
}

/*!
    Returns whether the user wants to install the update for this component.

    \sa {component::updateRequested}{component.updateRequested}
*/
bool Component::updateRequested()
{
    return d->m_updateIsAvailable && isSelected() && !isUnstable();
}

/*!
    Returns \c true if that component will be changed (update, installation, or uninstallation).

    \sa {component::componentChangeRequested}{component.componentChangeRequested}
*/
bool Component::componentChangeRequested()
{
    return updateRequested() || isSelectedForInstallation() || uninstallationRequested();
}


/*!
    \sa {component::setUninstalled}{component.setUninstalled}
*/
void Component::setUninstalled()
{
    setValue(scCurrentState, scUninstalled);
}

/*!
    Returns whether the component is uninstalled.

    \sa {component::isUninstalled}{component.isUninstalled}
*/
bool Component::isUninstalled() const
{
    return scUninstalled == d->m_vars.value(scCurrentState);
}

bool Component::isUnstable() const
{
    return scTrue == d->m_vars.value(scUnstable);
}

void Component::setUnstable(PackageManagerCore::UnstableError error, const QString &errorMessage)
{
    QList<Component*> dependencies = d->m_core->dependees(this);
    // Mark this component unstable
    markComponentUnstable();

    // Marks all components unstable that depend on the unstable component
    foreach (Component *dependency, dependencies) {
        dependency->markComponentUnstable();
        foreach (Component *descendant, dependency->descendantComponents()) {
            descendant->markComponentUnstable();
        }
    }

    // Marks all child components unstable
    foreach (Component *descendant, this->descendantComponents()) {
        descendant->markComponentUnstable();
    }
    QMetaEnum metaEnum = QMetaEnum::fromType<PackageManagerCore::UnstableError>();
    emit packageManagerCore()->unstableComponentFound(QLatin1String(metaEnum.valueToKey(error)), errorMessage, this->name());
}

/*!
    Returns whether the user wants to uninstall the component.

    \sa {component::uninstallationRequested}{component.uninstallationRequested}
*/
bool Component::uninstallationRequested() const
{
    if (packageManagerCore()->isUpdater())
        return false;
    return isInstalled() && !isSelected();
}

/*!
    Returns whether this component has been loaded from an online repository.

    \sa {component::isFromOnlineRepository}{component.isFromOnlineRepository}
    \sa addDownloadableArchive(), fromOnlineRepository
*/
bool Component::isFromOnlineRepository() const
{
    return !repositoryUrl().isEmpty();
}

/*!
    Contains the repository URL this component is downloaded from.
    If this component is not downloaded from an online repository, returns an empty QUrl.
*/
QUrl Component::repositoryUrl() const
{
    return d->m_repositoryUrl;
}

/*!
    Sets this component's repository URL as \a url.
*/
void Component::setRepositoryUrl(const QUrl &url)
{
    d->m_repositoryUrl = url;
}

/*!
    Returns the path to the local directory where the component is temporarily stored.
*/
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
        setData(data.toLower() == scTrue
                ? d->m_core->virtualComponentsFont()
                : QFont(), Qt::FontRole);
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
        QString tooltipText
                = QString::fromLatin1("<html><body>%1</body></html>").arg(d->m_vars.value(scDescription));
        if (isUnstable()) {
            tooltipText += QLatin1String("<br>") + tr("There was an error loading the selected component. "
                                                          "This component can not be installed.");
        }
        setData(tooltipText, Qt::ToolTipRole);
    } else {
        QString tooltipText
                = d->m_vars.value(scDescription) + QLatin1String("<br><br>")
                + tr("Update Info: ") + updateInfo;
        if (isUnstable()) {
            tooltipText += QLatin1String("<br>") + tr("There was an error loading the selected component. "
                                                          "This component can not be updated.");
        }

        setData(tooltipText, Qt::ToolTipRole);
    }
}

/*!
    Returns the debugging output stream, \a dbg, for the component \a component.
*/
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
