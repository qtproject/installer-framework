/**************************************************************************
**
** This file is part of Qt SDK**
**
** Copyright (c) 2011 Nokia Corporation and/or its subsidiary(-ies).*
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
#include "packagemanagercore_p.h"

#include "adminauthorization.h"
#include "common/binaryformat.h"
#include "common/errors.h"
#include "common/fileutils.h"
#include "common/utils.h"
#include "component.h"
#include "fsengineclient.h"
#include "messageboxhandler.h"
#include "packagemanagercore.h"
#include "progresscoordinator.h"
#include "qprocesswrapper.h"
#include "qsettingswrapper.h"

#include <KDToolsCore/KDSaveFile>
#include <KDToolsCore/KDSelfRestarter>

#include <KDUpdater/KDUpdater>

#include <QtCore/QtConcurrentRun>
#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QDirIterator>
#include <QtCore/QFuture>
#include <QtCore/QFutureWatcher>
#include <QtCore/QTemporaryFile>

#include <errno.h>

namespace QInstaller {

static bool runOperation(KDUpdater::UpdateOperation *op, PackageManagerCorePrivate::OperationType type)
{
    switch (type) {
        case PackageManagerCorePrivate::Backup:
            op->backup();
            return true;
        case PackageManagerCorePrivate::Perform:
            return op->performOperation();
        case PackageManagerCorePrivate::Undo:
            return op->undoOperation();
        default:
            Q_ASSERT(!"unexpected operation type");
    }
    return false;
}

/*!
    \internal
    Creates and initializes a FSEngineClientHandler -> makes us get admin rights for QFile operations
*/
static FSEngineClientHandler *sClientHandlerInstance = 0;
static FSEngineClientHandler *initFSEngineClientHandler()
{
    if (sClientHandlerInstance == 0) {
        sClientHandlerInstance = &FSEngineClientHandler::instance();

        // Initialize the created FSEngineClientHandler instance.
        const int port = 30000 + qrand() % 1000;
        sClientHandlerInstance->init(port);
        sClientHandlerInstance->setStartServerCommand(QCoreApplication::applicationFilePath(),
            QStringList() << QLatin1String("--startserver") << QString::number(port)
            << sClientHandlerInstance->authorizationKey(), true);
    }
    return sClientHandlerInstance;
}

static QStringList checkRunningProcessesFromList(const QStringList &processList)
{
    const QList<KDSysInfo::ProcessInfo> allProcesses = KDSysInfo::runningProcesses();
    QStringList stillRunningProcesses;
    foreach (const QString &process, processList) {
        if (!process.isEmpty() && PackageManagerCorePrivate::isProcessRunning(process, allProcesses))
            stillRunningProcesses.append(process);
    }
    return stillRunningProcesses;
}

static void deferredRename(const QString &oldName, const QString &newName, bool restart = false)
{
#ifdef Q_OS_WIN
    QStringList arguments;
    {
        QTemporaryFile f(QDir::temp().absoluteFilePath(QLatin1String("deferredrenameXXXXXX.vbs")));
        openForWrite(&f, f.fileName());
        f.setAutoRemove(false);

        arguments << QDir::toNativeSeparators(f.fileName()) << QDir::toNativeSeparators(oldName)
            << QDir::toNativeSeparators(QFileInfo(oldName).dir().absoluteFilePath(QFileInfo(newName)
            .fileName()));

        QTextStream batch(&f);
        batch << "Set fso = WScript.CreateObject(\"Scripting.FileSystemObject\")\n";
        batch << "Set tmp = WScript.CreateObject(\"WScript.Shell\")\n";
        batch << QString::fromLatin1("file = \"%1\"\n").arg(arguments[2]);
        batch << "on error resume next\n";

        batch << "while fso.FileExists(file)\n";
        batch << "    fso.DeleteFile(file)\n";
        batch << "    WScript.Sleep(1000)\n";
        batch << "wend\n";
        batch << QString::fromLatin1("fso.MoveFile \"%1\", file\n").arg(arguments[1]);
        if (restart)
            batch <<  QString::fromLatin1("tmp.exec \"%1 --updater\"\n").arg(arguments[2]);
        batch << "fso.DeleteFile(WScript.ScriptFullName)\n";
    }

    QProcessWrapper::startDetached(QLatin1String("cscript"), QStringList() << QLatin1String("//Nologo")
        << arguments[0]);
#else
        QFile::remove(newName);
        QFile::rename(oldName, newName);
        KDSelfRestarter::setRestartOnQuit(restart);
#endif
}


// -- PackageManagerCorePrivate

PackageManagerCorePrivate::PackageManagerCorePrivate(PackageManagerCore *core)
    : m_FSEngineClientHandler(0)
    , m_core(core)
    , m_repoFetched(false)
    , m_updateSourcesAdded(false)
{
}

PackageManagerCorePrivate::PackageManagerCorePrivate(PackageManagerCore *core, qint64 magicInstallerMaker,
        const QList<KDUpdater::UpdateOperation*> &performedOperations)
    : m_FSEngineClientHandler(initFSEngineClientHandler())
    , m_status(PackageManagerCore::Unfinished)
    , m_forceRestart(false)
    , m_testChecksum(false)
    , m_launchedAsRoot(AdminAuthorization::hasAdminRights())
    , m_completeUninstall(false)
    , m_needToWriteUninstaller(false)
    , m_performedOperationsOld(performedOperations)
    , m_core(core)
    , m_magicBinaryMarker(magicInstallerMaker)
    , m_repoFetched(false)
    , m_updateSourcesAdded(false)
{
    connect(this, SIGNAL(installationStarted()), m_core, SIGNAL(installationStarted()));
    connect(this, SIGNAL(installationFinished()), m_core, SIGNAL(installationFinished()));
    connect(this, SIGNAL(uninstallationStarted()), m_core, SIGNAL(uninstallationStarted()));
    connect(this, SIGNAL(uninstallationFinished()), m_core, SIGNAL(uninstallationFinished()));
}

PackageManagerCorePrivate::~PackageManagerCorePrivate()
{
    clearAllComponentLists();
    clearUpdaterComponentLists();

    qDeleteAll(m_ownedOperations);
    qDeleteAll(m_performedOperationsOld);
    qDeleteAll(m_performedOperationsCurrentSession);

    // check for fake installer case
    if (m_FSEngineClientHandler)
        m_FSEngineClientHandler->setActive(false);
}

/*!
    Return true, if a process with \a name is running. On Windows, comparison is case-insensitive.
*/
/* static */
bool PackageManagerCorePrivate::isProcessRunning(const QString &name,
    const QList<KDSysInfo::ProcessInfo> &processes)
{
    QList<KDSysInfo::ProcessInfo>::const_iterator it;
    for (it = processes.constBegin(); it != processes.constEnd(); ++it) {
        if (it->name.isEmpty())
            continue;

#ifndef Q_WS_WIN
        if (it->name == name)
            return true;
        const QFileInfo fi(it->name);
        if (fi.fileName() == name || fi.baseName() == name)
            return true;
#else
        if (it->name.toLower() == name.toLower())
            return true;
        if (it->name.toLower() == QDir::toNativeSeparators(name.toLower()))
            return true;
        const QFileInfo fi(it->name);
        if (fi.fileName().toLower() == name.toLower() || fi.baseName().toLower() == name.toLower())
            return true;
#endif
    }
    return false;
}

/* static */
bool PackageManagerCorePrivate::performOperationThreaded(KDUpdater::UpdateOperation *op, OperationType type)
{
    QFutureWatcher<bool> futureWatcher;
    const QFuture<bool> future = QtConcurrent::run(runOperation, op, type);

    QEventLoop loop;
    loop.connect(&futureWatcher, SIGNAL(finished()), SLOT(quit()), Qt::QueuedConnection);
    futureWatcher.setFuture(future);

    if (!future.isFinished())
        loop.exec();

    return future.result();
}

QString PackageManagerCorePrivate::targetDir() const
{
    return m_core->value(scTargetDir);
}

QString PackageManagerCorePrivate::configurationFileName() const
{
    return m_core->value(scTargetConfigurationFile, QString::fromLatin1("components.xml"));
}

QString PackageManagerCorePrivate::componentsXmlPath() const
{
    return QDir::toNativeSeparators(QDir(QDir::cleanPath(targetDir()))
        .absoluteFilePath(configurationFileName()));
}

void PackageManagerCorePrivate::clearAllComponentLists()
{
    qDeleteAll(m_rootComponents);
    m_rootComponents.clear();

    const QList<QPair<Component*, Component*> > list = m_componentsToReplaceAllMode.values();
    for (int i = 0; i < list.count(); ++i)
        delete list.at(i).second;
    m_componentsToReplaceAllMode.clear();
}

