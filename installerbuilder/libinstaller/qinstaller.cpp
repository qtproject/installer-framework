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
#include "qinstaller.h"
#include "qinstallergui.h"
#include "qinstallerglobal.h"
#include "qinstallercomponent.h"
#include "downloadarchivesjob.h"
#include "getrepositoriesmetainfojob.h"
#include "adminauthorization.h"
#include "messageboxhandler.h"
#include "progresscoordinator.h"

#include "common/installersettings.h"
#include "common/binaryformat.h"
#include "common/utils.h"

#include "fsengineclient.h"
#include "fsengineserver.h"

#include <QtCore/QBuffer>
#include <QtCore/QCoreApplication>
#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QDirIterator>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QHash>
#include <QtCore/QLocale>
#include <QtCore/QPointer>
#include <QtCore/QProcess>
#include <QtCore/QResource>
#include <QtCore/QSettings>
#include <QtCore/QVector>
#include <QtCore/QTemporaryFile>
#include <QtCore/QTranslator>
#include <QtCore/QUrl>
#include <QtCore/QThread>
#include <QtCore/QSet>
#include <QtGui/QDesktopServices>
#include <QtGui/QMessageBox>
#include <QtUiTools/QUiLoader>
#include <QtScript/QScriptEngine>
#include <QtScript/QScriptContext>
#include <QtScript/QScriptContextInfo>
#include <QFuture>
#include <QFutureWatcher>
#include <QtConcurrentRun>

#include <KDUpdater/KDUpdater>
#include <KDUpdater/Application>
#include <KDUpdater/PackagesInfo>

#include <KDToolsCore/KDSaveFile>
#include <KDToolsCore/KDSysInfo>
#include <KDToolsCore/KDSelfRestarter>

#include "common/errors.h"
#include "common/fileutils.h"

#include <cassert>
#include <cerrno>
#include <functional>
#include <memory>
#include <algorithm>

#ifdef Q_OS_WIN
#include "qt_windows.h"
#endif

using namespace QInstaller;

namespace {
    enum OperationType {
        Backup,
        Perform,
        Undo
    };

    static bool runOperation(KDUpdater::UpdateOperation *op, OperationType type) {
        switch (type)
        {
        case(Backup):
            op->backup();
            return true;
        case (Perform):
            return op->performOperation();
        case (Undo) :
            return op->undoOperation();
        }
        Q_ASSERT(!"unexpected operation type");
        return false;
    }

    template <typename T>
    void letTheUiRunTillFinished(const QFuture<T>& f) {
        QFutureWatcher<T> futureWatcher;
        futureWatcher.setFuture(f);
        QEventLoop loop;
        loop.connect(&futureWatcher, SIGNAL(finished()), SLOT(quit()), Qt::QueuedConnection);
        if (!f.isFinished())
            loop.exec();
    }

}

static bool performOperationThreaded(KDUpdater::UpdateOperation *op, OperationType type=Perform)
{
    QFuture<bool> future = QtConcurrent::run(runOperation, op, type);
    letTheUiRunTillFinished(future);
    return future.result();
}

static QScriptValue checkArguments(QScriptContext* context, int amin, int amax)
{
    if (context->argumentCount() < amin || context->argumentCount() > amax) {
        if (amin != amax) {
            return context->throwError(QObject::tr("Invalid arguments: %1 arguments given, %2 to "
                "%3 expected.").arg(QString::number(context->argumentCount()),
                QString::number(amin), QString::number(amax)));
        }
        return context->throwError(QObject::tr("Invalid arguments: %1 arguments given, %2 expected.")
            .arg(QString::number(context->argumentCount()), QString::number(amin)));
    }
    return QScriptValue();
}

