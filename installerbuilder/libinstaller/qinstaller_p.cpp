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
#include "qinstaller_p.h"

#include "adminauthorization.h"
#include "common/binaryformat.h"
#include "common/errors.h"
#include "common/fileutils.h"
#include "common/installersettings.h"
#include "common/utils.h"
#include "fsengineclient.h"
#include "messageboxhandler.h"
#include "progresscoordinator.h"
#include "qinstaller.h"
#include "qinstallercomponent.h"

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

static bool runOperation(KDUpdater::UpdateOperation *op, InstallerPrivate::OperationType type)
{
    switch (type) {
        case InstallerPrivate::Backup:
            op->backup();
            return true;
        case InstallerPrivate::Perform:
            return op->performOperation();
        case InstallerPrivate::Undo:
            return op->undoOperation();
        default:
            Q_ASSERT(!"unexpected operation type");
    }
    return false;
}

template <typename T>
void letTheUiRunTillFinished(const QFuture<T>& f)
{
    QFutureWatcher<T> futureWatcher;

    QEventLoop loop;
    loop.connect(&futureWatcher, SIGNAL(finished()), SLOT(quit()), Qt::QueuedConnection);
    futureWatcher.setFuture(f);

    if (!f.isFinished())
        loop.exec();
}

/*!
    \internal
    Initializes the created FSEngineClientHandler instance \a handler.
*/
static void initEngineHandler(/*QInstaller::*/FSEngineClientHandler *handler)
{
#ifdef FSENGINE_TCP
    const int port = 30000 + qrand() % 1000;
    handler->init(port);
    handler->setStartServerCommand(QCoreApplication::applicationFilePath(), QStringList()
        << QLatin1String("--startserver") << QString::number(port) << handler->authorizationKey(),
        true);
#else
    const QString name = QInstaller::generateTemporaryFileName();
    handler->init(name);
    handler->setStartServerCommand(qApp->applicationFilePath(), QStringList()
        << QLatin1String("--startserver") << name << handler->authorizationKey(), true);
#endif
}

/*!
    \internal
    Creates and initializes a FSEngineClientHandler -> makes us get admin rights for QFile operations
*/
static /*QInstaller::*/FSEngineClientHandler* createEngineClientHandler()
{
    static FSEngineClientHandler* clientHandlerInstance = 0;
    if (clientHandlerInstance == 0) {
        clientHandlerInstance = new FSEngineClientHandler;
        initEngineHandler(clientHandlerInstance);
    }
    return clientHandlerInstance;
}

static QStringList checkRunningProcessesFromList(const QStringList &processList)
{
    const QList<KDSysInfo::ProcessInfo> allProcesses = KDSysInfo::runningProcesses();
    QStringList stillRunningProcesses;
    foreach (const QString &process, processList) {
        if (!process.isEmpty() && InstallerPrivate::isProcessRunning(process, allProcesses))
            stillRunningProcesses.append(process);
    }
    return stillRunningProcesses;
}