void PackageManagerCorePrivate::clearUpdaterComponentLists()
{
    qDeleteAll(m_updaterComponents);
    m_updaterComponents.clear();

    qDeleteAll(m_updaterComponentsDeps);
    m_updaterComponentsDeps.clear();

    const QList<QPair<Component*, Component*> > list = m_componentsToReplaceUpdaterMode.values();
    for (int i = 0; i < list.count(); ++i)
        delete list.at(i).second;
    m_componentsToReplaceUpdaterMode.clear();
}

QHash<QString, QPair<Component*, Component*> > &PackageManagerCorePrivate::componentsToReplace()
{
    return m_core->runMode() == AllMode ? m_componentsToReplaceAllMode : m_componentsToReplaceUpdaterMode;
}

void PackageManagerCorePrivate::initialize()
{
    try {
        m_settings = Settings(Settings::fromFileAndPrefix(QLatin1String(":/metadata/installer-config/config.xml"),
            QLatin1String(":/metadata/installer-config/")));
    } catch (const Error &e) {
        qCritical("Could not parse Config: %s", qPrintable(e.message()));
        // TODO: try better error handling
        return;
    }

    // first set some common variables that may used e.g. as placeholder
    // in some of the settings variables or in a script or...
    m_vars.insert(QLatin1String("rootDir"), QDir::rootPath());
    m_vars.insert(QLatin1String("homeDir"), QDir::homePath());

#ifdef Q_WS_WIN
    m_vars.insert(QLatin1String("os"), QLatin1String("win"));
#elif defined(Q_WS_MAC)
    m_vars.insert(QLatin1String("os"), QLatin1String("mac"));
#elif defined(Q_WS_X11)
    m_vars.insert(QLatin1String("os"), QLatin1String("x11"));
#elif defined(Q_WS_QWS)
    m_vars.insert(QLatin1String("os"), QLatin1String("Qtopia"));
#else
    // TODO: add more platforms as needed...
#endif

    // fill the variables defined in the settings
    m_vars.insert(QLatin1String("ProductName"), m_settings.applicationName());
    m_vars.insert(QLatin1String("ProductVersion"), m_settings.applicationVersion());
    m_vars.insert(scTitle, m_settings.title());
    m_vars.insert(scMaintenanceTitle, m_settings.maintenanceTitle());
    m_vars.insert(scPublisher, m_settings.publisher());
    m_vars.insert(QLatin1String("Url"), m_settings.url());
    m_vars.insert(scStartMenuDir, m_settings.startMenuDir());
    m_vars.insert(scTargetConfigurationFile, m_settings.configurationFileName());
    m_vars.insert(QLatin1String("LogoPixmap"), m_settings.logo());
    m_vars.insert(QLatin1String("LogoSmallPixmap"), m_settings.logoSmall());
    m_vars.insert(QLatin1String("WatermarkPixmap"), m_settings.watermark());

    m_vars.insert(scRunProgram, replaceVariables(m_settings.runProgram()));
    const QString desc = m_settings.runProgramDescription();
    if (!desc.isEmpty())
        m_vars.insert(scRunProgramDescription, desc);
#ifdef Q_WS_X11
    if (m_launchedAsRoot)
        m_vars.insert(scTargetDir, replaceVariables(m_settings.adminTargetDir()));
    else
#endif
        m_vars.insert(scTargetDir, replaceVariables(m_settings.targetDir()));
    m_vars.insert(scRemoveTargetDir, replaceVariables(m_settings.removeTargetDir()));

    QSettingsWrapper creatorSettings(QSettingsWrapper::IniFormat, QSettingsWrapper::UserScope,
        QLatin1String("Nokia"), QLatin1String("QtCreator"));
    QFileInfo info(creatorSettings.fileName());
    if (info.exists())
        m_vars.insert(QLatin1String("QtCreatorSettingsFile"), info.absoluteFilePath());

    if (!m_core->isInstaller()) {
#ifdef Q_WS_MAC
        readUninstallerIniFile(QCoreApplication::applicationDirPath() + QLatin1String("/../../.."));
#else
        readUninstallerIniFile(QCoreApplication::applicationDirPath());
#endif
    }

    foreach (KDUpdater::UpdateOperation *currentOperation, m_performedOperationsOld)
        currentOperation->setValue(QLatin1String("installer"), QVariant::fromValue(m_core));

    disconnect(this, SIGNAL(installationStarted()), ProgressCoordninator::instance(), SLOT(reset()));
    connect(this, SIGNAL(installationStarted()), ProgressCoordninator::instance(), SLOT(reset()));
    disconnect(this, SIGNAL(uninstallationStarted()), ProgressCoordninator::instance(), SLOT(reset()));
    connect(this, SIGNAL(uninstallationStarted()), ProgressCoordninator::instance(), SLOT(reset()));

    m_updaterApplication.packagesInfo()->setFileName(componentsXmlPath());
    m_updaterApplication.updateSourcesInfo()->setFileName(QLatin1String(""));

    m_updaterApplication.packagesInfo()->setApplicationName(m_settings.applicationName());
    m_updaterApplication.packagesInfo()->setApplicationVersion(m_settings.applicationVersion());
    if (isInstaller()) {
        m_updaterApplication.addUpdateSource(m_settings.applicationName(), m_settings.applicationName(),
            QString(), QUrl(QLatin1String("resource://metadata/")), 0);
        m_updaterApplication.updateSourcesInfo()->setModified(false);
    }

    if (!m_repoFetched) {
        m_repoMetaInfoJob.clear();
        m_repoMetaInfoJob =
            QSharedPointer<GetRepositoriesMetaInfoJob>(new GetRepositoriesMetaInfoJob(m_settings.publicKey()));
        connect(m_repoMetaInfoJob.data(), SIGNAL(infoMessage(KDJob*, QString)), m_core,
            SIGNAL(metaJobInfoMessage(KDJob*, QString)));
    }
}

QString PackageManagerCorePrivate::installerBinaryPath() const
{
    return qApp->applicationFilePath();
}

bool PackageManagerCorePrivate::isInstaller() const
{
    return m_magicBinaryMarker == MagicInstallerMarker;
}

bool PackageManagerCorePrivate::isUninstaller() const
{
    return m_magicBinaryMarker == MagicUninstallerMarker;
}

bool PackageManagerCorePrivate::isUpdater() const
{
    return m_magicBinaryMarker == MagicUpdaterMarker;
}

bool PackageManagerCorePrivate::isPackageManager() const
{
    return m_magicBinaryMarker == MagicPackageManagerMarker;
}

bool PackageManagerCorePrivate::statusCanceledOrFailed() const
{
    return m_status == PackageManagerCore::Canceled || m_status == PackageManagerCore::Failure;
}

void PackageManagerCorePrivate::setStatus(int status, const QString &error)
{
    m_error = error;
    if (m_status != status) {
        m_status = status;
        emit m_core->statusChanged(PackageManagerCore::Status(m_status));
    }
}

QString PackageManagerCorePrivate::replaceVariables(const QString &str) const
{
    static const QChar at = QLatin1Char('@');
    QString res;
    int pos = 0;
    while (true) {
        const int pos1 = str.indexOf(at, pos);
        if (pos1 == -1)
            break;
        const int pos2 = str.indexOf(at, pos1 + 1);
        if (pos2 == -1)
            break;
        res += str.mid(pos, pos1 - pos);
        const QString name = str.mid(pos1 + 1, pos2 - pos1 - 1);
        res += m_core->value(name);
        pos = pos2 + 1;
    }
    res += str.mid(pos);
    return res;
}

QByteArray PackageManagerCorePrivate::replaceVariables(const QByteArray &ba) const
{
    static const QChar at = QLatin1Char('@');
    QByteArray res;
    int pos = 0;
    while (true) {
        const int pos1 = ba.indexOf(at, pos);
        if (pos1 == -1)
            break;
        const int pos2 = ba.indexOf(at, pos1 + 1);
        if (pos2 == -1)
            break;
        res += ba.mid(pos, pos1 - pos);
        const QString name = QString::fromLocal8Bit(ba.mid(pos1 + 1, pos2 - pos1 - 1));
        res += m_core->value(name).toLocal8Bit();
        pos = pos2 + 1;
    }
    res += ba.mid(pos);
    return res;
}

/*!
    \internal
    Creates an update operation owned by the installer, not by any component.
 */
KDUpdater::UpdateOperation* PackageManagerCorePrivate::createOwnedOperation(const QString &type)
{
    m_ownedOperations.append(KDUpdater::UpdateOperationFactory::instance().create(type));
    return m_ownedOperations.last();
}

/*!
    \internal
    Removes \a opertion from the operations owned by the installer, returns the very same operation if the
    operation was found, otherwise 0.
 */
