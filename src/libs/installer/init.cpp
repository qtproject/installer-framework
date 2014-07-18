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
#include "init.h"

#include "createshortcutoperation.h"
#include "createdesktopentryoperation.h"
#include "createlocalrepositoryoperation.h"
#include "extractarchiveoperation.h"
#include "globalsettingsoperation.h"
#include "environmentvariablesoperation.h"
#include "registerfiletypeoperation.h"
#include "selfrestartoperation.h"
#include "installiconsoperation.h"
#include "elevatedexecuteoperation.h"
#include "fakestopprocessforupdateoperation.h"
#include "createlinkoperation.h"
#include "simplemovefileoperation.h"
#include "copydirectoryoperation.h"
#include "replaceoperation.h"
#include "linereplaceoperation.h"
#include "minimumprogressoperation.h"
#include "licenseoperation.h"
#include "applyproductkeyoperation.h"
#include "settingsoperation.h"

// QtSDK specific
#include "qtpatchoperation.h"
#include "consumeoutputoperation.h"
#include "setqtcreatorvalueoperation.h"
#include "addqtcreatorarrayvalueoperation.h"

#ifdef Q_OS_OSX
#   include "macreplaceinstallnamesoperation.h"
#endif // Q_OS_OSX

#include "utils.h"

#include "kdupdaterupdateoperationfactory.h"
#include "kdupdaterfiledownloaderfactory.h"

#include "7zCrc.h"

#include <QtPlugin>
#include <QElapsedTimer>

#include <iostream>

namespace NArchive {
    namespace NXz {
        void registerArcxz();
    }
    namespace NSplit {
        void registerArcSplit();
    }
    namespace NLzma {
        namespace NLzmaAr {
            void registerArcLzma();
        }
        namespace NLzma86Ar {
            void registerArcLzma86();
        }
    }
}

void registerArc7z();

void registerCodecBCJ();
void registerCodecBCJ2();

void registerCodecLZMA();
void registerCodecLZMA2();

void registerCodecDelta();
void registerCodecBranch();
void registerCodecByteSwap();

using namespace KDUpdater;
using namespace QInstaller;

static void initArchives()
{
    CrcGenerateTable();

    registerArc7z();

    registerCodecBCJ();
    registerCodecBCJ2();
    registerCodecLZMA();
    registerCodecLZMA2();

    registerCodecDelta();
    registerCodecBranch();
    registerCodecByteSwap();

    NArchive::NXz::registerArcxz();
    NArchive::NSplit::registerArcSplit();
    NArchive::NLzma::NLzmaAr::registerArcLzma();
    NArchive::NLzma::NLzma86Ar::registerArcLzma86();
}

#if defined(QT_STATIC)
static void initResources()
{
    Q_INIT_RESOURCE(patch_file_lists);
    Q_INIT_RESOURCE(installer);
}
#endif

static QString trimAndPrepend(QtMsgType type, const QString &msg)
{
    QString ba(msg);
    // last character is a space from qDebug
    if (ba.endsWith(QLatin1Char(' ')))
        ba.chop(1);

    // remove quotes if the whole message is surrounded with them
    if (ba.startsWith(QLatin1Char('"')) && ba.endsWith(QLatin1Char('"')))
        ba = ba.mid(1, ba.length() - 2);

    // prepend the message type, skip QtDebugMsg
    switch (type) {
        case QtWarningMsg:
            ba.prepend(QStringLiteral("Warning: "));
        break;

        case QtCriticalMsg:
            ba.prepend(QStringLiteral("Critical: "));
        break;

        case QtFatalMsg:
            ba.prepend(QStringLiteral("Fatal: "));
        break;

        default:
            break;
    }
    return ba;
}

// start timer on construction (so we can use it as static member)
class Uptime : public QElapsedTimer {
public:
    Uptime() { start(); }
};

