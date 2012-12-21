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

#include "registerqtvqnxoperation.h"

#include "packagemanagercore.h"
#include "qtcreator_constants.h"
#include "persistentsettings.h"

#include <QString>
#include <QFileInfo>
#include <QDir>
#include <QSettings>
#include <QDebug>


using namespace QInstaller;

// TODO: move this to a general location it is used on some classes
static QString fromNativeSeparatorsAllOS(const QString &pathName)
{
    QString n = pathName;
    for (int i = 0; i < n.size(); ++i) {
        if (n.at(i) == QLatin1Char('\\'))
            n[i] = QLatin1Char('/');
    }
    return n;
}

static QString absoluteQmakePath(const QString &path)
{
    QString versionQmakePath = QDir(path).absolutePath();
    if (!versionQmakePath.endsWith(QLatin1String("qmake"))
         && !versionQmakePath.endsWith(QLatin1String("qmake.exe"))) {
#if defined (Q_OS_WIN)
        versionQmakePath.append(QLatin1String("/bin/qmake.exe"));
#elif defined(Q_OS_UNIX)
        versionQmakePath.append(QLatin1String("/bin/qmake"));
#endif
    }
    return fromNativeSeparatorsAllOS(versionQmakePath);
}

RegisterQtInCreatorQNXOperation::RegisterQtInCreatorQNXOperation()
{
    setName(QLatin1String("RegisterQtInCreatorQNX"));
}

void RegisterQtInCreatorQNXOperation::backup()
{
}

// Parameter List:
// Name - String displayed as name in Qt Creator
// qmake path - location of the qmake binary
// Type identifier - Desktop, Simulator, Symbian, ...
// SDK identifier - unique string to identify Qt version inside of the SDK (eg. desk473, simu11, ...)
// System Root Path
// sbs path
bool RegisterQtInCreatorQNXOperation::performOperation()
{
    const QStringList args = arguments();

    if (args.count() < 5) {
        setError(InvalidArguments);
        setErrorString(tr("Invalid arguments in %0: %1 arguments given, minimum 5 expected.")
                        .arg(name()).arg(args.count()));
        return false;
    }

    PackageManagerCore *const core = value(QLatin1String("installer")).value<PackageManagerCore*>();
    if (!core) {
        setError(UserDefinedError);
        setErrorString(tr("Needed installer object in \"%1\" operation is empty.").arg(name()));
        return false;
    }

    if (core->value(scQtCreatorInstallerQtVersionFile).isEmpty()) {
        setError(UserDefinedError);
        setErrorString(tr("There is no value set for %1 on the installer object.").arg(
            scQtCreatorInstallerQtVersionFile));
        return false;
    }
    const QString qtVersionsFileName = core->value(scQtCreatorInstallerQtVersionFile);
    int argCounter = 0;
    const QString &versionName = args.at(argCounter++);
    const QString &path = QDir::toNativeSeparators(args.value(argCounter++));
    const QString versionQmakePath = absoluteQmakePath(path);

    const QString &sdkPath = args.at(argCounter++);
    const QString &versionTypeIdentifier = args.at(argCounter++);
    const QString &versionSDKIdentifier = args.at(argCounter++);
    const QString &versionSystemRoot = fromNativeSeparatorsAllOS(args.value(argCounter++));
    const QString &versionSbsPath = fromNativeSeparatorsAllOS(args.value(argCounter++));

    ProjectExplorer::PersistentSettingsReader reader;
    int qtVersionCount = 0;
    QVariantMap map;
    if (reader.load(qtVersionsFileName)) {
        map = reader.restoreValues();
        qtVersionCount = map.value(QLatin1String("QtVersion.Count")).toInt();
        map.remove(QLatin1String("QtVersion.Count"));
        map.remove(QLatin1String("Version"));
    }

    ProjectExplorer::PersistentSettingsWriter writer;
    // Store old qt versions
    if (!map.isEmpty()) {
        for (int i = 0; i < qtVersionCount; ++i) {
            writer.saveValue(QString::fromLatin1("QtVersion.%1").arg(i)
                             , map[QLatin1String("QtVersion.") + QString::number(i)].toMap());
        }
        map.clear();
    }
    // Enter new version
    map.insert(QLatin1String("Id"), -1);
    map.insert(QLatin1String("Arch"), 1);
    map.insert(QLatin1String("Name"), versionName);
    map.insert(QLatin1String("QMakePath"), versionQmakePath);
    map.insert(QLatin1String("SDKPath"), sdkPath);
    map.insert(QLatin1String("QtVersion.Type"),
               QLatin1String("Qt4ProjectManager.QtVersion.") + versionTypeIdentifier);
    map.insert(QLatin1String("isAutodetected"), true);
    map.insert(QLatin1String("autodetectionSource"),
               QLatin1String("SDK.") + versionSDKIdentifier);
    if (!versionSystemRoot.isEmpty())
        map.insert(QLatin1String("SystemRoot"), versionSystemRoot);
    if (!versionSbsPath.isEmpty())
        map.insert(QLatin1String("SBSv2Directory"), versionSbsPath);

    writer.saveValue(QLatin1String("QtVersion.") + QString::number(qtVersionCount), map);

    writer.saveValue(QLatin1String("Version"), 1);
    writer.saveValue(QLatin1String("QtVersion.Count"), qtVersionCount + 1);
    QDir().mkpath(QFileInfo(qtVersionsFileName).absolutePath());
    writer.save(qtVersionsFileName, QLatin1String("QtCreatorQtVersions"));

    return true;
}

