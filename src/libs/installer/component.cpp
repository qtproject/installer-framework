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
#include "component.h"
#include "scriptengine.h"

#include "errors.h"
#include "fileutils.h"
#include "globals.h"
#include "archivefactory.h"
#include "messageboxhandler.h"
#include "packagemanagercore.h"
#include "remoteclient.h"
#include "settings.h"
#include "utils.h"
#include "constants.h"

#include "updateoperationfactory.h"

#include <productkeycheck.h>

#include <QtCore/QDirIterator>
#include <QtCore/QTranslator>
#include <QtCore/QRegularExpression>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QtCore/QTextCodec>
#endif

#include <QApplication>
#include <QtConcurrentFilter>

#include <QtUiTools/QUiLoader>

#if QT_VERSION >= QT_VERSION_CHECK(5,14,0)
#include <private/qv4engine_p.h>
#else
#include <private/qv8engine_p.h>
#endif
#include <private/qv4scopedvalue_p.h>
#include <private/qv4object_p.h>

#include <algorithm>
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
#include <QJSEngine>
#else
#include <QQmlEngine>
#endif

using namespace QInstaller;

/*!
    \enum QInstaller::Component::UnstableError

    This enum type holds the component unstable error reason:

    \value  DepencyToUnstable
            Component has a dependency to an unstable component.
    \value  ShaMismatch
            Component has packages with non-matching SHA values.
    \value  ScriptLoadingFailed
            Component script has errors or loading fails.
    \value  MissingDependency
            Component has dependencies to missing components.
    \value  InvalidTreeName
            Component has an invalid tree name.
    \value  DescendantOfUnstable
            Component is descendant of an unstable component.
*/

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::Component::SortingPriorityLessThan
    \brief The SortingPriorityLessThan class sets an increasing sorting order for child components.

    If the component contains several children and has this sorting priority set, the child list
    is sorted so that the child component with the lowest priority is placed on top.
*/

/*!
    \fn QInstaller::Component::SortingPriorityLessThan::operator() (const Component *lhs, const Component *rhs) const

    Returns \c true if \a lhs is less than \a rhs; otherwise returns \c false.
*/

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::Component::SortingPriorityGreaterThan
    \brief The SortingPriorityGreaterThan class sets a decreasing sorting priority for child
    components.

    If the component contains several children and has this sorting priority set, the child list
    is sorted so that the child component with the highest priority is placed on top.
*/

/*!
    \fn QInstaller::Component::SortingPriorityGreaterThan::operator() (const Component *lhs, const Component *rhs) const

    Returns \c true if \a lhs is greater than \a rhs; otherwise returns \c false.
*/

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::Component
    \brief The Component class represents the current component.
*/

/*!
    \property QInstaller::Component::name

    \brief The name of the component as set in the \c <Name> tag of the package
    information file.
*/

/*!
    \property QInstaller::Component::displayName

    \brief The name of the component as shown in the user interface.
*/

/*!
    \property QInstaller::Component::autoCreateOperations

    \brief Whether some standard operations for the component should be
    automatically created when the installation starts.

    The default is \c true.
*/

/*!
    \property QInstaller::Component::archives

    \brief The list of archive URLs registered for the component.

    The URLs are prefixed with \c {installer://}.

    \sa addDownloadableArchive(), removeDownloadableArchive()
*/

/*!
    \property QInstaller::Component::dependencies

    \brief The components this component depends on. This is a read-only property.
*/

/*!
    \property QInstaller::Component::autoDependencies

    \brief The value of the \c <AutoDependOn> element in the package information file.
*/

/*!
    \property QInstaller::Component::fromOnlineRepository

    \brief Whether this component has been loaded from an online repository.
*/

/*!
    \property QInstaller::Component::repositoryUrl

    \brief The repository URL the component is downloaded from.

    If this component is not downloaded from an online repository, returns an empty QUrl.
*/

/*!
    \property QInstaller::Component::default

    \brief Whether the component is a default one. This is a read-only property.

    \note Always \c false for virtual components.
*/

