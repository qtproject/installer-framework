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

#include "../shared/packagemanager.h"
#include "../shared/verifyinstaller.h"

#include <component.h>
#include <packagemanagercore.h>

#include <QLoggingCategory>
#include <QTest>

using namespace QInstaller;

class tst_ComponentReplace : public QObject
{
    Q_OBJECT

private:
    void setRepository(const QString &repository, PackageManagerCore *core)
    {
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
    }

    void replaceNonInstalledItem()
    {
        QScopedPointer<PackageManagerCore> core(PackageManager::getPackageManagerWithInit
                (m_installDir, ":///data/repositoryWithReplace"));
        QCOMPARE(PackageManagerCore::Success, core->installSelectedComponentsSilently(QStringList() << "A" << "B"));
        QList<Component*> installedComponents = core->orderedComponentsToInstall();

        VerifyInstaller::verifyInstallerResources(m_installDir, "B", "2.0.0content.txt");
        VerifyInstaller::verifyInstallerResources(m_installDir, "B.sub1", "1.0.0content.txt");
        VerifyInstaller::verifyFileExistence(m_installDir, QStringList() << "components.xml"
                << "Bsub1.txt" << "B.txt");
    }

    void replaceInstalledItem()
    {
        PackageManagerCore *core = PackageManager::getPackageManagerWithInit
                (m_installDir, ":///data/installPackagesRepository");
        QCOMPARE(PackageManagerCore::Success, core->installSelectedComponentsSilently(QStringList() << "A"));

        QCOMPARE(core->orderedComponentsToInstall().count(), 2);
        VerifyInstaller::verifyInstallerResources(m_installDir, "A", "1.0.0content.txt");
        VerifyInstaller::verifyInstallerResources(m_installDir, "A.sub1", "1.0.0content.txt");
        VerifyInstaller::verifyFileExistence(m_installDir, QStringList() << "components.xml"
                << "Asub1.txt" << "A.txt");

        core->commitSessionOperations();
        core->setPackageManager();
        setRepository(":///data/repositoryWithReplace", core);
        QCOMPARE(PackageManagerCore::Success, core->installSelectedComponentsSilently(QStringList() << "B"));
        QCOMPARE(core->componentsToUninstall().count(), 1);
        QVERIFY(core->componentByName("B.sub1") != 0);
        QVERIFY(core->componentByName("B") != 0);
        VerifyInstaller::verifyInstallerResourceFileDeletion(m_installDir, "A", "1.0.0content.txt");
        VerifyInstaller::verifyInstallerResources(m_installDir, "A.sub1", "1.0.0content.txt");
        VerifyInstaller::verifyInstallerResources(m_installDir, "B", "2.0.0content.txt");
        VerifyInstaller::verifyInstallerResources(m_installDir, "B.sub1", "1.0.0content.txt");
        VerifyInstaller::verifyFileExistence(m_installDir, QStringList() << "components.xml"
                << "Bsub1.txt" << "B.txt" << "Asub1.txt");
        delete core;
    }

    void replaceInstalledItemInUpdate()
    {
        PackageManagerCore *core = PackageManager::getPackageManagerWithInit
                (m_installDir, ":///data/installPackagesRepository");
        QCOMPARE(PackageManagerCore::Success, core->installSelectedComponentsSilently(QStringList() << "A" << "B"));

        QCOMPARE(core->orderedComponentsToInstall().count(), 4);
        VerifyInstaller::verifyInstallerResources(m_installDir, "A", "1.0.0content.txt");
        VerifyInstaller::verifyInstallerResources(m_installDir, "A.sub1", "1.0.0content.txt");
        VerifyInstaller::verifyInstallerResources(m_installDir, "B", "1.0.0content.txt");
        VerifyInstaller::verifyInstallerResources(m_installDir, "B.sub1", "1.0.0content.txt");
        VerifyInstaller::verifyFileExistence(m_installDir, QStringList() << "components.xml"
                << "Asub1.txt" << "A.txt"<< "Bsub1.txt" << "B.txt");

        core->commitSessionOperations();
        core->setUpdater();
        setRepository(":///data/repositoryWithReplace", core);
        QCOMPARE(PackageManagerCore::Success, core->updateComponentsSilently(QStringList() << "B"));
        QCOMPARE(core->componentsToUninstall().count(), 1);
        VerifyInstaller::verifyInstallerResourceFileDeletion(m_installDir, "A", "1.0.0content.txt");
        VerifyInstaller::verifyInstallerResources(m_installDir, "A.sub1", "1.0.0content.txt");
        VerifyInstaller::verifyInstallerResources(m_installDir, "B", "2.0.0content.txt");
        VerifyInstaller::verifyInstallerResources(m_installDir, "B.sub1", "1.0.0content.txt");
        VerifyInstaller::verifyFileExistence(m_installDir, QStringList() << "components.xml"
                << "Bsub1.txt" << "B.txt" << "Asub1.txt");
        delete core;
    }

