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
#include "init.h"
#include "../shared/commonfunctions.h"

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

    PackageManagerCore &initPackagemanager(const QString &repository)
    {
        PackageManagerCore *core = new PackageManagerCore(BinaryContent::MagicInstallerMarker, QList<OperationBlob> ());
        QString appFilePath = QCoreApplication::applicationFilePath();
        core->setAllowedRunningProcesses(QStringList() << appFilePath);
        QSet<Repository> repoList;
        Repository repo = Repository::fromUserInput(repository);
        repoList.insert(repo);
        core->settings().setDefaultRepositories(repoList);

        m_installDir = QInstaller::generateTemporaryFileName();
        QDir().mkpath(m_installDir);
        core->setValue(scTargetDir, m_installDir);
        return *core;
    }

private slots:
    void testListAvailablePackages()
    {
        QString loggingRules = (QLatin1String("ifw.* = false\n"
                                "ifw.package.name = true\n"));

        QTest::ignoreMessage(QtDebugMsg, "Operations sanity check succeeded.");

        PackageManagerCore &core = initPackagemanager(":///data/repository");

        QLoggingCategory::setFilterRules(loggingRules);

        setIgnoreMessage();
        core.listAvailablePackages(QLatin1String("."));

        QTest::ignoreMessage(QtDebugMsg, "Id: A");
        QTest::ignoreMessage(QtDebugMsg, "Id: AB");
        core.listAvailablePackages(QLatin1String("A"));

        QTest::ignoreMessage(QtDebugMsg, "Id: A");
        QTest::ignoreMessage(QtDebugMsg, "Id: AB");
        core.listAvailablePackages(QLatin1String("A.*"));


        QTest::ignoreMessage(QtDebugMsg, "Id: B");
        core.listAvailablePackages(QLatin1String("^B"));

        QTest::ignoreMessage(QtDebugMsg, "Id: B");
        core.listAvailablePackages(QLatin1String("^B.*"));

        QTest::ignoreMessage(QtDebugMsg, "Id: C");
        core.listAvailablePackages(QLatin1String("^C"));
    }

    void testInstallPackageFails()
    {
        QString loggingRules = (QLatin1String("ifw.* = false\n"
                                "ifw.installer.installlog = true\n"));

        PackageManagerCore &core = initPackagemanager(":///data/uninstallableComponentsRepository");

        QLoggingCategory::setFilterRules(loggingRules);

        QTest::ignoreMessage(QtDebugMsg, "\"Preparing meta information download...\"");
        QTest::ignoreMessage(QtDebugMsg, "Cannot install component A. Component is installed only as automatic dependency to autoDep.");
        core.installSelectedComponentsSilently(QStringList() << QLatin1String("A"));

        QTest::ignoreMessage(QtDebugMsg, "\"Preparing meta information download...\"");
        QTest::ignoreMessage(QtDebugMsg, "Cannot install component AB. Component is not checkable meaning you have to select one of the subcomponents.");
        core.installSelectedComponentsSilently(QStringList() << QLatin1String("AB"));

        QTest::ignoreMessage(QtDebugMsg, "\"Preparing meta information download...\"");
        QTest::ignoreMessage(QtDebugMsg, "Cannot install B. Component is virtual.");
        core.installSelectedComponentsSilently(QStringList() << QLatin1String("B"));

        QTest::ignoreMessage(QtDebugMsg, "\"Preparing meta information download...\"");
        QTest::ignoreMessage(QtDebugMsg, "Cannot install MissingComponent. Component not found.");
        core.installSelectedComponentsSilently(QStringList() << QLatin1String("MissingComponent"));
    }

    void testUninstallPackageFails()
    {
        QString loggingRules = (QLatin1String("ifw.installer.installog = true\n"));
        PackageManagerCore core;
        core.setPackageManager();
        QString appFilePath = QCoreApplication::applicationFilePath();
        core.setAllowedRunningProcesses(QStringList() << appFilePath);
        QLoggingCategory::setFilterRules(loggingRules);

        m_installDir = QInstaller::generateTemporaryFileName();
        QVERIFY(QDir().mkpath(m_installDir));
        QVERIFY(QFile::copy(":/data/componentsFromInstallPackagesRepository.xml", m_installDir + "/components.xml"));

        core.setValue(scTargetDir, m_installDir);
        QTest::ignoreMessage(QtWarningMsg, "Cannot uninstall ForcedInstallation component componentE");
        core.uninstallComponentsSilently(QStringList() << "componentE");

        QTest::ignoreMessage(QtWarningMsg, "Cannot uninstall component componentD because it is added as auto dependency to componentA,componentB");
        core.uninstallComponentsSilently(QStringList() << "componentD");

        QTest::ignoreMessage(QtWarningMsg, "Cannot uninstall component MissingComponent. Component not found in install tree.");
        core.uninstallComponentsSilently(QStringList() << "MissingComponent");
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

    void testInstallPackageSilently()
    {
        QInstaller::init(); //This will eat debug output
        PackageManagerCore &core = initPackagemanager(":///data/installPackagesRepository");
        core.installSelectedComponentsSilently(QStringList() << QLatin1String("componentA"));
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentA", "1.0.0content.txt");
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentE", "1.0.0content.txt"); //ForcedInstall
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentG", "1.0.0content.txt"); //Depends on componentA
        VerifyInstaller::verifyFileExistence(m_installDir, QStringList() << "components.xml" << "installcontent.txt"
                            << "installcontentA.txt" << "installcontentE.txt" << "installcontentG.txt");
    }

    void testUninstallPackageSilently()
    {
        QInstaller::init(); //This will eat debug output
        PackageManagerCore &core = initPackagemanager(":///data/installPackagesRepository");
        core.installSelectedComponentsSilently(QStringList() << QLatin1String("componentA"));
        VerifyInstaller::verifyFileExistence(m_installDir, QStringList() << "components.xml" << "installcontentE.txt"
                            << "installcontentA.txt" << "installcontent.txt" << "installcontentG.txt");

        core.commitSessionOperations();
        core.setPackageManager();
        core.uninstallComponentsSilently(QStringList() << QLatin1String("componentA"));
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentE", "1.0.0content.txt"); //ForcedInstall
        VerifyInstaller::verifyInstallerResourcesDeletion(m_installDir, "componentA", "1.0.0content.txt");
        VerifyInstaller::verifyInstallerResourcesDeletion(m_installDir, "componentG", "1.0.0content.txt"); //Depends on componentA
        VerifyInstaller::verifyFileExistence(m_installDir, QStringList() << "components.xml" << "installcontentE.txt");
    }

    void testInstallWithDependencySilently()
    {
        QInstaller::init(); //This will eat debug output
        PackageManagerCore &core = initPackagemanager(":///data/installPackagesRepository");
        core.installSelectedComponentsSilently(QStringList() << QLatin1String("componentC"));
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentA", "1.0.0content.txt"); //Dependency for componentC
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentB", "1.0.0content.txt"); //Dependency for componentC
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentE", "1.0.0content.txt"); //ForcedInstall
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentG", "1.0.0content.txt"); //Depends on componentA
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentD", "1.0.0content.txt"); //Autodepend on componentA and componentB
        VerifyInstaller::verifyFileExistence(m_installDir, QStringList() << "components.xml" << "installcontentC.txt"
                            << "installcontent.txt" << "installcontentA.txt" << "installcontentB.txt"
                            << "installcontentD.txt"<< "installcontentE.txt" << "installcontentG.txt");
    }

    void testUninstallWithDependencySilently()
    {
        QInstaller::init(); //This will eat debug output
        PackageManagerCore &core = initPackagemanager(":///data/installPackagesRepository");
        core.installSelectedComponentsSilently(QStringList() << QLatin1String("componentC"));
        VerifyInstaller::verifyFileExistence(m_installDir, QStringList() << "components.xml" << "installcontentC.txt"
                            << "installcontent.txt" << "installcontentA.txt" << "installcontentB.txt"
                            << "installcontentD.txt"<< "installcontentE.txt" << "installcontentG.txt");

        core.commitSessionOperations();
        core.setPackageManager();
        core.uninstallComponentsSilently(QStringList() << QLatin1String("componentC"));
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentA", "1.0.0content.txt"); //Dependency for componentC
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentB", "1.0.0content.txt"); //Dependency for componentC
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentE", "1.0.0content.txt"); //ForcedInstall
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentG", "1.0.0content.txt"); //Depends on componentA
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentD", "1.0.0content.txt"); //Autodepend on componentA and componentB
        VerifyInstaller::verifyInstallerResourcesDeletion(m_installDir, "componentC", "1.0.0content.txt");
        VerifyInstaller::verifyFileExistence(m_installDir, QStringList() << "components.xml"
                            << "installcontent.txt" << "installcontentA.txt" << "installcontentB.txt"
                            << "installcontentD.txt"<< "installcontentE.txt" << "installcontentG.txt");
    }

    void testInstallSubcomponentSilently()
    {
        QInstaller::init(); //This will eat debug output
        PackageManagerCore &core = initPackagemanager(":///data/installPackagesRepository");
        core.installSelectedComponentsSilently(QStringList() << QLatin1String("componentF.subcomponent2.subsubcomponent2"));
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentF.subcomponent2.subsubcomponent2", "1.0.0content.txt");
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentF.subcomponent2", "1.0.0content.txt");
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentF", "1.0.0content.txt");
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentA", "1.0.0content.txt"); //Dependency for componentG
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentE", "1.0.0content.txt"); //ForcedInstall
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentG", "1.0.0content.txt"); //Default install
        VerifyInstaller::verifyFileExistence(m_installDir, QStringList() << "components.xml" << "installcontentF.txt"
                            << "installcontentF_2.txt"  << "installcontentF_2_2.txt"
                            << "installcontent.txt" << "installcontentA.txt"
                            << "installcontentE.txt" << "installcontentG.txt");
    }

    void testUninstallSubcomponentSilently()
    {
        QInstaller::init(); //This will eat debug output
        PackageManagerCore &core = initPackagemanager(":///data/installPackagesRepository");
        core.installSelectedComponentsSilently(QStringList() << QLatin1String("componentF.subcomponent2.subsubcomponent2"));
        VerifyInstaller::verifyFileExistence(m_installDir, QStringList() << "components.xml" << "installcontentF.txt"
                            << "installcontentF_2.txt"  << "installcontentF_2_2.txt"
                            << "installcontent.txt" << "installcontentA.txt"
                            << "installcontentE.txt" << "installcontentG.txt");
        core.commitSessionOperations();
        core.setPackageManager();
        core.uninstallComponentsSilently(QStringList() << QLatin1String("componentF.subcomponent2"));
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentA", "1.0.0content.txt"); //Dependency for componentG
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentE", "1.0.0content.txt"); //ForcedInstall
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentG", "1.0.0content.txt"); //Default install
        VerifyInstaller::verifyInstallerResourcesDeletion(m_installDir, "componentF.subcomponent2.subsubcomponent2", "1.0.0content.txt");
        VerifyInstaller::verifyInstallerResourcesDeletion(m_installDir, "componentF.subcomponent2", "1.0.0content.txt");
        VerifyInstaller::verifyInstallerResourcesDeletion(m_installDir, "componentF", "1.0.0content.txt");
        VerifyInstaller::verifyFileExistence(m_installDir, QStringList() << "components.xml"
                            << "installcontent.txt" << "installcontentA.txt"
                            << "installcontentE.txt" << "installcontentG.txt");
    }

    void testInstallDefaultPackagesSilently()
    {
        QInstaller::init(); //This will eat debug output
        PackageManagerCore &core = initPackagemanager(":///data/installPackagesRepository");
        core.installDefaultComponentsSilently();
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentA", "1.0.0content.txt"); //Dependency for componentG
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentE", "1.0.0content.txt"); //ForcedInstall
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentG", "1.0.0content.txt"); //Default
        VerifyInstaller::verifyFileExistence(m_installDir, QStringList() << "components.xml" << "installcontent.txt"
                            << "installcontentA.txt" << "installcontentE.txt" << "installcontentG.txt");
    }

    void testUnInstallDefaultPackagesSilently()
    {
        QInstaller::init(); //This will eat debug output
        PackageManagerCore &core = initPackagemanager(":///data/installPackagesRepository");
        core.installDefaultComponentsSilently();
        VerifyInstaller::verifyFileExistence(m_installDir, QStringList() << "components.xml" << "installcontent.txt"
                            << "installcontentA.txt" << "installcontentE.txt" << "installcontentG.txt");

        core.commitSessionOperations();
        core.setPackageManager();
        core.uninstallComponentsSilently(QStringList() << "componentG");
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentA", "1.0.0content.txt"); //Dependency for componentG
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentE", "1.0.0content.txt"); //ForcedInstall
        VerifyInstaller::verifyInstallerResourcesDeletion(m_installDir, "componentG", "1.0.0content.txt");
        VerifyInstaller::verifyFileExistence(m_installDir, QStringList() << "components.xml" << "installcontent.txt"
                            << "installcontentA.txt" << "installcontentE.txt");
    }

    void testUninstallForcedPackagesSilenly()
    {
        QInstaller::init(); //This will eat debug output
        PackageManagerCore &core = initPackagemanager(":///data/installPackagesRepository");
        core.installDefaultComponentsSilently();
        core.commitSessionOperations();
        core.setPackageManager();
        core.uninstallComponentsSilently(QStringList() << "componentE");
        //Nothing is uninstalled as componentE is forced install and cannot be uninstalled
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentA", "1.0.0content.txt"); //Dependency for componentG
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentE", "1.0.0content.txt"); //ForcedInstall
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentG", "1.0.0content.txt"); //Default
        VerifyInstaller::verifyFileExistence(m_installDir, QStringList() << "components.xml" << "installcontent.txt"
                            << "installcontentA.txt" << "installcontentE.txt" << "installcontentG.txt");
    }

    void testUninstallAutodependencyPackagesSilenly()
    {
        QInstaller::init(); //This will eat debug output
        PackageManagerCore &core = initPackagemanager(":///data/installPackagesRepository");
        core.installSelectedComponentsSilently(QStringList() << "componentA" << "componentB");
        core.commitSessionOperations();
        core.setPackageManager();
        core.uninstallComponentsSilently(QStringList() << "componentD");
        //Nothing is uninstalled as componentD is installed as autodependency to componentA and componentB
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentA", "1.0.0content.txt");
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentB", "1.0.0content.txt");
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentD", "1.0.0content.txt");
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentE", "1.0.0content.txt"); //ForcedInstall
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentG", "1.0.0content.txt"); //Default
        VerifyInstaller::verifyFileExistence(m_installDir, QStringList() << "components.xml" << "installcontent.txt"
                            << "installcontentA.txt" << "installcontentE.txt" << "installcontentG.txt"
                            << "installcontentB.txt" << "installcontentD.txt");
    }

    void cleanup()
    {
        QDir dir(m_installDir);
        QVERIFY(dir.removeRecursively());
    }

private:
    QString m_installDir;
};


QTEST_MAIN(tst_CLIInterface)

#include "tst_cliinterface.moc"