/*!
    \property QInstaller::Component::installed

    \brief Whether the component is installed.  This is a read-only property.
*/

/*!
    \property QInstaller::Component::enabled

    \brief Whether the component is currently enabled. The property is both readable and writable.
*/

/*!
    \property QInstaller::Component::unstable

    \brief Whether the component is unstable. This is a read-only property.

    \note Unstable components cannot be selected for installation.
*/

/*!
    \property QInstaller::Component::treeName

    \brief The tree name of the component. Specifies the location of the component in the install tree view.

    \note If the tree name is not set, the tree view is organized based on the component name.

    \note Must be unique, the tree name must not conflict with other component's names or tree names.
*/

/*!
    \fn QInstaller::Component::loaded()

    \sa {component::loaded}{component.loaded}
*/

/*!
    \fn QInstaller::Component::valueChanged(const QString &key, const QString &value)

    Emitted when the value of the variable with the name \a key changes to \a value.

    \sa {component::valueChanged}{component.valueChanged}, setValue()
*/

/*!
    \fn QInstaller::Component::virtualStateChanged()

    \sa {component::virtualStateChanged}{component.virtualStateChanged}
*/


/*!
    Creates a new component in the package manager specified by \a core.
*/
Component::Component(PackageManagerCore *core)
    : d(new ComponentPrivate(core, this))
    , m_defaultArchivePath(scTargetDirPlaceholder)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    QJSEngine::setObjectOwnership(this, QJSEngine::CppOwnership);
#else
    QQmlEngine::setObjectOwnership(this, QQmlEngine::CppOwnership);
#endif
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
    setValue(scLastUpdateDate, package.lastUpdateDate.toString());
    setValue(scInstallDate, package.installDate.toString());
    setValue(scUncompressedSize, QString::number(package.uncompressedSize));
    setValue(scDependencies, package.dependencies.join(scCommaWithSpace));
    setValue(scAutoDependOn, package.autoDependencies.join(scCommaWithSpace));
    setValue(scSortingPriority, QString::number(package.sortingPriority));

    setValue(scForcedInstallation, package.forcedInstallation ? scTrue : scFalse);
    setValue(scVirtual, package.virtualComp ? scTrue : scFalse);
    setValue(scCurrentState, scInstalled);
    setValue(scCheckable, package.checkable ? scTrue : scFalse);
    setValue(scExpandedByDefault, package.expandedByDefault ? scTrue : scFalse);
    setValue(scContentSha1, package.contentSha1);

    setValue(scTreeName, package.treeName.first);
    d->m_treeNameMoveChildren = package.treeName.second;

    // scDependencies might be updated from repository later,
    // keep the local dependencies as well.
    setValue(scLocalDependencies, value(scDependencies));
}

