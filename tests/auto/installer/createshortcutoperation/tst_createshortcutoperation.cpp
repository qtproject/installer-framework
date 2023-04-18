/**************************************************************************
**
** Copyright (C) 2020 The Qt Company Ltd.
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

#include <init.h>
#include <createshortcutoperation.h>
#include <utils.h>
#include <binarycontent.h>
#include <packagemanagercore.h>
#include <settings.h>
#include <fileutils.h>

#include <QDir>
#include <QObject>
#include <QTest>
#include <QFile>
#include <qsettingswrapper.h>
#include <QDebug>

using namespace KDUpdater;
using namespace QInstaller;

class tst_createshortcutoperation : public QObject
{
    Q_OBJECT

private:
    void installFromCLI(const QString &repository)
    {
        QInstaller::init();
        QScopedPointer<PackageManagerCore> core(new PackageManagerCore(BinaryContent::MagicInstallerMarker,
            QList<OperationBlob> (), QString(), QString(), Protocol::DefaultAuthorizationKey,
            Protocol::Mode::Production, QHash<QString, QString>(), true));

        core->disableWriteMaintenanceTool();
        core->setAutoConfirmCommand();
        QSet<Repository> repoList;
        Repository repo = Repository::fromUserInput(repository);
        repoList.insert(repo);
        core->settings().setDefaultRepositories(repoList);

        QString installDir = QInstaller::generateTemporaryFileName();
        QVERIFY(QDir().mkpath(installDir));
        core->setValue(scTargetDir, installDir);
        core->installDefaultComponentsSilently();

        QSettingsWrapper user(QLatin1String("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\"
            "CurrentVersion\\Explorer\\User Shell Folders"), QSettings::NativeFormat);
        //Replace %USERS% from "Programs"
        const QString programs = replaceWindowsEnvironmentVariables(user.value(QLatin1String("Programs"), QString()).toString());
        const QString startMenuDir = programs + QDir::separator() + "Qt Installer Framework Unit Test";
        QCOMPARE(startMenuDir, core->value("StartMenuDir"));

        QString linkLocation = core->value("StartMenuDir") + QDir::separator() + "A.lnk";
        QFile file(linkLocation);
        QVERIFY(file.open(QIODevice::ReadOnly));
        file.close();

        core->setPackageManager();
        core->commitSessionOperations();
        core->uninstallComponentsSilently(QStringList() << "A");
        QVERIFY(!QFile::exists(linkLocation));
    }

private slots:
    void testMissingArguments()
    {
        CreateShortcutOperation op;

        QVERIFY(op.testOperation());
        QVERIFY(!op.performOperation());

        QCOMPARE(UpdateOperation::Error(op.error()), UpdateOperation::InvalidArguments);
        QCOMPARE(op.errorString(), QString("Invalid arguments in CreateShortcut: 0 arguments given,"
                                           " 2 or 3 arguments expected in the form: <target> <link location> "
                                           "[target arguments] [\"workingDirectory=...\"] [\"iconPath=...\"] "
                                           "[\"iconId=...\"] [\"description=...\"]."));
    }

    void testCreateShortcutFromScript()
    {
        installFromCLI(":///data/repository");
    }

    void testCreateShortcutFromXML()
    {
        installFromCLI(":///data/xmloperationrepository");
    }
};

QTEST_MAIN(tst_createshortcutoperation)

#include "tst_createshortcutoperation.moc"