KDUpdater::UpdateOperation* PackageManagerCorePrivate::takeOwnedOperation(KDUpdater::UpdateOperation *operation)
{
    if (!m_ownedOperations.contains(operation))
        return 0;

    m_ownedOperations.removeAll(operation);
    return operation;
}

QString PackageManagerCorePrivate::uninstallerName() const
{
    QString filename = m_settings.uninstallerName();
#if defined(Q_WS_MAC)
    if (QFileInfo(QCoreApplication::applicationDirPath() + QLatin1String("/../..")).isBundle())
        filename += QLatin1String(".app/Contents/MacOS/") + filename;
#elif defined(Q_OS_WIN)
    filename += QLatin1String(".exe");
#endif
    return QString::fromLatin1("%1/%2").arg(targetDir()).arg(filename);
}

void PackageManagerCorePrivate::readUninstallerIniFile(const QString &targetDir)
{
    const QString iniPath = targetDir + QLatin1Char('/') + m_settings.uninstallerIniFile();
    QSettingsWrapper cfg(iniPath, QSettingsWrapper::IniFormat);
    const QVariantHash vars = cfg.value(QLatin1String("Variables")).toHash();
    QHash<QString, QVariant>::ConstIterator it = vars.constBegin();
    while (it != vars.constEnd()) {
        m_vars.insert(it.key(), it.value().toString());
        ++it;
    }

    QList<Repository> repositories;
    const QStringList list = cfg.value(scRepositories).toStringList();
    foreach (const QString &url, list)
        repositories.append(Repository(url));
    m_settings.addUserRepositories(repositories);
}

void PackageManagerCorePrivate::stopProcessesForUpdates(const QList<Component*> &components)
{
    QStringList processList;
    foreach (const Component* const i, components)
        processList << m_core->replaceVariables(i->stopProcessForUpdateRequests());

    qSort(processList);
    processList.erase(std::unique(processList.begin(), processList.end()), processList.end());
    if (processList.isEmpty())
        return;

    while (true) {
        const QList<KDSysInfo::ProcessInfo> allProcesses = KDSysInfo::runningProcesses();
        const QStringList processes = checkRunningProcessesFromList(processList);
        if (processes.isEmpty())
            return;

        const QMessageBox::StandardButton button =
            MessageBoxHandler::warning(MessageBoxHandler::currentBestSuitParent(),
            QLatin1String("stopProcessesForUpdates"), tr("Stop Processes"), tr("These processes "
            "should be stopped to continue:\n\n%1").arg(QDir::toNativeSeparators(processes
            .join(QLatin1String("\n")))), QMessageBox::Retry | QMessageBox::Ignore
            | QMessageBox::Cancel, QMessageBox::Retry);
        if (button == QMessageBox::Ignore)
            return;
        if (button == QMessageBox::Cancel) {
            m_core->setCanceled();
            throw Error(tr("Installation canceled by user"));
        }
    }
}

int PackageManagerCorePrivate::countProgressOperations(const QList<KDUpdater::UpdateOperation*> &operations)
{
    int operationCount = 0;
    foreach (KDUpdater::UpdateOperation *operation, operations) {
        if (QObject *operationObject = dynamic_cast<QObject*> (operation)) {
            const QMetaObject *const mo = operationObject->metaObject();
            if (mo->indexOfSignal(QMetaObject::normalizedSignature("progressChanged(double)")) > -1)
                operationCount++;
        }
    }
    return operationCount;
}

int PackageManagerCorePrivate::countProgressOperations(const QList<Component*> &components)
{
    int operationCount = 0;
    foreach (Component* component, components)
        operationCount += countProgressOperations(component->operations());

    return operationCount;
}

void PackageManagerCorePrivate::connectOperationToInstaller(KDUpdater::UpdateOperation *const operation,
    double progressOperationPartSize)
{
    Q_ASSERT(progressOperationPartSize);
    QObject* const operationObject = dynamic_cast< QObject* >(operation);
    if (operationObject != 0) {
        const QMetaObject* const mo = operationObject->metaObject();
        if (mo->indexOfSignal(QMetaObject::normalizedSignature("outputTextChanged(QString)")) > -1) {
            connect(operationObject, SIGNAL(outputTextChanged(QString)), ProgressCoordninator::instance(),
                SLOT(emitDetailTextChanged(QString)));
        }

        if (mo->indexOfSlot(QMetaObject::normalizedSignature("cancelOperation()")) > -1)
            connect(m_core, SIGNAL(installationInterrupted()), operationObject, SLOT(cancelOperation()));

        if (mo->indexOfSignal(QMetaObject::normalizedSignature("progressChanged(double)")) > -1) {
            ProgressCoordninator::instance()->registerPartProgress(operationObject,
                SIGNAL(progressChanged(double)), progressOperationPartSize);
        }
    }
}

KDUpdater::UpdateOperation* PackageManagerCorePrivate::createPathOperation(const QFileInfo &fileInfo,
    const QString &componentName)
{
    const bool isDir = fileInfo.isDir();
    // create an operation with the dir/ file as target, it will get deleted on undo
    KDUpdater::UpdateOperation *op = createOwnedOperation(QLatin1String(isDir ? "Mkdir" : "Copy"));
    if (isDir)
        op->setValue(QLatin1String("createddir"), fileInfo.absoluteFilePath());
    op->setValue(QLatin1String("component"), componentName);
    op->setArguments(isDir ? QStringList() << fileInfo.absoluteFilePath()
        : QStringList() << QString() << fileInfo.absoluteFilePath());
    return op;
}

