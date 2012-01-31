/**************************************************************************
**
** This file is part of Qt SDK**
**
** Copyright (c) 2012 Nokia Corporation and/or its subsidiary(-ies).*
**
** Contact:  Nokia Corporation qt-info@nokia.com**
**
** No Commercial Usage
**
** This file contains pre-release code and may not be distributed.
** You may use this file in accordance with the terms and conditions
** contained in the Technology Preview License Agreement accompanying
** this package.
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception version
** 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** If you are unsure which license is appropriate for your use, please contact
** (qt-info@nokia.com).
**
**************************************************************************/
#include "component.h"

#include "common/errors.h"
#include "common/fileutils.h"
#include "fsengineclient.h"
#include "lib7z_facade.h"
#include "packagemanagercore.h"
#include "qinstallerglobal.h"
#include "messageboxhandler.h"

#include <kdupdaterupdatesourcesinfo.h>
#include <kdupdaterupdateoperationfactory.h>

#include <QtCore/QDirIterator>
#include <QtCore/QTranslator>

#include <QtGui/QApplication>

#include <QtUiTools/QUiLoader>

#include <algorithm>

using namespace QInstaller;

static const QLatin1String scScript("Script");
static const QLatin1String scDefault("Default");
static const QLatin1String scAutoDependOn("AutoDependOn");
static const QLatin1String scVirtual("Virtual");
static const QLatin1String scInstalled("Installed");
static const QLatin1String scUpdateText("UpdateText");
static const QLatin1String scUninstalled("Uninstalled");
static const QLatin1String scCurrentState("CurrentState");
static const QLatin1String scForcedInstallation("ForcedInstallation");

static QRegExp scCommaRegExp(QLatin1String("\\b(,|, )\\b"));

/*!
    \class QInstaller::Component
    Component describes a component within the installer.
*/

/*!
    Constructor. Creates a new Component inside of \a installer.
*/
Component::Component(PackageManagerCore *core)
    : d(new ComponentPrivate(core, this))
{
    setPrivate(d);

    connect(this, SIGNAL(valueChanged(QString, QString)), this, SLOT(updateModelData(QString, QString)));
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
    Q_ASSERT(!package.name().isEmpty());

    setValue(scName, package.data(scName).toString());
    setValue(scDisplayName, package.data(scDisplayName).toString());
    setValue(scDescription, package.data(scDescription).toString());
    setValue(scDefault, package.data(scDefault).toString());
    setValue(scAutoDependOn, package.data(scAutoDependOn).toString());
    setValue(scCompressedSize, QString::number(package.compressedSize()));
    setValue(scUncompressedSize, QString::number(package.uncompressedSize()));
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

    setValue(scScript, package.data(scScript).toString());
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

    setLocalTempPath(QInstaller::pathFromUrl(package.sourceInfo().url));
    const QStringList uis = package.data(QLatin1String("UserInterfaces")).toString().split(scCommaRegExp,
        QString::SkipEmptyParts);
    if (!uis.isEmpty())
        loadUserInterfaces(QDir(QString::fromLatin1("%1/%2").arg(localTempPath(), name())), uis);

    const QStringList qms = package.data(QLatin1String("Translations")).toString().split(scCommaRegExp,
        QString::SkipEmptyParts);
    if (!qms.isEmpty())
        loadTranslations(QDir(QString::fromLatin1("%1/%2").arg(localTempPath(), name())), qms);

    QHash<QString, QVariant> licenseHash = package.data(QLatin1String("Licenses")).toHash();
    if (!licenseHash.isEmpty())
        loadLicenses(QString::fromLatin1("%1/%2/").arg(localTempPath(), name()), licenseHash);
}

quint64 Component::updateUncompressedSize()
{
    quint64 size = 0;

    if (isSelected())
        size = (quint64)value(scUncompressedSize).toDouble();

    foreach (Component* comp, d->m_allChildComponents)
        size += comp->updateUncompressedSize();

    setValue(scUncompressedSizeSum, QString::number(size));
    setData(uncompressedSize(), UncompressedSize);

    return size;
}

