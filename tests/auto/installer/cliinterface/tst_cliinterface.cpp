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

#include "metadatajob.h"
#include "settings.h"

#include <binarycontent.h>
#include <component.h>
#include <errors.h>
#include <fileutils.h>
#include <packagemanagercore.h>
#include <progresscoordinator.h>

#include <QLoggingCategory>
#include <QTest>

using namespace QInstaller;

class tst_CLIInterface : public QObject
{
    Q_OBJECT

private:
    void setIgnoreMessage()
    {
        QTest::ignoreMessage(QtDebugMsg, "Id: A");
        QTest::ignoreMessage(QtDebugMsg, "Id: B");
        QTest::ignoreMessage(QtDebugMsg, "Id: C");
        QTest::ignoreMessage(QtDebugMsg, "Id: AB");
    }

private slots:
    void testListAvailablePackages()
    {
        QString loggingRules = (QLatin1String("ifw.* = false\n"
                                "ifw.package.name = true\n"));
        Settings settings = Settings::fromFileAndPrefix(":///data/config.xml", ":///data");
        const QList<OperationBlob> ops;

        QTest::ignoreMessage(QtDebugMsg, "Operations sanity check succeeded.");
        PackageManagerCore *core = new PackageManagerCore(BinaryContent::MagicInstallerMarker, ops);
        QSet<Repository> repoList;
        Repository repo = Repository::fromUserInput(":///data/repository");
        repoList.insert(repo);

        core->settings().setDefaultRepositories(repoList);

        QLoggingCategory::setFilterRules(loggingRules);

        setIgnoreMessage();
        core->listAvailablePackages(QLatin1String("."));

        QTest::ignoreMessage(QtDebugMsg, "Id: A");
        QTest::ignoreMessage(QtDebugMsg, "Id: AB");
        core->listAvailablePackages(QLatin1String("A"));

        QTest::ignoreMessage(QtDebugMsg, "Id: A");
        QTest::ignoreMessage(QtDebugMsg, "Id: AB");
        core->listAvailablePackages(QLatin1String("A.*"));


        QTest::ignoreMessage(QtDebugMsg, "Id: B");
        core->listAvailablePackages(QLatin1String("^B"));

        QTest::ignoreMessage(QtDebugMsg, "Id: B");
        core->listAvailablePackages(QLatin1String("^B.*"));

        QTest::ignoreMessage(QtDebugMsg, "Id: C");
        core->listAvailablePackages(QLatin1String("^C"));
    }

    void testInstallPackages()
    {
        QString loggingRules = (QLatin1String("ifw.* = false\n"
                                "ifw.installer.installlog = true\n"));
        PackageManagerCore *core = new PackageManagerCore(BinaryContent::MagicInstallerMarker, QList<OperationBlob> ());
        QLoggingCategory::setFilterRules(loggingRules);
        QSet<Repository> repoList;
        Repository repo = Repository::fromUserInput(":///data/uninstallableComponentsRepository");
        repoList.insert(repo);

        core->settings().setDefaultRepositories(repoList);

        QTest::ignoreMessage(QtDebugMsg, "Cannot install component A. Component is installed only as automatic dependency to autoDep.");
        core->installSelectedComponentsSilently(QStringList() << QLatin1String("A"));

        QTest::ignoreMessage(QtDebugMsg, "Cannot install component AB. Component is not checkable meaning you have to select one of the subcomponents.");
        core->installSelectedComponentsSilently(QStringList() << QLatin1String("AB"));

        QTest::ignoreMessage(QtDebugMsg, "Cannot install B. Component is virtual.");
        core->installSelectedComponentsSilently(QStringList() << QLatin1String("B"));

        QTest::ignoreMessage(QtDebugMsg, "Cannot install MissingComponent. Component not found.");
        core->installSelectedComponentsSilently(QStringList() << QLatin1String("MissingComponent"));
    }

    void testListInstalledPackages()
    {
        QString loggingRules = (QLatin1String("ifw.* = false\n"
                                              "ifw.package.name = true\n"));
        PackageManagerCore core;
        core.setPackageManager();
        QLoggingCategory::setFilterRules(loggingRules);

        const QString testDirectory = QInstaller::generateTemporaryFileName();
        QVERIFY(QDir().mkpath(testDirectory));
        QVERIFY(QFile::copy(":/data/components.xml", testDirectory + "/components.xml"));

        core.setValue(scTargetDir, testDirectory);

        QTest::ignoreMessage(QtDebugMsg, "Id: A");
        QTest::ignoreMessage(QtDebugMsg, "Id: B");
        core.listInstalledPackages();
        QDir dir(testDirectory);

        QVERIFY(dir.removeRecursively());
    }
};


QTEST_MAIN(tst_CLIInterface)

#include "tst_cliinterface.moc"
