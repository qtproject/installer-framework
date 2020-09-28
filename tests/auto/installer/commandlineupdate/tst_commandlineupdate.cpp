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

#include "../shared/packagemanager.h"
#include "../shared/verifyinstaller.h"

#include <component.h>
#include <packagemanagercore.h>

#include <QLoggingCategory>
#include <QTest>

using namespace QInstaller;

class tst_CommandLineUpdate : public QObject
{
    Q_OBJECT

private:
    void setRepository(const QString &repository)
    {
        core->reset();
        core->cancelMetaInfoJob(); //Call cancel to reset metadata so that update repositories are fetched

        QSet<Repository> repoList;
        Repository repo = Repository::fromUserInput(repository);
        repoList.insert(repo);
        core->settings().setDefaultRepositories(repoList);
    }

private slots:
    void initTestCase()
    {
        m_installDir = QInstaller::generateTemporaryFileName();
        core = PackageManager::getPackageManagerWithInit(m_installDir);
    }

    void testInstallWhenEssentialUpdate()
    {
        setRepository(":///data/installPackagesRepository");
        QCOMPARE(PackageManagerCore::Success, core->installSelectedComponentsSilently(QStringList()
                << "componentA"));
        QCOMPARE(PackageManagerCore::Success, core->status());
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentA", "1.0.0content.txt");
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentE", "1.0.0content.txt");
        VerifyInstaller::verifyFileExistence(m_installDir, QStringList() << "components.xml" << "installcontent.txt"
                           << "installcontentA.txt" << "installcontentE.txt" << "installcontentG.txt");
        core->commitSessionOperations();
        core->setPackageManager();
        setRepository(":///data/installPackagesRepositoryUpdate");
        QCOMPARE(PackageManagerCore::ForceUpdate, core->installSelectedComponentsSilently(QStringList()
                << "componentB"));
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentA", "1.0.0content.txt");
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentE", "1.0.0content.txt");
        VerifyInstaller::verifyFileExistence(m_installDir, QStringList() << "components.xml" << "installcontent.txt"
                           << "installcontentA.txt" << "installcontentE.txt" << "installcontentG.txt");
    }

    void testUpdateEssentialPackageSilently()
    {
        QCOMPARE(PackageManagerCore::EssentialUpdated, core->updateComponentsSilently(QStringList()));
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentA", "2.0.0content.txt");
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentE", "1.0.0content.txt");
        VerifyInstaller::verifyFileExistence(m_installDir, QStringList() << "components.xml"
                << "installcontentA_update.txt" << "installcontentE.txt" << "installcontentG.txt");
        VerifyInstaller::verifyInstallerResourceFileDeletion(m_installDir, "componentA", "1.0.0content.txt");
        //As we are using the same core in tests, clean the essentalupdate value
        core->setFoundEssentialUpdate(false);
    }

    void testUpdateEssentialWithAutodependOnSilently()
    {
        setRepository(":///data/repositoryWithDependencyToEssential");
        QCOMPARE(PackageManagerCore::EssentialUpdated, core->updateComponentsSilently(QStringList()));

        VerifyInstaller::verifyInstallerResources(m_installDir, "componentA", "3.0.0content.txt");
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentAutoDependOnA", "1.0content.txt");
        VerifyInstaller::verifyFileExistence(m_installDir, QStringList() << "components.xml"
                << "installcontentA_update.txt" << "installcontentE.txt" << "installcontentG.txt"
                << "installContentAutoDependOnA.txt");

        //As we are using the same core in tests, clean the essentalupdate value
        core->setFoundEssentialUpdate(false);
    }