/*!
    Sets variables according to the values set in the package.xml file of \a package.
    Also loads UI files, licenses and translations if they are referenced in the package.xml.
    If the \c PackageManagerCore object of this component is run as package viewer, then
    only sets the variables without loading referenced files.
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
    setValue(scForcedUpdate, package.data(scForcedUpdate).toString());
    setValue(scUpdateText, package.data(scUpdateText).toString());
    setValue(scNewComponent, package.data(scNewComponent).toString());
    setValue(scRequiresAdminRights, package.data(scRequiresAdminRights).toString());
    d->m_scriptHash = package.data(scScriptTag).toHash();
    setValue(scReplaces, package.data(scReplaces).toString());
    setValue(scReleaseDate, package.data(scReleaseDate).toString());
    setValue(scCheckable, package.data(scCheckable).toString());
    setValue(scExpandedByDefault, package.data(scExpandedByDefault).toString());

    QString forced = package.data(scForcedInstallation, scFalse).toString().toLower();
    if (PackageManagerCore::noForceInstallation())
        forced = scFalse;
    setValue(scForcedInstallation, forced);
    setValue(scContentSha1, package.data(scContentSha1).toString());
    setValue(scCheckSha1CheckSum, package.data(scCheckSha1CheckSum, scTrue).toString().toLower());

    const auto treeNamePair = package.data(scTreeName).value<QPair<QString, bool>>();
    setValue(scTreeName, treeNamePair.first);
    d->m_treeNameMoveChildren = treeNamePair.second;

    if (d->m_core->isPackageViewer())
        return;

    setLocalTempPath(QInstaller::pathFromUrl(package.packageSource().url));

    const QStringList uiList = QInstaller::splitStringWithComma(package.data(scUserInterfaces).toString());
    if (!uiList.isEmpty())
        loadUserInterfaces(QDir(scTwoArgs.arg(localTempPath(), name())), uiList);

#ifndef IFW_DISABLE_TRANSLATIONS
    const QStringList qms = QInstaller::splitStringWithComma(package.data(scTranslations).toString());
    if (!qms.isEmpty())
        loadTranslations(QDir(scTwoArgs.arg(localTempPath(), name())), qms);
#endif
    QHash<QString, QVariant> licenseHash = package.data(scLicenses).toHash();
    if (!licenseHash.isEmpty())
        loadLicenses(scTwoArgs.arg(localTempPath(), name()), licenseHash);
    QVariant operationsVariant = package.data(scOperations);
    if (operationsVariant.canConvert<QList<QPair<QString, QVariant>>>())
        m_operationsList = operationsVariant.value<QList<QPair<QString, QVariant>>>();
}

/*!
    Returns the size of a compressed archive.
*/
quint64 Component::updateUncompressedSize()
{
    quint64 size = 0;

    const bool installOrKeepInstalled = (installAction() == ComponentModelHelper::Install
        || installAction() == ComponentModelHelper::KeepInstalled);

    if (installOrKeepInstalled)
        size = d->m_vars.value(scUncompressedSize).toLongLong();

    foreach (Component* comp, d->m_allChildComponents)
        size += comp->updateUncompressedSize();

    setValue(scUncompressedSizeSum, QString::number(size));
    if (size == 0 && !installOrKeepInstalled)
        setData(QVariant(), UncompressedSize);
    else
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
    Removes all the values that have the \a key from the variables set for this component.
    Returns the number of values removed which is 1 if the key exists in the variables,
    and 0 otherwise.
*/
int Component::removeValue(const QString &key)
{
    return d->m_vars.remove(key);
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
    if (key == scDefault && d->m_core->noDefaultInstallation())
        normalizedValue = scFalse;

    if (key == scName)
        d->m_componentName = normalizedValue;
    if (key == scCheckable) // Non-checkable components can still be toggled in updater
        this->setCheckable(normalizedValue.toLower() == scTrue || d->m_core->isUpdater());
    if (key == scExpandedByDefault)
        this->setExpandedByDefault(normalizedValue.toLower() == scTrue);
    if (key == scForcedInstallation) {
        if (value == scTrue && !d->m_core->isUpdater() && !PackageManagerCore::noForceInstallation()) {
            // Forced installation components can still be toggled in updater or when
            // core is set to ignore forced installations.
            setCheckable(false);
            setCheckState(Qt::Checked);
        }
    }
    if (key == scAutoDependOn)
        packageManagerCore()->createAutoDependencyHash(name(), d->m_vars[key], normalizedValue);
    if (key == scLocalDependencies)
        packageManagerCore()->createLocalDependencyHash(name(), normalizedValue);

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
    Returns this component's location in the tree view. If the tree name is not
    set, returns the component name. The tree name must be unique, it must not
    conflict with other tree names or component names.
*/
QString Component::treeName() const
{
    const QString defaultValue = d->m_vars.value(scAutoTreeName, name());
    return d->m_vars.value(scTreeName, defaultValue);
}

/*!
    Returns \c true if descendants of this component should have automatically
    created tree names in relation to the parent component's modified location,
    \c false otherwise.
*/
bool Component::treeNameMoveChildren() const
{
    return d->m_treeNameMoveChildren;
}

/*!
    Loads the component script into the script engine. Call this method with
    \a postLoad \c true to a list of components that are updated or installed
    to improve performance if the amount of components is huge and there are no script
    functions that need to be called before the installation starts.
*/
void Component::loadComponentScript(const bool postLoad)
{
    const QString installScript(!postLoad ? d->m_scriptHash.value(scInstallScript).toString()
                                      : d->m_scriptHash.value(scPostLoadScript).toString());

    if (!localTempPath().isEmpty() && !installScript.isEmpty()) {
        evaluateComponentScript(scThreeArgs.arg(localTempPath(), name()
                        , installScript), postLoad);
    }
}

/*!
    \internal
*/
void Component::evaluateComponentScript(const QString &fileName, const bool postScriptContent)
{
    // introduce the component object as javascript value and call the name to check that it
    // was successful
    try {
        if (postScriptContent) {
            d->m_postScriptContext = d->scriptEngine()->loadInContext(scComponent, fileName,
                scComponentScriptTest.arg(name()));
        } else {
            d->m_scriptContext = d->scriptEngine()->loadInContext(scComponent, fileName,
                scComponentScriptTest.arg(name()));
        }
    } catch (const Error &error) {
        qCWarning(QInstaller::lcDeveloperBuild) << error.message();
        setUnstable(Component::UnstableError::ScriptLoadingFailed, error.message());
        // evaluateComponentScript is called with postScriptContent after we have selected components
        // and are about to install. Do not allow install if unstable components are allowed
        // as we then end up installing a component which has invalid script.
        if (!packageManagerCore()->settings().allowUnstableComponents() || postScriptContent)
            throw error;
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
    callScriptMethod(scRetranslateUi);
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
    const QString uiLanguage = QLocale().uiLanguages().value(0, scEn);
    while (it.hasNext()) {
        const QString filename = it.next();
        const QString basename = QFileInfo(filename).baseName();

        if (!translations.isEmpty()) {
            bool found = false;
            foreach (const QString &translation, translations)
                found |= translation.startsWith(scIfw_ + basename, Qt::CaseInsensitive);
            if (!found) // don't load the file if it does match the UI language but is not allowed to be used
                continue;
        } else if (!uiLanguage.startsWith(QFileInfo(filename).baseName(), Qt::CaseInsensitive)) {
            continue; // do not load the file if it does not match the UI language
        }

        std::unique_ptr<QTranslator> translator(new QTranslator(this));
        if (translator->load(filename)) {
            // Do not throw if translator returns false as it may just be an intentionally
            // empty file. See also QTBUG-31031
            qApp->installTranslator(translator.release());
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
            throw Error(tr("Cannot open the requested UI file \"%1\": %2.\n\n%3 \"%4\"").arg(
                            it.fileName(), file.errorString(), tr(scClearCacheHint), packageManagerCore()->settings().localCachePath()));
        }

        QUiLoader *const loader = ProductKeyCheck::instance()->uiLoader();
        loader->setTranslationEnabled(true);
        loader->setLanguageChangeEnabled(true);
        QWidget *const widget = loader->load(&file, 0);
        if (!widget) {
            throw Error(tr("Cannot load the requested UI file \"%1\": %2.\n\n%3 \"%4\"").arg(
                it.fileName(), loader->errorString(), tr(scClearCacheHint), packageManagerCore()->settings().localCachePath()));
        }
        d->scriptEngine()->newQObject(widget);
        d->m_userInterfaces.insert(widget->objectName(), widget);
    }
}

/*!
  Loads the text of the licenses contained in \a licenseHash from \a directory.
  This is saved into a new hash containing the filename, the text and the priority of that file.
*/
void Component::loadLicenses(const QString &directory, const QHash<QString, QVariant> &licenseHash)
{
    QHash<QString, QVariant>::const_iterator it;
    for (it = licenseHash.begin(); it != licenseHash.end(); ++it) {
        QVariantMap license = it.value().toMap();
        const QString &fileName = license.value(scFile).toString();

        if (!ProductKeyCheck::instance()->isValidLicenseTextFile(fileName))
            continue;

        QFileInfo fileInfo(directory, fileName);
        foreach (const QString &lang, QLocale().uiLanguages()) {
            if (QLocale(lang).language() == QLocale::English) // we assume English is the default language
                break;

            QList<QFileInfo> fileCandidates;
            foreach (const QString &locale, QInstaller::localeCandidates(lang)) {
                fileCandidates << QFileInfo(scLocalesArgs.arg(
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
            throw Error(tr("Cannot open the requested license file \"%1\": %2.\n\n%3 \"%4\"").arg(
                            file.fileName(), file.errorString(), tr(scClearCacheHint), packageManagerCore()->settings().localCachePath()));
        }
        QTextStream stream(&file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        stream.setCodec("UTF-8");
#endif
        license.insert(scContent, stream.readAll());
        d->m_licenses.insert(it.key(), license);
    }
}

/*!
  Loads all operations defined in the component.xml except Extract operation.
  Operations are added to the list of operations needed to install this component.
*/
void Component::loadXMLOperations()
{
    for (auto operation: qAsConst(m_operationsList)) {
        if (operation.first != scExtract)
           addOperation(operation.first, operation.second.toStringList());
    }
}

/*!
  Loads all Extract operations defined in the component.xml.
  Operations are overwriting the default implementation of Extract operation.
*/
void Component::loadXMLExtractOperations()
{
    for (auto &operation: qAsConst(m_operationsList)) {
        if (operation.first == scExtract) {
            // Create hash for Extract operations. Operation has a mandatory extract folder as
            // first argument and optional archive name as second argument.
            const QStringList &operationArgs = operation.second.toStringList();
            if (operationArgs.count() == 2) {
                const QString archiveName = value(scVersion) + operationArgs.at(1);
                const QString archivePath = scInstallerPrefixWithTwoArgs.arg(name()).arg(archiveName);
                m_archivesHash.insert(archivePath, operationArgs.at(0));
            } else if (operationArgs.count() == 1) {
                m_defaultArchivePath = operationArgs.at(0);
            }
        }
    }
}

/*!
    \property QInstaller::Component::userInterfaces

    \brief A list of all user interface class names known to this component.
*/
QStringList Component::userInterfaces() const
{
    return d->m_userInterfaces.keys();
}

/*!
    Returns a hash that contains the file names, text and priorities of license files for the component.
*/
QHash<QString, QVariantMap> Component::licenses() const
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
    if (fi.suffix() == scSha1 && QFileInfo(fi.dir(), fi.completeBaseName()).exists())
        return;

    // the script can override this method
    if (!callScriptMethod(scCreateOperationsForPath, QJSValueList() << path).isUndefined())
        return;

    QString target;
    static const QString prefix = scInstallerPrefix;
    target = scTargetDirPlaceholderWithArg.arg(path.mid(prefix.length() + name().length()));

    if (fi.isFile()) {
        static const QString copy = scCopy;
        addOperation(copy, QStringList() << fi.filePath() << target);
    } else if (fi.isDir()) {
        qApp->processEvents();
        static const QString mkdir = scMkdir;
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
    if (fi.suffix() == scSha1 && QFileInfo(fi.dir(), fi.completeBaseName()).exists())
        return;

    // the script can override this method
    if (!callScriptMethod(scCreateOperationsForArchive, QJSValueList() << archive).isUndefined())
        return;

    QScopedPointer<AbstractArchive> archiveFile(ArchiveFactory::instance().create(archive));
    const bool isZip = (archiveFile && archiveFile->open(QIODevice::ReadOnly) && archiveFile->isSupported());

    if (isZip) {
        // component.xml can override this value
        if (m_archivesHash.contains(archive))
            addOperation(scExtract, QStringList() << archive << m_archivesHash.value(archive));
        else
            addOperation(scExtract, QStringList() << archive << m_defaultArchivePath);
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
    callScriptMethod(scBeginInstallation);
}

/*!
    \sa {component::createOperations}{component.createOperations}
    \sa createOperationsForArchive()
*/
void Component::createOperations()
{
    // the script can override this method
    if (!callScriptMethod(scCreateOperations).isUndefined()) {
        d->m_operationsCreated = true;
        return;
    }
    loadXMLExtractOperations();
    foreach (const QString &archive, archives())
        createOperationsForArchive(archive);

    loadXMLOperations();
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
    static const QRegularExpression regExp(scCaretSymbol);
    QString pathString = scInstallerPrefixWithOneArgs.arg(name());
    QStringList archivesNameList = QDir(pathString).entryList();

    // In resources we may have older version of archives, this can happen
    // when there is offline installer with same component with lower version
    // number and newer version is available online
    archivesNameList = archivesNameList.filter(value(scVersion));

    //RegExp "^" means line beginning
    archivesNameList.replaceInStrings(regExp, pathString);
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
    qCDebug(QInstaller::lcDeveloperBuild) << "addDownloadable" << path;
    d->m_downloadableArchives.append(d->m_vars.value(scVersion) + path);
}

/*!
    \internal
*/
void Component::addDownloadableArchives(const QString& archives)
{
    Q_ASSERT(isFromOnlineRepository());
    d->m_downloadableArchivesVariable = archives;
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
    Should be called only once when the installation starts.
*/
QStringList Component::downloadableArchives()
{
    const QStringList downloadableArchives = d->m_downloadableArchivesVariable
                .split(QInstaller::commaRegExp(), Qt::SkipEmptyParts);
    foreach (const QString downloadableArchive, downloadableArchives)
        addDownloadableArchive(downloadableArchive);

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

    The \a mask parameter filters the returned operations by their group.
*/
OperationList Component::operations(const Operation::OperationGroups &mask) const
{
    if (d->m_autoCreateOperations && !d->m_operationsCreated) {
        const_cast<Component*>(this)->createOperations();

        if (!d->m_minimumProgressOperation) {
            d->m_minimumProgressOperation = KDUpdater::UpdateOperationFactory::instance()
                .create(scMinimumProgress, d->m_core);
            d->m_minimumProgressOperation->setValue(scComponentSmall, name());
            d->m_operations.append(d->m_minimumProgressOperation);
        }

        if (!d->m_licenses.isEmpty()) {
            d->m_licenseOperation = KDUpdater::UpdateOperationFactory::instance()
                .create(scLicense, d->m_core);
            d->m_licenseOperation->setValue(scComponentSmall, name());

            QVariantMap licenses;
            const QList<QVariantMap> values = d->m_licenses.values();
            for (int i = 0; i < values.count(); ++i) {
                licenses.insert(values.at(i).value(scFile).toString(),
                        values.at(i).value(scContent));
            }
            d->m_licenseOperation->setValue(scLicensesValue, licenses);
            d->m_operations.append(d->m_licenseOperation);
        }
    }
    OperationList operations;
    std::copy_if(d->m_operations.begin(), d->m_operations.end(), std::back_inserter(operations),
        [&](const Operation *op) {
            return mask.testFlag(op->group());
        }
    );
    return operations;
}

/*!
    Adds \a operation to the list of operations needed to install this component.
*/
void Component::addOperation(Operation *operation)
{
    d->m_operations.append(operation);
    if (RemoteClient::instance().isActive())
        operation->setValue(scAdmin, true);
}

/*!
    Adds \a operation to the list of operations needed to install this component. \a operation
    is executed with elevated rights.
*/
void Component::addElevatedOperation(Operation *operation)
{
    addOperation(operation);
    operation->setValue(scAdmin, true);
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
                .arg(operationName), QMessageBox::Abort | QMessageBox::Ignore, QMessageBox::Abort);
        if (button == QMessageBox::Abort)
            d->m_operationsCreatedSuccessfully = false;
        return operation;
    }

    // Operation can contain variables which are resolved when performing the operation
    if (operation->requiresUnreplacedVariables())
        operation->setArguments(parameters);
    else
        operation->setArguments(d->m_core->replaceVariables(parameters));


    operation->setValue(scComponentSmall, name());
    return operation;
}

void Component::markComponentUnstable(Component::UnstableError error, const QString &errorMessage)
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
    QMetaEnum metaEnum = QMetaEnum::fromType<Component::UnstableError>();
    emit packageManagerCore()->unstableComponentFound(QLatin1String(metaEnum.valueToKey(error)), errorMessage, this->name());

    // Update the description and tooltip texts to contain
    // information about the unstable error.
    updateModelData(scDescription, QString());
}

QJSValue Component::callScriptMethod(const QString &methodName, const QJSValueList &arguments) const
{
    QJSValue scriptContext;
    if (!d->m_postScriptContext.isUndefined() && d->m_postScriptContext.property(methodName).isCallable())
        scriptContext = d->m_postScriptContext;
    else
        scriptContext = d->m_scriptContext;
    return d->scriptEngine()->callScriptMethod(scriptContext,
            methodName, arguments);
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
                valtmp = array->get(ii);
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

    Returns \c true if the operation is created; otherwise returns \c false.

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

    Returns \c true if the operation is created; otherwise returns \c false.

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

    Returns \c false when component's script will create all operations; otherwise returns \c true.

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
    Returns whether this component is essential. Essential components
    are always installed, and updated before other components.
*/
bool Component::isEssential() const
{
    return d->m_vars.value(scEssential, scFalse).toLower() == scTrue;
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
        return callScriptMethod(validatorCallbackName).toBool();
    return true;
}

/*!
    Adds the component specified by \a newDependency to the list of dependencies.
    Alternatively, multiple components can be specified by separating each with
    a comma.

    \sa {component::addDependency}{component.addDependency}
    \sa dependencies
*/

void Component::addDependency(const QString &newDependency)
{
    QString oldDependencies = d->m_vars.value(scDependencies);
    if (oldDependencies.isEmpty())
        setValue(scDependencies, newDependency);
    else
        setValue(scDependencies, oldDependencies + scCommaWithSpace + newDependency);
}

/*!
    Returns a list of dependencies defined in the the repository or in the package.xml.
*/
QStringList Component::dependencies() const
{
    return QInstaller::splitStringWithComma(d->m_vars.value(scDependencies));
}

/*!
    Returns a list of installed components dependencies defined in the components.xml.
*/
QStringList Component::localDependencies() const
{
    return QInstaller::splitStringWithComma(d->m_vars.value(scLocalDependencies));
}

/*!
    Adds the component specified by \a newDependOn to the automatic depend-on list.
    Alternatively, multiple components can be specified by separating each with
    a comma.

    \sa {component::addAutoDependOn}{component.addAutoDependOn}
    \sa autoDependencies
*/

void Component::addAutoDependOn(const QString &newDependOn)
{
    QString oldDependOn = d->m_vars.value(scAutoDependOn);
    if (oldDependOn.isEmpty())
        setValue(scAutoDependOn, newDependOn);
    else
        setValue(scAutoDependOn, oldDependOn + scCommaWithSpace + newDependOn);
}

QStringList Component::autoDependencies() const
{
    return d->m_vars.value(scAutoDependOn).split(QInstaller::commaRegExp(), Qt::SkipEmptyParts);
}

/*!
    Returns a list of dependencies that the component currently has. The
    dependencies can vary when component is already installed with different
    dependency list than what is introduced in the repository. If component is
    not installed, or update is requested to an installed component,
    current dependencies are read from repository so that correct dependencies
    are calculated for the component when it is installed or updated.
*/
QStringList Component::currentDependencies() const
{
    QStringList dependenciesList;
    if (isInstalled() && !updateRequested())
        dependenciesList = localDependencies();
    else
        dependenciesList = dependencies();
    return dependenciesList;
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

    // If there is an essential update and autodepend on is not for essential
    // update component, do not add the autodependency to an installed component as
    // essential updates needs to be installed first, otherwise non-essential components
    // will be installed
    if (packageManagerCore()->foundEssentialUpdate()) {
        const QSet<QString> autoDependOnSet(autoDependOnList.begin(), autoDependOnList.end());
        if (componentsToInstall.contains(autoDependOnSet)) {
            foreach (const QString &autoDep, autoDependOnSet) {
                Component *component = packageManagerCore()->componentByName(autoDep);
                if ((component->value(scEssential, scFalse).toLower() == scTrue)
                        || component->isForcedUpdate()) {
                    return true;
                }
            }
        }
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

bool Component::isDefault() const
{
     if (isVirtual())
         return false;

    // the script can override this method
    if (d->m_vars.value(scDefault).compare(scScript, Qt::CaseInsensitive) == 0) {
        QJSValue valueFromScript;
        try {
            valueFromScript = callScriptMethod(scIsDefault);
        } catch (const Error &error) {
            MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(),
                QLatin1String("isDefaultError"), tr("Cannot resolve isDefault in %1").arg(name()),
                    error.message());
            return false;
        }
        if (!valueFromScript.isError())
            return valueFromScript.toBool();
        qCWarning(QInstaller::lcDeveloperBuild) << "Value from script is not valid."
            << (valueFromScript.toString().isEmpty()
            ? QString::fromLatin1("Unknown error.") : valueFromScript.toString());
        return false;
    }

    return d->m_vars.value(scDefault).compare(scTrue, Qt::CaseInsensitive) == 0;
}

bool Component::isInstalled(const QString &version) const
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
    Returns whether update is available for this component.

    \sa {component::isUpdateAvailable}{component.isUpdateAvailable}
*/
bool Component::isUpdateAvailable() const
{
    return d->m_updateIsAvailable && !isUnstable();
}

/*!
    Returns whether the user wants to install the update for this component.

    \sa {component::updateRequested}{component.updateRequested}
*/
bool Component::updateRequested() const
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
    Returns \c true if the component is installed and has a \c ForcedUpdate flag set.
    ForcedUpdate components will be updated together with essential components before
    any other component can be updated or installed.

    \sa {component::isForcedUpdate}{component.isForcedUpdate}
*/
bool Component::isForcedUpdate()
{
    return isInstalled() && (value(scForcedUpdate, scFalse).toLower() == scTrue);
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

/*!
    Returns \c true if this component is unstable.
*/

bool Component::isUnstable() const
{
    return scTrue == d->m_vars.value(scUnstable);
}

/*!
    Sets this component, all its child components and all the components depending on
    this component unstable with the error code \a error and message \a errorMessage.
*/
void Component::setUnstable(Component::UnstableError error, const QString &errorMessage)
{
    QList<Component*> dependencies = d->m_core->dependees(this);
    // Mark this component unstable
    markComponentUnstable(error, errorMessage);

    // Marks all components unstable that depend on the unstable component
    foreach (Component *dependency, dependencies) {
        dependency->markComponentUnstable(UnstableError::DepencyToUnstable,
               QLatin1String("Dependent on unstable component"));
        foreach (Component *descendant, dependency->descendantComponents()) {
            descendant->markComponentUnstable(UnstableError::DescendantOfUnstable,
                    QLatin1String("Descendant of unstable component"));
        }
    }

    // Marks all child components unstable
    foreach (Component *descendant, this->descendantComponents()) {
        descendant->markComponentUnstable(UnstableError::DescendantOfUnstable,
                QLatin1String("Descendant of unstable component"));
    }
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

    if (key == scUpdateText || key == scDescription) {
        QString tooltipText;
        const QString &updateInfo = d->m_vars.value(scUpdateText);
        if (!d->m_core->isUpdater() || updateInfo.isEmpty()) {
            tooltipText = QString::fromLatin1("<html><body>%1</body></html>").arg(d->m_vars.value(scDescription));
        } else {
            tooltipText = d->m_vars.value(scDescription) + scBr + scBr
                          + tr("Update Info: ") + updateInfo;
        }
        if (isUnstable()) {
            tooltipText += scBr + tr("There was an error loading the selected component. "
                                                      "This component cannot be installed.");
        }
        static const QRegularExpression externalLinkRegexp(QLatin1String("{external-link}='(.*?)'"));
        static const QLatin1String externalLinkElement(QLatin1String("<a href=\"\\1\">\\1</a>"));
        // replace {external-link}='' fields in component description with proper link tags
        tooltipText.replace(externalLinkRegexp, externalLinkElement);

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