void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    // suppress warning from QPA minimal plugin
    if (msg.contains(QLatin1String("This plugin does not support propagateSizeHints")))
        return;

    static Uptime uptime;

    QString ba = QLatin1Char('[') + QString::number(uptime.elapsed()) + QStringLiteral("] ")
            + trimAndPrepend(type, msg);

    if (type != QtDebugMsg) {
        ba += QString(QStringLiteral(" (%1:%2, %3)")).arg(
                    QString::fromLatin1(context.file)).arg(context.line).arg(
                    QString::fromLatin1(context.function));
    }

    if (VerboseWriter *log = VerboseWriter::instance())
        log->appendLine(ba);

    if (type != QtDebugMsg || isVerbose())
        std::cout << qPrintable(ba) << std::endl;

    if (type == QtFatalMsg) {
        QtMessageHandler oldMsgHandler = qInstallMessageHandler(0);
        qt_message_output(type, context, msg);
        qInstallMessageHandler(oldMsgHandler);
    }
}

void QInstaller::init()
{
    ::initArchives();
#if defined(QT_STATIC)
    ::initResources();
#endif

    UpdateOperationFactory &factory = UpdateOperationFactory::instance();
    factory.registerUpdateOperation<CreateShortcutOperation>(QLatin1String("CreateShortcut"));
    factory.registerUpdateOperation<CreateDesktopEntryOperation>(QLatin1String("CreateDesktopEntry"));
    factory.registerUpdateOperation<CreateLocalRepositoryOperation>(QLatin1String("CreateLocalRepository"));
    factory.registerUpdateOperation<ExtractArchiveOperation>(QLatin1String("Extract"));
    factory.registerUpdateOperation<GlobalSettingsOperation>(QLatin1String("GlobalConfig"));
    factory.registerUpdateOperation<EnvironmentVariableOperation>(QLatin1String("EnvironmentVariable"));
    factory.registerUpdateOperation<RegisterFileTypeOperation>(QLatin1String("RegisterFileType"));
    factory.registerUpdateOperation<SelfRestartOperation>(QLatin1String("SelfRestart"));
    factory.registerUpdateOperation<InstallIconsOperation>(QLatin1String("InstallIcons"));
    factory.registerUpdateOperation<ElevatedExecuteOperation>(QLatin1String("Execute"));
    factory.registerUpdateOperation<FakeStopProcessForUpdateOperation>(QLatin1String("FakeStopProcessForUpdate"));
    factory.registerUpdateOperation<CreateLinkOperation>(QLatin1String("CreateLink"));
    factory.registerUpdateOperation<SimpleMoveFileOperation>(QLatin1String("SimpleMoveFile"));
    factory.registerUpdateOperation<CopyDirectoryOperation>(QLatin1String("CopyDirectory"));
    factory.registerUpdateOperation<ReplaceOperation>(QLatin1String("Replace"));
    factory.registerUpdateOperation<LineReplaceOperation>(QLatin1String("LineReplace"));
    factory.registerUpdateOperation<MinimumProgressOperation>(QLatin1String("MinimumProgress"));
    factory.registerUpdateOperation<LicenseOperation>(QLatin1String("License"));
    factory.registerUpdateOperation<ApplyProductKeyOperation>(QLatin1String("ApplyProductKey"));
    factory.registerUpdateOperation<ConsumeOutputOperation>(QLatin1String("ConsumeOutput"));
    factory.registerUpdateOperation<SettingsOperation>(QLatin1String("Settings"));

    // QtSDK specific
    factory.registerUpdateOperation<SetQtCreatorValueOperation>(QLatin1String("SetQtCreatorValue"));
    factory.registerUpdateOperation<AddQtCreatorArrayValueOperation>(QLatin1String("AddQtCreatorArrayValue"));
    factory.registerUpdateOperation<QtPatchOperation>(QLatin1String("QtPatch"));

    FileDownloaderFactory::setFollowRedirects(true);

#ifdef Q_OS_OSX
    factory.registerUpdateOperation<MacReplaceInstallNamesOperation>(QLatin1String("ReplaceInstallNames"));
#endif // Q_OS_OSX
   qInstallMessageHandler(messageHandler);
}