#ifdef Q_WS_WIN
static void deferredRename(const QString &oldName, const QString &newName, bool restart = false)
{
    QString batchfile;

    QStringList arguments;
    arguments << QDir::toNativeSeparators(batchfile) << QDir::toNativeSeparators(oldName)
        << QDir::toNativeSeparators(QFileInfo(oldName).dir().absoluteFilePath(newName));

    {
        QTemporaryFile f(QDir::temp().absoluteFilePath(QLatin1String("deferredrenameXXXXXX.vbs")));
        openForWrite(&f, f.fileName());
        f.setAutoRemove(false);

        batchfile = f.fileName();

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

    QProcess::startDetached(QLatin1String("cscript"), QStringList() << QLatin1String("//Nologo")
        << QDir::toNativeSeparators(batchfile));
}
#endif // Q_WS_WIN


// -- InstallerPrivate


InstallerPrivate::InstallerPrivate(Installer *installer, qint64 magicInstallerMaker,
        const QVector<KDUpdater::UpdateOperation*> &performedOperations)
    : m_app(0)
    , m_tempDirDeleter(new TempDirDeleter())
    , m_installerSettings(0)
    , m_FSEngineClientHandler(createEngineClientHandler())
    , m_status(Installer::Unfinished)
    , m_forceRestart(false)
    , m_silentRetries(3)
    , m_testChecksum(false)
    , m_launchedAsRoot(AdminAuthorization::hasAdminRights())
    , m_completeUninstall(false)
    , m_needToWriteUninstaller(false)
    , m_performedOperationsOld(performedOperations)
    , q(installer)
    , m_magicBinaryMarker(magicInstallerMaker)
{
    connect(this, SIGNAL(installationStarted()), q, SIGNAL(installationStarted()));
    connect(this, SIGNAL(installationFinished()), q, SIGNAL(installationFinished()));
    connect(this, SIGNAL(uninstallationStarted()), q, SIGNAL(uninstallationStarted()));
    connect(this, SIGNAL(uninstallationFinished()), q, SIGNAL(uninstallationFinished()));
}

InstallerPrivate::~InstallerPrivate()
{
    qDeleteAll(m_rootComponents);
    qDeleteAll(m_updaterComponents);
    qDeleteAll(m_updaterComponentsDeps);

    qDeleteAll(m_performedOperationsOld);
    qDeleteAll(m_performedOperationsCurrentSession);

    delete m_tempDirDeleter;
    delete m_installerSettings;
    delete m_FSEngineClientHandler;
}

/*!
    Return true, if a process with \a name is running. On Windows, comparision is case-insensitive.
*/
/* static */
bool InstallerPrivate::isProcessRunning(const QString &name,
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
bool InstallerPrivate::performOperationThreaded(KDUpdater::UpdateOperation *op,
    InstallerPrivate::OperationType type)
{
    QFuture<bool> future = QtConcurrent::run(runOperation, op, type);
    letTheUiRunTillFinished(future);
    return future.result();
}

QString InstallerPrivate::targetDir() const
{
    return q->value(QLatin1String("TargetDir"));
}

QString InstallerPrivate::configurationFileName() const
{
    return q->value(QLatin1String("TargetConfigurationFile"), QString::fromLatin1("components.xml"));
}

QString InstallerPrivate::componentsXmlPath() const
{
    return QDir::toNativeSeparators(QDir(QDir::cleanPath(targetDir()))
        .absoluteFilePath(configurationFileName()));
}

QString InstallerPrivate::localComponentsXmlPath() const
{
    const QString &appDirPath = QCoreApplication::applicationDirPath();
    if (QFileInfo(appDirPath + QLatin1String("/../..")).isBundle()) {
        return QDir::toNativeSeparators(QFileInfo(QDir::cleanPath(appDirPath
            + QLatin1String("/../../../") + configurationFileName())).absoluteFilePath());
    }
    return componentsXmlPath();
}

void InstallerPrivate::initialize()
{
    try {
        delete m_installerSettings;
        m_installerSettings = new InstallerSettings(InstallerSettings::fromFileAndPrefix(
            QLatin1String(":/metadata/installer-config/config.xml"),
            QLatin1String(":/metadata/installer-config/")));
    } catch (const Error &e) {
        qCritical("Could not parse Config: %s", qPrintable(e.message()));
        //TODO try better error handling
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
    //TODO add more platforms as needed...
#endif

    // fill the variables defined in the settings
    m_vars.insert(QLatin1String("ProductName"), m_installerSettings->applicationName());
    m_vars.insert(QLatin1String("ProductVersion"), m_installerSettings->applicationVersion());
    m_vars.insert(QLatin1String("Title"), m_installerSettings->title());
    m_vars.insert(QLatin1String("MaintenanceTitle"), m_installerSettings->maintenanceTitle());
    m_vars.insert(QLatin1String("Publisher"), m_installerSettings->publisher());
    m_vars.insert(QLatin1String("Url"), m_installerSettings->url());
    m_vars.insert(QLatin1String("StartMenuDir"), m_installerSettings->startMenuDir());

    m_vars.insert(QLatin1String("TargetConfigurationFile"), m_installerSettings->configurationFileName());
    m_vars.insert(QLatin1String("LogoPixmap"), m_installerSettings->logo());
    m_vars.insert(QLatin1String("LogoSmallPixmap"), m_installerSettings->logoSmall());
    m_vars.insert(QLatin1String("WatermarkPixmap"), m_installerSettings->watermark());

    m_vars.insert(QLatin1String("RunProgram"), replaceVariables(m_installerSettings->runProgram()));
    const QString desc = m_installerSettings->runProgramDescription();
    if (!desc.isEmpty())
        m_vars.insert(QLatin1String("RunProgramDescription"), desc);
#ifdef Q_WS_X11
    if (m_launchedAsRoot)
        m_vars.insert(QLatin1String("TargetDir"), replaceVariables(m_installerSettings->adminTargetDir()));
    else
#endif
    m_vars.insert(QLatin1String("TargetDir"), replaceVariables(m_installerSettings->targetDir()));
    m_vars.insert(QLatin1String("RemoveTargetDir"), replaceVariables(m_installerSettings->removeTargetDir()));

    QSettings creatorSettings(QSettings::IniFormat, QSettings::UserScope, QLatin1String("Nokia"),
        QLatin1String("QtCreator"));
    QFileInfo info(creatorSettings.fileName());
    if (info.exists())
        m_vars.insert(QLatin1String("QtCreatorSettingsFile"), info.absoluteFilePath());

    if (!q->isInstaller()) {
#ifdef Q_WS_MAC
        readUninstallerIniFile(QCoreApplication::applicationDirPath() + QLatin1String("/../../.."));
#else
        readUninstallerIniFile(QCoreApplication::applicationDirPath());
#endif
    }

    disconnect(this, SIGNAL(installationStarted()), ProgressCoordninator::instance(), SLOT(reset()));
    connect(this, SIGNAL(installationStarted()), ProgressCoordninator::instance(), SLOT(reset()));
    disconnect(this, SIGNAL(uninstallationStarted()), ProgressCoordninator::instance(), SLOT(reset()));
    connect(this, SIGNAL(uninstallationStarted()), ProgressCoordninator::instance(), SLOT(reset()));
}

QString InstallerPrivate::installerBinaryPath() const
{
    return qApp->applicationFilePath();
}

bool InstallerPrivate::isInstaller() const
{
    return m_magicBinaryMarker == MagicInstallerMarker;
}

bool InstallerPrivate::isUninstaller() const
{
    return m_magicBinaryMarker == MagicUninstallerMarker;
}

bool InstallerPrivate::isUpdater() const
{
    return m_magicBinaryMarker == MagicUpdaterMarker;
}

bool InstallerPrivate::isPackageManager() const
{
    return m_magicBinaryMarker == MagicPackageManagerMarker;
}

bool InstallerPrivate::statusCanceledOrFailed() const
{
    return m_status == Installer::Canceled
        || m_status == Installer::Failure;
}

void InstallerPrivate::setStatus(int status)
{
    if (m_status != status) {
        m_status = status;
        emit q->statusChanged(Installer::Status(m_status));
    }
}

QString InstallerPrivate::replaceVariables(const QString &str) const
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
        res += q->value(name);
        pos = pos2 + 1;
    }
    res += str.mid(pos);
    return res;
}

QByteArray InstallerPrivate::replaceVariables(const QByteArray &ba) const
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
        res += q->value(name).toLocal8Bit();
        pos = pos2 + 1;
    }
    res += ba.mid(pos);
    return res;
}

/*!
 Creates an update operation owned by the installer, not by any component.
 \internal
 */
KDUpdater::UpdateOperation* InstallerPrivate::createOwnedOperation(const QString &type)
{
    KDUpdater::UpdateOperation* const op = KDUpdater::UpdateOperationFactory::instance().create(type);
    ownedOperations.push_back(op);
    return op;
}

QString InstallerPrivate::uninstallerName() const
{
    QString filename = m_installerSettings->uninstallerName();
#if defined(Q_WS_MAC)
    if (QFileInfo(QCoreApplication::applicationDirPath() + QLatin1String("/../..")).isBundle())
        filename += QLatin1String(".app/Contents/MacOS/") + filename;
#elif defined(Q_OS_WIN)
    filename += QLatin1String(".exe");
#endif
    return QString::fromLatin1("%1/%2").arg(targetDir()).arg(filename);
}

void InstallerPrivate::readUninstallerIniFile(const QString &targetDir)
{
    const QString iniPath = targetDir + QLatin1Char('/') + m_installerSettings->uninstallerIniFile();
    QSettings cfg(iniPath, QSettings::IniFormat);
    const QVariantHash vars = cfg.value(QLatin1String("Variables")).toHash();
    QHash<QString, QVariant>::ConstIterator it = vars.constBegin();
    while (it != vars.constEnd()) {
        m_vars.insert(it.key(), it.value().toString());
        ++it;
    }
}

void InstallerPrivate::stopProcessesForUpdates(const QList<Component*> &components)
{
    QStringList processList;
    foreach (const Component* const i, components)
        processList << q->replaceVariables(i->stopProcessForUpdateRequests());

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
            q->setCanceled();
            throw Error(tr("Installation canceled by user"));
        }
    }
}