/*!
    This creates fake operations which remove stuff which was registered for uninstallation afterwards
*/
void PackageManagerCorePrivate::registerPathesForUninstallation(
    const QList<QPair<QString, bool> > &pathesForUninstallation, const QString &componentName)
{
    if (pathesForUninstallation.isEmpty())
        return;

    QList<QPair<QString, bool> >::const_iterator it;
    for (it = pathesForUninstallation.begin(); it != pathesForUninstallation.end(); ++it) {
        const bool wipe = it->second;
        const QString path = replaceVariables(it->first);

        const QFileInfo fi(path);
        // create a copy operation with the file as target -> it will get deleted on undo
        KDUpdater::UpdateOperation *op = createPathOperation(fi, componentName);
        if (fi.isDir())
            op->setValue(QLatin1String("forceremoval"), wipe ? scTrue : scFalse);
        addPerformed(takeOwnedOperation(op));

        // get recursive afterwards
        if (fi.isDir() && !wipe) {
            QDirIterator dirIt(path, QDir::Hidden | QDir::AllEntries | QDir::System
                | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
            while (dirIt.hasNext()) {
                dirIt.next();
                op = createPathOperation(dirIt.fileInfo(), componentName);
                addPerformed(takeOwnedOperation(op));
            }
        }
    }
}

void PackageManagerCorePrivate::writeUninstallerBinary(QFile *const input, qint64 size, bool writeBinaryLayout)
{
    verbose() << "Writing uninstaller: " << (uninstallerName()  + QLatin1String(".new")) << std::endl;

    KDSaveFile out(uninstallerName() + QLatin1String(".new"));
    openForWrite(&out, out.fileName()); // throws an exception in case of error

    if (!input->seek(0))
        throw Error(QObject::tr("Failed to seek in file %1: %2").arg(input->fileName(), input->errorString()));

    appendData(&out, input, size);
    if (writeBinaryLayout) {
        appendInt64(&out, 0);   // resource count
        appendInt64(&out, 4 * sizeof(qint64));   // data block size
        appendInt64(&out, QInstaller::MagicUninstallerMarker);
        appendInt64(&out, QInstaller::MagicCookie);
    }
    out.setPermissions(out.permissions() | QFile::WriteUser | QFile::ReadGroup | QFile::ReadOther
        | QFile::ExeOther | QFile::ExeGroup | QFile::ExeUser);

    if (!out.commit(KDSaveFile::OverwriteExistingFile))
        throw Error(tr("Could not write uninstaller to %1: %2").arg(out.fileName(), out.errorString()));
}

void PackageManagerCorePrivate::writeUninstallerBinaryData(QIODevice *output, QFile *const input,
    const QList<KDUpdater::UpdateOperation*> &performedOperations, const BinaryLayout &layout,
    bool compressOperations, bool forceUncompressedResources)
{
    const qint64 dataBlockStart = output->pos();

    QVector<Range<qint64> >resourceSegments;
    foreach (const Range<qint64> &segment, layout.metadataResourceSegments) {
        input->seek(segment.start());
        if (isInstaller()) {
            const qint64 compressedSize = appendCompressedData(output, input, segment.length());
            resourceSegments.append(Range<qint64>::fromStartAndLength(output->pos() - compressedSize,
                compressedSize));
        } else if (forceUncompressedResources) {
            QBuffer resource;
            resource.setData(retrieveCompressedData(input, segment.length()));
            resource.open(QIODevice::ReadOnly);
            resourceSegments.append(Range<qint64>::fromStartAndLength(output->pos(), resource.size()));
            appendData(output, &resource, resource.size());
        } else {
            resourceSegments.append(Range<qint64>::fromStartAndLength(output->pos(), segment.length()));
            appendData(output, input, segment.length());
        }
    }

    const qint64 operationsStart = output->pos();
    appendInt64(output, performedOperations.count());
    foreach (KDUpdater::UpdateOperation *op, performedOperations) {
        // the installer can't be put into XML, remove it first
        op->clearValue(QLatin1String("installer"));

        appendString(output, op->name());
        compressOperations ? appendByteArray(output, qCompress(op->toXml().toByteArray()))
            : appendString(output, op->toXml().toString());

        // for the ui not to get blocked
        qApp->processEvents();
    }
    appendInt64(output, performedOperations.count());
    const qint64 operationsEnd = output->pos();

    // we don't save any component-indexes.
    const qint64 numComponents = 0;
    appendInt64(output, numComponents); // for the indexes
    // we don't save any components.
    const qint64 compIndexStart = output->pos();
    appendInt64(output, numComponents); // and 2 times number of components,
    appendInt64(output, numComponents); // one before and one after the components
    const qint64 compIndexEnd = output->pos();

    appendInt64Range(output, Range<qint64>::fromStartAndEnd(compIndexStart, compIndexEnd)
        .moved(-dataBlockStart));
    foreach (const Range<qint64> segment, resourceSegments)
        appendInt64Range(output, segment.moved(-dataBlockStart));
    appendInt64Range(output, Range<qint64>::fromStartAndEnd(operationsStart, operationsEnd)
        .moved(-dataBlockStart));
    appendInt64(output, layout.resourceCount);
    //data block size, from end of .exe to end of file
    appendInt64(output, output->pos() + 3 * sizeof(qint64) - dataBlockStart);
    appendInt64(output, MagicUninstallerMarker);
}

void PackageManagerCorePrivate::writeUninstaller(QList<KDUpdater::UpdateOperation*> performedOperations)
{
    bool gainedAdminRights = false;
    QTemporaryFile tempAdminFile(targetDir() + QString::fromLatin1("/testjsfdjlkdsjflkdsjfldsjlfds")
        + QString::number(qrand() % 1000));
    if (!tempAdminFile.open() || !tempAdminFile.isWritable()) {
        m_core->gainAdminRights();
        gainedAdminRights = true;
    }

    const QString targetAppDirPath = QFileInfo(uninstallerName()).path();
    if (!QDir().exists(targetAppDirPath)) {
        // create the directory containing the uninstaller (like a bundle structor, on Mac...)
        KDUpdater::UpdateOperation* op = createOwnedOperation(QLatin1String("Mkdir"));
        op->setArguments(QStringList() << targetAppDirPath);
        performOperationThreaded(op, Backup);
        performOperationThreaded(op);
        performedOperations.append(takeOwnedOperation(op));
    }

    {
        // write current state (variables) to the uninstaller ini file
        const QString iniPath = targetDir() + QLatin1Char('/') + m_settings.uninstallerIniFile();
        QSettingsWrapper cfg(iniPath, QSettingsWrapper::IniFormat);
        QVariantHash vars;
        QHash<QString, QString>::ConstIterator it = m_vars.constBegin();
        while (it != m_vars.constEnd()) {
            const QString &key = it.key();
            if (key != scRunProgramDescription && key != scRunProgram)
                vars.insert(key, it.value());
            ++it;
        }
        cfg.setValue(QLatin1String("Variables"), vars);

        QStringList list;
        foreach (const Repository &repository, m_settings.userRepositories())
            list.append(repository.url().toString());
        cfg.setValue(scRepositories, list);

        cfg.sync();
        if (cfg.status() != QSettingsWrapper::NoError) {
            const QString reason = cfg.status() == QSettingsWrapper::AccessError ? tr("Access error")
                : tr("Format error");
            throw Error(tr("Could not write installer configuration to %1: %2").arg(iniPath, reason));
        }
    }

#ifdef Q_WS_MAC
    // if it is a bundle, we need some stuff in it...
    const QString sourceAppDirPath = QCoreApplication::applicationDirPath();
    if (isInstaller() && QFileInfo(sourceAppDirPath + QLatin1String("/../..")).isBundle()) {
        KDUpdater::UpdateOperation* op = createOwnedOperation(QLatin1String("Copy"));
        op->setArguments(QStringList() << (sourceAppDirPath + QLatin1String("/../PkgInfo"))
            << (targetAppDirPath + QLatin1String("/../PkgInfo")));
        performOperationThreaded(op, Backup);
        performOperationThreaded(op);

        // copy Info.plist to target directory
        op = createOwnedOperation(QLatin1String("Copy"));
        op->setArguments(QStringList() << (sourceAppDirPath + QLatin1String("/../Info.plist"))
            << (targetAppDirPath + QLatin1String("/../Info.plist")));
        performOperationThreaded(op, Backup);
        performOperationThreaded(op);

        // patch the Info.plist after copying it
        QFile sourcePlist(sourceAppDirPath + QLatin1String("/../Info.plist"));
        openForRead(&sourcePlist, sourcePlist.fileName());
        QFile targetPlist(targetAppDirPath + QLatin1String("/../Info.plist"));
        openForWrite(&targetPlist, targetPlist.fileName());

        QTextStream in(&sourcePlist);
        QTextStream out(&targetPlist);
        const QString before = QLatin1String("<string>") + QFileInfo(QCoreApplication::applicationFilePath())
            .baseName() + QLatin1String("</string>");
        const QString after = QLatin1String("<string>") + QFileInfo(uninstallerName()).baseName()
            + QLatin1String("</string>");
        while (!in.atEnd())
            out << in.readLine().replace(before, after) << endl;

        // copy qt_menu.nib if it exists
        op = createOwnedOperation(QLatin1String("CopyDirectory"));
        op->setArguments(QStringList() << (sourceAppDirPath + QLatin1String("/../Resources/qt_menu.nib"))
            << (targetAppDirPath + QLatin1String("/../Resources/qt_menu.nib")));
        performOperationThreaded(op);

        op = createOwnedOperation(QLatin1String("Mkdir"));
        op->setArguments(QStringList() << (QFileInfo(targetAppDirPath).path() + QLatin1String("/Resources")));
        performOperationThreaded(op, Backup);
        performOperationThreaded(op);

        // copy application icons if it exists
        const QString icon = QFileInfo(QCoreApplication::applicationFilePath()).baseName()
            + QLatin1String(".icns");
        op = createOwnedOperation(QLatin1String("Copy"));
        op->setArguments(QStringList() << (sourceAppDirPath + QLatin1String("/../Resources/") + icon)
            << (targetAppDirPath + QLatin1String("/../Resources/") + icon));
        performOperationThreaded(op, Backup);
        performOperationThreaded(op);

        // finally, copy everything within Frameworks and plugins
        op = createOwnedOperation(QLatin1String("CopyDirectory"));
        op->setArguments(QStringList() << (sourceAppDirPath + QLatin1String("/../Frameworks"))
            << (targetAppDirPath + QLatin1String("/../Frameworks")));
        performOperationThreaded(op);

        op = createOwnedOperation(QLatin1String("CopyDirectory"));
        op->setArguments(QStringList() << (sourceAppDirPath + QLatin1String("/../plugins"))
            << (targetAppDirPath + QLatin1String("/../plugins")));
        performOperationThreaded(op);
    }
#endif

    try {
        // 1 - check if we have a installer base replacement
        //   |--- if so, write out the new tool and remove the replacement
        //   |--- remember to restart and that we need to replace the original binary
        //
        // 2 - if we do not have a replacement, try to open the binary data file as input
        //   |--- try to read the binary layout
        //      |--- on error (see 2.1)
        //          |--- remember we might to append uncompressed resource data (see 3)
        //          |--- set the installer or maintenance binary as input to take over binary data
        //          |--- in case we did not have a replacement, write out an new maintenance tool binary
        //              |--- remember that we need to replace the original binary
        //
        // 3 - open a new binary data file
        //   |--- try to write out the binary data based on the loaded input file (see 2)
        //      |--- on error (see 3.1)
        //          |--- if we wrote a new maintenance tool, take this as output - if not, write out a new
        //                  one and set it as output file, remember we did this
        //          |--- append the binary data based on the loaded input file (see 2), make sure we force
        //                 uncompression of the resource section if we read from a binary data file (see 4.1).
        //
        // 4 - force a deferred rename on the .dat file (see 4.1)
        // 5 - force a deferred rename on the maintenance file (see 5.1)

        // 2.1 - Error cases are: no data file (in fact we are the installer or an old installation),
        //          could not find the data file magic cookie (unknown .dat file), failed to read binary
        //          layout (mostly likely the resource section or we couldn't seek inside the file)
        //
        // 3.1 - most likely the commit operation will fail
        // 4.1 - if 3 failed, this makes sure the .dat file will get removed and on the next run all
        //          binary data is read from the maintenance tool, otherwise it get replaced be the new one
        // 5.1 - this will only happen -if- we wrote out a new binary

        bool newBinaryWritten = false;
        bool replacementExists = false;
        const QString installerBaseBinary = m_core->replaceVariables(m_installerBaseBinaryUnreplaced);
        if (!installerBaseBinary.isEmpty() && QFileInfo(installerBaseBinary).exists()) {
            verbose() << "Got a replacement installer base binary: " << installerBaseBinary << std::endl;

            QFile replacementBinary(installerBaseBinary);
            try {
                openForRead(&replacementBinary, replacementBinary.fileName());
                writeUninstallerBinary(&replacementBinary, replacementBinary.size(), true);

                newBinaryWritten = true;
                replacementExists = true;
            } catch (const Error &error) {
                verbose() << error.message() << std::endl;
            }

            if (!replacementBinary.remove()) {
                // Is there anything more sensible we can do with this error? I think not. It's not serious
                // enough for throwing/ aborting the process.
                verbose() << "Could not remove installer base binary (" << installerBaseBinary
                    << ") after updating the uninstaller: " << replacementBinary.errorString() << std::endl;
            }
            m_installerBaseBinaryUnreplaced.clear();
        }

        QFile input;
        BinaryLayout layout;
        bool forceUncompressedResourcesOnError = false;
        const QString dataFile = targetDir() + QLatin1Char('/') + m_settings.uninstallerName()
            + QLatin1String(".dat");
        try {
            if (isInstaller()) {
                throw Error(tr("Found a binary data file, but we are the installer and we should read the "
                    "binary resource from our very own binary!"));
            }
            input.setFileName(dataFile);
            openForRead(&input, input.fileName());
            layout = BinaryContent::readBinaryLayout(&input, findMagicCookie(&input, MagicCookieDat));
            forceUncompressedResourcesOnError = true;
        } catch (const Error &/*error*/) {
            input.setFileName(isInstaller() ? installerBinaryPath() : uninstallerName());
            openForRead(&input, input.fileName());
            layout = BinaryContent::readBinaryLayout(&input, findMagicCookie(&input, MagicCookie));
            if (!newBinaryWritten) {
                newBinaryWritten = true;
                writeUninstallerBinary(&input, layout.endOfData - layout.dataBlockSize, true);
            }
        }

        try {
            KDSaveFile file(dataFile + QLatin1String(".new"));
            openForWrite(&file, file.fileName());
            writeUninstallerBinaryData(&file, &input, performedOperations, layout, true, false);
            appendInt64(&file, MagicCookieDat);
            file.setPermissions(file.permissions() | QFile::WriteUser | QFile::ReadGroup
                | QFile::ReadOther);
            if (!file.commit(KDSaveFile::OverwriteExistingFile)) {
                throw Error(tr("Could not write uninstaller binary data to %1: %2").arg(file.fileName(),
                    file.errorString()));
            }
        } catch (const Error &/*error*/) {
            if (!newBinaryWritten) {
                newBinaryWritten = true;
                QFile tmp(isInstaller() ? installerBinaryPath() : uninstallerName());
                openForRead(&tmp, tmp.fileName());
                BinaryLayout tmpLayout = BinaryContent::readBinaryLayout(&tmp, findMagicCookie(&tmp, MagicCookie));
                writeUninstallerBinary(&tmp, tmpLayout.endOfData - tmpLayout.dataBlockSize, false);
            }

            QFile file(uninstallerName() + QLatin1String(".new"));
            openForAppend(&file, file.fileName());
            file.seek(file.size());
            writeUninstallerBinaryData(&file, &input, performedOperations, layout, false,
                forceUncompressedResourcesOnError);
            appendInt64(&file, MagicCookie);
        }
        input.close();
        deferredRename(dataFile + QLatin1String(".new"), dataFile, false);

        if (newBinaryWritten) {
            verbose() << "Needs restart: " << (replacementExists && isUpdater()) << std::endl;
            deferredRename(uninstallerName() + QLatin1String(".new"), uninstallerName(),
                replacementExists && isUpdater());
        }
    } catch (const Error &err) {
        setStatus(PackageManagerCore::Failure);
        if (gainedAdminRights)
            m_core->dropAdminRights();
        m_needToWriteUninstaller = false;
        throw err;
    }

    if (gainedAdminRights)
        m_core->dropAdminRights();

    commitSessionOperations();

    m_needToWriteUninstaller = false;
}

QString PackageManagerCorePrivate::registerPath() const
{
#ifdef Q_OS_WIN
    QString productName = m_vars.value(QLatin1String("ProductName"));
    if (productName.isEmpty())
        throw Error(tr("ProductName should be set"));

    QString path = QLatin1String("HKEY_CURRENT_USER");
    if (m_vars.value(QLatin1String("AllUsers")) == scTrue)
        path = QLatin1String("HKEY_LOCAL_MACHINE");

    return path + QLatin1String("\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\")
        + productName;
#endif
    return QString();
}

void PackageManagerCorePrivate::runInstaller()
{
    bool adminRightsGained = false;
    try {
        setStatus(PackageManagerCore::Running);
        emit installationStarted(); //resets also the ProgressCoordninator

        //to have some progress for writeUninstaller
        ProgressCoordninator::instance()->addReservePercentagePoints(1);

        const QString target = targetDir();
        if (target.isEmpty())
            throw Error(tr("Variable 'TargetDir' not set."));

        if (QDir(target).exists()) {
            QTemporaryFile tempAdminFile(target + QLatin1String("/adminrights"));
            if (!tempAdminFile.open() || !tempAdminFile.isWritable())
                adminRightsGained = m_core->gainAdminRights();
        }

        // add the operation to create the target directory
        KDUpdater::UpdateOperation *mkdirOp = createOwnedOperation(QLatin1String("Mkdir"));
        mkdirOp->setArguments(QStringList() << target);
        mkdirOp->setValue(QLatin1String("forceremoval"), true);
        mkdirOp->setValue(QLatin1String("uninstall-only"), true);

        performOperationThreaded(mkdirOp, Backup);
        if (!performOperationThreaded(mkdirOp)) {
            // if we cannot create the target dir, we try to activate the admin rights
            adminRightsGained = m_core->gainAdminRights();
            if (!performOperationThreaded(mkdirOp))
                throw Error(mkdirOp->errorString());
        }
        const QString remove = m_core->value(scRemoveTargetDir);
        if (QVariant(remove).toBool())
            addPerformed(takeOwnedOperation(mkdirOp));

        // to show that there was some work
        ProgressCoordninator::instance()->addManualPercentagePoints(1);
        ProgressCoordninator::instance()->emitLabelAndDetailTextChanged(tr("Preparing the installation..."));

        const QList<Component*> componentsToInstall = m_core->componentsToInstall(AllMode);
        verbose() << "Install size: " << componentsToInstall.size() << " components" << std::endl;

        if (!adminRightsGained) {
            QList<Component*> componentsToInstall = m_core->componentsToInstall(m_core->runMode());
            foreach (Component *component, componentsToInstall) {
                if (component->value(scRequiresAdminRights, scFalse) == scFalse)
                    continue;

                m_core->gainAdminRights();
                m_core->dropAdminRights();
                break;
            }
        }

        const double downloadPartProgressSize = double(1) / double(3);
        double componentsInstallPartProgressSize = double(2) / double(3);
        const int downloadedArchivesCount = m_core->downloadNeededArchives(AllMode,
            downloadPartProgressSize);

        // if there was no download we have the whole progress for installing components
        if (!downloadedArchivesCount)
            componentsInstallPartProgressSize = double(1);

        // Clear the packages as we might install into an already existing installation folder.
        m_updaterApplication.packagesInfo()->clearPackageInfoList();

        stopProcessesForUpdates(componentsToInstall);

        const int progressOperationCount = countProgressOperations(componentsToInstall);
        double progressOperationSize = componentsInstallPartProgressSize / progressOperationCount;

        foreach (Component *component, componentsToInstall)
            installComponent(component, progressOperationSize, adminRightsGained);

        emit m_core->titleMessageChanged(tr("Creating Uninstaller"));

        writeUninstaller(m_performedOperationsOld + m_performedOperationsCurrentSession);
        registerUninstaller();

        // this is the reserved one from the beginning
        ProgressCoordninator::instance()->addManualPercentagePoints(1);
        if (ProgressCoordninator::instance()->progressInPercentage() < 100)
            ProgressCoordninator::instance()->partProgressChanged(0.99);
        ProgressCoordninator::instance()->emitLabelAndDetailTextChanged(tr("\nInstallation finished!"));

        if (adminRightsGained)
            m_core->dropAdminRights();
        emit installationFinished();
        setStatus(PackageManagerCore::Success);
    } catch (const Error &err) {
        if (m_core->status() != PackageManagerCore::Canceled) {
            setStatus(PackageManagerCore::Failure);
            verbose() << "INSTALLER FAILED: " << err.message() << std::endl;
            MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(),
                QLatin1String("installationError"), tr("Error"), err.message());
            verbose() << "ROLLING BACK operations=" << m_performedOperationsCurrentSession.count()
                << std::endl;
        }

        m_core->rollBackInstallation();

        ProgressCoordninator::instance()->emitLabelAndDetailTextChanged(tr("\nInstallation aborted!"));
        if (adminRightsGained)
            m_core->dropAdminRights();
        emit installationFinished();

        throw;
    }
}

void PackageManagerCorePrivate::runPackageUpdater()
{
    bool adminRightsGained = false;
    try {
        if (m_completeUninstall) {
            runUninstaller();
            return;
        }

        setStatus(PackageManagerCore::Running);
        emit installationStarted(); //resets also the ProgressCoordninator

        //to have some progress for the cleanup/write component.xml step
        ProgressCoordninator::instance()->addReservePercentagePoints(1);

        const QString packagesXml = componentsXmlPath();
        // check if we need admin rights and ask before the action happens
        if (!QFileInfo(installerBinaryPath()).isWritable() || !QFileInfo(packagesXml).isWritable())
            adminRightsGained = m_core->gainAdminRights();

        const QList<Component*> componentsToInstall = m_core->componentsToInstall(m_core->runMode());
        verbose() << "Install size: " << componentsToInstall.size() << " components " << std::endl;

        bool updateAdminRights = false;
        if (!adminRightsGained) {
            foreach (Component *component, componentsToInstall) {
                if (component->value(scRequiresAdminRights, scFalse) == scFalse)
                    continue;

                updateAdminRights = true;
                break;
            }
        }

        QHash<QString, Component*> componentsByName;
        QList<KDUpdater::UpdateOperation*> undoOperations;
        QList<KDUpdater::UpdateOperation*> nonRevertedOperations;

        // build a list of undo operations based on the checked state of the component
        foreach (KDUpdater::UpdateOperation *operation, m_performedOperationsOld) {
            const QString &name = operation->value(QLatin1String("component")).toString();
            Component *component = componentsByName.value(name, 0);
            if (!component)
                component = m_core->componentByName(name);
            if (component) {
                componentsByName.insert(name, component);
                // if we're _not_ removing everything and this component is still selected, -> next
                if (component->isSelected()) {
                    nonRevertedOperations.append(operation);
                    continue;
                }
            }

            // In package manager mode a component (one to replace) might be scheduled for uninstall, but we
            // don't know if it should be uninstalled. To figure this out we need to check the actual
            // replacement if it is still checked. If so, skip the undo operation. This avoids an update if we
            // actually do a completely different component install/ uninstall.
            if (isPackageManager() && m_componentsToReplaceAllMode.contains(name)) {
                if (m_componentsToReplaceAllMode.value(name).first->isSelected())
                    continue;
            }

            // Filter out the create target dir undo operation, it's only needed for full uninstall.
            // TODO: Figure out a compat way for old create target dir undo operation (missing name and tag).
            if (operation->value(QLatin1String("uninstall-only")).toBool()) {
                nonRevertedOperations.append(operation);
                continue;
            }

            undoOperations.prepend(operation);
            updateAdminRights |= operation->value(QLatin1String("admin")).toBool();
        }

        // we did not request admin rights till we found out that a component/ undo needs admin rights
        if (updateAdminRights && !adminRightsGained) {
            m_core->gainAdminRights();
            m_core->dropAdminRights();
        }

        double undoOperationProgressSize = 0;
        const double downloadPartProgressSize = double(2) / double(5);
        double componentsInstallPartProgressSize = double(3) / double(5);
        if (undoOperations.count() > 0) {
            undoOperationProgressSize = double(1) / double(5);
            componentsInstallPartProgressSize = downloadPartProgressSize;
            undoOperationProgressSize /= countProgressOperations(undoOperations);
        }

        ProgressCoordninator::instance()->emitLabelAndDetailTextChanged(tr("Removing deselected components..."));

        runUndoOperations(undoOperations, undoOperationProgressSize, adminRightsGained, true);
        m_performedOperationsOld = nonRevertedOperations; // these are all operations left: those not reverted

        ProgressCoordninator::instance()->emitLabelAndDetailTextChanged(tr("Preparing the installation..."));

        // following, we download the needed archives
        m_core->downloadNeededArchives(AllMode, downloadPartProgressSize);

        stopProcessesForUpdates(componentsToInstall);

        const double progressOperationCount = countProgressOperations(componentsToInstall);
        const double progressOperationSize = componentsInstallPartProgressSize / progressOperationCount;

        foreach (Component *component, componentsToInstall)
            installComponent(component, progressOperationSize, adminRightsGained);

        emit m_core->titleMessageChanged(tr("Creating Uninstaller"));

        commitSessionOperations(); //end session, move ops to "old"
        m_needToWriteUninstaller = true;

        //this is the reserved one from the beginning
        ProgressCoordninator::instance()->addManualPercentagePoints(1);
        if (ProgressCoordninator::instance()->progressInPercentage() < 100)
            ProgressCoordninator::instance()->partProgressChanged(0.99);
        ProgressCoordninator::instance()->emitLabelAndDetailTextChanged(tr("\nUpdate finished!"));

        if (adminRightsGained)
            m_core->dropAdminRights();
        emit installationFinished();
        setStatus(PackageManagerCore::Success);
    } catch (const Error &err) {
        if (m_core->status() != PackageManagerCore::Canceled) {
            setStatus(PackageManagerCore::Failure);
            verbose() << "INSTALLER FAILED: " << err.message() << std::endl;
            MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(),
                QLatin1String("installationError"), tr("Error"), err.message());
            verbose() << "ROLLING BACK operations=" << m_performedOperationsCurrentSession.count()
                << std::endl;
        }

        m_core->rollBackInstallation();

        ProgressCoordninator::instance()->emitLabelAndDetailTextChanged(tr("\nUpdate aborted!"));
        if (adminRightsGained)
            m_core->dropAdminRights();
        emit installationFinished();

        throw;
    }
}