    void replaceInstalledItemContainingUpdateInUpdate()
    {
        PackageManagerCore *core = PackageManager::getPackageManagerWithInit
                (m_installDir, ":///data/installPackagesRepository");
        QCOMPARE(PackageManagerCore::Success, core->installSelectedComponentsSilently(QStringList() << "A" << "B"));

        QCOMPARE(core->orderedComponentsToInstall().count(), 4);
        VerifyInstaller::verifyInstallerResources(m_installDir, "A", "1.0.0content.txt");
        VerifyInstaller::verifyInstallerResources(m_installDir, "A.sub1", "1.0.0content.txt");
        VerifyInstaller::verifyInstallerResources(m_installDir, "B", "1.0.0content.txt");
        VerifyInstaller::verifyInstallerResources(m_installDir, "B.sub1", "1.0.0content.txt");
        VerifyInstaller::verifyFileExistence(m_installDir, QStringList() << "components.xml"
                << "Asub1.txt" << "A.txt"<< "Bsub1.txt" << "B.txt");

        core->commitSessionOperations();
        core->setUpdater();
        setRepository(":///data/repositoryWithUpdateToReplaceble", core);
        QCOMPARE(PackageManagerCore::Success, core->updateComponentsSilently(QStringList() << "B"));
        QCOMPARE(core->componentsToUninstall().count(), 1);
        VerifyInstaller::verifyInstallerResourceFileDeletion(m_installDir, "A", "1.0.0content.txt");
        VerifyInstaller::verifyInstallerResourceFileDeletion(m_installDir, "A", "2.0.0content.txt");
        VerifyInstaller::verifyInstallerResources(m_installDir, "A.sub1", "1.0.0content.txt");
        VerifyInstaller::verifyInstallerResources(m_installDir, "B", "2.0.0content.txt");
        VerifyInstaller::verifyInstallerResources(m_installDir, "B.sub1", "1.0.0content.txt");
        VerifyInstaller::verifyFileExistence(m_installDir, QStringList() << "components.xml"
                << "Bsub1.txt" << "B.txt" << "Asub1.txt");
        delete core;
    }

    void replaceMultipleInstalledItems()
    {
        PackageManagerCore *core = PackageManager::getPackageManagerWithInit
                (m_installDir, ":///data/installPackagesRepository");
        QCOMPARE(PackageManagerCore::Success, core->installSelectedComponentsSilently(QStringList() << "A"));

        VerifyInstaller::verifyInstallerResources(m_installDir, "A", "1.0.0content.txt");
        VerifyInstaller::verifyInstallerResources(m_installDir, "A.sub1", "1.0.0content.txt");
        VerifyInstaller::verifyFileExistence(m_installDir, QStringList() << "components.xml"
                << "Asub1.txt" << "A.txt");

        core->commitSessionOperations();
        core->setPackageManager();
        setRepository(":///data/repositoryWithMultiReplace", core);
        QCOMPARE(PackageManagerCore::Success, core->installSelectedComponentsSilently(QStringList() << "B"));
        QCOMPARE(core->componentsToUninstall().count(), 2);
        VerifyInstaller::verifyInstallerResourceFileDeletion(m_installDir, "A", "1.0.0content.txt");
        VerifyInstaller::verifyInstallerResourceFileDeletion(m_installDir, "A.sub1", "1.0.0content.txt");
        VerifyInstaller::verifyInstallerResources(m_installDir, "B", "1.0.0content.txt");
        VerifyInstaller::verifyInstallerResources(m_installDir, "B.sub1", "1.0.0content.txt");
        VerifyInstaller::verifyFileExistence(m_installDir, QStringList() << "components.xml"
                << "Bsub1.txt" << "B.txt");
        delete core;
    }

    void replaceMultipleInstalledItemsInUpdate()
    {
        PackageManagerCore *core = PackageManager::getPackageManagerWithInit
                (m_installDir, ":///data/installPackagesRepository");
        QCOMPARE(PackageManagerCore::Success, core->installSelectedComponentsSilently(QStringList() << "A" << "B"));

        QCOMPARE(core->orderedComponentsToInstall().count(), 4);
        VerifyInstaller::verifyInstallerResources(m_installDir, "A", "1.0.0content.txt");
        VerifyInstaller::verifyInstallerResources(m_installDir, "A.sub1", "1.0.0content.txt");
        VerifyInstaller::verifyInstallerResources(m_installDir, "B", "1.0.0content.txt");
        VerifyInstaller::verifyInstallerResources(m_installDir, "B.sub1", "1.0.0content.txt");
        VerifyInstaller::verifyFileExistence(m_installDir, QStringList() << "components.xml"
                << "Asub1.txt" << "A.txt"<< "Bsub1.txt" << "B.txt");

        core->commitSessionOperations();
        core->setUpdater();
        setRepository(":///data/repositoryWithMultiReplaceInUpdate", core);
        QCOMPARE(PackageManagerCore::Success, core->updateComponentsSilently(QStringList() << "B"));
        QCOMPARE(core->componentsToUninstall().count(), 2);
        VerifyInstaller::verifyInstallerResourceFileDeletion(m_installDir, "A", "1.0.0content.txt");
        VerifyInstaller::verifyInstallerResourceFileDeletion(m_installDir, "A.sub1", "1.0.0content.txt");
        VerifyInstaller::verifyInstallerResources(m_installDir, "B", "2.0.0content.txt");
        VerifyInstaller::verifyInstallerResources(m_installDir, "B.sub1", "1.0.0content.txt");
        VerifyInstaller::verifyFileExistence(m_installDir, QStringList() << "components.xml"
                << "Bsub1.txt" << "B.txt");
        delete core;
    }