int InstallerPrivate::countProgressOperations(const QList<KDUpdater::UpdateOperation*> &operations)
{
    int operationCount = 0;
    QList<KDUpdater::UpdateOperation*>::const_iterator oIt;
    for (oIt = operations.constBegin(); oIt != operations.constEnd(); oIt++) {
        KDUpdater::UpdateOperation* const operation = *oIt;
        QObject* const operationObject = dynamic_cast< QObject* >(operation);
        if (operationObject != 0) {
            const QMetaObject* const mo = operationObject->metaObject();
            if (mo->indexOfSignal(QMetaObject::normalizedSignature("progressChanged(double)")) > -1)
                operationCount++;
        }
    }
    return operationCount;
}

int InstallerPrivate::countProgressOperations(const QList<Component*> &components)
{
    int operationCount = 0;
    foreach (Component* component, components)
        operationCount += countProgressOperations(component->operations());

    return operationCount;
}

void InstallerPrivate::connectOperationToInstaller(KDUpdater::UpdateOperation* const operation,
    double progressOperationPartSize)
{
    Q_ASSERT(progressOperationPartSize);
    QObject* const operationObject = dynamic_cast< QObject* >(operation);
    if (operationObject != 0) {
        const QMetaObject* const mo = operationObject->metaObject();
        if (mo->indexOfSignal(QMetaObject::normalizedSignature("outputTextChanged(QString)")) > -1) {
            connect(operationObject, SIGNAL(outputTextChanged(QString)),
                    ProgressCoordninator::instance(), SLOT(emitDetailTextChanged(QString)));
        }

        if (mo->indexOfSlot(QMetaObject::normalizedSignature("cancelOperation()")) > -1)
            connect(q, SIGNAL(installationInterrupted()), operationObject, SLOT(cancelOperation()));

        if (mo->indexOfSignal(QMetaObject::normalizedSignature("progressChanged(double)")) > -1) {
            ProgressCoordninator::instance()->registerPartProgress(operationObject,
                SIGNAL(progressChanged(double)), progressOperationPartSize);
        }
    }
}