void PackageManagerCorePrivate::runUninstaller()
{
    bool adminRightsGained = false;
    try {
        setStatus(PackageManagerCore::Running);
        emit uninstallationStarted();

        // check if we need administration rights and ask before the action happens
        if (!QFileInfo(installerBinaryPath()).isWritable() || !QFileInfo(componentsXmlPath()).isWritable())
            adminRightsGained = m_core->gainAdminRights();

        bool updateAdminRights = false;
        QList<KDUpdater::UpdateOperation*> undoOperations;
        foreach (KDUpdater::UpdateOperation *op, m_performedOperationsOld) {
            undoOperations.prepend(op);
            updateAdminRights |= op->value(QLatin1String("admin")).toBool();
        }

        // we did not request administration rights till we found out that a undo needs administration rights
        if (updateAdminRights && !adminRightsGained) {
            m_core->gainAdminRights();
            m_core->dropAdminRights();
        }

        const int uninstallOperationCount = countProgressOperations(undoOperations);
        const double undoOperationProgressSize = double(1) / double(uninstallOperationCount);

        runUndoOperations(undoOperations, undoOperationProgressSize, adminRightsGained, false);
        // No operation delete here, as all old undo operations are deleted in the destructor.

        const QString startMenuDir = m_vars.value(scStartMenuDir);
        if (!startMenuDir.isEmpty()) {
            try {
                QInstaller::removeDirectory(startMenuDir);
            } catch (const Error &error) {
                verbose() << "Could not remove " << startMenuDir << ": " << error.message() << std::endl;
            }
        } else {
            verbose() << "Start menu dir not set." << std::endl;
        }

        // this will also delete the TargetDir on Windows
        deleteUninstaller();

        if (QVariant(m_core->value(scRemoveTargetDir)).toBool()) {
            // on !Windows, we need to remove TargetDir manually
            verbose() << "Complete uninstallation is chosen" << std::endl;
            const QString target = targetDir();
            if (!target.isEmpty()) {
                if (updateAdminRights && !adminRightsGained) {
                    // we were root at least once, so we remove the target dir as root
                    m_core->gainAdminRights();
                    removeDirectoryThreaded(target, true);
                    m_core->dropAdminRights();
                } else {
                    removeDirectoryThreaded(target, true);
                }
            }
        }

        unregisterUninstaller();
        m_needToWriteUninstaller = false;

        setStatus(PackageManagerCore::Success);
        ProgressCoordninator::instance()->emitLabelAndDetailTextChanged(tr("\nDeinstallation finished!"));
        if (adminRightsGained)
            m_core->dropAdminRights();
        emit uninstallationFinished();
    } catch (const Error &err) {
        if (m_core->status() != PackageManagerCore::Canceled) {
            setStatus(PackageManagerCore::Failure);
            verbose() << "INSTALLER FAILED: " << err.message() << std::endl;
            MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(),
                QLatin1String("installationError"), tr("Error"), err.message());
        }

        ProgressCoordninator::instance()->emitLabelAndDetailTextChanged(tr("\nDeinstallation aborted!"));
        if (adminRightsGained)
            m_core->dropAdminRights();
        emit installationFinished();

        throw;
    }
}

