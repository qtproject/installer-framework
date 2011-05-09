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
#include "init.h"

#include "createshortcutoperation.h"
#include "createdesktopentryoperation.h"
#include "extractarchiveoperation.h"
#include "globalsettingsoperation.h"
#include "environmentvariablesoperation.h"
#include "registerfiletypeoperation.h"
#include "selfrestartoperation.h"
#include "installiconsoperation.h"
#include "elevatedexecuteoperation.h"
#include "fakestopprocessforupdateoperation.h"

//added for NDK
#include "copydirectoryoperation.h"
#include "qtpatchoperation.h"
#include "setdemospathonqtoperation.h"
#include "setexamplespathonqtoperation.h"
#include "setpluginpathonqtcoreoperation.h"
#include "setimportspathonqtcoreoperation.h"
#include "replaceoperation.h"
#include "licenseoperation.h"
#include "linereplaceoperation.h"
#include "registerdocumentationoperation.h"
#include "registerqtoperation.h"
#include "registerqtv2operation.h"
#include "setqtcreatorvalueoperation.h"
#include "simplemovefileoperation.h"
#include "registertoolchainoperation.h"

#include "minimumprogressoperation.h"

#ifdef Q_OS_MAC
    #include "macreplaceinstallnamesoperation.h"
#endif // Q_OS_MAC

#include "common/utils.h"


#include <QtPlugin>

#include <KDUpdater/UpdateOperation>
#include <KDUpdater/UpdateOperationFactory>
#include <KDUpdater/FileDownloaderFactory>

#include <QNetworkProxyFactory>

#include <unix/C/7zCrc.h>

namespace NArchive
{
namespace NBz2{void registerArcBZip2();}
namespace NGz{void registerArcGZip();}
namespace NLzma{ namespace NLzmaAr { void registerArcLzma();}}
namespace NLzma{ namespace NLzma86Ar { void registerArcLzma86();}}
namespace NSplit{void registerArcSplit();}
namespace NXz{void registerArcxz();}
namespace NZ{void registerArcZ();}
}
void registerArc7z();
void registerArcCab();
void registerArcTar();
void registerArcZip();

void  registerCodecBCJ2();
void  registerCodecBCJ();
void  registerCodecBCJ();
void  registerCodecByteSwap();
void  registerCodecBZip2();
void  registerCodecCopy();
void  registerCodecDeflate64();
void  registerCodecDeflate();
void  registerCodecDelta();
void  registerCodecLZMA2();
void  registerCodecLZMA();
void  registerCodecPPMD();
void  registerCodec7zAES();

using namespace NArchive;

static void initArchives()
{
    NBz2::registerArcBZip2();
    NGz::registerArcGZip();
    NLzma::NLzmaAr::registerArcLzma();
    NLzma::NLzma86Ar::registerArcLzma86();
    NArchive::NSplit::registerArcSplit();
    NArchive::NXz::registerArcxz();
    NArchive::NZ::registerArcZ();
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
    Q_INIT_RESOURCE( openssl );
    Q_INIT_RESOURCE( patch_file_lists );
#if !defined( QT_SHARED ) && !defined( QT_DLL )
    Q_IMPORT_PLUGIN( qsqlite ); //RegisterDocumentationOperation needs this
#endif
}

static void messageHandler( QtMsgType type, const char* msg )
{
    QInstaller::verbose() << msg << std::endl;
    if (type != QtFatalMsg && QString::fromLatin1(msg).contains(QLatin1String("Object::connect: "))) {
        //qFatal(msg);
    }
}