    void installOtherComponentsWhileReplacementsExists()
    {
        PackageManagerCore *core = PackageManager::getPackageManagerWithInit
                (m_installDir, ":///data/installPackagesRepository");
        QCOMPARE(PackageManagerCore::Success, core->installSelectedComponentsSilently(QStringList() << "A" << "B"));

        QCOMPARE(core->orderedComponentsToInstall().count(), 4);
        VerifyInstaller::verifyInstallerResources(m_installDir, "A", "1.0.0content.txt");
        VerifyInstaller::verifyInstallerResources(m_installDir, "A.sub1", "1.0.0content.txt");
        VerifyInstaller::verifyInstallerResources(m_installDir, "B", "1.0.0content.txt");
        VerifyInstaller::verifyInstallerResources(m_installDir, "B.sub1", "1.0.0content.txt");
        VerifyInstaller::verifyFileExistence(m_installDir, QStringList() << "components.xml"
                << "Asub1.txt" << "A.txt"<< "Bsub1.txt" << "B.txt");

        core->commitSessionOperations();
        core->setPackageManager();
        setRepository(":///data/repositoryWithMultiReplaceInUpdate", core);
        QCOMPARE(PackageManagerCore::Success, core->installSelectedComponentsSilently(QStringList() << "C"));
        QCOMPARE(core->orderedComponentsToInstall().count(), 1);
        QCOMPARE(core->componentsToUninstall().count(), 0);
        VerifyInstaller::verifyInstallerResources(m_installDir, "A", "1.0.0content.txt");
        VerifyInstaller::verifyInstallerResources(m_installDir, "A.sub1", "1.0.0content.txt");
        VerifyInstaller::verifyInstallerResources(m_installDir, "B", "1.0.0content.txt");
        VerifyInstaller::verifyInstallerResources(m_installDir, "B.sub1", "1.0.0content.txt");
        VerifyInstaller::verifyInstallerResources(m_installDir, "C", "2.0.0content.txt");
        VerifyInstaller::verifyFileExistence(m_installDir, QStringList() << "components.xml"
                << "Asub1.txt" << "A.txt"<< "Bsub1.txt" << "B.txt" << "C.txt");

        delete core;
    }

    void updateOtherComponentsWhileReplacementsExists()
    {
        PackageManagerCore *core = PackageManager::getPackageManagerWithInit
                (m_installDir, ":///data/installPackagesRepository");
        QCOMPARE(PackageManagerCore::Success, core->installSelectedComponentsSilently(QStringList() << "A" << "B" << "C"));

        QCOMPARE(core->orderedComponentsToInstall().count(), 5);
        VerifyInstaller::verifyInstallerResources(m_installDir, "A", "1.0.0content.txt");
        VerifyInstaller::verifyInstallerResources(m_installDir, "A.sub1", "1.0.0content.txt");
        VerifyInstaller::verifyInstallerResources(m_installDir, "B", "1.0.0content.txt");
        VerifyInstaller::verifyInstallerResources(m_installDir, "B.sub1", "1.0.0content.txt");
        VerifyInstaller::verifyFileExistence(m_installDir, QStringList() << "components.xml"
                << "Asub1.txt" << "A.txt"<< "Bsub1.txt" << "B.txt" << "C.txt");

        core->commitSessionOperations();
        core->setUpdater();
        setRepository(":///data/repositoryWithMultiReplaceInUpdate", core);
        QCOMPARE(PackageManagerCore::Success, core->updateComponentsSilently(QStringList() << "C"));
        QCOMPARE(core->orderedComponentsToInstall().count(), 1);
        QCOMPARE(core->componentsToUninstall().count(), 0);
        VerifyInstaller::verifyInstallerResources(m_installDir, "A", "1.0.0content.txt");
        VerifyInstaller::verifyInstallerResources(m_installDir, "A.sub1", "1.0.0content.txt");
        VerifyInstaller::verifyInstallerResources(m_installDir, "B", "1.0.0content.txt");
        VerifyInstaller::verifyInstallerResources(m_installDir, "B.sub1", "1.0.0content.txt");
        VerifyInstaller::verifyInstallerResources(m_installDir, "C", "2.0.0content.txt");
        VerifyInstaller::verifyFileExistence(m_installDir, QStringList() << "components.xml"
                << "Asub1.txt" << "A.txt"<< "Bsub1.txt" << "B.txt" << "C.txt");

        delete core;
    }

    void cleanup()
    {
        QDir dir(m_installDir);
        QVERIFY(dir.removeRecursively());
    }
private:
    QString m_installDir;
};


QTEST_MAIN(tst_ComponentReplace)

#include "tst_componentreplace.moc"
