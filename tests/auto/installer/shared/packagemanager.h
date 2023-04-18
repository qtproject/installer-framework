/**************************************************************************
**
** Copyright (C) 2022 The Qt Company Ltd.
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

#ifndef PACKAGEMANAGER_H
#define PACKAGEMANAGER_H

#include <packagemanagercore.h>
#include <binaryformatenginehandler.h>
#include <binarycontent.h>
#include <fileutils.h>
#include <settings.h>
#include <init.h>
#include <errors.h>

#include <QTest>

using namespace QInstaller;

void silentTestMessageHandler(QtMsgType, const QMessageLogContext &, const QString &) {}
void exitOnWarningMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    const QByteArray localMsg = msg.toLocal8Bit();
    const char *file = context.file ? context.file : "";
    const char *function = context.function ? context.function : "";
    if (!(type == QtDebugMsg) && !(type == QtInfoMsg)) {
        fprintf(stderr, "Caught message: %s (%s:%u, %s)\n", localMsg.constData(), file, context.line, function);
        exit(1);
    }
}

struct PackageManager
{
    static PackageManagerCore *getPackageManager(const QString &targetDir, const QString &repository = QString())
    {
        BinaryFormatEngineHandler::instance()->clear();

        PackageManagerCore *core = new PackageManagerCore(BinaryContent::MagicInstallerMarker, QList<OperationBlob> ());
        QString appFilePath = QCoreApplication::applicationFilePath();
        core->disableWriteMaintenanceTool();
        core->setAutoConfirmCommand();
        QSet<Repository> repoList;
        Repository repo = Repository::fromUserInput(repository);
        repoList.insert(repo);
        core->settings().setDefaultRepositories(repoList);

        core->setValue(scTargetDir, targetDir);
        return core;
    }

    static PackageManagerCore *getPackageManagerWithInit(const QString &targetDir, const QString &repository = QString())
    {
        QInstaller::init();
        qInstallMessageHandler(silentTestMessageHandler);
        return getPackageManager(targetDir, repository);
    }
};
#endif