void PackageManagerCorePrivate::installComponent(Component *component, double progressOperationSize,
    bool adminRightsGained)
{
    const QList<KDUpdater::UpdateOperation*> operations = component->operations();
    if (!component->operationsCreatedSuccessfully())
        m_core->setCanceled();

    const int opCount = operations.count();
    // show only components which do something, MinimumProgress is only for progress calculation safeness
    if (opCount > 1 || (opCount == 1 && operations.at(0)->name() != QLatin1String("MinimumProgress"))) {
            ProgressCoordninator::instance()->emitLabelAndDetailTextChanged(tr("\nInstalling component %1")
                .arg(component->displayName()));
    }

    foreach (KDUpdater::UpdateOperation *operation, operations) {
        if (statusCanceledOrFailed())
            throw Error(tr("Installation canceled by user"));

        // maybe this operations wants us to be admin...
        bool becameAdmin = false;
        if (!adminRightsGained && operation->value(QLatin1String("admin")).toBool()) {
            becameAdmin = m_core->gainAdminRights();
            verbose() << operation->name() << " as admin: " << becameAdmin << std::endl;
        }

        connectOperationToInstaller(operation, progressOperationSize);
        // allow the operation to backup stuff before performing the operation
        PackageManagerCorePrivate::performOperationThreaded(operation, PackageManagerCorePrivate::Backup);

        bool ignoreError = false;
        bool ok = PackageManagerCorePrivate::performOperationThreaded(operation);
        while (!ok && !ignoreError && m_core->status() != PackageManagerCore::Canceled) {
            verbose() << QString(QLatin1String("operation '%1' with arguments: '%2' failed: %3"))
                .arg(operation->name(), operation->arguments().join(QLatin1String("; ")),
                operation->errorString()) << std::endl;;
            const QMessageBox::StandardButton button =
                MessageBoxHandler::warning(MessageBoxHandler::currentBestSuitParent(),
                QLatin1String("installationErrorWithRetry"), tr("Installer Error"),
                tr("Error during installation process (%1):\n%2").arg(component->name(),
                operation->errorString()),
                QMessageBox::Retry | QMessageBox::Ignore | QMessageBox::Cancel, QMessageBox::Retry);

            if (button == QMessageBox::Retry)
                ok = PackageManagerCorePrivate::performOperationThreaded(operation);
            else if (button == QMessageBox::Ignore)
                ignoreError = true;
            else if (button == QMessageBox::Cancel)
                m_core->interrupt();
        }

        if (ok || operation->error() > KDUpdater::UpdateOperation::InvalidArguments) {
            // Remember that the operation was performed, what allows us to undo it if a following operation
            // fails or if this operation failed but still needs an undo call to cleanup.
            addPerformed(operation);
            operation->setValue(QLatin1String("component"), component->name());
            // Add the progress operation size to the progress coordinator, so we will show the right
            // progress at the end of the install/ update after we had an ignore error.
            if (ignoreError)
                ProgressCoordninator::instance()->partProgressChanged(progressOperationSize);
        }

        if (becameAdmin)
            m_core->dropAdminRights();

        if (!ok && !ignoreError)
            throw Error(operation->errorString());

        if (component->value(scImportant, scFalse) == scTrue)
            m_forceRestart = true;
    }

    registerPathesForUninstallation(component->pathesForUninstallation(), component->name());

    if (!component->stopProcessForUpdateRequests().isEmpty()) {
        KDUpdater::UpdateOperation *stopProcessForUpdatesOp =
            KDUpdater::UpdateOperationFactory::instance().create(QLatin1String("FakeStopProcessForUpdate"));
        const QStringList arguments(component->stopProcessForUpdateRequests().join(QLatin1String(",")));
        stopProcessForUpdatesOp->setArguments(arguments);
        addPerformed(stopProcessForUpdatesOp);
        stopProcessForUpdatesOp->setValue(QLatin1String("component"), component->name());
    }

    // now mark the component as installed
    KDUpdater::PackagesInfo &packages = *m_updaterApplication.packagesInfo();
    packages.installPackage(component->name(), component->value(scVersion), component->value(scDisplayName),
        component->value(scDescription), component->dependencies(), component->forcedInstallation(),
        component->isVirtual(), component->value(scUncompressedSize).toULongLong());
    packages.writeToDisk();

    component->setInstalled();
    component->markAsPerformedInstallation();
}