    void testUpdatePackageSilently()
    {
        setRepository(":///data/installPackagesRepository");
        QCOMPARE(PackageManagerCore::Success, core->installSelectedComponentsSilently(QStringList()
                << "componentB"));
        QCOMPARE(PackageManagerCore::Success, core->status());

        VerifyInstaller::verifyInstallerResources(m_installDir, "componentA", "3.0.0content.txt");
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentB", "1.0.0content.txt");
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentD", "1.0.0content.txt");
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentAutoDependOnA", "1.0content.txt");
        VerifyInstaller::verifyFileExistence(m_installDir, QStringList() << "components.xml"
                           << "installcontentA_update.txt" << "installcontentE.txt" << "installcontentG.txt"
                           << "installcontentB.txt" << "installcontentD.txt"
                           << "installContentAutoDependOnA.txt");
        core->commitSessionOperations();

        setRepository(":///data/installPackagesRepositoryUpdate");
        QCOMPARE(PackageManagerCore::Success, core->updateComponentsSilently(QStringList()
                << "componentB"));
        // componentD is autodependent and cannot be deselected
        // componentE is a forced component and thus will be updated
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentA", "3.0.0content.txt");
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentB", "2.0.0content.txt");
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentD", "2.0.0content.txt");
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentE", "2.0.0content.txt");
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentAutoDependOnA", "1.0content.txt");

        VerifyInstaller::verifyInstallerResourceFileDeletion(m_installDir, "componentD", "1.0.0content.txt");
        VerifyInstaller::verifyInstallerResourceFileDeletion(m_installDir, "componentE", "1.0.0content.txt");
        VerifyInstaller::verifyFileExistence(m_installDir, QStringList() << "components.xml" << "installcontentA_update.txt"
                           << "installcontentE_update.txt" << "installcontentG.txt"
                           << "installcontentB_update.txt" << "installcontentD_update.txt"
                           << "installContentAutoDependOnA.txt");
    }

    void testUpdateNoUpdatesForSelectedPackage()
    {
        setRepository(":///data/installPackagesRepositoryUpdate");
        // Succeeds as no updates available for component so nothing to do
        QCOMPARE(PackageManagerCore::Success, core->updateComponentsSilently(QStringList()
                << "componentInvalid"));
    }

    void testUpdateTwoPackageSilently()
    {
        setRepository(":///data/installPackagesRepository");
        QCOMPARE(PackageManagerCore::Success, core->installSelectedComponentsSilently(QStringList()
                << "componentA" << "componentB" << "componentG"));
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentB", "2.0.0content.txt");
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentG", "1.0.0content.txt");
        core->commitSessionOperations();

        setRepository(":///data/installPackagesRepositoryUpdate");
        QCOMPARE(PackageManagerCore::Success, core->updateComponentsSilently(QStringList()
                << "componentB" << "componentG"));
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentB", "2.0.0content.txt");
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentG", "2.0.0content.txt");
        VerifyInstaller::verifyInstallerResourceFileDeletion(m_installDir, "componentB", "1.0.0content.txt");
        VerifyInstaller::verifyInstallerResourceFileDeletion(m_installDir, "componentG", "1.0.0content.txt");
    }

    void testUpdateAllPackagesSilently()
    {
        setRepository(":///data/installPackagesRepository");
        QCOMPARE(PackageManagerCore::Success, core->installSelectedComponentsSilently(QStringList()
                << "componentA" << "componentB" << "componentG" << "componentF"));
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentF", "1.0.0content.txt");
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentF.subcomponent1", "1.0.0content.txt");
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentF.subcomponent1.subsubcomponent1", "1.0.0content.txt");
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentF.subcomponent2", "1.0.0content.txt");
        core->commitSessionOperations();

        setRepository(":///data/installPackagesRepositoryUpdate");
        QCOMPARE(PackageManagerCore::Success, core->updateComponentsSilently(QStringList()));
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentF", "2.0.0content.txt");
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentF.subcomponent2", "2.0.0content.txt");
        VerifyInstaller::verifyInstallerResourceFileDeletion(m_installDir, "componentF", "1.0.0content.txt");
        VerifyInstaller::verifyInstallerResourceFileDeletion(m_installDir, "componentF.subcomponent2", "1.0.0content.txt");
    }

    void cleanupTestCase()
    {
        QDir dir(m_installDir);
        QVERIFY(dir.removeRecursively());
        delete core;
    }

private:
    QString m_installDir;
    PackageManagerCore *core;
};


QTEST_MAIN(tst_CommandLineUpdate)

#include "tst_commandlineupdate.moc"