QString Component::uncompressedSize() const
{
    double size = value(scUncompressedSizeSum).toDouble();
    if (size < 1000.0)
        return tr("%L1 Bytes").arg(size);
    size /= 1024.0;
    if (size < 1000.0)
        return tr("%L1 kBytes").arg(size, 0, 'f', 2);
    size /= 1024.0;
    if (size < 1000.0)
        return tr("%L1 MBytes").arg(size, 0, 'f', 2);
    size /= 1024.0;

    return tr("%L1 GBytes").arg(size, 0, 'f', 2);
}

void Component::markAsPerformedInstallation()
{
    d->m_newlyInstalled = true;
}

/*!
    \property Component::removeBeforeUpdate
    Specifies whether this component gets removed by the installer system before it gets updated. Get this
    property's value by using %removeBeforeUpdate(), and set it using %setRemoveBeforeUpdate(). The default
    value is true.
*/
bool Component::removeBeforeUpdate() const
{
    return d->m_removeBeforeUpdate;
}

void Component::setRemoveBeforeUpdate(bool removeBeforeUpdate)
{
    d->m_removeBeforeUpdate = removeBeforeUpdate;
}

/*
    Returns a key/value based hash of all variables set for this component.
*/
QHash<QString,QString> Component::variables() const
{
    return d->m_vars;
}

/*!
    Returns the value of variable name \a key.
    If \a key is not known yet, \a defaultValue is returned.
*/
QString Component::value(const QString &key, const QString &defaultValue) const
{
    return d->m_vars.value(key, defaultValue);
}

/*!
    Sets the value of the variable with \a key to \a value.
*/
void Component::setValue(const QString &key, const QString &value)
{
    if (d->m_vars.value(key) == value)
        return;

    if (key == scName)
        d->m_componentName = value;

    d->m_vars[key] = value;
    emit valueChanged(key, value);
}

/*!
    Returns the installer this component belongs to.
*/
PackageManagerCore *Component::packageManagerCore() const
{
    return d->m_core;
}

/*!
    Returns the parent of this component. If this component is com.nokia.sdk.qt, its
    parent is com.nokia.sdk, as far as this exists.
*/
Component *Component::parentComponent() const
{
    return d->m_parentComponent;
}