void QInstaller::init()
{
    ::initResources();
    KDUpdater::UpdateOperationFactory::instance().registerUpdateOperation< QInstaller::CreateShortcutOperation >( QLatin1String( "CreateShortcut" ) );
    KDUpdater::UpdateOperationFactory::instance().registerUpdateOperation< QInstaller::CreateDesktopEntryOperation >( QLatin1String( "CreateDesktopEntry" ) );
    KDUpdater::UpdateOperationFactory::instance().registerUpdateOperation< QInstaller::ExtractArchiveOperation >( QLatin1String( "Extract" ) );
    KDUpdater::UpdateOperationFactory::instance().registerUpdateOperation< QInstaller::GlobalSettingsOperation >( QLatin1String( "GlobalConfig" ) );
    KDUpdater::UpdateOperationFactory::instance().registerUpdateOperation< QInstaller::EnvironmentVariableOperation >( QLatin1String( "EnvironmentVariable" ) );
    KDUpdater::UpdateOperationFactory::instance().registerUpdateOperation< QInstaller::RegisterFileTypeOperation >( QLatin1String( "RegisterFileType" ) );
    KDUpdater::UpdateOperationFactory::instance().registerUpdateOperation< QInstaller::SelfRestartOperation >( QLatin1String( "SelfRestart" ) );
    KDUpdater::UpdateOperationFactory::instance().registerUpdateOperation< QInstaller::InstallIconsOperation >( QLatin1String( "InstallIcons" ) );
    KDUpdater::UpdateOperationFactory::instance().registerUpdateOperation< QInstaller::ElevatedExecuteOperation >( QLatin1String( "Execute" ) );
    KDUpdater::UpdateOperationFactory::instance().registerUpdateOperation< QInstaller::FakeStopProcessForUpdateOperation >( QLatin1String( "FakeStopProcessForUpdate" ) );

    //added for NDK
    KDUpdater::UpdateOperationFactory::instance().registerUpdateOperation< QInstaller::SimpleMoveFileOperation >( QLatin1String( "SimpleMoveFile") );
    KDUpdater::UpdateOperationFactory::instance().registerUpdateOperation< QInstaller::CopyDirectoryOperation >( QLatin1String( "CopyDirectory") );
    KDUpdater::UpdateOperationFactory::instance().registerUpdateOperation< QInstaller::RegisterDocumentationOperation >( QLatin1String( "RegisterDocumentation") );
    KDUpdater::UpdateOperationFactory::instance().registerUpdateOperation< QInstaller::RegisterQtInCreatorOperation>( QLatin1String( "RegisterQtInCreator") );
    KDUpdater::UpdateOperationFactory::instance().registerUpdateOperation< QInstaller::RegisterQtInCreatorV2Operation>( QLatin1String( "RegisterQtInCreatorV2") );
    KDUpdater::UpdateOperationFactory::instance().registerUpdateOperation< QInstaller::RegisterToolChainOperation>( QLatin1String( "RegisterToolChain") );
    KDUpdater::UpdateOperationFactory::instance().registerUpdateOperation< QInstaller::SetDemosPathOnQtOperation>( QLatin1String( "SetDemosPathOnQt") );
    KDUpdater::UpdateOperationFactory::instance().registerUpdateOperation< QInstaller::SetExamplesPathOnQtOperation>( QLatin1String( "SetExamplesPathOnQt") );
    KDUpdater::UpdateOperationFactory::instance().registerUpdateOperation< QInstaller::SetPluginPathOnQtCoreOperation>( QLatin1String( "SetPluginPathOnQtCore") );
    KDUpdater::UpdateOperationFactory::instance().registerUpdateOperation< QInstaller::SetImportsPathOnQtCoreOperation>( QLatin1String( "SetImportsPathOnQtCore") );
    KDUpdater::UpdateOperationFactory::instance().registerUpdateOperation< QInstaller::SetQtCreatorValueOperation>( QLatin1String( "SetQtCreatorValue") );
    KDUpdater::UpdateOperationFactory::instance().registerUpdateOperation< QInstaller::QtPatchOperation >( QLatin1String( "QtPatch" ) );
    KDUpdater::UpdateOperationFactory::instance().registerUpdateOperation< QInstaller::ReplaceOperation >( QLatin1String( "Replace" ) );
    KDUpdater::UpdateOperationFactory::instance().registerUpdateOperation< QInstaller::LineReplaceOperation >( QLatin1String( "LineReplace" ) );

    KDUpdater::UpdateOperationFactory::instance().registerUpdateOperation<QInstaller::MinimumProgressOperation>(QLatin1String("MinimumProgress"));
    KDUpdater::UpdateOperationFactory::instance().registerUpdateOperation<QInstaller::LicenseOperation>(QLatin1String("License"));

    KDUpdater::FileDownloaderFactory::setFollowRedirects( true );

#ifdef Q_OS_MAC
    KDUpdater::UpdateOperationFactory::instance().registerUpdateOperation< QInstaller::MacReplaceInstallNamesOperation >( QLatin1String( "ReplaceInstallNames" ) );
#endif // Q_OS_MAC

    // load 7z stuff, if we're a static lib
    ::initArchives();

   // proxy settings
   //QNetworkProxyFactory::setUseSystemConfiguration( true );

   // qDebug -> verbose()
   qInstallMsgHandler( messageHandler );
}