bool RegisterQtInCreatorQNXOperation::undoOperation()
{
    const QStringList args = arguments();

    if (args.count() < 4) {
        setError(InvalidArguments);
        setErrorString(tr("Invalid arguments in %0: %1 arguments given, minimum 4 expected.")
                        .arg(name()).arg(args.count()));
        return false;
    }

    PackageManagerCore *const core = value(QLatin1String("installer")).value<PackageManagerCore*>();
    if (!core) {
        setError(UserDefinedError);
        setErrorString(tr("Needed installer object in \"%1\" operation is empty.").arg(name()));
        return false;
    }

    // default value is the old value to keep the possibility that old saved operations can run undo
#ifdef Q_OS_MAC
    QString qtVersionsFileName = core->value(scQtCreatorInstallerQtVersionFile,
        QString::fromLatin1("%1/Qt Creator.app/Contents/Resources/Nokia/qtversion.xml").arg(
        core->value(QLatin1String("TargetDir"))));
#else
    QString qtVersionsFileName = core->value(scQtCreatorInstallerQtVersionFile,
        QString::fromLatin1("%1/QtCreator/share/qtcreator/Nokia/qtversion.xml").arg(core->value(
        QLatin1String("TargetDir"))));
#endif

    ProjectExplorer::PersistentSettingsReader reader;
    // If no file, then it has been removed already
    if (!reader.load(qtVersionsFileName))
        return true;

    const QVariantMap map = reader.restoreValues();

    ProjectExplorer::PersistentSettingsWriter writer;
    const int qtVersionCount = map.value(QLatin1String("QtVersion.Count")).toInt();

    int currentVersionIndex = 0;
    for (int i = 0; i < qtVersionCount; ++i) {
        QVariantMap versionMap = map[QLatin1String("QtVersion.") + QString::number(i)].toMap();

        const QString path = QDir::toNativeSeparators(args.value(1));
        const QString versionQmakePath = absoluteQmakePath(path);

        //use absoluteQmakePath function to normalize the path string, for example //
        const QString existingQtQMakePath = absoluteQmakePath(
            versionMap[QLatin1String("QMakePath")].toString());
        if (existingQtQMakePath == versionQmakePath)
            continue;
        writer.saveValue(QString::fromLatin1("QtVersion.%1").arg(currentVersionIndex++), versionMap);
    }

    writer.saveValue(QLatin1String("QtVersion.Count"), currentVersionIndex);
    writer.saveValue(QLatin1String("Version"), map[QLatin1String("Version")].toInt());

    writer.save(qtVersionsFileName, QLatin1String("QtCreatorQtVersions"));
    return true;
}

bool RegisterQtInCreatorQNXOperation::testOperation()
{
    return true;
}

Operation *RegisterQtInCreatorQNXOperation::clone() const
{
    return new RegisterQtInCreatorQNXOperation();
}