KDUpdater::UpdateOperation* InstallerPrivate::createPathOperation(const QFileInfo &fileInfo,
    const QString &componentName)
{
    const bool isDir = fileInfo.isDir();
    // create an operation with the dir/ file as target, it will get deleted on undo
    KDUpdater::UpdateOperation *op = createOwnedOperation(QLatin1String(isDir
        ? "Mkdir" : "Copy"));
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
void InstallerPrivate::registerPathesForUninstallation(
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
        KDUpdater::UpdateOperation* const op = createPathOperation(fi, componentName);
        if (fi.isDir()) {
            op->setValue(QLatin1String("forceremoval"), wipe ? QLatin1String("true")
                : QLatin1String("false"));
        }
        addPerformed(op);

        // get recursive afterwards
        if (fi.isDir() && !wipe) {
            QDirIterator dirIt(path, QDir::Hidden | QDir::AllEntries | QDir::System
                | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
            while (dirIt.hasNext()) {
                dirIt.next();
                addPerformed(createPathOperation(dirIt.fileInfo(), componentName));
            }
        }
    }
}

void InstallerPrivate::writeUninstaller(QVector<KDUpdater::UpdateOperation*> performedOperations)
{
    bool gainedAdminRights = false;
    QTemporaryFile tempAdminFile(targetDir() + QString::fromLatin1("/testjsfdjlkdsjflkdsjfldsjlfds")
        + QString::number(qrand() % 1000));
    if (!tempAdminFile.open() || !tempAdminFile.isWritable()) {
        q->gainAdminRights();
        gainedAdminRights = true;
    }

    verbose() << "QInstaller::InstallerPrivate::writeUninstaller uninstaller=" << uninstallerName()
        << std::endl;

    // create the directory containing the uninstaller (like a bundle structor, on Mac...)
    KDUpdater::UpdateOperation* op = createOwnedOperation(QLatin1String("Mkdir"));
    op->setArguments(QStringList() << QFileInfo(uninstallerName()).path());
    performOperationThreaded(op, Backup);
    performOperationThreaded(op);
    performedOperations.push_back(op);

    {
        // write current state (variables) to the uninstaller ini file
        const QString iniPath = targetDir() + QLatin1Char('/')
            + m_installerSettings->uninstallerIniFile();
        QSettings cfg(iniPath, QSettings::IniFormat);
        QVariantHash vars;
        QHash<QString, QString>::ConstIterator it = m_vars.constBegin();
        while (it != m_vars.constEnd()) {
            const QString &key = it.key();
            if (key != QLatin1String("RunProgramDescription") && key != QLatin1String("RunProgram"))
                vars.insert(key, it.value());
            ++it;
        }
        cfg.setValue(QLatin1String("Variables"), vars);
        cfg.sync();
        if (cfg.status() != QSettings::NoError) {
            const QString reason = cfg.status() == QSettings::AccessError ? tr("Access error")
                : tr("Format error");
            throw Error(tr("Could not write installer configuration to %1: %2").arg(iniPath, reason));
        }
    }

#ifdef Q_WS_MAC
    // if it is a bundle, we need some stuff in it...
    if (isInstaller()
        && QFileInfo(QCoreApplication::applicationDirPath() + QLatin1String("/../..")).isBundle()) {
        op = createOwnedOperation(QLatin1String("Copy"));
        op->setArguments(QStringList() << (QCoreApplication::applicationDirPath()
            + QLatin1String("/../PkgInfo")) << (QFileInfo(uninstallerName()).path()
            + QLatin1String("/../PkgInfo")));
        performOperationThreaded(op, Backup);
        performOperationThreaded(op);

        op = createOwnedOperation(QLatin1String("Copy"));
        op->setArguments(QStringList() << (QCoreApplication::applicationDirPath()
            + QLatin1String("/../Info.plist")) << (QFileInfo(uninstallerName()).path()
            + QLatin1String("/../Info.plist")));
        performOperationThreaded(op, Backup);
        performOperationThreaded(op);

        verbose() << "Checking for qt_menu.nib" << std::endl;
        QString sourceDirName = QCoreApplication::applicationDirPath()
            + QLatin1String("/../Resources/qt_menu.nib");
        if (QFileInfo(sourceDirName).exists()) {
            verbose() << "qt_menu.nib has been found. Isn't it great?" << std::endl;
            QString targetDirName = QFileInfo(QFileInfo(uninstallerName()).path()
                + QLatin1String("/../Resources/qt_menu.nib")).absoluteFilePath();

            // IFW has been built with a static Cocoa Qt. The app bundle must contain the qt_menu.nib.
            // ### use the CopyDirectory operation in 1.1
            op = createOwnedOperation(QLatin1String("Mkdir"));
            op->setArguments(QStringList() << targetDirName);
            if (!op->performOperation()) {
                verbose() << "ERROR in Mkdir operation: " << op->errorString() << std::endl;
            }

            QDir sourceDir(sourceDirName);
            foreach (const QString &filename, sourceDir.entryList(QDir::Files)) {
                QString src = sourceDirName + QLatin1String("/") + filename;
                QString dst = targetDirName + QLatin1String("/") + filename;
                op = createOwnedOperation(QLatin1String("Copy"));
                op->setArguments(QStringList() << src << dst);
                if (!op->performOperation())
                    verbose() << "ERROR in Copy operation: copy " << src << " to " << dst << std::endl
                              << "error message: " << op->errorString() << std::endl;
            }
        }

        // patch the Info.plist while copying it
        QFile sourcePlist(QCoreApplication::applicationDirPath() + QLatin1String("/../Info.plist"));
        openForRead(&sourcePlist, sourcePlist.fileName());
        QFile targetPlist(QFileInfo(uninstallerName()).path() + QLatin1String("/../Info.plist"));
        openForWrite(&targetPlist, targetPlist.fileName());

        QTextStream in(&sourcePlist);
        QTextStream out(&targetPlist);

        while (!in.atEnd())
        {
            QString line = in.readLine();
            line = line.replace(QLatin1String("<string>")
                + QFileInfo(QCoreApplication::applicationFilePath()).baseName()
                + QLatin1String("</string>"), QLatin1String("<string>")
                + QFileInfo(uninstallerName()).baseName() + QLatin1String("</string>"));
            out << line << endl;
        }

        op = createOwnedOperation(QLatin1String("Mkdir"));
        op->setArguments(QStringList() << (QFileInfo(QFileInfo(uninstallerName()).path()).path()
            + QLatin1String("/Resources")));
        performOperationThreaded(op, Backup);
        performOperationThreaded(op);

        const QString icon = QFileInfo(QCoreApplication::applicationFilePath()).baseName()
            + QLatin1String(".icns");
        op = createOwnedOperation(QLatin1String("Copy"));
        op->setArguments(QStringList() << (QCoreApplication::applicationDirPath()
            + QLatin1String("/../Resources/") + icon) << (QFileInfo(uninstallerName()).path()
            + QLatin1String("/../Resources/") + icon));
        performOperationThreaded(op, Backup);
        performOperationThreaded(op);

        // finally, copy everything within Frameworks and plugins
        if (QDir(QCoreApplication::applicationDirPath() + QLatin1String("/../Frameworks")).exists()) {
            copyDirectoryContents(QCoreApplication::applicationDirPath()
                + QLatin1String("/../Frameworks"), QFileInfo(uninstallerName()).path()
                + QLatin1String("/../Frameworks"));
        }

        if (QDir(QCoreApplication::applicationDirPath() + QLatin1String("/../plugins")).exists()) {
            copyDirectoryContents(QCoreApplication::applicationDirPath()
                + QLatin1String("/../plugins"), QFileInfo(uninstallerName()).path()
                + QLatin1String("/../plugins"));
        }
    }
#endif

    QFile in;
    if (isInstaller() || isUninstaller() || isPackageManager())
        in.setFileName(installerBinaryPath());
    else
        in.setFileName(uninstallerName());  // we're the updater

    const QString installerBaseBinary = q->replaceVariables(m_installerBaseBinaryUnreplaced);
    if (!installerBaseBinary.isEmpty())
        verbose() << "Got a replacement installer base binary: " << installerBaseBinary << std::endl;

    const bool haveSeparateExec = QFile::exists(installerBaseBinary) && !installerBaseBinary.isEmpty();
    verbose() << "Need to restart after exit: " << haveSeparateExec << " "
        << qPrintable(installerBaseBinary) << std::endl;

#ifdef Q_WS_WIN
    KDSaveFile out(uninstallerName() + QLatin1String(".org"));
#else
    KDSaveFile out(uninstallerName());
#endif

    try {
        verbose() << "CREATING UNINSTALLER " << performedOperations.size() << std::endl;

        QFile* execIn = &in;
        qint64 execSize = 0;
        QFile ibbIn;
        if (haveSeparateExec) {
            ibbIn.setFileName(installerBaseBinary);
            openForRead(&ibbIn, ibbIn.fileName());
            execIn = &ibbIn;
            execSize = ibbIn.size();
        }

        openForRead(&in, in.fileName());
        openForWrite(&out, out.fileName());

        const qint64 magicCookiePos = findMagicCookie(&in);
        if (magicCookiePos < 0) {
            throw Error(QObject::tr("Can not find the magic cookie in file %1. Are you sure this "
                "is a valid installer?").arg(installerBinaryPath()));
        }

        if (!in.seek(magicCookiePos - 7 * sizeof(qint64))) {
            throw Error(QObject::tr("Failed to seek in file %1: %2").arg(installerBinaryPath(),
                in.errorString()));
        }

        qint64 resourceStart = retrieveInt64(&in);
        const qint64 resourceLength = retrieveInt64(&in);
        Q_ASSERT(resourceLength >= 0);
        const qint64 _operationsStart = retrieveInt64(&in);
        Q_UNUSED(_operationsStart);
        Q_ASSERT(_operationsStart >= 0);
        const qint64 _operationsLength = retrieveInt64(&in);
        Q_UNUSED(_operationsLength);
        Q_ASSERT(_operationsLength >= 0);
        const qint64 count = retrieveInt64(&in); // atm always "1"
        Q_ASSERT(count == 1); // we have just 1 resource atm
        const qint64 dataBlockSize = retrieveInt64(&in);
        const qint64 dataBlockStart = magicCookiePos + sizeof(qint64) - dataBlockSize;
        resourceStart += dataBlockStart;
        if (!haveSeparateExec)
            execSize = dataBlockStart;

        // consider size difference between old and new installerbase executable (if updated)
        const qint64 newResourceStart = execSize;
        const qint64 magicmarker = retrieveInt64(&in);
        Q_UNUSED(magicmarker);
        Q_ASSERT(magicmarker == MagicInstallerMarker || magicmarker == MagicUninstallerMarker);



        if (!execIn->seek(0)) {
            throw Error(QObject::tr("Failed to seek in file %1: %2").arg(execIn->fileName(),
                execIn->errorString()));
        }
        appendData(&out, execIn, execSize);

        // copy the first few bytes to take the executable+resources over to the uninstaller.
        if (! in.seek(resourceStart)) {
            throw Error(QObject::tr("Failed to seek in file %1: %2").arg(in.fileName(),
                in.errorString()));
        }
        Q_ASSERT(in.pos() == resourceStart && out.pos() == execSize);

        appendData(&out, &in, resourceLength);
        const qint64 uninstallerDataBlockStart = out.pos();
        // compared to the installer we do not have component data but details about
        // the performed operations during the installation to allow to undo them.
        const qint64 operationsStart = out.pos();
        appendInt64(&out, performedOperations.count());
        foreach (KDUpdater::UpdateOperation *op, performedOperations) {
            // the installer can't be put into XML, remove it first
            op->clearValue(QLatin1String("installer"));

            appendString(&out, op->name());
            appendString(&out, op->toXml().toString());

            // for the ui not to get blocked
            qApp->processEvents();
        }
        appendInt64(&out, performedOperations.count());
        const qint64 operationsEnd = out.pos();

        // we dont save any component-indexes.
        const qint64 numComponents = 0;
        appendInt64(&out, numComponents); // for the indexes
        // we dont save any components.
        const qint64 compIndexStart = out.pos();
        appendInt64(&out, numComponents); // and 2 times number of components,
        appendInt64(&out, numComponents); // one before and one after the components
        const qint64 compIndexEnd = out.pos();

        appendInt64Range(&out, Range<qint64>::fromStartAndEnd(compIndexStart, compIndexEnd)
            .moved(-uninstallerDataBlockStart));
        appendInt64Range(&out, Range<qint64>::fromStartAndLength(newResourceStart, resourceLength)
            .moved(-uninstallerDataBlockStart));
        appendInt64Range(&out, Range<qint64>::fromStartAndEnd(operationsStart, operationsEnd)
            .moved(-uninstallerDataBlockStart));
        appendInt64(&out, count);
        //data block size, from end of .exe to end of file
        appendInt64(&out, out.pos() + 3 * sizeof(qint64) - uninstallerDataBlockStart);
        appendInt64(&out, MagicUninstallerMarker);
        appendInt64(&out, MagicCookie);

        out.setPermissions(out.permissions() | QFile::WriteUser | QFile::ReadGroup | QFile::ReadOther
            | QFile::ExeOther | QFile::ExeGroup | QFile::ExeUser);

        if (!out.commit(KDSaveFile::OverwriteExistingFile)) {
            throw Error(tr("Could not write uninstaller to %1: %2").arg(uninstallerName(),
                out.errorString()));
        }

        //delete the installerbase binary temporarily installed for the uninstaller update
        if (haveSeparateExec) {
            QFile tmp(installerBaseBinary);
            // Is there anything more sensible we can do with this error? I think not.
            // It's not serious enough for throwing/aborting.
            if (!tmp.remove()) {
                verbose() << "Could not remove installerbase binary (" << installerBaseBinary
                    << ") after updating the uninstaller: " << tmp.errorString() << std::endl;
            }
            m_installerBaseBinaryUnreplaced.clear();
        }

#ifdef Q_WS_WIN
        deferredRename(out.fileName(), QFileInfo(uninstallerName()).fileName(),
            haveSeparateExec && !isInstaller());
#else
        verbose() << " preparing restart " << std::endl;
        if (haveSeparateExec && !isInstaller())
            KDSelfRestarter::setRestartOnQuit(true);
#endif
    } catch (const Error &err) {
        setStatus(Installer::Failure);
        if (gainedAdminRights)
            q->dropAdminRights();
        m_needToWriteUninstaller = false;
        throw err;
    }

    if (gainedAdminRights)
        q->dropAdminRights();

    commitSessionOperations();

    m_needToWriteUninstaller = false;
}

QString InstallerPrivate::registerPath() const
{
#ifdef Q_OS_WIN
    QString productName = m_vars.value(QLatin1String("ProductName"));
    if (productName.isEmpty())
        throw Error(tr("ProductName should be set"));

    QString path = QLatin1String("HKEY_CURRENT_USER");
    if (m_vars.value(QLatin1String("AllUsers")) == QLatin1String("true"))
        path = QLatin1String("HKEY_LOCAL_MACHINE");

    return path + QLatin1String("\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\")
        + productName;
#else
    return QString();
#endif
}

void InstallerPrivate::registerUninstaller()
{
#ifdef Q_OS_WIN
    QSettings settings(registerPath(), QSettings::NativeFormat);
    settings.setValue(QLatin1String("DisplayName"), m_vars.value(QLatin1String("ProductName")));
    settings.setValue(QLatin1String("DisplayVersion"), m_vars.value(QLatin1String("ProductVersion")));
    const QString uninstaller = QDir::toNativeSeparators(uninstallerName());
    settings.setValue(QLatin1String("DisplayIcon"), uninstaller);
    settings.setValue(QLatin1String("Publisher"), m_vars.value(QLatin1String("Publisher")));
    settings.setValue(QLatin1String("UrlInfoAbout"), m_vars.value(QLatin1String("Url")));
    settings.setValue(QLatin1String("Comments"), m_vars.value(QLatin1String("Title")));
    settings.setValue(QLatin1String("InstallDate"), QDateTime::currentDateTime().toString());
    settings.setValue(QLatin1String("InstallLocation"), QDir::toNativeSeparators(targetDir()));
    settings.setValue(QLatin1String("UninstallString"), uninstaller);
    settings.setValue(QLatin1String("ModifyPath"), uninstaller + QLatin1String(" --manage-packages"));
    settings.setValue(QLatin1String("EstimatedSize"), QFileInfo(installerBinaryPath()).size());
    settings.setValue(QLatin1String("NoModify"), 1);    // TODO: set to 0 and support modify
    settings.setValue(QLatin1String("NoRepair"), 1);
#endif
}

void InstallerPrivate::unregisterUninstaller()
{
#ifdef Q_OS_WIN
    QSettings settings(registerPath(), QSettings::NativeFormat);
    settings.remove(QString());
#endif
}

void InstallerPrivate::runInstaller()
{
    try {
        setStatus(Installer::Running);
        emit installationStarted(); //resets also the ProgressCoordninator

        //to have some progress for writeUninstaller
        ProgressCoordninator::instance()->addReservePercentagePoints(1);

        const QString target = targetDir();
        if (target.isEmpty())
            throw Error(tr("Variable 'TargetDir' not set."));

        // add the operation to create the target directory
        bool installToAdminDirectory = false;
        if (!QDir(target).exists()) {
            QScopedPointer<KDUpdater::UpdateOperation> mkdirOp(createOwnedOperation(QLatin1String("Mkdir")));
            mkdirOp->setValue(QLatin1String("forceremoval"), true);
            Q_ASSERT(mkdirOp.data());
            mkdirOp->setArguments(QStringList() << target);
            performOperationThreaded(mkdirOp.data(), Backup);
            if (!performOperationThreaded(mkdirOp.data())) {
                // if we cannot create the target dir, we try to activate the admin rights
                installToAdminDirectory = true;
                if (!q->gainAdminRights() || !performOperationThreaded(mkdirOp.data()))
                    throw Error(mkdirOp->errorString());
            }
            QString remove = q->value(QLatin1String("RemoveTargetDir"));
            if (QVariant(remove).toBool())
                addPerformed(mkdirOp.take());
        } else {
            QTemporaryFile tempAdminFile(target + QLatin1String("/adminrights"));
            if (!tempAdminFile.open() || !tempAdminFile.isWritable())
                installToAdminDirectory = q->gainAdminRights();
        }

        // to show that there was some work
        ProgressCoordninator::instance()->addManualPercentagePoints(1);

        ProgressCoordninator::instance()->emitLabelAndDetailTextChanged(tr("Preparing the installation..."));
        const QList<Component*> componentsToInstall = q->componentsToInstall(AllMode);
        verbose() << "Install size: " << componentsToInstall.size() << " components" << std::endl;

        // check if we need admin rights and ask before the action happens
        for (QList<Component*>::const_iterator it = componentsToInstall.begin();
            it != componentsToInstall.end() && !installToAdminDirectory; ++it) {
                Component* const component = *it;
                bool requiredAdmin = false;

                if (component->value(QLatin1String("RequiresAdminRights"),
                    QLatin1String("false")) == QLatin1String("true")) {
                        requiredAdmin = q->gainAdminRights();
                }

                if (requiredAdmin) {
                    q->dropAdminRights();
                    break;
                }
        }

        const double downloadPartProgressSize = double(1) / 3;
        double componentsInstallPartProgressSize = double(2) / 3;
        const int downloadedArchivesCount = q->downloadNeededArchives(AllMode,
            downloadPartProgressSize);

        //if there was no download we have the whole progress for installing components
        if (!downloadedArchivesCount) {
             //componentsInstallPartProgressSize + downloadPartProgressSize;
            componentsInstallPartProgressSize = double(1);
        }

        // put the installed packages info into the target dir
        KDUpdater::PackagesInfo* const packages = m_app->packagesInfo();
        packages->setFileName(componentsXmlPath());
        packages->setApplicationName(m_installerSettings->applicationName());
        packages->setApplicationVersion(m_installerSettings->applicationVersion());

        stopProcessesForUpdates(componentsToInstall);

        const int progressOperationCount = countProgressOperations(componentsToInstall);
        double progressOperationSize = componentsInstallPartProgressSize / progressOperationCount;

        foreach (Component *component, componentsToInstall)
            q->installComponent(component, progressOperationSize);


        emit q->titleMessageChanged(tr("Creating Uninstaller"));

        m_app->packagesInfo()->writeToDisk();

        writeUninstaller(m_performedOperationsOld + m_performedOperationsCurrentSession);
        registerUninstaller();

        //this is the reserved one from the beginning
        ProgressCoordninator::instance()->addManualPercentagePoints(1);

        setStatus(Installer::Success);
        ProgressCoordninator::instance()->emitLabelAndDetailTextChanged(tr("\nInstallation finished!"));

        emit installationFinished();

        // disable the FSEngineClientHandler afterwards
        m_FSEngineClientHandler->setActive(false);
    } catch (const Error &err) {
        if (q->status() != Installer::Canceled) {
            setStatus(Installer::Failure);
            verbose() << "INSTALLER FAILED: " << err.message() << std::endl;
            MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(),
                QLatin1String("installationError"), tr("Error"), err.message());
            verbose() << "ROLLING BACK operations=" << m_performedOperationsCurrentSession.count()
                << std::endl;
        }

        q->rollBackInstallation();

        ProgressCoordninator::instance()->emitLabelAndDetailTextChanged(tr("Installation aborted"));
        emit installationFinished();

        // disable the FSEngineClientHandler afterwards
        m_FSEngineClientHandler->setActive(false);
        throw;
    }
}

void InstallerPrivate::deleteUninstaller()
{
#ifdef Q_OS_WIN
    // Since Windows does not support that the uninstaller deletes itself we  have to go with a
    // rather dirty hack. What we do is to create a batchfile that will try to remove the uninstaller
    // once per second. Then we start that batchfile detached, finished our job and close outself.
    // Once that's done the batchfile will succeed in deleting our uninstall.exe and, if the
    // installation directory was created but us and if it's empty after the uninstall, deletes
    // the installation-directory.
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

    if (!QProcess::startDetached(QLatin1String("cscript"), arguments, QDir::rootPath()))
        throw Error(tr("Cannot start uninstall"));
#else
    // every other platform has no problem if we just delete ourself now
    QFile uninstaller(QFileInfo(installerBinaryPath()).absoluteFilePath());
    uninstaller.remove();
#ifdef Q_WS_MAC
    const QLatin1String cdUp("/../../..");
    if (QFileInfo(QFileInfo(installerBinaryPath() + cdUp).absoluteFilePath()).isBundle()) {
        removeDirectoryThreaded(QFileInfo(installerBinaryPath() + cdUp).absoluteFilePath());
        QFile::remove(QFileInfo(installerBinaryPath() + cdUp).absolutePath()
            + QLatin1String("/") + configurationFileName());
    }
    else
#endif
#endif
    {
        // finally remove the components.xml, since it still exists now
        QFile::remove(QFileInfo(installerBinaryPath()).absolutePath() + QLatin1String("/")
            + configurationFileName());
    }
}

void InstallerPrivate::runPackageUpdater()
{
    try {
        if (m_completeUninstall) {
            runUninstaller();
            return;
        }

        setStatus(Installer::Running);
        emit installationStarted(); //resets also the ProgressCoordninator

        //to have some progress for the cleanup/write component.xml step
        ProgressCoordninator::instance()->addReservePercentagePoints(1);

        if (!QFileInfo(installerBinaryPath()).isWritable())
            q->gainAdminRights();

        KDUpdater::PackagesInfo* const packages = m_app->packagesInfo();
        packages->setFileName(componentsXmlPath());
        packages->setApplicationName(m_installerSettings->applicationName());
        packages->setApplicationVersion(m_installerSettings->applicationVersion());

        const QString packagesXml = componentsXmlPath();
        if (!QFile(packagesXml).open(QIODevice::Append))
            q->gainAdminRights();

        // first check, if we need admin rights for the installation part
        QList<Component*> availableComponents = q->components(true, AllMode);
        foreach (Component *component, availableComponents) {
            if (!component->uninstallationRequested())
                continue;

            bool requiredAdmin = false;
            // check if we need admin rights and ask before the action happens
            if (component->value(QLatin1String("RequiresAdminRights"),
                QLatin1String("false")) == QLatin1String("true")) {
                    requiredAdmin = q->gainAdminRights();
            }

            if (requiredAdmin) {
                q->dropAdminRights();
                break;
            }
        }

        //to have 1/5 for undoOperationProgressSize and 2/5 for componentsInstallPartProgressSize
        const double downloadPartProgressSize = double(2) / 5;
        // following, we download the needed archives
        q->downloadNeededArchives(AllMode, downloadPartProgressSize);

        ProgressCoordninator::instance()->emitLabelAndDetailTextChanged(tr("Removing deselected components..."));
        QVector< KDUpdater::UpdateOperation* > nonRevertedOperations;

        QList<KDUpdater::UpdateOperation*> undoOperations;
        for (int i = m_performedOperationsOld.count() - 1; i >= 0; --i) {
            KDUpdater::UpdateOperation* const currentOperation = m_performedOperationsOld[i];

            const QString componentName = currentOperation->value(QLatin1String("component")).toString();
            Component* comp = q->componentByName(componentName);

            // if we're _not_ removing everything an this component is still selected, -> next
            if (comp == 0 || comp->isSelected()) {
                nonRevertedOperations.push_front(currentOperation);
                continue;
            }
            undoOperations.append(currentOperation);
        }

        double undoOperationProgressSize = 0;
        int progressUndoOperationCount = 0;
        double progressUndoOperationSize = 0;
        double componentsInstallPartProgressSize = double(2) / 5;
        if (undoOperations.count() > 0) {
            componentsInstallPartProgressSize = double(2) / 5;
            undoOperationProgressSize = double(1) / 5;
            progressUndoOperationCount = countProgressOperations(undoOperations);
            progressUndoOperationSize = undoOperationProgressSize / progressUndoOperationCount;
        } else {
            componentsInstallPartProgressSize = double(3) / 5;
        }

        QSet<Component*> uninstalledComponents;
        foreach (KDUpdater::UpdateOperation* const currentOperation, undoOperations) {
            if (statusCanceledOrFailed())
                throw Error(tr("Installation canceled by user"));
            connectOperationToInstaller(currentOperation, progressUndoOperationSize);

            const QString componentName = currentOperation->value(QLatin1String("component")).toString();
            Component* comp = q->componentByName(componentName);

            verbose() << "undo operation=" << currentOperation->name() << std::endl;
            uninstalledComponents |= comp;

            const bool becameAdmin = !m_FSEngineClientHandler->isActive()
                && currentOperation->value(QLatin1String("admin")).toBool() && q->gainAdminRights();

            bool ignoreError = false;
            performOperationThreaded(currentOperation, Undo);
            bool ok = currentOperation->error() == KDUpdater::UpdateOperation::NoError
                || componentName == QLatin1String("");
            while (!ok && !ignoreError && q->status() != Installer::Canceled) {
                verbose() << QString(QLatin1String("operation '%1' with arguments: '%2' failed: %3"))
                    .arg(currentOperation->name(), currentOperation->arguments()
                    .join(QLatin1String("; ")), currentOperation->errorString()) << std::endl;;

                const QMessageBox::StandardButton button =
                    MessageBoxHandler::warning(MessageBoxHandler::currentBestSuitParent(),
                    QLatin1String("installationErrorWithRetry"), tr("Installer Error"),
                    tr("Error during installation process:\n%1").arg(currentOperation->errorString()),
                    QMessageBox::Retry | QMessageBox::Ignore, QMessageBox::Retry);

                if (button == QMessageBox::Retry) {
                    performOperationThreaded(currentOperation, Undo);
                    ok = currentOperation->error() == KDUpdater::UpdateOperation::NoError;
                }
                else if (button == QMessageBox::Ignore)
                    ignoreError = true;
            }

            if (becameAdmin)
                q->dropAdminRights();

            delete currentOperation;
        }

        const QList<Component*> allComponents = q->components(true, AllMode);
        foreach (Component *component, allComponents) {
            if (!component->isSelected())
                uninstalledComponents |= component;
        }

        foreach (Component* component, uninstalledComponents) {
            component->setUninstalled();
            packages->removePackage(component->name());
        }

        // these are all operations left: those which were not reverted
        m_performedOperationsOld = nonRevertedOperations;

        //write components.xml in case the user cancels the update
        packages->writeToDisk();
        ProgressCoordninator::instance()->emitLabelAndDetailTextChanged(tr("Preparing the installation..."));

        const QList<Component*> componentsToInstall = q->componentsToInstall(q->runMode());

        verbose() << "Install size: " << componentsToInstall.size() << " components " << std::endl;

        stopProcessesForUpdates(componentsToInstall);

        int progressOperationCount = countProgressOperations(componentsToInstall);
        double progressOperationSize = componentsInstallPartProgressSize / progressOperationCount;

        foreach (Component *component, componentsToInstall)
            q->installComponent(component, progressOperationSize);

        packages->writeToDisk();

        emit q->titleMessageChanged(tr("Creating Uninstaller"));

        commitSessionOperations(); //end session, move ops to "old"
        m_needToWriteUninstaller = true;

        //this is the reserved one from the beginning
        ProgressCoordninator::instance()->addManualPercentagePoints(1);

        setStatus(Installer::Success);
        ProgressCoordninator::instance()->emitLabelAndDetailTextChanged(tr("\nInstallation finished!"));
        emit installationFinished();

        // disable the FSEngineClientHandler afterwards
        m_FSEngineClientHandler->setActive(false);
    } catch(const Error &err) {
        if (q->status() != Installer::Canceled) {
            setStatus(Installer::Failure);
            verbose() << "INSTALLER FAILED: " << err.message() << std::endl;
            MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(),
                QLatin1String("installationError"), tr("Error"), err.message());
            verbose() << "ROLLING BACK operations=" << m_performedOperationsCurrentSession.count()
                << std::endl;
        }

        q->rollBackInstallation();

        ProgressCoordninator::instance()->emitLabelAndDetailTextChanged(tr("Installation aborted"));
        emit installationFinished();

        // disable the FSEngineClientHandler afterwards
        m_FSEngineClientHandler->setActive(false);
        throw;
    }
}