// -- private

void PackageManagerCorePrivate::deleteUninstaller()
{
#ifdef Q_OS_WIN
    // Since Windows does not support that the uninstaller deletes itself we  have to go with a rather dirty
    // hack. What we do is to create a batchfile that will try to remove the uninstaller once per second. Then
    // we start that batchfile detached, finished our job and close ourself. Once that's done the batchfile
    // will succeed in deleting our uninstall.exe and, if the installation directory was created but us and if
    // it's empty after the uninstall, deletes the installation-directory.
    const QString batchfile = QDir::toNativeSeparators(QFileInfo(QDir::tempPath(),
        QLatin1String("uninstall.vbs")).absoluteFilePath());
    QFile f(batchfile);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        throw Error(tr("Cannot prepare uninstall"));

    QTextStream batch(&f);
    batch << "Set fso = WScript.CreateObject(\"Scripting.FileSystemObject\")\n";
    batch << "file = WScript.Arguments.Item(0)\n";
    batch << "folderpath = WScript.Arguments.Item(1)\n";
    batch << "Set folder = fso.GetFolder(folderpath)\n";
    batch << "on error resume next\n";

    batch << "while fso.FileExists(file)\n";
    batch << "    fso.DeleteFile(file)\n";
    batch << "    WScript.Sleep(1000)\n";
    batch << "wend\n";
//    batch << "if folder.SubFolders.Count = 0 and folder.Files.Count = 0 then\n";
    batch << "    Set folder = Nothing\n";
    batch << "    fso.DeleteFolder folderpath, true\n";
//    batch << "end if\n";
    batch << "fso.DeleteFile(WScript.ScriptFullName)\n";

    f.close();

    QStringList arguments;
    arguments << QLatin1String("//Nologo") << batchfile; // execute the batchfile
    arguments << QDir::toNativeSeparators(QFileInfo(installerBinaryPath()).absoluteFilePath());
    if (!m_performedOperationsOld.isEmpty()) {
        const KDUpdater::UpdateOperation* const op = m_performedOperationsOld.first();
        if (op->name() == QLatin1String("Mkdir")) // the target directory name
            arguments << QDir::toNativeSeparators(QFileInfo(op->arguments().first()).absoluteFilePath());
    }

    if (!QProcessWrapper::startDetached(QLatin1String("cscript"), arguments, QDir::rootPath()))
        throw Error(tr("Cannot start uninstall"));
#else
    // every other platform has no problem if we just delete ourself now
    QFile uninstaller(QFileInfo(installerBinaryPath()).absoluteFilePath());
    uninstaller.remove();
# ifdef Q_WS_MAC
    const QLatin1String cdUp("/../../..");
    if (QFileInfo(QFileInfo(installerBinaryPath() + cdUp).absoluteFilePath()).isBundle()) {
        removeDirectoryThreaded(QFileInfo(installerBinaryPath() + cdUp).absoluteFilePath());
        QFile::remove(QFileInfo(installerBinaryPath() + cdUp).absolutePath()
            + QLatin1String("/") + configurationFileName());
    } else
# endif
#endif
    {
        // finally remove the components.xml, since it still exists now
        QFile::remove(QFileInfo(installerBinaryPath()).absolutePath() + QLatin1String("/")
            + configurationFileName());
    }
}

