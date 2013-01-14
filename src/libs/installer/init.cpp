/**************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
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

// QtSDK specific
#include "qtpatchoperation.h"
#include "setdemospathonqtoperation.h"
#include "setexamplespathonqtoperation.h"
#include "setpluginpathonqtcoreoperation.h"
#include "setimportspathonqtcoreoperation.h"
#include "setpathonqtcoreoperation.h"
#include "registerqtvqnxoperation.h"
#include "setqtcreatorvalueoperation.h"
#include "addqtcreatorarrayvalueoperation.h"
#include "registertoolchainoperation.h"
#include "registerdefaultdebuggeroperation.h"


#ifdef Q_OS_MAC
#   include "macreplaceinstallnamesoperation.h"
#endif // Q_OS_MAC

#include "utils.h"

#include "kdupdaterupdateoperationfactory.h"
#include "kdupdaterfiledownloaderfactory.h"

#include <unix/C/7zCrc.h>

#include <iostream>

namespace NArchive {
namespace NBz2 { void registerArcBZip2(); }
namespace NGz { void registerArcGZip(); }
namespace NLzma { namespace NLzmaAr { void registerArcLzma(); } }
namespace NLzma { namespace NLzma86Ar { void registerArcLzma86(); } }
namespace NSplit { void registerArcSplit(); }
namespace NXz { void registerArcxz(); }
namespace NZ { void registerArcZ(); }
}

void registerArc7z();
void registerArcCab();
void registerArcTar();
void registerArcZip();

void registerCodecBCJ2();
void registerCodecBCJ();
void registerCodecBCJ();
void registerCodecByteSwap();
void registerCodecBZip2();
void registerCodecCopy();
void registerCodecDeflate64();
void registerCodecDeflate();
void registerCodecDelta();
void registerCodecLZMA2();
void registerCodecLZMA();
void registerCodecPPMD();
void registerCodec7zAES();

using namespace NArchive;
using namespace KDUpdater;
using namespace QInstaller;

static void initArchives()
{
    NBz2::registerArcBZip2();
    NGz::registerArcGZip();
    NLzma::NLzmaAr::registerArcLzma();
    NLzma::NLzma86Ar::registerArcLzma86();
    NSplit::registerArcSplit();
    NXz::registerArcxz();
    NZ::registerArcZ();
    registerArc7z();
    registerArcCab();
    registerArcTar();
    registerArcZip();

    registerCodecBCJ2();
    registerCodecBCJ();
    registerCodecBCJ();
    registerCodecByteSwap();
    registerCodecBZip2();
    registerCodecCopy();
    registerCodecDeflate64();
    registerCodecDeflate();
    registerCodecDelta();
    registerCodecLZMA2();
    registerCodecLZMA();
    registerCodecPPMD();
    registerCodec7zAES();

    CrcGenerateTable();
}

static void initResources()
{
    Q_INIT_RESOURCE(patch_file_lists);
}

static QByteArray trimAndPrepend(QtMsgType type, const QByteArray &msg)
{
    QByteArray ba(msg);
    // last character is a space from qDebug
    if (ba.endsWith(' '))
        ba.chop(1);

    // remove quotes if the whole message is surrounded with them
    if (ba.startsWith('"') && ba.endsWith('"'))
        ba = ba.mid(1, ba.length() - 2);

    // prepend the message type, skip QtDebugMsg
    switch (type) {
        case QtWarningMsg:
            ba.prepend("Warning: ");
        break;

        case QtCriticalMsg:
            ba.prepend("Critical: ");
        break;

        case QtFatalMsg:
            ba.prepend("Fatal: ");
        break;

        default:
            break;
    }
    return ba;
}

#if QT_VERSION < 0x050000
static void messageHandler(QtMsgType type, const char *msg)
{
    const QByteArray ba = trimAndPrepend(type, msg);
#else
void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QByteArray ba = trimAndPrepend(type, msg.toLocal8Bit());
    if (type != QtDebugMsg) {
        ba += QByteArray(" (") + context.file + QByteArray(":").append(context.line) + QByteArray(", ")
            + context.function + QByteArray(")");
    }
#endif

    verbose() << ba.constData() << std::endl;
    if (!isVerbose() && type != QtDebugMsg)
        std::cout << ba.constData() << std::endl << std::endl;

    if (type == QtFatalMsg) {
#if QT_VERSION < 0x050000
        QtMsgHandler oldMsgHandler = qInstallMsgHandler(0);
        qt_message_output(type, msg);
        qInstallMsgHandler(oldMsgHandler);
#else
        QtMessageHandler oldMsgHandler = qInstallMessageHandler(0);
        qt_message_output(type, context, msg);
        qInstallMessageHandler(oldMsgHandler);
#endif
    }
}

void QInstaller::init()
{
    ::initResources();

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

    // QtSDK specific
    factory.registerUpdateOperation<RegisterQtInCreatorQNXOperation>(QLatin1String("RegisterQtInCreatorQNX"));
    factory.registerUpdateOperation<RegisterToolChainOperation>(QLatin1String("RegisterToolChain"));
    factory.registerUpdateOperation<RegisterDefaultDebuggerOperation>(QLatin1String("RegisterDefaultDebugger"));
    factory.registerUpdateOperation<SetDemosPathOnQtOperation>(QLatin1String("SetDemosPathOnQt"));
    factory.registerUpdateOperation<SetExamplesPathOnQtOperation>(QLatin1String("SetExamplesPathOnQt"));
    factory.registerUpdateOperation<SetPluginPathOnQtCoreOperation>(QLatin1String("SetPluginPathOnQtCore"));
    factory.registerUpdateOperation<SetImportsPathOnQtCoreOperation>(QLatin1String("SetImportsPathOnQtCore"));
    factory.registerUpdateOperation<SetPathOnQtCoreOperation>(QLatin1String("SetPathOnQtCore"));
    factory.registerUpdateOperation<SetQtCreatorValueOperation>(QLatin1String("SetQtCreatorValue"));
    factory.registerUpdateOperation<AddQtCreatorArrayValueOperation>(QLatin1String("AddQtCreatorArrayValue"));
    factory.registerUpdateOperation<QtPatchOperation>(QLatin1String("QtPatch"));

    FileDownloaderFactory::setFollowRedirects(true);

#ifdef Q_OS_MAC
    factory.registerUpdateOperation<MacReplaceInstallNamesOperation>(QLatin1String("ReplaceInstallNames"));
#endif // Q_OS_MAC

    // load 7z stuff, if we're a static lib
    ::initArchives();

   // qDebug -> verbose()
#if QT_VERSION < 0x050000
   qInstallMsgHandler(messageHandler);
#else
   qInstallMessageHandler(messageHandler);
#endif
}