/*!
    Appends \a component as a child of this component. If \a component already has a parent,
    it is removed from the previous parent.
*/
void Component::appendComponent(Component *component)
{
    if (!component->isVirtual()) {
        d->m_childComponents.append(component);
        std::sort(d->m_childComponents.begin(), d->m_childComponents.end(), Component::SortingPriorityLessThan());
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
    Returns a list of child components. If \a recursive is set to true, the returned list
    contains not only the direct children, but all ancestors. Note: The returned list does include ALL
    children, non virtual components as well as virtual components.
*/
QList<Component*> Component::childComponents(bool recursive, RunMode runMode) const
{
    QList<Component*> result;
    if (runMode == UpdaterMode)
        return result;

    if (!recursive)
        return d->m_allChildComponents;

    foreach (Component *component, d->m_allChildComponents) {
        result.append(component);
        result += component->childComponents(true, runMode);
    }
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
    return value(scDisplayName);
}

void Component::loadComponentScript()
{
    const QString script = value(scScript);
    if (!localTempPath().isEmpty() && !script.isEmpty())
        loadComponentScript(QString::fromLatin1("%1/%2/%3").arg(localTempPath(), name(), script));
}

/*!
    Loads the script at \a fileName into this component's script engine. The installer and all its
    components as well as other useful stuff are being exported into the script.
    Read \link componentscripting Component Scripting \endlink for details.
    \throws Error when either the script at \a fileName couldn't be opened, or the QScriptEngine
    couldn't evaluate the script.
*/
void Component::loadComponentScript(const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        throw Error(tr("Could not open the requested script file at %1: %2.").arg(fileName, file.errorString()));
    }

    d->scriptEngine()->evaluate(QLatin1String(file.readAll()), fileName);
    if (d->scriptEngine()->hasUncaughtException()) {
        throw Error(tr("Exception while loading the component script: %1")
            .arg(uncaughtExceptionString(d->scriptEngine()/*, QFileInfo(file).absoluteFilePath()*/)));
    }

    const QList<Component*> components = d->m_core->availableComponents();
    QScriptValue comps = d->scriptEngine()->newArray(components.count());
    for (int i = 0; i < components.count(); ++i)
        comps.setProperty(i, d->scriptEngine()->newQObject(components[i]));

    d->scriptEngine()->globalObject().property(QLatin1String("installer"))
        .setProperty(QLatin1String("components"), comps);

    QScriptValue comp = d->scriptEngine()->evaluate(QLatin1String("Component"), fileName);
    if (!d->scriptEngine()->hasUncaughtException()) {
        d->m_scriptComponent = comp;
        d->m_scriptComponent.construct();
    }

    //evaluate("Component") and construct can have an exception
    if (d->scriptEngine()->hasUncaughtException()) {
        throw Error(tr("Exception while loading the component script: %1")
            .arg(uncaughtExceptionString(d->scriptEngine(), QFileInfo(file).absoluteFilePath())));
    }

    emit loaded();
    languageChanged();

    //Solves a freeze seen on updater/ package manger restart.
    QCoreApplication::processEvents();
}

/*!
    \internal
    Calls the script method \link retranslateUi() \endlink, if any. This is done whenever a
    QTranslator file is being loaded.
*/
void Component::languageChanged()
{
    callScriptMethod(QLatin1String("retranslateUi"));
}

/*!
    Tries to call the method with \a name within the script and returns the result. If the method
    doesn't exist, an invalid result is returned. If the method has an uncaught exception, its
    string representation is thrown as an Error exception.

    \note The method is not called, if the current script context is the same method, to avoid
    infinite recursion.
*/
QScriptValue Component::callScriptMethod(const QString &methodName, const QScriptValueList &arguments) const
{
    if (!d->m_unexistingScriptMethods.value(methodName, true))
        return QScriptValue();

    // don't allow such a recursion
    if (d->scriptEngine()->currentContext()->backtrace().first().startsWith(methodName))
        return QScriptValue();

    QScriptValue method = d->m_scriptComponent.property(QString::fromLatin1("prototype"))
        .property(methodName);
    if (!method.isValid()) // this marks the method to be called not any longer
        d->m_unexistingScriptMethods[methodName] = false;

    const QScriptValue result = method.call(d->m_scriptComponent, arguments);
    if (!result.isValid())
        return result;

    if (d->scriptEngine()->hasUncaughtException())
        throw Error(uncaughtExceptionString(d->scriptEngine()/*, name()*/));

    return result;
}

/*!
    Loads the translations matching the name filters \a qms inside \a directory. Only translations
    with a \link QFileInfo::baseName() baseName \endlink matching the current locales \link
    QLocale::name() name \endlink are loaded.
    Read \ref componenttranslation for details.
*/
void Component::loadTranslations(const QDir &directory, const QStringList &qms)
{
    QDirIterator it(directory.path(), qms, QDir::Files);
    while (it.hasNext()) {
        const QString filename = it.next();
        if (QFileInfo(filename).baseName().toLower() != QLocale().name().toLower())
            continue;

        QScopedPointer<QTranslator> translator(new QTranslator(this));
        if (!translator->load(filename))
            throw Error(tr("Could not open the requested translation file at %1").arg(filename));
        qApp->installTranslator(translator.take());
    }
}

/*!
    Loads the user interface files matching the name filters \a uis inside \a directory. The loaded
    interface can be accessed via userInterfaces by using the class name set in the ui file.
    Read \ref componentuserinterfaces for details.
*/
void Component::loadUserInterfaces(const QDir &directory, const QStringList &uis)
{
    if (QApplication::type() == QApplication::Tty)
        return;

    QDirIterator it(directory.path(), uis, QDir::Files);
    while (it.hasNext()) {
        QFile file(it.next());
        if (!file.open(QIODevice::ReadOnly)) {
            throw Error(tr("Could not open the requested UI file at %1: %2").arg(it.fileName(),
                file.errorString()));
        }

        static QUiLoader loader;
        loader.setTranslationEnabled(true);
        loader.setLanguageChangeEnabled(true);
        QWidget *const w = loader.load(&file, MessageBoxHandler::currentBestSuitParent());
        d->m_userInterfaces.insert(w->objectName(), w);
    }
}


void Component::loadLicenses(const QString &directory, const QHash<QString, QVariant> &licenseHash)
{
    QHash<QString, QVariant>::const_iterator it;
    for (it = licenseHash.begin(); it != licenseHash.end(); ++it) {
        const QString &fileName = it.value().toString();
        QFile file(directory + fileName);
        if (!file.open(QIODevice::ReadOnly)) {
            throw Error(tr("Could not open the requested license file at %1: %2").arg(fileName,
                file.errorString()));
        }
        d->m_licenses.insert(it.key(), qMakePair(fileName, QTextStream(&file).readAll()));
    }
}

/*!
    Contains a list of all user interface class names known to this component.
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
    Returns the QWidget created for \a name or 0 if the widget already has been deleted or cannot be found.
*/
QWidget *Component::userInterface(const QString &name) const
{
    return d->m_userInterfaces.value(name).data();
}

/*!
    Creates all operations needed to install this component's \a path. \a path is a full qualified
    filename including the component's name. This methods gets called from
    Component::createOperationsForArchive. You can override this method by providing a method with
    the same name in the component script.

    \note RSA signature files are omitted by this method.
    \note If you call this method from a script, it won't call the scripts method with the same name.

    The default implementation is recursively creating Copy and Mkdir operations for all files
    and folders within \a path.
*/
void Component::createOperationsForPath(const QString &path)
{
    const QFileInfo fi(path);

    // don't copy over a signature
    if (fi.suffix() == QLatin1String("sig") && QFileInfo(fi.dir(), fi.completeBaseName()).exists())
        return;

    // the script can override this method
    if (callScriptMethod(QLatin1String("createOperationsForPath"), QScriptValueList() << path).isValid())
        return;

    QString target;
    static const QString zipPrefix = QString::fromLatin1("7z://installer://");
    // if the path is an archive, remove the archive file name from the target path
    if (path.startsWith(zipPrefix)) {
        target = path.mid(zipPrefix.length() + name().length() + 1); // + 1 for the /
        const int nextSlash = target.indexOf(QLatin1Char('/'));
        if (nextSlash != -1)
            target = target.mid(nextSlash);
        else
            target.clear();
        target.prepend(QLatin1String("@TargetDir@"));
    } else {
        static const QString prefix = QString::fromLatin1("installer://");
        target = QString::fromLatin1("@TargetDir@%1").arg(path.mid(prefix.length() + name().length()));
    }

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
    Creates all operations needed to install this component's \a archive. This method gets called
    from Component::createOperations. You can override this method by providing a method with the
    same name in the component script.

    \note If you call this method from a script, it won't call the scripts method with the same name.

    The default implementation calls createOperationsForPath for everything contained in the archive.
    If \a archive is a compressed archive known to the installer system, an Extract operation is
    created, instead.
*/
void Component::createOperationsForArchive(const QString &archive)
{
    // the script can override this method
    if (callScriptMethod(QLatin1String("createOperationsForArchive"), QScriptValueList() << archive).isValid())
        return;

    const QFileInfo fi(QString::fromLatin1("installer://%1/%2").arg(name(), archive));
    const bool isZip = Lib7z::isSupportedArchive(fi.filePath());

    if (isZip) {
        // archives get completely extracted per default (if the script isn't doing other stuff)
        addOperation(QLatin1String("Extract"), fi.filePath(), QLatin1String("@TargetDir@"));
    } else {
        createOperationsForPath(fi.filePath());
    }
}

void Component::beginInstallation()
{
    // the script can override this method
    if (callScriptMethod(QLatin1String("beginInstallation")).isValid()) {
        return;
    }
}


/*!
    Creates all operations needed to install this component.
    You can override this method by providing a method with the same name in the component script.

    \note If you call this method from a script, it won't call the scripts method with the same name.

    The default implementation calls createOperationsForArchive for all archives in this component.
*/
void Component::createOperations()
{
    // the script can override this method
    if (callScriptMethod(QLatin1String("createOperations")).isValid()) {
        d->m_operationsCreated = true;
        return;
    }

    foreach (const QString &archive, archives())
        createOperationsForArchive(archive);

    d->m_operationsCreated = true;
}

/*!
    Registers the file or directory at \a path for being removed when this component gets uninstalled.
    In case of a directory, this will be recursive. If \a wipe is set to true, the directory will
    also be deleted if it contains changes done by the user after installation.
*/
void Component::registerPathForUninstallation(const QString &path, bool wipe)
{
    d->m_pathesForUninstallation.append(qMakePair(path, wipe));
}

/*!
    Returns the list of paths previously registered for uninstallation with
    #registerPathForUninstallation.
*/
QList<QPair<QString, bool> > Component::pathesForUninstallation() const
{
    return d->m_pathesForUninstallation;
}

/*!
    Contains the names of all archives known to this component. This does not contain archives added
    with #addDownloadableArchive.
*/
QStringList Component::archives() const
{
    return QDir(QString::fromLatin1("installer://%1/").arg(name())).entryList();
}

/*!
    Adds the archive \a path to this component. This can only be called when this component was
    downloaded from an online repository. When adding \a path, it will be downloaded from the
    repository when the installation starts.

    Read \ref sec_repogen for details. \sa fromOnlineRepository
*/
void Component::addDownloadableArchive(const QString &path)
{
    Q_ASSERT(isFromOnlineRepository());

    const QString versionPrefix = value(scRemoteVersion);
    qDebug() << "addDownloadable" << path;
    d->m_downloadableArchives.append(versionPrefix + path);
}

/*!
    Removes the archive \a path previously added via addDownloadableArchive from this component.
    This can only be called when this component was downloaded from an online repository.

    Read \ref sec_repogen for details.
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
    Adds a request for quitting the process @p process before installing/updating/uninstalling the
    component.
*/
void Component::addStopProcessForUpdateRequest(const QString &process)
{
    d->m_stopProcessForUpdateRequests.append(process);
}

/*!
    Removes the request for quitting the process @p process again.
*/
void Component::removeStopProcessForUpdateRequest(const QString &process)
{
    d->m_stopProcessForUpdateRequests.removeAll(process);
}

/*!
    Convenience: Add/remove request depending on @p requested (add if @p true, remove if @p false).
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
    Returns the operations needed to install this component. If autoCreateOperations is true,
    createOperations is called, if no operations have been auto-created yet.
*/
OperationList Component::operations() const
{
    if (d->m_autoCreateOperations && !d->m_operationsCreated) {
        const_cast<Component*>(this)->createOperations();

        if (!d->m_minimumProgressOperation) {
            d->m_minimumProgressOperation = KDUpdater::UpdateOperationFactory::instance()
                .create(QLatin1String("MinimumProgress"));
            d->m_operations.append(d->m_minimumProgressOperation);
        }

        if (!d->m_licenses.isEmpty()) {
            d->m_licenseOperation = KDUpdater::UpdateOperationFactory::instance()
                .create(QLatin1String("License"));
            d->m_licenseOperation->setValue(QLatin1String("installer"), QVariant::fromValue(d->m_core));

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
    if (FSEngineClientHandler::instance().isActive())
        operation->setValue(QLatin1String("admin"), true);
}

/*!
    Adds \a operation to the list of operations needed to install this component. \a operation
    is executed with elevated rights.
*/
void Component::addElevatedOperation(Operation *operation)
{
    if (value(scRequiresAdminRights, scFalse) != scTrue) {
        qWarning() << QString::fromLatin1("component %1 uses addElevatedOperation in the script, but it doesn't"
            "have the needed RequiresAdminRights tag").arg(name());
    }
    addOperation(operation);
    operation->setValue(QLatin1String("admin"), true);
}

bool Component::operationsCreatedSuccessfully() const
{
    return d->m_operationsCreatedSuccessfully;
}

Operation *Component::createOperation(const QString &operation, const QString &parameter1,
    const QString &parameter2, const QString &parameter3, const QString &parameter4, const QString &parameter5,
    const QString &parameter6, const QString &parameter7, const QString &parameter8, const QString &parameter9,
    const QString &parameter10)
{
    Operation *op = KDUpdater::UpdateOperationFactory::instance().create(operation);
    if (op == 0) {
        const QMessageBox::StandardButton button =
            MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(),
            QLatin1String("OperationDoesNotExistError"), tr("Error"), tr("Error: Operation %1 does not exist")
                .arg(operation), QMessageBox::Abort | QMessageBox::Ignore);
        if (button == QMessageBox::Abort)
            d->m_operationsCreatedSuccessfully = false;
        return op;
    }

    if (op->name() == QLatin1String("Delete"))
        op->setValue(QLatin1String("performUndo"), false);
    op->setValue(QLatin1String("installer"), qVariantFromValue(d->m_core));

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
    op->setArguments(d->m_core->replaceVariables(arguments));

    return op;
}
/*!
    Creates and adds an installation operation for \a operation. Add any number of \a parameter1,
    \a parameter2, \a parameter3, \a parameter4, \a parameter5 and \a parameter6. The contents of
    the parameters get variables like "@TargetDir@" replaced with their values, if contained.
    \sa installeroperations
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
    Creates and adds an installation operation for \a operation. Add any number of \a parameter1,
    \a parameter2, \a parameter3, \a parameter4, \a parameter5 and \a parameter6. The contents of
    the parameters get variables like "@TargetDir@" replaced with their values, if contained.
    \a operation is executed with elevated rights.
    \sa installeroperations
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
    Specifies whether operations should be automatically created when the installation starts. This
    would be done by calling #createOperations. If you set this to false, it's completely up to the
    component's script to create all operations.
*/
bool Component::autoCreateOperations() const
{
    return d->m_autoCreateOperations;
}

void Component::setAutoCreateOperations(bool autoCreateOperations)
{
    d->m_autoCreateOperations = autoCreateOperations;
}

bool Component::isVirtual() const
{
    return value(scVirtual, scFalse).toLower() == scTrue;
}

/*!
    \property Component::selected
    Specifies whether this component is selected for installation. Get this property's value by using
    %isSelected(), and set it using %setSelected().
*/
bool Component::isSelected() const
{
    return checkState() != Qt::Unchecked;
}

bool Component::forcedInstallation() const
{
    return value(scForcedInstallation, scFalse).toLower() == scTrue;
}

/*!
    Marks the component for installation. Emits the selectedChanged() signal if the check state changes.
*/
void Component::setSelected(bool selected)
{
    Q_UNUSED(selected)
    qDebug() << Q_FUNC_INFO << QString::fromLatin1("on \"%1\" is deprecated!!!").arg(d->m_componentName);
}

void Component::addDependency(const QString &newDependency)
{
    QString oldDependencies = value(scDependencies);
    if (oldDependencies.isEmpty())
        setValue(scDependencies, newDependency);
    else
        setValue(scDependencies, oldDependencies + QLatin1String(", ") + newDependency);
}


/*!
    Contains this component dependencies.
    Read \ref componentdependencies for details.
*/
QStringList Component::dependencies() const
{
    return value(scDependencies).split(scCommaRegExp, QString::SkipEmptyParts);
}

QStringList Component::autoDependencies() const
{
    QStringList autoDependencyStringList =
        value(scAutoDependOn).split(scCommaRegExp, QString::SkipEmptyParts);
    autoDependencyStringList.removeAll(QLatin1String("script"));
    return autoDependencyStringList;
}

/*!
    Set's the components state to installed.
*/
void Component::setInstalled()
{
    setValue(scCurrentState, scInstalled);
}

/*!
    Determines if the component comes as an auto dependency. Returns true if the component needs to be
    installed.
*/
bool Component::isAutoDependOn(const QSet<QString> &componentsToInstall) const
{
    // If there is no auto depend on value or the value is empty, we have nothing todo. The component does
    // not need to be installed as an auto dependency.
    QStringList autoDependOnList = autoDependencies();
    if (autoDependOnList.isEmpty())
        return false;

    // The script can override this method and determines if the component needs to be installed.
    if (autoDependOnList.first().compare(QLatin1String("script"), Qt::CaseInsensitive) == 0) {
        QScriptValue valueFromScript;
        try {
            valueFromScript = callScriptMethod(QLatin1String("isAutoDependOn"));
        } catch (const Error &error) {
            MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(),
                QLatin1String("isAutoDependOnError"), tr("Can't resolve isAutoDependOn in %1"
                    ).arg(name()), error.message());
            return false;
        }

        if (valueFromScript.isValid())
            return valueFromScript.toBool();
        qDebug() << "value from script is not valid";
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
    Determines if the component is a default one.
*/
bool Component::isDefault() const
{
    // the script can override this method
    if (value(scDefault).compare(QLatin1String("script"), Qt::CaseInsensitive) == 0) {
        QScriptValue valueFromScript;
        try {
            valueFromScript = callScriptMethod(QLatin1String("isDefault"));
        } catch (const Error &error) {
            MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(),
                QLatin1String("isDefaultError"), tr("Can't resolve isDefault in %1").arg(name()),
                    error.message());
            return false;
        }
        if (valueFromScript.isValid())
            return valueFromScript.toBool();
        qDebug() << "value from script is not valid";
        return false;
    }

    return value(scDefault).compare(scTrue, Qt::CaseInsensitive) == 0;
}

/*!
    Determines if the component is installed.
*/
bool Component::isInstalled() const
{
    return scInstalled == value(scCurrentState);
}

/*!
    Determines if the user wants to install the component
*/
bool Component::installationRequested() const
{
    return !isInstalled() && isSelected();
}

/*!
    Sets a flag that the core found an update
*/
void Component::setUpdateAvailable(bool isUpdateAvailable)
{
    d->m_updateIsAvailable = isUpdateAvailable;
}

/*!
    Determines if the user wants to install the update for this component
*/
bool Component::updateRequested()
{
    return d->m_updateIsAvailable && isSelected();
}

/*!
    Returns true if that component will be changed (update/installation/uninstallation)
*/
bool Component::componentChangeRequested()
{
    return updateRequested() || installationRequested() || uninstallationRequested();
}


/*!
    Sets the component state to uninstalled.
*/
void Component::setUninstalled()
{
    setValue(scCurrentState, scUninstalled);
}

/*!
    Determines if the component is uninstalled.
*/
bool Component::isUninstalled() const
{
    return scUninstalled == value(scCurrentState);
}

/*!
    Determines if the user wants to uninstall the component.
*/
bool Component::uninstallationRequested() const
{
    if (packageManagerCore()->isUpdater())
        return false;
    return isInstalled() && !isSelected();
}

/*!
    \property Component::fromOnlineRepository

    Determines whether this component has been loaded from an online repository. Get this property's
    value by using %isFromOnlineRepository. \sa addDownloadableArchive
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
    }

    if (key == scRemoteDisplayVersion)
        setData(data, RemoteDisplayVersion);

    if (key == scDisplayName)
        setData(data, Qt::DisplayRole);

    if (key == scDisplayVersion)
        setData(data, LocalDisplayVersion);

    if (key == scUncompressedSize)
        setData(uncompressedSize(), UncompressedSize);

    const QString &updateInfo = value(scUpdateText);
    if (!d->m_core->isUpdater() || updateInfo.isEmpty()) {
        setData(QLatin1String("<html><body>") + value(scDescription) + QLatin1String("</body></html>"),
            Qt::ToolTipRole);
    } else {
        setData(value(scDescription) + QLatin1String("<br><br>Update Info: ") + updateInfo, Qt::ToolTipRole);
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