void PackageManagerCorePrivate::registerUninstaller()
{
#ifdef Q_OS_WIN
    QSettingsWrapper settings(registerPath(), QSettingsWrapper::NativeFormat);
    settings.setValue(scDisplayName, m_vars.value(QLatin1String("ProductName")));
    settings.setValue(QLatin1String("DisplayVersion"), m_vars.value(QLatin1String("ProductVersion")));
    const QString uninstaller = QDir::toNativeSeparators(uninstallerName());
    settings.setValue(QLatin1String("DisplayIcon"), uninstaller);
    settings.setValue(scPublisher, m_vars.value(scPublisher));
    settings.setValue(QLatin1String("UrlInfoAbout"), m_vars.value(QLatin1String("Url")));
    settings.setValue(QLatin1String("Comments"), m_vars.value(scTitle));
    settings.setValue(QLatin1String("InstallDate"), QDateTime::currentDateTime().toString());
    settings.setValue(QLatin1String("InstallLocation"), QDir::toNativeSeparators(targetDir()));
    settings.setValue(QLatin1String("UninstallString"), uninstaller);
    settings.setValue(QLatin1String("ModifyPath"), uninstaller + QLatin1String(" --manage-packages"));
    settings.setValue(QLatin1String("EstimatedSize"), QFileInfo(installerBinaryPath()).size());
    settings.setValue(QLatin1String("NoModify"), 0);
    settings.setValue(QLatin1String("NoRepair"), 1);
#endif
}

void PackageManagerCorePrivate::unregisterUninstaller()
{
#ifdef Q_OS_WIN
    QSettingsWrapper settings(registerPath(), QSettingsWrapper::NativeFormat);
    settings.remove(QString());
#endif
}

void PackageManagerCorePrivate::runUndoOperations(const QList<KDUpdater::UpdateOperation*> &undoOperations,
    double undoOperationProgressSize, bool adminRightsGained, bool deleteOperation)
{
    KDUpdater::PackagesInfo &packages = *m_updaterApplication.packagesInfo();
    try {
        foreach (KDUpdater::UpdateOperation *undoOperation, undoOperations) {
            if (statusCanceledOrFailed())
                throw Error(tr("Installation canceled by user"));

            bool becameAdmin = false;
            if (!adminRightsGained && undoOperation->value(QLatin1String("admin")).toBool())
                becameAdmin = m_core->gainAdminRights();

            connectOperationToInstaller(undoOperation, undoOperationProgressSize);
            verbose() << "undo operation=" << undoOperation->name() << std::endl;
            performOperationThreaded(undoOperation, PackageManagerCorePrivate::Undo);

            const QString componentName = undoOperation->value(QLatin1String("component")).toString();
            if (undoOperation->error() != KDUpdater::UpdateOperation::NoError) {
                if (!componentName.isEmpty()) {
                    bool run = true;
                    while (run && m_core->status() != PackageManagerCore::Canceled) {
                        const QMessageBox::StandardButton button =
                            MessageBoxHandler::warning(MessageBoxHandler::currentBestSuitParent(),
                            QLatin1String("installationErrorWithRetry"), tr("Installer Error"),
                            tr("Error during uninstallation process:\n%1").arg(undoOperation->errorString()),
                            QMessageBox::Retry | QMessageBox::Ignore, QMessageBox::Retry);

                        if (button == QMessageBox::Retry) {
                            performOperationThreaded(undoOperation, Undo);
                            if (undoOperation->error() == KDUpdater::UpdateOperation::NoError)
                                run = false;
                        } else if (button == QMessageBox::Ignore) {
                            run = false;
                        }
                    }
                }
            }

            if (!componentName.isEmpty()) {
                Component *component = m_core->componentByName(componentName);
                if (!component)
                    component = componentsToReplace().value(componentName).second;
                if (component) {
                    component->setUninstalled();
                    packages.removePackage(component->name());
                }
            }

            if (becameAdmin)
                m_core->dropAdminRights();

            if (deleteOperation)
                delete undoOperation;
        }
    } catch (const Error &error) {
        packages.writeToDisk();
        throw Error(error.message());
    } catch (...) {
        packages.writeToDisk();
        throw Error(tr("Unknown error"));
    }
    packages.writeToDisk();
}

bool PackageManagerCorePrivate::fetchMetaInformationFromRepositories()
{
    if (m_repoFetched)
        return m_repoFetched;

    m_repoFetched = false;
    if ((isInstaller() && !m_core->isOfflineOnly()) || (isUpdater() || isPackageManager()))
        m_repoMetaInfoJob->setRepositories(m_settings.repositories());

    try {
        m_repoMetaInfoJob->setAutoDelete(false);
        m_repoMetaInfoJob->start();
        m_repoMetaInfoJob->waitForFinished();
    } catch (Error &error) {
        verbose() << tr("Could not retrieve meta information: %1").arg(error.message()) << std::endl;
        return m_repoFetched;
    }

    if (m_repoMetaInfoJob->isCanceled() || m_repoMetaInfoJob->error() != KDJob::NoError) {
        if (m_repoMetaInfoJob->error() != QInstaller::UserIgnoreError) {
            verbose() << m_repoMetaInfoJob->errorString() << std::endl;
            setStatus(PackageManagerCore::Failure, m_repoMetaInfoJob->errorString());
            return m_repoFetched;
        }
    }

    m_repoFetched = true;
    return m_repoFetched;
}

bool PackageManagerCorePrivate::addUpdateResourcesFromRepositories(bool parseChecksum)
{
    if (m_updateSourcesAdded)
        return m_updateSourcesAdded;

    if (m_repoMetaInfoJob->temporaryDirectories().isEmpty()) {
        m_updateSourcesAdded = true;
        return m_updateSourcesAdded;
    }

    m_updateSourcesAdded = false;
    const QString &appName = m_settings.applicationName();
    const QStringList tempDirs = m_repoMetaInfoJob->temporaryDirectories();
    foreach (const QString &tmpDir, tempDirs) {
        if (tmpDir.isEmpty())
            continue;

        if (parseChecksum) {
            const QString updatesXmlPath = tmpDir + QLatin1String("/Updates.xml");
            QFile updatesFile(updatesXmlPath);
            try {
                openForRead(&updatesFile, updatesFile.fileName());
            } catch(const Error &e) {
                verbose() << tr("Error opening Updates.xml: ") << e.message() << std::endl;
                setStatus(PackageManagerCore::Failure, tr("Could not add temporary update source information."));
                return false;
            }

            int line = 0;
            int column = 0;
            QString error;
            QDomDocument doc;
            if (!doc.setContent(&updatesFile, &error, &line, &column)) {
                verbose() << tr("Parse error in File %4 : %1 at line %2 col %3").arg(error,
                    QString::number(line), QString::number(column), updatesFile.fileName()) << std::endl;
                setStatus(PackageManagerCore::Failure, tr("Could not add temporary update source information."));
                return false;
            }

            const QDomNode checksum = doc.documentElement().firstChildElement(QLatin1String("Checksum"));
            if (!checksum.isNull())
                m_core->setTestChecksum(checksum.toElement().text().toLower() == scTrue);
        }
        m_updaterApplication.addUpdateSource(appName, appName, QString(), QUrl::fromLocalFile(tmpDir), 1);
    }
    m_updaterApplication.updateSourcesInfo()->setModified(false);

    if (m_updaterApplication.updateSourcesInfo()->updateSourceInfoCount() == 0) {
        setStatus(PackageManagerCore::Failure, tr("Could not find any update source information."));
        return false;
    }

    m_updateSourcesAdded = true;
    return m_updateSourcesAdded;
}

/*!
    Returns a hash containing the installed package name and it's associated package information. If
    the application is running in installer mode or the local components file could not be parsed, the
    hash is empty.
*/
QHash<QString, KDUpdater::PackageInfo> PackageManagerCorePrivate::localInstalledPackages()
{
    QHash<QString, KDUpdater::PackageInfo> installedPackages;

    KDUpdater::PackagesInfo &packagesInfo = *m_updaterApplication.packagesInfo();
    if (!isInstaller()) {
        if (!packagesInfo.isValid()) {
            packagesInfo.setFileName(componentsXmlPath());
            packagesInfo.setApplicationName(m_settings.applicationName());
            packagesInfo.setApplicationVersion(m_settings.applicationVersion());
        }

        if (packagesInfo.error() != KDUpdater::PackagesInfo::NoError)
            setStatus(PackageManagerCore::Failure, tr("Failure to read packages from: %1.").arg(componentsXmlPath()));

        foreach (const KDUpdater::PackageInfo &info, packagesInfo.packageInfos())
            installedPackages.insert(info.name, info);
     }

    return installedPackages;
}

}   // QInstaller