void InstallerPrivate::runUninstaller()
{
    try {
        emit uninstallationStarted();

        if (!QFileInfo(installerBinaryPath()).isWritable())
            q->gainAdminRights();

        const QString packagesXml = componentsXmlPath();
        if (!QFile(packagesXml).open(QIODevice::Append))
            q->gainAdminRights();

        KDUpdater::PackagesInfo* const packages = m_app->packagesInfo();
        packages->setFileName(componentsXmlPath());
        packages->setApplicationName(m_installerSettings->applicationName());
        packages->setApplicationVersion(m_installerSettings->applicationVersion());

        // iterate over all components - if they're all marked for uninstall, it's a complete uninstall
        bool allMarkedForUninstall = true;

        QList<KDUpdater::UpdateOperation*> uninstallOperations;
        QVector<KDUpdater::UpdateOperation*> nonRevertedOperations;

        // just rollback all operations done before
        for (int i = m_performedOperationsOld.count() - 1; i >= 0; --i) {
            KDUpdater::UpdateOperation* const operation = m_performedOperationsOld[i];

            const QString componentName = operation->value(QLatin1String("component")).toString();
            const Component* const comp = q->componentByName(componentName);

            // if we're _not_ removing everything an this component is still selected, -> next
            if (!m_completeUninstall && (comp == 0 || !comp->isSelected())) {
                nonRevertedOperations.push_front(operation);
                continue;
            }
            uninstallOperations.append(operation);
        }

        const int progressUninstallOperationCount = countProgressOperations(uninstallOperations);
        const double progressUninstallOperationSize = double(1) / progressUninstallOperationCount;

        foreach (KDUpdater::UpdateOperation* const currentOperation, uninstallOperations) {
            if (statusCanceledOrFailed())
                throw Error(tr("Installation canceled by user"));

            connectOperationToInstaller(currentOperation, progressUninstallOperationSize);
            verbose() << "undo operation=" << currentOperation->name() << std::endl;

            const QString componentName = currentOperation->value(QLatin1String("component")).toString();

            const bool becameAdmin = !m_FSEngineClientHandler->isActive()
                && currentOperation->value(QLatin1String("admin")).toBool() && q->gainAdminRights();

            bool ignoreError = false;
            performOperationThreaded(currentOperation, Undo);
            bool ok = currentOperation->error() == KDUpdater::UpdateOperation::NoError
                || componentName == QLatin1String("");
            while (!ok && !ignoreError && q->status() != Installer::Canceled) {
                verbose() << QString(QLatin1String("operation '%1' with arguments: '%2' failed: %3"))
                    .arg(currentOperation->name(), currentOperation->arguments()
                    .join(QLatin1String("; ")), currentOperation->errorString()) << std::endl;;
                const QMessageBox::StandardButton button =
                    MessageBoxHandler::warning(MessageBoxHandler::currentBestSuitParent(),
                    QLatin1String("installationErrorWithRetry"), tr("Installer Error"),
                    tr("Error during installation process:\n%1").arg(currentOperation->errorString()),
                    QMessageBox::Retry | QMessageBox::Ignore, QMessageBox::Retry);

                if (button == QMessageBox::Retry) {
                    performOperationThreaded(currentOperation, Undo);
                    ok = currentOperation->error() == KDUpdater::UpdateOperation::NoError;
                } else if (button == QMessageBox::Ignore)
                    ignoreError = true;
            }

            if (becameAdmin)
                q->dropAdminRights();

            if (!m_completeUninstall)
                delete currentOperation;
        }

        const QList<Component*> allComponents = q->components(true, AllMode);
        if (!m_completeUninstall) {
            foreach (Component *comp, allComponents) {
                if (comp->isSelected()) {
                    allMarkedForUninstall = false;
                } else {
                    comp->setUninstalled();
                    packages->removePackage(comp->name());
                }
            }
            m_completeUninstall = m_completeUninstall || allMarkedForUninstall;
        }

        const QString startMenuDir = m_vars.value(QLatin1String("StartMenuDir"));
        if (!startMenuDir.isEmpty()) {
            errno = 0;
            if (!QDir().rmdir(startMenuDir)) {
                verbose() << "Could not remove " << startMenuDir << " : "
                    << QLatin1String(strerror(errno)) << std::endl;
            } else {
                verbose() << "Startmenu dir not set" << std::endl;
            }
        }

        if (m_completeUninstall) {
            // this will also delete the TargetDir on Windows
            deleteUninstaller();
            foreach (Component *component, allComponents) {
                component->setUninstalled();
                packages->removePackage(component->name());
            }

            QString remove = q->value(QLatin1String("RemoveTargetDir"));
            if (QVariant(remove).toBool()) {
                // on !Windows, we need to remove TargetDir manually
                verbose() << "Complete Uninstallation is chosen" << std::endl;
                const QString target = targetDir();
                if (!target.isEmpty()) {
                    if (m_FSEngineClientHandler->isServerRunning() && !m_FSEngineClientHandler->isActive()) {
                        // we were root at least once, so we remove the target dir as root
                        q->gainAdminRights();
                        removeDirectoryThreaded(target, true);
                        q->dropAdminRights();
                    } else {
                        removeDirectoryThreaded(target, true);
                    }
                }
            }

            unregisterUninstaller();
            m_needToWriteUninstaller = false;
        } else {
            // rewrite the uninstaller with the operation we did not undo
            writeUninstaller(nonRevertedOperations);
        }

        setStatus(Installer::Success);
        ProgressCoordninator::instance()->emitLabelAndDetailTextChanged(tr("\nDeinstallation finished"));

        m_FSEngineClientHandler->setActive(false);
    } catch (const Error &err) {
        if (q->status() != Installer::Canceled) {
            setStatus(Installer::Failure);
            verbose() << "INSTALLER FAILED: " << err.message() << std::endl;
            MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(),
                QLatin1String("installationError"), tr("Error"), err.message());
            verbose() << "ROLLING BACK operations=" << m_performedOperationsCurrentSession.count()
                << std::endl;
        }

        ProgressCoordninator::instance()->emitLabelAndDetailTextChanged(tr("Installation aborted"));
        emit installationFinished();

        // disable the FSEngineClientHandler afterwards
        m_FSEngineClientHandler->setActive(false);
        throw;
    }
    emit uninstallationFinished();
}

}   // QInstaller
