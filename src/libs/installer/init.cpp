/**************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
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
#include "settingsoperation.h"
#include "consumeoutputoperation.h"
#include "loggingutils.h"

#ifdef IFW_LIB7Z
#include "lib7z_facade.h"
#endif

#include "updateoperationfactory.h"
#include "filedownloaderfactory.h"

#include <QtPlugin>

using namespace KDUpdater;
using namespace QInstaller;

#if defined(QT_STATIC)
static void initResources()
{
    Q_INIT_RESOURCE(installer);
}
#endif

/*!
    Initializes the 7z library and installer resources. Registers
    custom operations and installs a custom message handler.
*/
void QInstaller::init()
{
#ifdef IFW_LIB7Z
    Lib7z::initSevenZ();
#endif
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
    factory.registerUpdateOperation<ConsumeOutputOperation>(QLatin1String("ConsumeOutput"));
    factory.registerUpdateOperation<SettingsOperation>(QLatin1String("Settings"));

    FileDownloaderFactory::setFollowRedirects(true);

    auto messageHandler = [](QtMsgType type, const QMessageLogContext &context, const QString &msg) {
        LoggingHandler::instance().messageHandler(type, context, msg);
    };
    qInstallMessageHandler(messageHandler);
}