#ifdef Q_WS_WIN
static void deferredRename(const QString& oldName, const QString& newName, bool restart = false)
{
    QString batchfile;

    QStringList arguments;
    arguments << QDir::toNativeSeparators(batchfile)
              << QDir::toNativeSeparators(oldName)
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

/*!
    Appends \a comp preceded by its dependencies to \a components. Makes sure components contains
    every component only once.
    \internal
*/
static void appendComponentAndMissingDependencies(QList<Component*>& components, Component* comp)
{
    if (comp == 0)
        return;

    const QList<Component*> deps = comp->installer()->missingDependencies(comp);
    for (QList<Component*>::const_iterator it = deps.begin(); it != deps.end(); ++it)
        appendComponentAndMissingDependencies(components, *it);
    if (!components.contains(comp))
        components.push_back(comp);
}

/*!
 Scriptable version of Installer::componentByName(QString).
 \sa Installer::componentByName
 */
QScriptValue QInstaller::qInstallerComponentByName(QScriptContext* context, QScriptEngine* engine)
{
    const QScriptValue check = checkArguments(context, 1, 1);
    if (check.isError())
        return check;

    // well... this is our "this" pointer
    Installer* const installer = dynamic_cast< Installer* >(engine->globalObject()
        .property(QLatin1String("installer")).toQObject());

    const QString name = context->argument(0).toString();
    Component* const c = installer->componentByName(name);
    return engine->newQObject(c);
}

QScriptValue QInstaller::qDesktopServicesOpenUrl(QScriptContext* context, QScriptEngine* engine)
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

QScriptValue QInstaller::qDesktopServicesDisplayName(QScriptContext* context, QScriptEngine* engine)
{
    Q_UNUSED(engine);
    const QScriptValue check = checkArguments(context, 1, 1);
    if (check.isError())
        return check;
    const QDesktopServices::StandardLocation location =
        static_cast< QDesktopServices::StandardLocation >(context->argument(0).toInt32());
    return QDesktopServices::displayName(location);
}

QScriptValue QInstaller::qDesktopServicesStorageLocation(QScriptContext* context, QScriptEngine* engine)
{
    Q_UNUSED(engine);
    const QScriptValue check = checkArguments(context, 1, 1);
    if (check.isError())
        return check;
    const QDesktopServices::StandardLocation location =
        static_cast< QDesktopServices::StandardLocation >(context->argument(0).toInt32());
    return QDesktopServices::storageLocation(location);
}

QString QInstaller::uncaughtExceptionString(QScriptEngine *scriptEngine/*, const QString &context*/)
{
    //QString errorString(QLatin1String("%1 %2\n%3"));
    QString errorString(QLatin1String("\t\t%1\n%2"));
    //if (!context.isEmpty())
    //    errorString.prepend(context + QLatin1String(": "));

    //usually the linenumber is in the backtrace
    errorString = errorString.arg(/*QString::number(scriptEngine->uncaughtExceptionLineNumber()),*/
                    scriptEngine->uncaughtException().toString(),
                    scriptEngine->uncaughtExceptionBacktrace().join(QLatin1String("\n")));
    return errorString;
}


/*!
 \class QInstaller::Installer
 Installer forms the core of the installation and uninstallation system.
 */

/*!
  \enum QInstaller::Installer::WizardPage
  WizardPage is used to number the different pages known to the Installer GUI.
 */

/*!
  \var QInstaller::Installer::Introduction
  Introduction page.
 */

/*!
  \var QInstaller::Installer::LicenseCheck
  License check page
 */
/*!
  \var QInstaller::Installer::TargetDirectory
  Target directory selection page
 */
/*!
  \var QInstaller::Installer::ComponentSelection
  %Component selection page
 */
/*!
  \var QInstaller::Installer::StartMenuSelection
  Start menu directory selection page - Microsoft Windows only
 */
/*!
  \var QInstaller::Installer::ReadyForInstallation
  "Ready for Installation" page
 */
/*!
  \var QInstaller::Installer::PerformInstallation
  Page shown while performing the installation
 */
/*!
  \var QInstaller::Installer::InstallationFinished
  Page shown when the installation was finished
 */
/*!
  \var QInstaller::Installer::End
  Non-existing page - this value has to be used if you want to insert a page after \a InstallationFinished
 */

/*!
 Initializes the created FSEngineClientHandler instance \a handler.
 \internal
 */
static void initEngineHandler(FSEngineClientHandler* handler)
{
#ifdef FSENGINE_TCP
    const int port = 30000 + qrand() % 1000;
    handler->init(port);
    handler->setStartServerCommand(qApp->applicationFilePath(), QStringList()
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
    Creates and initializes a FSEngineClientHandler -> makes us get admin rights for QFile
    operations.
    \internal
 */
static FSEngineClientHandler* createEngineClientHandler()
{
    static FSEngineClientHandler* clientHandlerInstance = 0;
    if (clientHandlerInstance == 0)
    {
        clientHandlerInstance = new FSEngineClientHandler;
        initEngineHandler(clientHandlerInstance);
    }
    return clientHandlerInstance;
}


// -- Installer::Private


class QInstaller::Installer::Private : public QObject
{
    Q_OBJECT;

private:
    Installer* const q;

public:
    explicit Private(Installer *q, qint64 magicmaker,
        QVector<KDUpdater::UpdateOperation*> performedOperations);
    ~Private();

    void initialize();

    bool statusCanceledOrFailed() const;
    void setStatus(Installer::Status);

    void writeUninstaller(QVector<KDUpdater::UpdateOperation*> performedOperations);
    QString targetDir() const { return q->value(QLatin1String("TargetDir")); }

    QString configurationFileName() const {
        return q->value(QLatin1String("TargetConfigurationFile"),
                        QString::fromLatin1("components.xml"));
    }

    QString componentsXmlPath() const
    {
        return QDir::toNativeSeparators(QDir(QDir::cleanPath(targetDir()))
            .absoluteFilePath(configurationFileName()));
    }

    QString localComponentsXmlPath() const
    {
        const QString &appDirPath = QCoreApplication::applicationDirPath();
        if (QFileInfo(appDirPath + QLatin1String("/../..")).isBundle()) {
            return QDir::toNativeSeparators(QFileInfo(QDir::cleanPath(appDirPath
                + QLatin1String("/../../../") + configurationFileName())).absoluteFilePath());
        }
        return componentsXmlPath();
    }

    void runInstaller();
    void runUninstaller();
    void runPackageUpdater();
    void deleteUninstaller();
    QString uninstallerName() const;
    QString replaceVariables(const QString &str) const;
    QByteArray replaceVariables(const QByteArray &str) const;
    QString registerPath() const;
    void registerInstaller();
    void unregisterInstaller();
    QString installerBinaryPath() const;
    bool isInstaller() const;
    bool isUninstaller() const;
    bool isPackageManager() const;
    Installer *installer() const { return q; }
    KDUpdater::UpdateOperation* createOwnedOperation(const QString& type);
    void readUninstallerIniFile(const QString& targetDir);
    void stopProcessesForUpdates(const QList<Component*>& components);
    int countProgressOperations(const QList<Component*>& components);
    int countProgressOperations(const QList<KDUpdater::UpdateOperation*>& operations);
    void connectOperationToInstaller(KDUpdater::UpdateOperation* const operation,
        double progressOperationPartSize);
    void registerPathesForUninstallation(const QList<QPair<QString, bool> > &pathesForUninstallation,
        const QString& componentName);

    void addPerformed(KDUpdater::UpdateOperation* op) {
        m_performedOperationsCurrentSession.push_back(op);
    }

    void commitSessionOperations() {
        m_performedOperationsOld += m_performedOperationsCurrentSession;
        m_performedOperationsCurrentSession.clear();
    }

signals:
    void installationStarted();
    void installationFinished();
    void uninstallationStarted();
    void uninstallationFinished();

public:
    TempDirDeleter tempDirDeleter;

    FSEngineClientHandler* const engineClientHandler;

    QHash<QString, QString> m_vars;
    QHash<QString, bool> m_sharedFlags;
    Installer::Status m_status;

    bool packageManagingMode;
    bool m_completeUninstall;

    qint64 m_firstComponentStart;
    qint64 m_componentsCount;
    qint64 m_componentOffsetTableStart;

    qint64 m_firstComponentDictStart;
    qint64 m_componentsDictCount;
    qint64 m_componentDictOffsetTableStart;

    qint64 m_globalDictOffset;
    qint64 m_magicInstallerMarker;

    KDUpdater::Application * m_app;

    //this is a Hack, we don't need this in the refactor branch
    QList< QPointer<QInstaller::Component> > componentDeleteList;

    // Owned. Indexed by component name
    QList<Component*> m_rootComponents;
    QHash<QString, Component*> m_componentHash;

    QList<Component*> m_updaterComponents;
    QList<Component*> m_packageManagerComponents;

    //a hack to get the will be replaced components extra
    QList<QInstaller::Component*> willBeReplacedComponents;

    QList<KDUpdater::UpdateOperation*> ownedOperations;
    QVector<KDUpdater::UpdateOperation*> m_performedOperationsOld;
    QVector<KDUpdater::UpdateOperation*> m_performedOperationsCurrentSession;
    InstallerSettings m_settings;
    QString installerBaseBinaryUnreplaced;
    bool linearComponentList;
    bool m_launchedAsRoot;
    int m_silentRetries;
    bool m_forceRestart;
    bool m_needToWriteUninstaller;
    bool m_testChecksum;
};

Installer::Private::Private(Installer *q, qint64 magicmaker,
        QVector<KDUpdater::UpdateOperation*> performedOperations)
    : q(q),
      engineClientHandler(createEngineClientHandler()),
      m_status(Installer::InstallerUnfinished),
      packageManagingMode(false),
      m_completeUninstall(false),
      m_magicInstallerMarker(magicmaker),//? magicmaker : MagicInstallerMarker),
      m_performedOperationsOld(performedOperations),
      linearComponentList(false),
      m_launchedAsRoot(AdminAuthorization::hasAdminRights()),
      m_silentRetries(3),
      m_forceRestart (false),
      m_needToWriteUninstaller(false),
      m_testChecksum(false)
{
    connect(this, SIGNAL(installationStarted()), q, SIGNAL(installationStarted()));
    connect(this, SIGNAL(installationFinished()), q, SIGNAL(installationFinished()));
    connect(this, SIGNAL(uninstallationStarted()), q, SIGNAL(uninstallationStarted()));
    connect(this, SIGNAL(uninstallationFinished()), q, SIGNAL(uninstallationFinished()));
    verbose() << "has admin rights ? : " << engineClientHandler->isActive() << std::endl;
}

Installer::Private::~Private()
{
    qDeleteAll(componentDeleteList);
    componentDeleteList.clear();

    qDeleteAll(m_performedOperationsOld);
    qDeleteAll(m_performedOperationsCurrentSession);
}

KDUpdater::Application& Installer::updaterApplication() const
{
    return *d->m_app;
}

void Installer::setUpdaterApplication(KDUpdater::Application *app)
{
    d->m_app = app;
}

void Installer::writeUninstaller()
{
    if (d->m_needToWriteUninstaller) {
        bool error = false;
        QString errorMsg;
        try {
            d->writeUninstaller(d->m_performedOperationsOld  + d->m_performedOperationsCurrentSession);

            bool gainedAdminRights = false;
            QTemporaryFile tempAdminFile(d->targetDir()
                + QLatin1String("/testjsfdjlkdsjflkdsjfldsjlfds") + QString::number(qrand() % 1000));
            if (!tempAdminFile.open() || !tempAdminFile.isWritable()) {
                gainAdminRights();
                gainedAdminRights = true;
            }
            d->m_app->packagesInfo()->writeToDisk();
            if (gainedAdminRights)
                dropAdminRights();
            d->m_needToWriteUninstaller = false;
        } catch (const Error& e) {
            error = true;
            errorMsg = e.message();
        }

        if (error) {
            MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(),
                QLatin1String("WriteError"), tr("Error writing Uninstaller"), errorMsg,
                QMessageBox::Ok, QMessageBox::Ok);
        }
    }
}

void Installer::reset(const QHash<QString, QString> &params)
{
    d->m_completeUninstall = false;
    d->m_forceRestart = false;
    d->m_status = Installer::InstallerUnfinished;
    d->installerBaseBinaryUnreplaced.clear();
    d->m_vars.clear();
    d->m_vars = params;
    d->initialize();
}

void Installer::Private::initialize()
{
    try {
        m_settings = InstallerSettings::fromFileAndPrefix(
            QLatin1String(":/metadata/installer-config/config.xml"),
            QLatin1String(":/metadata/installer-config/"));
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
    m_vars.insert(QLatin1String("ProductName"), m_settings.applicationName());
    m_vars.insert(QLatin1String("ProductVersion"), m_settings.applicationVersion());
    m_vars.insert(QLatin1String("Title"), m_settings.title());
    m_vars.insert(QLatin1String("MaintenanceTitle"), m_settings.maintenanceTitle());
    m_vars.insert(QLatin1String("Publisher"), m_settings.publisher());
    m_vars.insert(QLatin1String("Url"), m_settings.url());
    m_vars.insert(QLatin1String("StartMenuDir"), m_settings.startMenuDir());

    m_vars.insert(QLatin1String("LogoPixmap"), m_settings.logo());
    m_vars.insert(QLatin1String("LogoSmallPixmap"), m_settings.logoSmall());
    m_vars.insert(QLatin1String("WatermarkPixmap"), m_settings.watermark());

    m_vars.insert(QLatin1String("TargetConfigurationFile"), m_settings.configurationFileName());
    m_vars.insert(QLatin1String("RunProgram"), replaceVariables(m_settings.runProgram()));
    const QString desc = m_settings.runProgramDescription();
    if (!desc.isEmpty())
        m_vars.insert(QLatin1String("RunProgramDescription"), desc);
#ifdef Q_WS_X11
    if (m_launchedAsRoot)
        m_vars.insert(QLatin1String("TargetDir"), replaceVariables(m_settings.adminTargetDir()));
    else
#endif
    m_vars.insert(QLatin1String("TargetDir"), replaceVariables(m_settings.targetDir()));
    m_vars.insert(QLatin1String("RemoveTargetDir"), replaceVariables(m_settings.removeTargetDir()));

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

    foreach (KDUpdater::UpdateOperation* currentOperation, m_performedOperationsOld) {
        currentOperation->setValue(QLatin1String("installer"), QVariant::fromValue(q));
    }

    connect(this, SIGNAL(installationStarted()), ProgressCoordninator::instance(), SLOT(reset()));
    connect(this, SIGNAL(uninstallationStarted()), ProgressCoordninator::instance(), SLOT(reset()));
}

/*!
 * Sets the uninstallation to be \a complete. If \a complete is false, only components deselected
 * by the user will be uninstalled.
 * This option applies only on uninstallation.
 */
void Installer::setCompleteUninstallation(bool complete)
{
    d->m_completeUninstall = complete;
    d->packageManagingMode = !d->m_completeUninstall;
}

QString Installer::Private::installerBinaryPath() const
{
    return qApp->applicationFilePath();
}

bool Installer::Private::isInstaller() const
{
    return m_magicInstallerMarker == MagicInstallerMarker;
}

bool Installer::Private::isUninstaller() const
{
    return m_magicInstallerMarker == MagicUninstallerMarker && !packageManagingMode;
}

bool Installer::Private::isPackageManager() const
{
    return m_magicInstallerMarker == MagicUninstallerMarker && packageManagingMode;
}

bool Installer::Private::statusCanceledOrFailed() const
{
    return m_status == Installer::InstallerCanceledByUser
        || m_status == Installer::InstallerFailed;
}

void Installer::Private::setStatus(Installer::Status status)
{
    if (m_status != status) {
        m_status = status;
        emit q->statusChanged(m_status);
    }
}

QString Installer::Private::replaceVariables(const QString &str) const
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

QByteArray Installer::Private::replaceVariables(const QByteArray &ba) const
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
KDUpdater::UpdateOperation* Installer::Private::createOwnedOperation(const QString &type)
{
    KDUpdater::UpdateOperation* const op = KDUpdater::UpdateOperationFactory::instance().create(type);
    ownedOperations.push_back(op);
    return op;
}

QString Installer::Private::uninstallerName() const
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

void Installer::Private::readUninstallerIniFile(const QString &targetDir)
{
    const QString iniPath = targetDir + QLatin1Char('/') + m_settings.uninstallerIniFile();
    QSettings cfg(iniPath, QSettings::IniFormat);
    const QVariantHash vars = cfg.value(QLatin1String("Variables")).toHash();
    QHash<QString, QVariant>::ConstIterator it = vars.constBegin();
    while (it != vars.constEnd()) {
        m_vars.insert(it.key(), it.value().toString());
        ++it;
    }
}

/*!
    Copied from QInstaller with some adjustments
    Return true, if a process with \a name is running. On Windows, the comparision is case-insensitive.
*/
static bool isProcessRunning(const QString& name, const QList<KDSysInfo::ProcessInfo> &processes)
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

static QStringList checkRunningProcessesFromList(const QStringList &processList)
{
    const QList<KDSysInfo::ProcessInfo> allProcesses = KDSysInfo::runningProcesses();
    QStringList stillRunningProcesses;
    foreach (const QString &process, processList) {
        if (!process.isEmpty() && isProcessRunning(process, allProcesses)) {
            stillRunningProcesses.append(process);
        }
    }
    return stillRunningProcesses;
}

void Installer::Private::stopProcessesForUpdates(const QList<Component*> &components)
{
    QStringList processList;
    foreach (const Component* const i, components)
        processList << q->replaceVariables(i->stopProcessForUpdateRequests());

    qSort(processList);
    processList.erase(std::unique(processList.begin(), processList.end()), processList.end());
    if (processList.isEmpty())
        return;

    while (true) {
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

int Installer::Private::countProgressOperations(const QList<KDUpdater::UpdateOperation*> &operations)
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

int Installer::Private::countProgressOperations(const QList<Component*> &components)
{
    int operationCount = 0;
    for (QList<Component*>::const_iterator It = components.begin(); It != components.end(); ++It)
        operationCount = operationCount + countProgressOperations((*It)->operations());

    return operationCount;
}

void Installer::Private::connectOperationToInstaller(KDUpdater::UpdateOperation* const operation,
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

/*!
    This creates fake operations which remove stuff which was registered for uninstallation afterwards
*/
void Installer::Private::registerPathesForUninstallation(
    const QList<QPair<QString, bool> > &pathesForUninstallation, const QString &componentName)
{
    if (pathesForUninstallation.isEmpty())
        return;

    QList<QPair<QString, bool> >::const_iterator it;
    for (it = pathesForUninstallation.begin(); it != pathesForUninstallation.end(); ++it) {
        const QString path = replaceVariables(it->first);
        const bool wipe = it->second;
        const QFileInfo fi(path);

        // create a copy operation with the file as target -> it will get deleted on undo
        KDUpdater::UpdateOperation* const op = createOwnedOperation(QLatin1String(fi.isDir()
            ? "Mkdir" : "Copy"));
        if (fi.isDir()) {
            op->setValue(QLatin1String("createddir"), fi.absoluteFilePath());
            op->setValue(QLatin1String("forceremoval"), wipe ? QLatin1String("true")
                : QLatin1String("false"));
        }
        op->setArguments(fi.isDir() ? QStringList() << fi.absoluteFilePath()
            : QStringList() << QString() << fi.absoluteFilePath());
        op->setValue(QLatin1String("component"), componentName);
        addPerformed(op);

        // get recursive afterwards
        if (fi.isDir() && !wipe) {
            QDirIterator dirIt(path, QDir::Hidden | QDir::AllEntries | QDir::System
                | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
            while (dirIt.hasNext()) {
                dirIt.next();
                const QFileInfo fi = dirIt.fileInfo();
                if (fi.isDir()) {
                    // create an mkdir operation with the dir as target -> it will get deleted on undo
                    KDUpdater::UpdateOperation* const op = createOwnedOperation(QLatin1String("Mkdir"));
                    op->setArguments(QStringList() << fi.absoluteFilePath());
                    op->setValue(QLatin1String("createddir"), fi.absoluteFilePath());
                    op->setValue(QLatin1String("component"), componentName);
                    addPerformed(op);
                } else {
                    // create a copy operation with the file as target -> it will get deleted on undo
                    KDUpdater::UpdateOperation* const op = createOwnedOperation(QLatin1String("Copy"));
                    op->setArguments(QStringList() << QString() << fi.absoluteFilePath());
                    op->setValue(QLatin1String("component"), componentName);
                    addPerformed(op);
                }
            }
        }
    }
}

void Installer::Private::writeUninstaller(QVector<KDUpdater::UpdateOperation*> performedOperations)
{
    bool gainedAdminRights = false;
    QTemporaryFile tempAdminFile(targetDir() + QString::fromLatin1("/testjsfdjlkdsjflkdsjfldsjlfds")
        + QString::number(qrand() % 1000));
    if (!tempAdminFile.open() || !tempAdminFile.isWritable()) {
        q->gainAdminRights();
        gainedAdminRights = true;
    }

    verbose() << "QInstaller::Installer::Private::writeUninstaller uninstaller=" << uninstallerName()
        << std::endl;

    // create the directory containing the uninstaller (like a bundle structor, on Mac...)
    KDUpdater::UpdateOperation* op = createOwnedOperation(QLatin1String("Mkdir"));
    op->setArguments(QStringList() << QFileInfo(uninstallerName()).path());
    performOperationThreaded(op, Backup);
    performOperationThreaded(op);
    performedOperations.push_back(op);

    {
        // write current state (variables) to the uninstaller ini file
        const QString iniPath = targetDir() + QLatin1Char('/') + m_settings.uninstallerIniFile();
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

    const QString installerBaseBinary = q->replaceVariables(installerBaseBinaryUnreplaced);
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
        ifVerbose("CREATING UNINSTALLER " << performedOperations.size());

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
            installerBaseBinaryUnreplaced.clear();
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
        setStatus(InstallerFailed);
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

QString Installer::Private::registerPath() const
{
    QString productName = m_vars.value(QLatin1String("ProductName"));
    if (productName.isEmpty())
        throw Error(tr("ProductName should be set"));

    QString path = QLatin1String("HKEY_CURRENT_USER");
    if (m_vars.value(QLatin1String("AllUsers")) == QLatin1String("true"))
        path = QLatin1String("HKEY_LOCAL_MACHINE");

    return path + QLatin1String("\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\")
        + productName;
}

void Installer::Private::registerInstaller()
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

void Installer::Private::unregisterInstaller()
{
#ifdef Q_OS_WIN
    QSettings settings(registerPath(), QSettings::NativeFormat);
    settings.remove(QString());
#endif
}

void Installer::autoAcceptMessageBoxes()
{
    MessageBoxHandler::instance()->setDefaultAction(MessageBoxHandler::Accept);
}

void Installer::autoRejectMessageBoxes()
{
    MessageBoxHandler::instance()->setDefaultAction(MessageBoxHandler::Reject);
}

void Installer::setMessageBoxAutomaticAnswer(const QString &identifier, int button)
{
    MessageBoxHandler::instance()->setAutomaticAnswer(identifier,
        static_cast<QMessageBox::Button>(button));
}

void Installer::installSelectedComponents()
{
    d->setStatus(InstallerRunning);
    // download

    double downloadPartProgressSize = double(1)/3;
    double componentsInstallPartProgressSize = double(2)/3;
    // get the list of packages we need to install in proper order and do it for the updater
    const int downloadedArchivesCount = downloadNeededArchives(UpdaterMode, downloadPartProgressSize);

    //if there was no download we have the whole progress for installing components
    if (!downloadedArchivesCount) {
         //componentsInstallPartProgressSize + downloadPartProgressSize;
        componentsInstallPartProgressSize = double(1);
    }

    // get the list of packages we need to install in proper order
    const QList<Component*> components = calculateComponentOrder(UpdaterMode);

    if (!isInstaller() && !QFileInfo(installerBinaryPath()).isWritable())
        gainAdminRights();

    d->stopProcessesForUpdates(components);
    int progressOperationCount = d->countProgressOperations(components);
    double progressOperationSize = componentsInstallPartProgressSize / progressOperationCount;

    //TODO: devide this in undo steps and install steps (2 "for" loops) for better progress calculation
    for (QList<Component*>::const_iterator it = components.begin(); it != components.end(); ++it) {
        if (d->statusCanceledOrFailed())
            throw Error(tr("Installation canceled by user"));
        Component* const currentComponent = *it;
        ProgressCoordninator::instance()->emitLabelAndDetailTextChanged(tr("\nRemoving the old "
            "version of: %1").arg(currentComponent->name()));
        if (isUpdater() && currentComponent->removeBeforeUpdate()) {
            QString replacesAsString = currentComponent->value(QLatin1String("Replaces"));
            QStringList possibleNames(replacesAsString.split(QLatin1String(","),
                QString::SkipEmptyParts));
            possibleNames.append(currentComponent->name());
            // undo all operations done by this component upon installation
            for (int i = d->m_performedOperationsOld.count() - 1; i >= 0; --i) {
                KDUpdater::UpdateOperation* const op = d->m_performedOperationsOld[i];
                if (!possibleNames.contains(op->value(QLatin1String("component")).toString()))
                    continue;
                const bool becameAdmin = !d->engineClientHandler->isActive()
                    && op->value(QLatin1String("admin")).toBool() && gainAdminRights();
                performOperationThreaded(op, Undo);
                if (becameAdmin)
                    dropAdminRights();
                d->m_performedOperationsOld.remove(i);
                delete op;
            }
            foreach(const QString possilbeName, possibleNames)
                d->m_app->packagesInfo()->removePackage(possilbeName);
            d->m_app->packagesInfo()->writeToDisk();
        }
        ProgressCoordninator::instance()->emitLabelAndDetailTextChanged(
            tr("\nInstalling the new version of: %1").arg(currentComponent->name()));
        installComponent(currentComponent, progressOperationSize);
        //commit all operations for this allready updated/installed component
        //so an undo during the installComponent function only undos the uncomplete installed one
        d->commitSessionOperations();
        d->m_needToWriteUninstaller = true;
    }

    d->setStatus(InstallerSucceeded);
    ProgressCoordninator::instance()->emitLabelAndDetailTextChanged(tr("\nUpdate finished!"));
    emit updateFinished();
}

quint64 Installer::requiredDiskSpace() const
{
    quint64 result = 0;
    QList<Component*>::const_iterator it;
    const QList<Component*> availableComponents = components(true);
    for (it = availableComponents.begin(); it != availableComponents.end(); ++it) {
        Component* const comp = *it;
        if (!comp->isSelected())
            continue;
        if (comp->value(QLatin1String("PreviousState")) == QLatin1String("Installed"))
            continue;
        result += comp->value(QLatin1String("UncompressedSize")).toLongLong();
    }
    return result;
}

quint64 Installer::requiredTemporaryDiskSpace() const
{
    quint64 result = 0;
    QList<Component*>::const_iterator it;
    const QList<Component*> availableComponents = components(true);
    for (it = availableComponents.begin(); it != availableComponents.end(); ++it) {
        Component* const comp = *it;
        if (!comp->isSelected())
            continue;
        if (comp->value(QLatin1String("PreviousState")) == QLatin1String("Installed"))
            continue;
        result += comp->value(QLatin1String("CompressedSize")).toLongLong();
    }
    return result;
}

/*!
    Returns the will be downloaded archives count
*/
int Installer::downloadNeededArchives(RunModes runMode, double partProgressSize)
{
    Q_ASSERT(partProgressSize >= 0 && partProgressSize <= 1);

    QList<Component*> neededComponents;
    QList<QPair<QString, QString> > archivesToDownload;

    QList<Component*>::const_iterator it;
    const QList<Component*> availableComponents = components(true, runMode);
    for (it = availableComponents.begin(); it != availableComponents.end(); ++it) {
        Component* const comp = *it;
        if (!comp->isSelected(runMode))
            continue;
        if (comp->value(QLatin1String("PreviousState")) == QLatin1String("Installed")
            && runMode == InstallerMode) {
                continue;
        }
        appendComponentAndMissingDependencies(neededComponents, comp);
    }

    for (it = neededComponents.begin(); it != neededComponents.end(); ++it) {
        Component* const comp = *it;

        // collect all archives to be downloaded
        const QStringList toDownload = comp->downloadableArchives();
        for (QStringList::const_iterator it2 = toDownload.begin(); it2 != toDownload.end(); ++it2) {
            QString versionFreeString(*it2);
            archivesToDownload.push_back(qMakePair(QString::fromLatin1("installer://%1/%2")
                .arg(comp->name(), versionFreeString), QString::fromLatin1("%1/%2/%3")
                .arg(comp->repositoryUrl().toString(), comp->name(), versionFreeString)));
        }
    }

    if (archivesToDownload.isEmpty())
        return 0;

    ProgressCoordninator::instance()->emitLabelAndDetailTextChanged(tr("\nDownloading packages..."));

    // don't have it on the stack, since it keeps the temporary files
    DownloadArchivesJob* const archivesJob = new DownloadArchivesJob(d->m_settings.publicKey(), this);
    archivesJob->setArchivesToDownload(archivesToDownload);
    archivesJob->setAutoDelete(false);
    connect(archivesJob, SIGNAL(outputTextChanged(QString)), ProgressCoordninator::instance(),
        SLOT(emitLabelAndDetailTextChanged(QString)));
    ProgressCoordninator::instance()->registerPartProgress(archivesJob,
        SIGNAL(progressChanged(double)), partProgressSize);
    connect(this, SIGNAL(installationInterrupted()), archivesJob, SLOT(cancel()));
    archivesJob->start();
    archivesJob->waitForFinished();

    if (archivesJob->error() == KDJob::Canceled)
        interrupt();
    else if (archivesJob->error() != DownloadArchivesJob::NoError)
        throw Error(archivesJob->errorString());
    if (d->statusCanceledOrFailed())
        throw Error(tr("Installation canceled by user"));

    return archivesToDownload.count();
}

QList<Component*> Installer::calculateComponentOrder(RunModes runMode) const
{
    return componentsToInstall(true, true, runMode);
}

void Installer::installComponent(Component* comp, double progressOperationSize)
{
    Q_ASSERT(progressOperationSize);

    d->setStatus(InstallerRunning);
    const QList<KDUpdater::UpdateOperation*> operations = comp->operations();

    // show only component which are doing something, MinimumProgress is only for progress
    // calculation safeness
    if (operations.count() > 1
        || (operations.count() == 1 && operations.at(0)->name() != QLatin1String("MinimumProgress"))) {
        ProgressCoordninator::instance()->emitLabelAndDetailTextChanged(tr("\nInstalling component %1")
            .arg(comp->displayName()));
    }

    if (!comp->operationsCreatedSuccessfully())
        setCanceled();

    QList<KDUpdater::UpdateOperation*>::const_iterator op;
    for (op = operations.begin(); op != operations.end(); ++op) {
        if (d->statusCanceledOrFailed())
            throw Error(tr("Installation canceled by user"));

        KDUpdater::UpdateOperation* const operation = *op;
        d->connectOperationToInstaller(operation, progressOperationSize);

        // maybe this operations wants us to be admin...
        const bool becameAdmin = !d->engineClientHandler->isActive()
            && operation->value(QLatin1String("admin")).toBool() && gainAdminRights();
        // perform the operation
        if (becameAdmin)
            verbose() << operation->name() << " as admin: " << becameAdmin << std::endl;

        // allow the operation to backup stuff before performing the operation
        performOperationThreaded(operation, Backup);

        bool ignoreError = false;
        bool ok = performOperationThreaded(operation);
        while (!ok && !ignoreError && status() != InstallerCanceledByUser) {
            verbose() << QString(QLatin1String("operation '%1' with arguments: '%2' failed: %3"))
                .arg(operation->name(), operation->arguments().join(QLatin1String("; ")),
                operation->errorString()) << std::endl;;
            const QMessageBox::StandardButton button =
                MessageBoxHandler::warning(MessageBoxHandler::currentBestSuitParent(),
                QLatin1String("installationErrorWithRetry"), tr("Installer Error"),
                tr("Error during installation process(%1):\n%2").arg(comp->name(),
                    operation->errorString()),
                QMessageBox::Retry | QMessageBox::Ignore | QMessageBox::Cancel, QMessageBox::Retry);

            if (button == QMessageBox::Retry)
                ok = performOperationThreaded(operation);
            else if (button == QMessageBox::Ignore)
                ignoreError = true;
            else if (button == QMessageBox::Cancel)
                interrupt();
        }

        if (ok || operation->error() > KDUpdater::UpdateOperation::InvalidArguments) {
            // remember that the operation was performed what allows us to undo it if a
            // following operaton fails or if this operation failed but still needs
            // an undo call to cleanup.
            d->addPerformed(operation);
            operation->setValue(QLatin1String("component"), comp->name());
        }

        if (becameAdmin)
            dropAdminRights();

        if (!ok && !ignoreError)
            throw Error(operation->errorString());

        if (comp->value(QLatin1String("Important"), QLatin1String("false")) == QLatin1String("true"))
            d->m_forceRestart = true;
    }

    d->registerPathesForUninstallation(comp->pathesForUninstallation(), comp->name());

    if (!comp->stopProcessForUpdateRequests().isEmpty()) {
        KDUpdater::UpdateOperation *stopProcessForUpdatesOp =
            KDUpdater::UpdateOperationFactory::instance().create(QLatin1String("FakeStopProcessForUpdate"));
        const QStringList arguments(comp->stopProcessForUpdateRequests().join(QLatin1String(",")));
        stopProcessForUpdatesOp->setArguments(arguments);
        d->addPerformed(stopProcessForUpdatesOp);
        stopProcessForUpdatesOp->setValue(QLatin1String("component"), comp->name());
    }

    // now mark the component as installed
    KDUpdater::PackagesInfo* const packages = d->m_app->packagesInfo();
    const bool forcedInstall =
        comp->value(QLatin1String("ForcedInstallation")).toLower() == QLatin1String("true")
        ? true : false;
    const bool virtualComponent =
        comp->value(QLatin1String ("Virtual")).toLower() == QLatin1String("true") ? true : false;
    packages->installPackage(comp->value(QLatin1String("Name")),
        comp->value(QLatin1String("Version")), comp->value(QLatin1String("DisplayName")),
        comp->value(QLatin1String("Description")), comp->dependencies(), forcedInstall,
        virtualComponent, comp->value(QLatin1String ("UncompressedSize")).toULongLong());

    comp->setValue(QLatin1String("CurrentState"), QLatin1String("Installed"));
    comp->markAsPerformedInstallation();
}

/*!
    If a component marked as important was installed during update
    process true is returned.
*/
bool Installer::needsRestart() const
{
    return d->m_forceRestart;
}

void Installer::rollBackInstallation()
{
    emit titleMessageChanged(tr("Cancelling the Installer"));
    // rolling back

    //this unregisters all operation progressChanged connects
    ProgressCoordninator::instance()->setUndoMode();
    int progressOperationCount =
        d->countProgressOperations(d->m_performedOperationsCurrentSession.toList());
    double progressOperationSize = double(1) / progressOperationCount;

    //reregister all the undooperations with the new size to the ProgressCoordninator
    foreach (KDUpdater::UpdateOperation* const operation, d->m_performedOperationsCurrentSession) {
        QObject* const operationObject = dynamic_cast<QObject*>(operation);
        if (operationObject != 0) {
            const QMetaObject* const mo = operationObject->metaObject();
            if (mo->indexOfSignal(QMetaObject::normalizedSignature("progressChanged(double)")) > -1) {
                ProgressCoordninator::instance()->registerPartProgress(operationObject,
                    SIGNAL(progressChanged(double)), progressOperationSize);
            }
        }
    }

    while (!d->m_performedOperationsCurrentSession.isEmpty()) {
        try {
            KDUpdater::UpdateOperation* const operation = d->m_performedOperationsCurrentSession.last();
            d->m_performedOperationsCurrentSession.pop_back();

            const bool becameAdmin = !d->engineClientHandler->isActive()
                && operation->value(QLatin1String("admin")).toBool() && gainAdminRights();
            performOperationThreaded(operation, Undo);
            if (becameAdmin)
                dropAdminRights();
        } catch(const Error &e) {
            MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(),
                QLatin1String("ElevationError"), tr("Authentication Error"), tr("Some components "
                "could not be removed completely because admin rights could not be acquired: %1.")
                .arg(e.message()));
        }
    }
}

void Installer::Private::runInstaller()
{
    try {
        setStatus(InstallerRunning);
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
        const QList<Component*> componentsToInstall = q->calculateComponentOrder();
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
        const int downloadedArchivesCount = q->downloadNeededArchives(InstallerMode,
            downloadPartProgressSize);

        //if there was no download we have the whole progress for installing components
        if (!downloadedArchivesCount) {
             //componentsInstallPartProgressSize + downloadPartProgressSize;
            componentsInstallPartProgressSize = double(1);
        }

        // put the installed packages info into the target dir
        KDUpdater::PackagesInfo* const packages = m_app->packagesInfo();
        packages->setFileName(componentsXmlPath());
        packages->setApplicationName(m_settings.applicationName());
        packages->setApplicationVersion(m_settings.applicationVersion());

        stopProcessesForUpdates(componentsToInstall);

        const int progressOperationCount = countProgressOperations(componentsToInstall);
        double progressOperationSize = componentsInstallPartProgressSize / progressOperationCount;

        QList<Component*>::const_iterator it;
        for (it = componentsToInstall.begin(); it != componentsToInstall.end(); ++it)
            q->installComponent(*it, progressOperationSize);

        registerInstaller();

        emit q->titleMessageChanged(tr("Creating Uninstaller"));

        m_app->packagesInfo()->writeToDisk();
        writeUninstaller(m_performedOperationsOld + m_performedOperationsCurrentSession);

        //this is the reserved one from the beginning
        ProgressCoordninator::instance()->addManualPercentagePoints(1);

        setStatus(InstallerSucceeded);
        ProgressCoordninator::instance()->emitLabelAndDetailTextChanged(tr("\nInstallation finished!"));

        emit installationFinished();

        // disable the FSEngineClientHandler afterwards
        engineClientHandler->setActive(false);
    } catch (const Error &err) {
        if (q->status() != InstallerCanceledByUser) {
            setStatus(InstallerFailed);
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
        engineClientHandler->setActive(false);
        throw;
    }
}

void Installer::Private::deleteUninstaller()
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
        QFile::remove(QFileInfo(installerBinaryPath()).absolutePath() + QLatin1String("/") + configurationFileName());
    }
}

bool Installer::isFileExtensionRegistered(const QString& extension) const
{
    QSettings settings(QLatin1String("HKEY_CLASSES_ROOT"), QSettings::NativeFormat);
    return settings.value(QString::fromLatin1(".%1/Default").arg(extension)).isValid();
}

void Installer::Private::runPackageUpdater()
{
    try {
        if (m_completeUninstall) {
            // well... I guess we would call that an uninstall, no? :-)
            packageManagingMode = !m_completeUninstall;
            runUninstaller();
            return;
        }

        setStatus(InstallerRunning);
        emit installationStarted(); //resets also the ProgressCoordninator

        //to have some progress for the cleanup/write component.xml step
        ProgressCoordninator::instance()->addReservePercentagePoints(1);

        if (!QFileInfo(installerBinaryPath()).isWritable())
            q->gainAdminRights();

        KDUpdater::PackagesInfo* const packages = m_app->packagesInfo();
        packages->setFileName(componentsXmlPath());
        packages->setApplicationName(m_settings.applicationName());
        packages->setApplicationVersion(m_settings.applicationVersion());

        const QString packagesXml = componentsXmlPath();
        if (!QFile(packagesXml).open(QIODevice::Append))
            q->gainAdminRights();

        // first check, if we need admin rights for the installation part
        QList<Component*>::const_iterator it;
        QList<Component*> availableComponents = q->components(true, InstallerMode);
        for (it = availableComponents.begin(); it != availableComponents.end(); ++it) {
            // check if we need admin rights and ask before the action happens
            Component* const currentComponent = *it;
            if (!currentComponent->isSelected(InstallerMode))
                continue;

            // we only need the uninstalled components
            if (currentComponent->value(QLatin1String("PreviousState")) == QLatin1String("Installed"))
                continue;

            bool requiredAdmin = false;
            if (currentComponent->value(QLatin1String("RequiresAdminRights"),
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
        q->downloadNeededArchives(InstallerMode, downloadPartProgressSize);

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

            const bool becameAdmin = !engineClientHandler->isActive()
                && currentOperation->value(QLatin1String("admin")).toBool() && q->gainAdminRights();

            bool ignoreError = false;
            performOperationThreaded(currentOperation, Undo);
            bool ok = currentOperation->error() == KDUpdater::UpdateOperation::NoError
                || componentName == QLatin1String("");
            while (!ok && !ignoreError && q->status() != InstallerCanceledByUser) {
                verbose() << QString(QLatin1String("operation '%1' with arguments: '%2' failed: %3"))
                    .arg(currentOperation->name(), currentOperation->arguments()
                    .join(QLatin1String("; ")), currentOperation->errorString()) << std::endl;;

                const QMessageBox::StandardButton button =
                    MessageBoxHandler::warning(MessageBoxHandler::currentBestSuitParent(),
                    QLatin1String("installationErrorWithRetry"), tr("Installer Error"),
                    tr("Error during installation process(%1):\n%2").arg(componentName,
                        currentOperation->errorString()),
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

        const QList<Component*> allComponents = q->components(true, InstallerMode);
        foreach (Component *comp, allComponents) {
            if (!comp->isSelected())
                uninstalledComponents |= comp;
        }

        QSet<Component*>::const_iterator it2;
        for (it2 = uninstalledComponents.begin(); it2 != uninstalledComponents.end(); ++it2) {
            packages->removePackage((*it2)->name());
            (*it2)->setValue(QLatin1String("CurrentState"), QLatin1String("Uninstalled"));
        }

        // these are all operations left: those which were not reverted
        m_performedOperationsOld = nonRevertedOperations;

        //write components.xml in case the user cancels the update
        packages->writeToDisk();
        ProgressCoordninator::instance()->emitLabelAndDetailTextChanged(tr("Preparing the installation..."));

        const QList<Component*> componentsToInstall = q->calculateComponentOrder();

        verbose() << "Install size: " << componentsToInstall.size() << " components " << std::endl;

        stopProcessesForUpdates(componentsToInstall);

        int progressOperationCount = countProgressOperations(componentsToInstall);
        double progressOperationSize = componentsInstallPartProgressSize / progressOperationCount;

        for (it = componentsToInstall.begin(); it != componentsToInstall.end(); ++it)
            q->installComponent(*it, progressOperationSize);

        packages->writeToDisk();

        emit q->titleMessageChanged(tr("Creating Uninstaller"));

        commitSessionOperations(); //end session, move ops to "old"
        m_needToWriteUninstaller = true;

        //this is the reserved one from the beginning
        ProgressCoordninator::instance()->addManualPercentagePoints(1);

        setStatus(InstallerSucceeded);
        ProgressCoordninator::instance()->emitLabelAndDetailTextChanged(tr("\nInstallation finished!"));
        emit installationFinished();

        // disable the FSEngineClientHandler afterwards
        engineClientHandler->setActive(false);
    } catch(const Error &err) {
        if (q->status() != InstallerCanceledByUser) {
            setStatus(InstallerFailed);
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
        engineClientHandler->setActive(false);
        throw;
    }
}

void Installer::Private::runUninstaller()
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
        packages->setApplicationName(m_settings.applicationName());
        packages->setApplicationVersion(m_settings.applicationVersion());

        // iterate over all components - if they're all marked for uninstall, it's a complete uninstall
        const QList<Component*> allComponents = q->components(true);
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

            const bool becameAdmin = !engineClientHandler->isActive()
                && currentOperation->value(QLatin1String("admin")).toBool() && q->gainAdminRights();

            bool ignoreError = false;
            performOperationThreaded(currentOperation, Undo);
            bool ok = currentOperation->error() == KDUpdater::UpdateOperation::NoError
                || componentName == QLatin1String("");
            while (!ok && !ignoreError && q->status() != InstallerCanceledByUser) {
                verbose() << QString(QLatin1String("operation '%1' with arguments: '%2' failed: %3"))
                    .arg(currentOperation->name(), currentOperation->arguments()
                    .join(QLatin1String("; ")), currentOperation->errorString()) << std::endl;;
                const QMessageBox::StandardButton button =
                    MessageBoxHandler::warning(MessageBoxHandler::currentBestSuitParent(),
                    QLatin1String("installationErrorWithRetry"), tr("Installer Error"),
                    tr("Error during installation process(%1):\n%2").arg(componentName,
                        currentOperation->errorString()),
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

        if (!m_completeUninstall) {
            QList<Component*>::const_iterator it;
            for (it = allComponents.begin(); it != allComponents.end(); ++it) {
                Component* const comp = *it;
                if (comp->isSelected()) {
                    allMarkedForUninstall = false;
                } else {
                    packages->removePackage(comp->name());
                    comp->setValue(QLatin1String("CurrentState"), QLatin1String("Uninstalled"));
                }
            }
            m_completeUninstall = m_completeUninstall || allMarkedForUninstall;
            packageManagingMode = ! m_completeUninstall;
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
            QList<Component*>::const_iterator it;
            for (it = allComponents.begin(); it != allComponents.end(); ++it) {
                packages->removePackage((*it)->name());
                (*it)->setValue(QLatin1String("CurrentState"), QLatin1String("Uninstalled"));
            }
            QString remove = q->value(QLatin1String("RemoveTargetDir"));
            if(QVariant(remove).toBool())
            {
                // on !Windows, we need to remove TargetDir manually
                packageManagingMode = ! m_completeUninstall;
                verbose() << "Complete Uninstallation is chosen" << std::endl;
                const QString target = targetDir();
                if (!target.isEmpty()) {
                    if (engineClientHandler->isServerRunning() && !engineClientHandler->isActive()) {
                        // we were root at least once, so we remove the target dir as root
                        q->gainAdminRights();
                        removeDirectoryThreaded(target, true);
                        q->dropAdminRights();
                    } else {
                        removeDirectoryThreaded(target, true);
                    }
                }
            }

            unregisterInstaller();
            m_needToWriteUninstaller = false;
        } else {
            // rewrite the uninstaller with the operation we did not undo
            writeUninstaller(nonRevertedOperations);
        }

        setStatus(InstallerSucceeded);
        ProgressCoordninator::instance()->emitLabelAndDetailTextChanged(tr("\nDeinstallation finished"));

        engineClientHandler->setActive(false);
    } catch (const Error &err) {
        if (q->status() != InstallerCanceledByUser) {
            setStatus(InstallerFailed);
            verbose() << "INSTALLER FAILED: " << err.message() << std::endl;
            MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(),
                QLatin1String("installationError"), tr("Error"), err.message());
            verbose() << "ROLLING BACK operations=" << m_performedOperationsCurrentSession.count()
                << std::endl;
        }

        ProgressCoordninator::instance()->emitLabelAndDetailTextChanged(tr("Installation aborted"));
        emit installationFinished();

        // disable the FSEngineClientHandler afterwards
        engineClientHandler->setActive(false);
        throw;
    }
    emit uninstallationFinished();
}


// -- QInstaller


Installer::Installer(qint64 magicmaker,
        const QVector<KDUpdater::UpdateOperation*>& performedOperations)
    : d(new Private(this, magicmaker, performedOperations))
{
    qRegisterMetaType< QInstaller::Installer::Status >("QInstaller::Installer::Status");
    qRegisterMetaType< QInstaller::Installer::WizardPage >("QInstaller::Installer::WizardPage");

    d->initialize();
}

Installer::~Installer()
{
    if (!isUninstaller() && !(isInstaller() && status() == InstallerCanceledByUser)) {
        QDir targetDir(value(QLatin1String("TargetDir")));
        QString logFileName = targetDir.absoluteFilePath(value(QLatin1String("LogFileName"),
            QLatin1String("InstallationLog.txt")));
        QInstaller::VerboseWriter::instance()->setOutputStream(logFileName);
    }

    d->engineClientHandler->setActive(false);
    delete d;
}

/*!
    Adds the widget with objectName() \a name registered by \a component as a new page
    into the installer's GUI wizard. The widget is added before \a page.
    \a page has to be a value of \ref QInstaller::Installer::WizardPage "WizardPage".
*/
bool Installer::addWizardPage(Component* component, const QString &name, int page)
{
    if (QWidget* const widget = component->userInterface(name)) {
        emit wizardPageInsertionRequested(widget, static_cast<WizardPage>(page));
        return true;
    }
    return false;
}

/*!
    Removes the widget with objectName() \a name previously added to the installer's wizard
    by \a component.
*/
bool Installer::removeWizardPage(Component *component, const QString &name)
{
    if (QWidget* const widget = component->userInterface(name)) {
        emit wizardPageRemovalRequested(widget);
        return true;
    }
    return false;
}

/*!
    Sets the visibility of the default page with id \a page to \a visible, i.e.
    removes or adds it from/to the wizard. This works only for pages which have been
    in the installer when it was started.
 */
bool Installer::setDefaultPageVisible(int page, bool visible)
{
    emit wizardPageVisibilityChangeRequested(visible, page);
    return true;
}

/*!
    Adds the widget with objectName() \a name registered by \a component as an GUI element
    into the installer's GUI wizard. The widget is added on \a page.
    \a page has to be a value of \ref QInstaller::Installer::WizardPage "WizardPage".
*/
bool Installer::addWizardPageItem(Component *component, const QString &name, int page)
{
    if (QWidget* const widget = component->userInterface(name)) {
        emit wizardWidgetInsertionRequested(widget, static_cast<WizardPage>(page));
        return true;
    }
    return false;
}

/*!
    Removes the widget with objectName() \a name previously added to the installer's wizard
    by \a component.
*/
bool Installer::removeWizardPageItem(Component *component, const QString &name)
{
    if (QWidget* const widget = component->userInterface(name)) {
        emit wizardWidgetRemovalRequested(widget);
        return true;
    }
    return false;
}

void Installer::setRemoteRepositories(const QList<Repository> &repositories)
{
    GetRepositoriesMetaInfoJob metaInfoJob(d->m_settings.publicKey(), isPackageManager());
    metaInfoJob.setRepositories(repositories);

    // start...
    metaInfoJob.setAutoDelete(false);
    metaInfoJob.start();
    metaInfoJob.waitForFinished();

    if (metaInfoJob.isCanceled())
        return;

    if (metaInfoJob.error() != KDJob::NoError)
        throw Error(tr("Could not retrieve updates: %1").arg(metaInfoJob.errorString()));

    KDUpdater::Application &updaterApp = *d->m_app;

    const QStringList tempDirs = metaInfoJob.temporaryDirectories();
    d->tempDirDeleter.add(tempDirs);
    foreach (const QString &tmpDir, tempDirs) {
        if (tmpDir.isEmpty())
            continue;
        const QString &applicationName = d->m_settings.applicationName();
        updaterApp.addUpdateSource(applicationName, applicationName, QString(),
            QUrl::fromLocalFile(tmpDir), 1);
    }

    if (updaterApp.updateSourcesInfo()->updateSourceInfoCount() == 0)
        throw Error(tr("Could not reach any update sources."));

    if (!setAndParseLocalComponentsFile(*updaterApp.packagesInfo()))
        return;

    // The changes done above by adding update source don't count as modification that
    // need to be saved cause they will be re-set on the next start anyway. This does
    // prevent creating of xml files not needed atm. We can still set the modified
    // state later once real things changed that we like to restore at the  next startup.
    updaterApp.updateSourcesInfo()->setModified(false);

    // create the packages info
    KDUpdater::UpdateFinder* const updateFinder = new KDUpdater::UpdateFinder(&updaterApp);
    connect(updateFinder, SIGNAL(error(int, QString)), &updaterApp, SLOT(printError(int, QString)));
    updateFinder->setUpdateType(KDUpdater::PackageUpdate | KDUpdater::NewPackage);
    updateFinder->run();

    // now create installable componets
    createComponents(updateFinder->updates(), metaInfoJob);
}

/*!
    Sets additional repository for this instance of the installer or updater
    Will be removed after invoking it again
*/
void Installer::setTemporaryRepositories(const QList<Repository> &repositories, bool replace)
{
    d->m_settings.setTemporaryRepositories(repositories, replace);
}

/*!
    Defines if the downloader should try to download sha1 checksums for archives
*/
void Installer::setTestChecksum(bool test)
{
    d->m_testChecksum = test;
}

/*!
    checks if the downloader should try to download sha1 checksums for archives
*/
bool Installer::testChecksum()
{
    return d->m_testChecksum;
}

/*!
    Creates components from the \a updates found by KDUpdater.
*/
void Installer::createComponentsV2(const QList<KDUpdater::Update*> &updates,
    const GetRepositoriesMetaInfoJob &metaInfoJob)
{
    verbose() << "entered create components V2 in installer" << std::endl;

    emit componentsAboutToBeCleared();

    qDeleteAll(d->m_componentHash);
    d->m_rootComponents.clear();
    d->m_updaterComponents.clear();
    d->m_componentHash.clear();
    QStringList importantUpdates;

    KDUpdater::Application &updaterApp = *d->m_app;
    KDUpdater::PackagesInfo &packagesInfo = *updaterApp.packagesInfo();

    if (isUninstaller() || isPackageManager()) {
        //reads all installed components from components.xml
        if (!setAndParseLocalComponentsFile(packagesInfo))
            return;
        packagesInfo.setApplicationName(d->m_settings.applicationName());
        packagesInfo.setApplicationVersion(d->m_settings.applicationVersion());
    }

    QHash<QString, KDUpdater::PackageInfo> alreadyInstalledPackagesHash;
    foreach (const KDUpdater::PackageInfo &info, packagesInfo.packageInfos()) {
        alreadyInstalledPackagesHash.insert(info.name, info);
    }

    QStringList globalUnNeededList;
    QList<Component*> componentsToSelectInPackagemanager;
    foreach (KDUpdater::Update * const update, updates) {
        const QString name = update->data(QLatin1String("Name")).toString();
        Q_ASSERT(!name.isEmpty());
        QInstaller::Component* component(new QInstaller::Component(this));
        d->componentDeleteList.append(component);
        component->loadDataFromUpdate(update);

        QString installedVersion;

        //to find the installed version number we need to check for all possible names
        QStringList possibleInstalledNameList;
        if (!update->data(QLatin1String("Replaces")).toString().isEmpty()) {
            QString replacesAsString = update->data(QLatin1String("Replaces")).toString();
            QStringList replaceList(replacesAsString.split(QLatin1String(","),
                QString::SkipEmptyParts));
            possibleInstalledNameList.append(replaceList);
            globalUnNeededList.append(replaceList);
        }
        if (alreadyInstalledPackagesHash.contains(name)) {
            possibleInstalledNameList.append(name);
        }
        foreach(const QString &possibleName, possibleInstalledNameList) {
            if (alreadyInstalledPackagesHash.contains(possibleName)) {
                if (!installedVersion.isEmpty()) {
                    qCritical("If installed version is not empty there are more then one package " \
                              "which would be replaced and this is completly wrong");
                    Q_ASSERT(false);
                }
                installedVersion = alreadyInstalledPackagesHash.value(possibleName).version;
            }
            //if we have newer data in the repository we don't need the data from harddisk
            alreadyInstalledPackagesHash.remove(name);
        }

        component->setLocalTempPath(QInstaller::pathFromUrl(update->sourceInfo().url));
        const Repository currentUsedRepository = metaInfoJob.repositoryForTemporaryDirectory(
                    component->localTempPath());
        component->setRepositoryUrl(currentUsedRepository.url());

        // the package manager should preselect the currently installed packages
        if (isPackageManager()) {

            //reset PreviousState and CurrentState to have a chance to get a diff, after the user
            //changed something
            if (installedVersion.isEmpty()) {
                setValue(QLatin1String("PreviousState"), QLatin1String("Uninstalled"));
                setValue(QLatin1String("CurrentState"), QLatin1String("Uninstalled"));
                if (component->value(QLatin1String("NewComponent")) == QLatin1String("true")) {
                    d->m_updaterComponents.append(component);
                }
            } else {
                setValue(QLatin1String("PreviousState"), QLatin1String("Installed"));
                setValue(QLatin1String("CurrentState"), QLatin1String("Installed"));
                const QString updateVersion = update->data(QLatin1String("Version")).toString();
                //check if it is an update
                if (KDUpdater::compareVersion(installedVersion, updateVersion) > 0) {
                    d->m_updaterComponents.append(component);
                }
                if (update->data(QLatin1String("Important"), false).toBool())
                    importantUpdates.append(name);
                componentsToSelectInPackagemanager.append(component);
            }
        }

        d->m_componentHash[name] = component;
    } //foreach (KDUpdater::Update * const update, updates)

    if (!importantUpdates.isEmpty()) {
        //ups, there was an important update - then we only want to show these
        d->m_updaterComponents.clear();
        //maybe in future we have more then one
        foreach (const QString &importantUpdate, importantUpdates) {
            d->m_updaterComponents.append(d->m_componentHash[importantUpdate]);
        }
    }

    // now append all components to their respective parents and loads the scripts,
    // except the components which are aimed for replace to another name(globalUnNeededList)
    QHash<QString, QInstaller::Component*>::iterator it;
    for (it = d->m_componentHash.begin(); it != d->m_componentHash.end(); ++it) {
        QInstaller::Component* currentComponent = *it;
        QString id = it.key();
        if (globalUnNeededList.contains(id))
            continue; //we don't want to append the unneeded components
        while (!id.isEmpty() && currentComponent->parentComponent() == 0) {
            id = id.section(QChar::fromLatin1('.'), 0, -2);
            //is there a component which is called like a parent
            if (d->m_componentHash.contains(id))
                d->m_componentHash[id]->appendComponent(currentComponent);
            else
                d->m_rootComponents.append(currentComponent);
        }
        currentComponent->loadComponentScript();
    }

    // select all components in the updater model
    foreach (QInstaller::Component* const i, d->m_updaterComponents)
        i->setSelected(true, UpdaterMode, Component::InitializeComponentTreeSelectMode);

    // select all components in the package manager model
    foreach (QInstaller::Component* const i, componentsToSelectInPackagemanager)
        i->setSelected(true, InstallerMode, Component::InitializeComponentTreeSelectMode);

    //signals for the qinstallermodel
    emit rootComponentsAdded(d->m_rootComponents);
    emit updaterComponentsAdded(d->m_updaterComponents);
}

/*!
    Creates components from the \a updates found by KDUpdater.
*/
void Installer::createComponents(const QList<KDUpdater::Update*> &updates,
    const GetRepositoriesMetaInfoJob &metaInfoJob)
{
    verbose() << "entered create components in installer" << std::endl;

    emit componentsAboutToBeCleared();

    qDeleteAll(d->componentDeleteList);
    d->componentDeleteList.clear();

    d->m_rootComponents.clear();
    d->m_updaterComponents.clear();
    d->m_packageManagerComponents.clear();
    d->m_componentHash.clear();

    KDUpdater::Application &updaterApp = *d->m_app;
    KDUpdater::PackagesInfo &packagesInfo = *updaterApp.packagesInfo();

    if (isUninstaller() || isPackageManager()) {
        if (!setAndParseLocalComponentsFile(packagesInfo))
            return;
        packagesInfo.setApplicationName(d->m_settings.applicationName());
        packagesInfo.setApplicationVersion(d->m_settings.applicationVersion());
    }

    bool containsImportantUpdates = false;
    QMap<QInstaller::Component*, QString> scripts;
    QMap<QString, QInstaller::Component*> components;
    QList<Component*> componentsToSelectUpdater, componentsToSelectInstaller;

// new from createComponentV2
    //use this hash instead of the plain list of packageinfos
    QHash<QString, KDUpdater::PackageInfo> alreadyInstalledPackagesHash;
    foreach (const KDUpdater::PackageInfo &info, packagesInfo.packageInfos()) {
        alreadyInstalledPackagesHash.insert(info.name, info);
    }
    QStringList globalUnNeededList;
// END -  new from createComponentV2

    //why do we need to add the components if there was a metaInfoJob.error()?
    //will this happen if there is no internet connection?
    //ok usually there should be the installed and the online tree
    if (metaInfoJob.error() == KDJob::UserDefinedError) {
        foreach (const KDUpdater::PackageInfo &info, packagesInfo.packageInfos()) {
            QScopedPointer<QInstaller::Component> component(new QInstaller::Component(this));
            d->componentDeleteList.append(component.data());

            if (components.contains(info.name)) {
                qCritical("Could not register component! Component with identifier %s already "
                    "registered", qPrintable(info.name));
            } else {
                component->loadDataFromPackageInfo(info);
                components.insert(info.name, component.take());
            }
        }
    } else {
        foreach (KDUpdater::Update * const update, updates) {
            const QString newComponentName = update->data(QLatin1String("Name")).toString();

// new from createComponentV2
            const QString name(newComponentName);

            //TODO: use installed version on component
            QString installedVersion;

            //to find the installed version number we need to check for all possible names
            QStringList possibleInstalledNameList;
            if (!update->data(QLatin1String("Replaces")).toString().isEmpty()) {
                QString replacesAsString = update->data(QLatin1String("Replaces")).toString();
                QStringList replaceList(replacesAsString.split(QLatin1String(","),
                    QString::SkipEmptyParts));
                possibleInstalledNameList.append(replaceList);
                globalUnNeededList.append(replaceList);
            }
            if (alreadyInstalledPackagesHash.contains(name)) {
                possibleInstalledNameList.append(name);
            }
            foreach(const QString &possibleName, possibleInstalledNameList) {
                if (alreadyInstalledPackagesHash.contains(possibleName)) {
                    if (!installedVersion.isEmpty()) {
                        qCritical("If installed version is not empty there are more then one package " \
                                  "which would be replaced and this is completly wrong");
                        Q_ASSERT(false);
                    }
                    installedVersion = alreadyInstalledPackagesHash.value(possibleName).version;
                }
            }
// END -  new from createComponentV2

/////////// old code as a check that the new one is working
            const int indexOfPackage = packagesInfo.findPackageInfo(newComponentName);

            QScopedPointer<QInstaller::Component> component(new QInstaller::Component(this));
            d->componentDeleteList.append(component.data());

            if (indexOfPackage > -1) {
                Q_ASSERT(packagesInfo.packageInfo(indexOfPackage).version == installedVersion);
            }
/////////// END - old code as a check that the new one is working

            component->loadDataFromUpdate(update);
            component->setValue(QLatin1String("InstalledVersion"), installedVersion);
            const Repository currentUsedRepository = metaInfoJob.repositoryForTemporaryDirectory(
                        QInstaller::pathFromUrl(update->sourceInfo().url));
            component->setRepositoryUrl(currentUsedRepository.url());

            bool isUpdate = true;
            // the package manager should preselect the currently installed packages
            if (isPackageManager()) {

// new from createComponentV2
                bool newSelect = false;
                foreach(const QString &possibleName, possibleInstalledNameList) {
                    if (alreadyInstalledPackagesHash.contains(possibleName)) {
                        newSelect = true;
                        break;
                    }
                }
// END -  new from createComponentV2

/////////// old code as a check that the new one is working
                {
                    const bool selected = indexOfPackage > -1;
                    if (possibleInstalledNameList.count() == 1 &&
                        possibleInstalledNameList.at(0) == newComponentName) {
                        Q_ASSERT(newSelect == selected);
                    }
                    /////////// END - old code as a check that the new one is working

                    component->updateState(newSelect);

                    if (newSelect)
                        componentsToSelectInstaller.append(component.data());
                }
/////////// END - old code as a check that the new one is working
                const QString pkgVersion = packagesInfo.packageInfo(indexOfPackage).version;
                const QString updateVersion = update->data(QLatin1String("Version")).toString();
                if (KDUpdater::compareVersion(updateVersion, pkgVersion) <= 0)
                    isUpdate = false;

                // It is quite possible that we may have already installed the update.
                // Lets check the last update date of the package and the release date
                // of the update. This way we can compare and figure out if the update
                // has been installed or not.
                const QDate updateDate = update->data(QLatin1String("ReleaseDate")).toDate();
                const QDate pkgDate = packagesInfo.packageInfo(indexOfPackage).lastUpdateDate;
                if (pkgDate > updateDate)
                    isUpdate = false;
            }

            if (!components.contains(newComponentName)) {
                const QString script = update->data(QLatin1String("Script")).toString();
                if (!script.isEmpty()) {
                    scripts.insert(component.data(), QString::fromLatin1("%1/%2/%3").arg(
                            QInstaller::pathFromUrl(update->sourceInfo().url), newComponentName, script));
                }

                Component *tmpComponent = component.data();
                const bool isInstalled = tmpComponent->value(QLatin1String("PreviousState"))
                    == QLatin1String("Installed");
                const bool isNewComponent = tmpComponent->value(QLatin1String("NewComponent"))
                    == QLatin1String("true") ? true : false;
                const bool newPackageForUpdater = !isInstalled && isNewComponent;
                isUpdate = isUpdate && isInstalled;

                if (newPackageForUpdater) {
                    d->m_updaterComponents.push_back(component.data());
                    d->m_componentHash[newComponentName] = tmpComponent;
                    components.insert(newComponentName, component.data());
                } else {
                    components.insert(newComponentName, component.data());
                }

                if (isPackageManager() && (isUpdate || newPackageForUpdater)) {
                    if (update->data(QLatin1String("Important")).toBool())
                        containsImportantUpdates = true;
                    componentsToSelectUpdater.append(tmpComponent);
                    d->m_packageManagerComponents.push_back(tmpComponent);
                }
            } else {
                qCritical("Could not register component! Component with identifier %s already "
                    "registered", qPrintable(newComponentName));
            }
            component.take();
        }
    }

    if (containsImportantUpdates) {
        foreach (QInstaller::Component *c, d->m_packageManagerComponents) {
            if (c->value(QLatin1String("Important")).toLower() == QLatin1String ("false")
                || c->value(QLatin1String("Important")).isEmpty()) {
                    if (isPackageManager())
                        d->m_packageManagerComponents.removeAll(c);
                    componentsToSelectUpdater.removeAll(c);
            }
        }
    }

    // now append all components to their respective parents
    QMap<QString, QInstaller::Component*>::const_iterator it;
    for (it = components.begin(); !d->linearComponentList && it != components.end(); ++it) {
        QInstaller::Component* const comp = *it;
        QString id = it.key();
        if (globalUnNeededList.contains(id))
            continue; //we don't want to append the unneeded components
        while (!id.isEmpty() && comp->parentComponent() == 0) {
            id = id.section(QChar::fromLatin1('.'), 0, -2);
            if (components.contains(id))
                components[id]->appendComponent(comp);
        }
    }

    // append all components w/o parent to the direct list
    for (it = components.begin(); it != components.end(); ++it) {
        if (globalUnNeededList.contains((*it)->name())) {
            //yes const cast is bad, but it is only a hack
            QInstaller::Component* yeahComponent(*it);
            d->willBeReplacedComponents.append(yeahComponent);
            continue; //we don't want to append the unneeded components
        }
        if (d->linearComponentList || (*it)->parentComponent() == 0)
            appendComponent(*it);
    }

    // after everything is set up, load the scripts
    QMapIterator< QInstaller::Component*, QString > scriptIt(scripts);
    while (scriptIt.hasNext()) {
        scriptIt.next();
        QInstaller::Component* const component = scriptIt.key();
        const QString script = scriptIt.value();
        verbose() << "Loading script for component " << component->name() << " (" << script << ")"
            << std::endl;
        component->loadComponentScript(script);
    }

    // select all components in the updater model
    foreach (QInstaller::Component* const i, componentsToSelectUpdater)
        i->setSelected(true, UpdaterMode, Component::InitializeComponentTreeSelectMode);

    // select all components in the package manager model
    foreach (QInstaller::Component* const i, componentsToSelectInstaller) {
        i->setSelected(true, InstallerMode, Component::InitializeComponentTreeSelectMode);
    }

    emit updaterComponentsAdded(d->m_packageManagerComponents);
    emit rootComponentsAdded(d->m_rootComponents);
}

void Installer::appendComponent(Component *component)
{
    d->m_rootComponents.append(component);
    d->m_componentHash[component->name()] = component;
    emit componentAdded(component);
}

int Installer::componentCount(RunModes runMode) const
{
    if (runMode == UpdaterMode)
        return d->m_packageManagerComponents.size();
    return d->m_rootComponents.size();
}

Component *Installer::component(int i, RunModes runMode) const
{
    if (runMode == UpdaterMode)
        return d->m_packageManagerComponents.at(i);
    return d->m_rootComponents.at(i);
}

Component *Installer::component(const QString &name) const
{
    return d->m_componentHash.contains(name) ? d->m_componentHash[name] : 0;
}

QList<Component*> Installer::components(bool recursive, RunModes runMode) const
{
    if (runMode == UpdaterMode)
        return d->m_packageManagerComponents;

    if (!recursive)
        return d->m_rootComponents;

    QList<Component*> result;
    QList<Component*>::const_iterator it;
    for (it = d->m_rootComponents.begin(); it != d->m_rootComponents.end(); ++it) {
        result.push_back(*it);
        result += (*it)->components(true);
    }

    if (runMode == AllMode) {
        for (it = d->m_updaterComponents.begin(); it != d->m_updaterComponents.end(); ++it) {
            result.push_back(*it);
            result += (*it)->components(false);
        }
    }

    return result;
}

QList<Component*> Installer::componentsToInstall(bool recursive, bool sort, RunModes runMode) const
{
    QList<Component*> availableComponents = components(recursive, runMode);
    if (sort) {
        std::sort(availableComponents.begin(), availableComponents.end(),
            Component::PriorityLessThan());
    }

    QList<Component*>::const_iterator it;
    QList<Component*> componentsToInstall;
    for (it = availableComponents.begin(); it != availableComponents.end(); ++it) {
        Component* const comp = *it;
        if (!comp->isSelected(runMode))
            continue;

        // it was already installed before, so don't add it
        if (comp->value(QLatin1String("PreviousState")) == QLatin1String("Installed")
            && runMode == InstallerMode)    // TODO: is the last condition right ????
            continue;

        appendComponentAndMissingDependencies(componentsToInstall, comp);
    }

    return componentsToInstall;
}

static bool componentMatches(const Component *component, const QString &name,
    const QString& version = QString())
{
    if (!name.isEmpty() && component->name() != name)
        return false;

    if (version.isEmpty())
        return true;

    return Installer::versionMatches(component->value(QLatin1String("Version")), version);
}

static Component* subComponentByName(const Installer *installer, const QString &name,
    const QString &version = QString(), Component *check = 0)
{
    if (check != 0 && componentMatches(check, name, version))
        return check;

    const QList<Component*> comps = check == 0 ? installer->components() : check->components();
    for (QList<Component*>::const_iterator it = comps.begin(); it != comps.end(); ++it) {
        Component* const result = subComponentByName(installer, name, version, *it);
        if (result != 0)
            return result;
    }

    const QList<Component*> uocomps =
        check == 0 ? installer->components(false, UpdaterMode) : check->components(false, UpdaterMode);
    for (QList<Component*>::const_iterator it = uocomps.begin(); it != uocomps.end(); ++it) {
        Component* const result = subComponentByName(installer, name, version, *it);
        if (result != 0)
            return result;
    }

    return 0;
}

void Installer::setLinearComponentList(bool showlinear)
{
    d->linearComponentList = showlinear;
}

bool Installer::hasLinearComponentList() const
{
    return d->linearComponentList;
}

/*!
    Returns a component matching \a name. \a name can also contains a version requirement.
    E.g. "com.nokia.sdk.qt" returns any component with that name, "com.nokia.sdk.qt->=4.5" requires
    the returned component to have at least version 4.5.
    If no component matches the requirement, 0 is returned.
*/
Component* Installer::componentByName(const QString &name) const
{
    if (name.contains(QChar::fromLatin1('-'))) {
        // the last part is considered to be the version, then
        const QString version = name.section(QLatin1Char('-'), 1);
        return subComponentByName(this, name.section(QLatin1Char('-'), 0, 0), version);
    }

    QHash< QString, QInstaller::Component* >::ConstIterator it = d->m_componentHash.constFind(name);
    Component * comp = 0;
    if (it != d->m_componentHash.constEnd())
        comp = *it;
    if (d->m_updaterComponents.contains(comp))
        return comp;

    return subComponentByName(this, name);
}

/*!
    Returns a list of packages depending on \a component.
*/
QList<Component*> Installer::dependees(const Component *component) const
{
    QList<Component*> result;

    const QList<Component*> allComponents = components(true, AllMode);
    for (QList<Component*>::const_iterator it = allComponents.begin(); it != allComponents.end(); ++it) {
        Component* const c = *it;

        const QStringList deps = c->value(QString::fromLatin1("Dependencies"))
            .split(QChar::fromLatin1(','), QString::SkipEmptyParts);

        const QLatin1Char dash('-');
        for (QStringList::const_iterator it2 = deps.begin(); it2 != deps.end(); ++it2) {
            // the last part is considered to be the version, then
            const QString id = it2->contains(dash) ? it2->section(dash, 0, 0) : *it2;
            const QString version = it2->contains(dash) ? it2->section(dash, 1) : QString();
            if (componentMatches(component, id, version))
                result.push_back(c);
        }
    }

    return result;
}

InstallerSettings Installer::settings() const
{
    return d->m_settings;
}

/*!
    Returns a list of dependencies for \a component.
    If there's a dependency which cannot be fullfilled, the list contains 0 values.
*/
QList<Component*> Installer::dependencies(const Component *component,
    QStringList *missingPackageNames) const
{
    QList<Component*> result;
    const QStringList deps = component->value(QString::fromLatin1("Dependencies"))
        .split(QChar::fromLatin1(','), QString::SkipEmptyParts);

    for (QStringList::const_iterator it = deps.begin(); it != deps.end(); ++it) {
        const QString name = *it;
        Component* comp = componentByName(*it);
        if (!comp && missingPackageNames)
            missingPackageNames->append(name);
        else
            result.push_back(comp);
    }
    return result;
}

/*!
    Returns the list of all missing (not installed) dependencies for \a component.
*/
QList<Component*> Installer::missingDependencies(const Component *component) const
{
    QList<Component*> result;
    const QStringList deps = component->value(QString::fromLatin1("Dependencies"))
        .split(QChar::fromLatin1(','), QString::SkipEmptyParts);

    const QLatin1Char dash('-');
    for (QStringList::const_iterator it = deps.begin(); it != deps.end(); ++it) {
        const bool containsVersionString = it->contains(dash);
        const QString version =  containsVersionString ? it->section(dash, 1) : QString();
        const QString name = containsVersionString ? it->section(dash, 0, 0) : *it;

        bool installed = false;
        const QList<Component*> compList = components(true);
        foreach (const Component* comp, compList) {
            if (!name.isEmpty() && comp->name() == name && !version.isEmpty()) {
                if (Installer::versionMatches(comp->value(QLatin1String("InstalledVersion")), version))
                    installed = true;
            } else if (comp->name() == name) {
                installed = true;
            }
        }

        foreach (const Component *comp,  d->m_updaterComponents) {
            if (!name.isEmpty() && comp->name() == name && !version.isEmpty()) {
                if (Installer::versionMatches(comp->value(QLatin1String("InstalledVersion")), version))
                    installed = true;
            } else if (comp->name() == name) {
                installed = true;
            }
        }

        if (!installed) {
            if (Component *comp = componentByName(name))
                result.push_back(comp);
        }
    }
    return result;
}

/*!
    This method tries to gain admin rights. On success, it returns true.
*/
bool Installer::gainAdminRights()
{
    if (AdminAuthorization::hasAdminRights())
        return true;

    d->engineClientHandler->setActive(true);
    if (!d->engineClientHandler->isActive())
        throw Error(QObject::tr("Error while elevating access rights."));
    return true;
}

/*!
    This method drops gained admin rights.
*/
void Installer::dropAdminRights()
{
    d->engineClientHandler->setActive(false);
}

/*!
    Return true, if a process with \a name is running. On Windows, the comparision is case-insensitive.
*/
bool Installer::isProcessRunning(const QString &name) const
{
    QList<KDSysInfo::ProcessInfo>::const_iterator it;
    const QList<KDSysInfo::ProcessInfo> processes = KDSysInfo::runningProcesses();
    for (it = processes.begin(); it != processes.end(); ++it) {
#ifndef Q_WS_WIN
        if (it->name == name)
            return true;
        const QFileInfo fi(it->name);
        if (fi.fileName() == name || fi.baseName() == name)
            return true;
#else
        if (it->name.toLower() == name.toLower())
            return true;
        const QFileInfo fi(it->name);
        if (fi.fileName().toLower() == name.toLower() || fi.baseName().toLower() == name.toLower())
            return true;
#endif
    }
    return false;
}

/*!
    Executes a program.

    \param program The program that should be executed.
    \param arguments Optional list of arguments.
    \param stdIn Optional stdin the program reads.
    \return If the command could not be executed, an empty QList, otherwise the output of the
            command as first item, the return code as second item.
    \note On Unix, the output is just the output to stdout, not to stderr.
*/
QList<QVariant> Installer::execute(const QString &program, const QStringList &arguments,
    const QString &stdIn) const
{
    QProcess p;
    p.start(program, arguments, stdIn.isNull() ? QIODevice::ReadOnly : QIODevice::ReadWrite);
    if (!p.waitForStarted())
        return QList< QVariant >();

    if (!stdIn.isNull()) {
        p.write(stdIn.toLatin1());
        p.closeWriteChannel();
    }

    QEventLoop loop;
    connect(&p, SIGNAL(finished(int, QProcess::ExitStatus)), &loop, SLOT(quit()));
    loop.exec();

    return QList< QVariant >() << QString::fromLatin1(p.readAllStandardOutput()) << p.exitCode();
}

/*!
    Returns an environment variable.
*/
QString Installer::environmentVariable(const QString &name) const
{
#ifdef Q_WS_WIN
    const LPCWSTR n =  (LPCWSTR) name.utf16();
    LPTSTR buff = (LPTSTR) malloc(4096 * sizeof(TCHAR));
    DWORD getenvret = GetEnvironmentVariable(n, buff, 4096);
    const QString actualValue = getenvret != 0
        ? QString::fromUtf16((const unsigned short *) buff) : QString();
    free(buff);
    return actualValue;
#else
    const char *pPath = name.isEmpty() ? 0 : getenv(name.toLatin1());
    return pPath ? QLatin1String(pPath) : QString();
#endif
}

/*!
    Instantly performns an operation \a name with \a arguments.
    \sa Component::addOperation
*/
bool Installer::performOperation(const QString &name, const QStringList &arguments)
{
    QScopedPointer<KDUpdater::UpdateOperation> op(KDUpdater::UpdateOperationFactory::instance()
        .create(name));
    if (!op.data())
        return false;

    op->setArguments(arguments);
    op->backup();
    if (!performOperationThreaded(op.data())) {
        performOperationThreaded(op.data(), Undo);
        return false;
    }
    return true;
}

/*!
    Returns true when \a version matches the \a requirement.
    \a requirement can be a fixed version number or it can be prefix by the comparaters '>', '>=',
    '<', '<=' and '='.
*/
bool Installer::versionMatches(const QString &version, const QString &requirement)
{
    QRegExp compEx(QLatin1String("([<=>]+)(.*)"));
    const QString comparator = compEx.exactMatch(requirement) ? compEx.cap(1) : QString::fromLatin1("=");
    const QString ver = compEx.exactMatch(requirement) ? compEx.cap(2) : requirement;

    const bool allowEqual = comparator.contains(QLatin1Char('='));
    const bool allowLess = comparator.contains(QLatin1Char('<'));
    const bool allowMore = comparator.contains(QLatin1Char('>'));

    if (allowEqual && version == ver)
        return true;

    if (allowLess && KDUpdater::compareVersion(ver, version) > 0)
        return true;

    if (allowMore && KDUpdater::compareVersion(ver, version) < 0)
        return true;

    return false;
}

/*!
    Finds a library named \a name in \a pathes.
    If \a pathes is empty, it gets filled with platform dependent default pathes.
    The resulting path is stored in \a library.
    This method can be used by scripts to check external dependencies.
*/
QString Installer::findLibrary(const QString &name, const QStringList &pathes)
{
    QStringList findPathes = pathes;
#if defined(Q_WS_WIN)
    return findPath(QString::fromLatin1("%1.lib").arg(name), findPathes);
#elif defined(Q_WS_MAC)
    if (findPathes.isEmpty()) {
        findPathes.push_back(QLatin1String("/lib"));
        findPathes.push_back(QLatin1String("/usr/lib"));
        findPathes.push_back(QLatin1String("/usr/local/lib"));
        findPathes.push_back(QLatin1String("/opt/local/lib"));
    }

    const QString dynamic = findPath(QString::fromLatin1("lib%1.dylib").arg(name), findPathes);
    if (!dynamic.isEmpty())
        return dynamic;
    return findPath(QString::fromLatin1("lib%1.a").arg(name), findPathes);
#else
    if (findPathes.isEmpty()) {
        findPathes.push_back(QLatin1String("/lib"));
        findPathes.push_back(QLatin1String("/usr/lib"));
        findPathes.push_back(QLatin1String("/usr/local/lib"));
        findPathes.push_back(QLatin1String("/opt/local/lib"));
    }
    const QString dynamic = findPath(QString::fromLatin1("lib%1.so*").arg(name), findPathes);
    if (!dynamic.isEmpty())
        return dynamic;
    return findPath(QString::fromLatin1("lib%1.a").arg(name), findPathes);
#endif
}

/*!
    Tries to find a file name \a name in one of \a pathes.
    The resulting path is stored in \a path.
    This method can be used by scripts to check external dependencies.
*/
QString Installer::findPath(const QString &name, const QStringList &pathes)
{
    for (QStringList::const_iterator it = pathes.begin(); it != pathes.end(); ++it) {
        const QDir dir(*it);
        const QStringList entries = dir.entryList(QStringList() << name, QDir::Files | QDir::Hidden);
        if (entries.isEmpty())
            continue;

        return dir.absoluteFilePath(entries.first());
    }
    return QString();
}

/*!
    sets the "installerbase" binary to use when writing the package manager/uninstaller.
    Set this if an update to installerbase is available.
    If not set, the executable segment of the running un/installer will be used.
*/
void Installer::setInstallerBaseBinary(const QString &path)
{
    d->m_forceRestart = true;
    d->installerBaseBinaryUnreplaced = path;
}

/*!
    Returns the installer value for \a key. If \a key is not known to the system, \a defaultValue is
    returned. Additionally, on Windows, \a key can be a registry key.
*/
QString Installer::value(const QString &key, const QString &defaultValue) const
{
#ifdef Q_WS_WIN
    if (!d->m_vars.contains(key)) {
        static const QRegExp regex(QLatin1String("\\\\|/"));
        const QString filename = key.section(regex, 0, -2);
        const QString regKey = key.section(regex, -1);
        const QSettings registry(filename, QSettings::NativeFormat);
        if (!filename.isEmpty() && !regKey.isEmpty() && registry.contains(regKey))
            return registry.value(regKey).toString();
    }
#else
    if (key == QLatin1String("TargetDir")) {
        const QString dir = d->m_vars.value(key, defaultValue);
        if (dir.startsWith(QLatin1String("~/")))
            return QDir::home().absoluteFilePath(dir.mid(2));
        else
            return dir;
    }
#endif
    return d->m_vars.value(key, defaultValue);
}

/*!
    Sets the installer value for \a key to \a value.
*/
void Installer::setValue(const QString &key, const QString &value)
{
    if (d->m_vars.value(key) == value)
        return;

    d->m_vars.insert(key, value);
    emit valueChanged(key, value);
}

/*!
    Returns true, when the installer contains a value for \a key.
*/
bool Installer::containsValue(const QString &key) const
{
    return d->m_vars.contains(key);
}

void Installer::setSharedFlag(const QString &key, bool value)
{
    d->m_sharedFlags.insert(key, value);
}

bool Installer::sharedFlag(const QString &key) const
{
    return d->m_sharedFlags.value(key, false);
}

bool Installer::isVerbose() const
{
    return QInstaller::isVerbose();
}

void Installer::setVerbose(bool on)
{
    QInstaller::setVerbose(on);
}

int Installer::status() const
{
    return d->m_status;
}
/*!
    returns true if at least one complete installation/update
    was successfull, even if the user cancelled the newest
    installation process.
*/
bool Installer::finishedWithSuccess() const
{
    return (d->m_status == InstallerSucceeded) || d->m_needToWriteUninstaller;
}

void Installer::interrupt()
{
    verbose() << "INTERRUPT INSTALLER" << std::endl;
    d->setStatus(InstallerCanceledByUser);
    emit installationInterrupted();
}

void Installer::setCanceled()
{
    d->setStatus(InstallerCanceledByUser);
}

/*!
    Replaces all variables within \a str by their respective values and
    returns the result.
*/
QString Installer::replaceVariables(const QString &str) const
{
    return d->replaceVariables(str);
}

/*!
    Replaces all variables in any of \a str by their respective values and
    returns the results.
    \overload
*/
QStringList Installer::replaceVariables(const QStringList &str) const
{
    QStringList result;
    for (QStringList::const_iterator it = str.begin(); it != str.end(); ++it)
        result.push_back(d->replaceVariables(*it));

    return result;
}

/*!
    Replaces all variables within \a ba by their respective values and
    returns the result.
    \overload
*/
QByteArray Installer::replaceVariables(const QByteArray &ba) const
{
    return d->replaceVariables(ba);
}

/*!
    Returns the path to the installer binary.
*/
QString Installer::installerBinaryPath() const
{
    return d->installerBinaryPath();
}

/*!
    Returns true when this is the installer running.
*/
bool Installer::isInstaller() const
{
    return d->isInstaller();
}

/*!
    Returns true when this is the uninstaller running.
*/
bool Installer::isUninstaller() const
{
    return d->isUninstaller();
}

/*!
    Returns true when this is the package manager running.
*/
bool Installer::isPackageManager() const
{
    return d->isPackageManager();
}


/*!
    Returns true if this is an offline-only installer.
*/
bool Installer::isOfflineOnly() const
{
    QSettings confInternal(QLatin1String(":/config/config-internal.ini"), QSettings::IniFormat);
    return confInternal.value(QLatin1String("offlineOnly")).toBool();
}

void Installer::setPackageManager()
{
    d->packageManagingMode = true;
}

/*!
    Returns thrue when this is neither an installer nor an uninstaller running.
    Must be an updater, then.
*/
bool Installer::isUpdater() const
{
    return !d->isInstaller() && !d->isUninstaller();
}

/*!
    Runs the installer. Returns true on success, false otherwise.
*/
bool Installer::runInstaller()
{
    try {
        d->runInstaller();
        return true;
    } catch (...) {
        return false;
    }
}

/*!
    Runs the uninstaller. Returns true on success, false otherwise.
*/
bool Installer::runUninstaller()
{
    try {
        d->runUninstaller();
        return true;
    } catch (...) {
        return false;
    }
}

/*!
    Runs the package updater. Returns true on success, false otherwise.
*/
bool Installer::runPackageUpdater()
{
    try {
        d->runPackageUpdater();
        return true;
    } catch (...) {
        return false;
    }
}

/*!
    \internal
    Calls languangeChanged on all components.
*/
void Installer::languageChanged()
{
    const QList<Component*> comps = components(true);
    foreach (Component* component, comps)
        component->languageChanged();
}

/*!
    Runs the installer or uninstaller, depending on the type of this binary.
*/
bool Installer::run()
{
    try {
        if (isInstaller())
            d->runInstaller();
        else if (isUninstaller())
            d->runUninstaller();
        else if (isPackageManager())
            d->runPackageUpdater();
        return true;
    } catch (const Error &err) {
        verbose() << "Caught Installer Error: " << err.message() << std::endl;
        return false;
    }
}

/*!
    Returns the path name of the ininstaller binary.
*/
QString Installer::uninstallerName() const
{
    return d->uninstallerName();
}

bool Installer::setAndParseLocalComponentsFile(KDUpdater::PackagesInfo &packagesInfo)
{
    packagesInfo.setFileName(d->localComponentsXmlPath());
    const QString localComponentsXml = d->localComponentsXmlPath();

    // handle errors occured by loading components.xml
    QFileInfo componentFileInfo(localComponentsXml);
    int silentRetries = d->m_silentRetries;
    while (!componentFileInfo.exists()) {
        if (silentRetries > 0) {
            --silentRetries;
        } else {
            Status status = handleComponentsFileSetOrParseError(localComponentsXml);
            if (status == InstallerCanceledByUser)
                return false;
        }
        packagesInfo.setFileName(localComponentsXml);
    }

    silentRetries = d->m_silentRetries;
    while (packagesInfo.error() != KDUpdater::PackagesInfo::NoError) {
        if (silentRetries > 0) {
            --silentRetries;
        } else {
            Status status = handleComponentsFileSetOrParseError(localComponentsXml);
            if (status == InstallerCanceledByUser)
                return false;
        }
        packagesInfo.setFileName(localComponentsXml);
    }

    silentRetries = d->m_silentRetries;
    while (packagesInfo.error() != KDUpdater::PackagesInfo::NoError) {
        if (silentRetries > 0) {
            --silentRetries;
        } else {
            bool retry = false;
            if (packagesInfo.error() != KDUpdater::PackagesInfo::InvalidContentError
                && packagesInfo.error() != KDUpdater::PackagesInfo::InvalidXmlError) {
                    retry = true;
            }
            Status status = handleComponentsFileSetOrParseError(componentFileInfo.fileName(),
                packagesInfo.errorString(), retry);
            if (status == InstallerCanceledByUser)
                return false;
        }
        packagesInfo.setFileName(localComponentsXml);
    }

    return true;
}

Installer::Status Installer::handleComponentsFileSetOrParseError(const QString &arg1,
    const QString &arg2, bool withRetry)
{
    QMessageBox::StandardButtons buttons = QMessageBox::Cancel;
    if (withRetry)
        buttons |= QMessageBox::Retry;

    const QMessageBox::StandardButton button =
        MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(),
        QLatin1String("Error loading component.xml"), tr("Loading error"),
        tr(arg2.isEmpty() ? "Could not load %1" : "Could not load %1 : %2").arg(arg1, arg2),
        buttons);

    if (button == QMessageBox::Cancel) {
        d->m_status = InstallerFailed;
        return InstallerCanceledByUser;
    }
    return InstallerUnfinished;
}

#include "qinstaller.moc"
