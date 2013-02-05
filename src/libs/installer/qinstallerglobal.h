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

#ifndef QINSTALLER_GLOBAL_H
#define QINSTALLER_GLOBAL_H

#include <installer_global.h>

#include <kdupdaterupdate.h>
#include <kdupdaterupdateoperation.h>
#include <kdupdaterpackagesinfo.h>

QT_BEGIN_NAMESPACE
class QScriptContext;
class QScriptEngine;
class QScriptValue;
QT_END_NAMESPACE


namespace QInstaller {

#define IFW_VERSION 0x010202
#define IFW_VERSION_STRING  "1.2.2"
#define IFW_VERSION_CHECK(major, minor, patch) \
    ((major << 16)|(minor << 8)|(patch))

static QRegExp scCommaRegExp(QLatin1String("\\b(,|, )\\b"));

enum INSTALLER_EXPORT RunMode
{
    AllMode,
    UpdaterMode
};

enum INSTALLER_EXPORT JobError
{
    InvalidUrl = 0x24B04,
    Timeout,
    DownloadError,
    InvalidUpdatesXml,
    InvalidMetaInfo,
    ExtractionError,
    UserIgnoreError,
    RepositoryUpdatesReceived
};

typedef KDUpdater::UpdateOperation Operation;
typedef QList<QInstaller::Operation*> OperationList;

typedef KDUpdater::Update Package;
typedef QList<QInstaller::Package*> PackagesList;

typedef KDUpdater::PackageInfo LocalPackage;
typedef QHash<QString, LocalPackage> LocalPackagesHash;

QString INSTALLER_EXPORT uncaughtExceptionString(QScriptEngine *scriptEngine, const QString &context = QString());
QScriptValue qInstallerComponentByName(QScriptContext *context, QScriptEngine *engine);

QScriptValue qDesktopServicesOpenUrl(QScriptContext *context, QScriptEngine *engine);
QScriptValue qDesktopServicesDisplayName(QScriptContext *context, QScriptEngine *engine);
QScriptValue qDesktopServicesStorageLocation(QScriptContext *context, QScriptEngine *engine);
QScriptValue qFileDialogGetExistingDirectory(QScriptContext *context, QScriptEngine *engine);

} // namespace QInstaller

#endif // QINSTALLER_GLOBAL_H
