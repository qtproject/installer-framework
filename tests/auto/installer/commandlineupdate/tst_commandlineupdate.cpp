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

typedef QList<QPair<QString, QString> > ComponentResourceHash;
typedef QPair<QString, QString> ComponentResource;

class tst_CommandLineUpdate : public QObject
{
    Q_OBJECT

private:
    void setRepository(const QString &repository, PackageManagerCore *core)
    {
        core->reset();
        core->cancelMetaInfoJob(); //Call cancel to reset metadata so that update repositories are fetched

        QSet<Repository> repoList;
        Repository repo = Repository::fromUserInput(repository);
        repoList.insert(repo);
        core->settings().setDefaultRepositories(repoList);
    }

private slots:

    void testUpdate_data()
    {
        QTest::addColumn<QString>("repository");
        QTest::addColumn<QStringList>("installComponents");
        QTest::addColumn<PackageManagerCore::Status>("status");
        QTest::addColumn<ComponentResourceHash>("componentResources");
        QTest::addColumn<QStringList >("installedFiles");
        QTest::addColumn<QString>("updateRepository");
        QTest::addColumn<QStringList>("updateComponents");
        QTest::addColumn<PackageManagerCore::Status>("updateStatus");
        QTest::addColumn<ComponentResourceHash>("componentResourcesAfterUpdate");
        QTest::addColumn<QStringList >("installedFilesAfterUpdate");
        QTest::addColumn<ComponentResourceHash >("deletedComponentResources");

        /*********** Update essential packages **********/
        ComponentResourceHash componentResources;
        componentResources.append(ComponentResource("componentA", "1.0.0content.txt"));
        componentResources.append(ComponentResource("componentE", "1.0.0content.txt"));

        ComponentResourceHash componentResourcesAfterUpdate;
        componentResourcesAfterUpdate.append(ComponentResource("componentA", "2.0.0content.txt"));
        componentResourcesAfterUpdate.append(ComponentResource("componentE", "1.0.0content.txt"));

        ComponentResourceHash deletedComponentResources;
        deletedComponentResources.append(ComponentResource("componentA", "1.0.0content.txt"));

        QTest::newRow("Update essential packages")
                << ":///data/installPackagesRepository"
                << (QStringList() << "componentA")
                << PackageManagerCore::Success
                << componentResources
                << (QStringList() <<  "components.xml" << "installcontent.txt"
                        << "installcontentA.txt" << "installcontentE.txt" << "installcontentG.txt")
                << ":///data/installPackagesRepositoryUpdateWithEssential"
                << (QStringList() << "componentB")
                << PackageManagerCore::EssentialUpdated
                << componentResourcesAfterUpdate
                << (QStringList() << "components.xml"
                    << "installcontentA_update.txt" << "installcontentE.txt" << "installcontentG.txt")
                << deletedComponentResources;

        /*********** Update essential with autodependon**********/
        componentResourcesAfterUpdate.clear();
        componentResourcesAfterUpdate.append(ComponentResource("componentA", "3.0.0content.txt"));
        componentResourcesAfterUpdate.append(ComponentResource("componentAutoDependOnA", "1.0content.txt"));

        QTest::newRow("Update essential with autodependon")
                << ":///data/installPackagesRepository"
                << (QStringList() << "componentA")
                << PackageManagerCore::Success
                << componentResources
                << (QStringList() <<  "components.xml" << "installcontent.txt"
                        << "installcontentA.txt" << "installcontentE.txt" << "installcontentG.txt")
                << ":///data/repositoryWithDependencyToEssential"
                << (QStringList())
                << PackageManagerCore::EssentialUpdated
                << componentResourcesAfterUpdate
                << (QStringList() << "components.xml"
                    << "installcontentA_update.txt" << "installcontentE.txt" << "installcontentG.txt"
                    << "installContentAutoDependOnA.txt")
                << deletedComponentResources;


        /*********** Update force update packages **********/
        componentResources.clear();
        componentResources.append(ComponentResource("componentH", "1.0.0content.txt"));
        componentResources.append(ComponentResource("componentE", "1.0.0content.txt"));

        componentResourcesAfterUpdate.clear();
        componentResourcesAfterUpdate.append(ComponentResource("componentH", "2.0.0content.txt"));
        componentResourcesAfterUpdate.append(ComponentResource("componentE", "1.0.0content.txt"));

        deletedComponentResources.clear();
        deletedComponentResources.append(ComponentResource("componentH", "1.0.0content.txt"));

        QTest::newRow("Update force update packages")
                << ":///data/installPackagesRepository"
                << (QStringList() << "componentH")
                << PackageManagerCore::Success
                << componentResources
                << (QStringList() <<  "components.xml" << "installcontent.txt"
                        << "installcontentA.txt" << "installcontentE.txt" << "installcontentG.txt"
                        << "installcontentH.txt")
                << ":///data/installPackagesRepositoryUpdateWithEssential"
                << (QStringList())
                << PackageManagerCore::EssentialUpdated
                << componentResourcesAfterUpdate
                << (QStringList() <<  "components.xml" << "installcontentA_update.txt"
                        << "installcontentE.txt" << "installcontentG.txt"
                        << "installcontentH_update.txt")
                << deletedComponentResources;

        /*********** Update packages **********/
        componentResources.clear();
        componentResources.append(ComponentResource("componentA", "1.0.0content.txt"));
        componentResources.append(ComponentResource("componentB", "1.0.0content.txt"));
        componentResources.append(ComponentResource("componentD", "1.0.0content.txt"));

        componentResourcesAfterUpdate.clear();
        componentResourcesAfterUpdate.append(ComponentResource("componentA", "1.0.0content.txt"));
        componentResourcesAfterUpdate.append(ComponentResource("componentB", "2.0.0content.txt"));
        componentResourcesAfterUpdate.append(ComponentResource("componentD", "2.0.0content.txt"));
        componentResourcesAfterUpdate.append(ComponentResource("componentE", "2.0.0content.txt"));

        deletedComponentResources.clear();
        deletedComponentResources.append(ComponentResource("componentB", "1.0.0content.txt"));
        deletedComponentResources.append(ComponentResource("componentD", "1.0.0content.txt"));
        deletedComponentResources.append(ComponentResource("componentE", "1.0.0content.txt"));

        QTest::newRow("Update packages")
                << ":///data/installPackagesRepository"
                << (QStringList() << "componentC" << "componentH")
                << PackageManagerCore::Success
                << componentResources
                << (QStringList() <<  "components.xml" << "installcontent.txt"
                        << "installcontentA.txt" << "installcontentB.txt" << "installcontentC.txt" << "installcontentD.txt"
                        << "installcontentE.txt" << "installcontentG.txt" << "installcontentH.txt")
                << ":///data/installPackagesRepositoryUpdate"
                << (QStringList())
                << PackageManagerCore::Success
                << componentResourcesAfterUpdate
                << (QStringList() <<  "components.xml" << "installcontent.txt"
                    << "installcontentA.txt" << "installcontentB_update.txt" << "installcontentC_update.txt" << "installcontentD_update.txt"
                    << "installcontentE_update.txt" << "installcontentG_update.txt" << "installcontentH.txt")
                << deletedComponentResources;

        /*********** Update two packages **********/
        componentResources.clear();
        componentResources.append(ComponentResource("componentA", "1.0.0content.txt"));
        componentResources.append(ComponentResource("componentB", "1.0.0content.txt"));
        componentResources.append(ComponentResource("componentD", "1.0.0content.txt"));
        componentResources.append(ComponentResource("componentE", "1.0.0content.txt"));
        componentResources.append(ComponentResource("componentG", "1.0.0content.txt"));

        componentResourcesAfterUpdate.clear();
        componentResourcesAfterUpdate.append(ComponentResource("componentA", "1.0.0content.txt"));
        componentResourcesAfterUpdate.append(ComponentResource("componentB", "1.0.0content.txt"));
        componentResourcesAfterUpdate.append(ComponentResource("componentD", "1.0.0content.txt"));//AutodepenOn componentA,componentB
        componentResourcesAfterUpdate.append(ComponentResource("componentE", "2.0.0content.txt"));
        componentResourcesAfterUpdate.append(ComponentResource("componentG", "2.0.0content.txt"));

        deletedComponentResources.clear();
        deletedComponentResources.append(ComponentResource("componentE", "1.0.0content.txt"));
        deletedComponentResources.append(ComponentResource("componentG", "1.0.0content.txt"));

        QTest::newRow("Update two packages")
                << ":///data/installPackagesRepository"
                << (QStringList()<< "componentA"  << "componentB" << "componentE" << "componentG")
                << PackageManagerCore::Success
                << componentResources
                << (QStringList() <<  "components.xml" << "installcontent.txt" << "installcontentA.txt"
                        << "installcontentD.txt" << "installcontentB.txt" << "installcontentE.txt"
                        << "installcontentG.txt")
                << ":///data/installPackagesRepositoryUpdate"
                << (QStringList() << "componentE" << "componentG")
                << PackageManagerCore::Success
                << componentResourcesAfterUpdate
                << (QStringList() <<  "components.xml" << "installcontent.txt" << "installcontentA.txt"
                        << "installcontentD.txt" << "installcontentB.txt"
                        << "installcontentE_update.txt" << "installcontentG_update.txt")
                << deletedComponentResources;

        /*********** Update all packages **********/
        componentResources.clear();
        componentResources.append(ComponentResource("componentA", "1.0.0content.txt"));
        componentResources.append(ComponentResource("componentB", "1.0.0content.txt"));
        componentResources.append(ComponentResource("componentC", "1.0.0content.txt"));
        componentResources.append(ComponentResource("componentD", "1.0.0content.txt"));
        componentResources.append(ComponentResource("componentE", "1.0.0content.txt"));
        componentResources.append(ComponentResource("componentF", "1.0.0content.txt"));
        componentResources.append(ComponentResource("componentF.subcomponent1", "1.0.0content.txt"));
        componentResources.append(ComponentResource("componentF.subcomponent1.subsubcomponent1", "1.0.0content.txt"));
        componentResources.append(ComponentResource("componentF.subcomponent1.subsubcomponent2", "1.0.0content.txt"));
        componentResources.append(ComponentResource("componentF.subcomponent2", "1.0.0content.txt"));
        componentResources.append(ComponentResource("componentF.subcomponent2.subsubcomponent1", "1.0.0content.txt"));
        componentResources.append(ComponentResource("componentF.subcomponent2.subsubcomponent2", "1.0.0content.txt"));
        componentResources.append(ComponentResource("componentG", "1.0.0content.txt"));
        componentResources.append(ComponentResource("componentH", "1.0.0content.txt"));

        componentResourcesAfterUpdate.clear();
        componentResourcesAfterUpdate.append(ComponentResource("componentA", "1.0.0content.txt"));
        componentResourcesAfterUpdate.append(ComponentResource("componentB", "2.0.0content.txt"));
        componentResourcesAfterUpdate.append(ComponentResource("componentC", "2.0.0content.txt"));
        componentResourcesAfterUpdate.append(ComponentResource("componentD", "2.0.0content.txt"));
        componentResourcesAfterUpdate.append(ComponentResource("componentE", "2.0.0content.txt"));
        componentResourcesAfterUpdate.append(ComponentResource("componentF", "2.0.0content.txt"));
        componentResourcesAfterUpdate.append(ComponentResource("componentF.subcomponent1", "1.0.0content.txt"));
        componentResourcesAfterUpdate.append(ComponentResource("componentF.subcomponent1.subsubcomponent1", "1.0.0content.txt"));
        componentResourcesAfterUpdate.append(ComponentResource("componentF.subcomponent1.subsubcomponent2", "1.0.0content.txt"));
        componentResourcesAfterUpdate.append(ComponentResource("componentF.subcomponent2", "2.0.0content.txt"));
        componentResourcesAfterUpdate.append(ComponentResource("componentF.subcomponent2.subsubcomponent1", "1.0.0content.txt"));
        componentResourcesAfterUpdate.append(ComponentResource("componentF.subcomponent2.subsubcomponent2", "1.0.0content.txt"));
        componentResourcesAfterUpdate.append(ComponentResource("componentG", "2.0.0content.txt"));
        componentResourcesAfterUpdate.append(ComponentResource("componentH", "1.0.0content.txt"));

        deletedComponentResources.clear();
        deletedComponentResources.append(ComponentResource("componentB", "1.0.0content.txt"));
        deletedComponentResources.append(ComponentResource("componentD", "1.0.0content.txt"));
        deletedComponentResources.append(ComponentResource("componentE", "1.0.0content.txt"));
        deletedComponentResources.append(ComponentResource("componentF", "1.0.0content.txt"));
        deletedComponentResources.append(ComponentResource("componentF.subcomponent2", "1.0.0content.txt"));
        deletedComponentResources.append(ComponentResource("componentG", "1.0.0content.txt"));

        QTest::newRow("Update all packages")
                << ":///data/installPackagesRepository"
                << (QStringList() << "componentA"  << "componentB" << "componentC" << "componentD" << "componentE"
                        << "componentF"  << "componentG" << "componentH")
                << PackageManagerCore::Success
                << componentResources
                << (QStringList() <<  "components.xml" << "installcontent.txt" << "installcontentA.txt"
                        << "installcontentB.txt" << "installcontentC.txt" << "installcontentD.txt"
                        << "installcontentE.txt" << "installcontentF.txt" << "installcontentF_1.txt" << "installcontentF_1_1.txt"
                        << "installcontentF_1_2.txt"  << "installcontentF_2.txt" << "installcontentF_2_1.txt" << "installcontentF_2_2.txt"
                        << "installcontentG.txt" << "installcontentH.txt")
                << ":///data/installPackagesRepositoryUpdate"
                << (QStringList())
                << PackageManagerCore::Success
                << componentResourcesAfterUpdate
                << (QStringList() <<  "components.xml" << "installcontent.txt" << "installcontentA.txt"
                        << "installcontentB_update.txt" << "installcontentC_update.txt" << "installcontentD_update.txt"
                        << "installcontentE_update.txt" << "installcontentF_update.txt" << "installcontentF_1.txt" << "installcontentF_1_1.txt"
                        << "installcontentF_1_2.txt"  << "installcontentF_2_update.txt" << "installcontentF_2_1.txt" << "installcontentF_2_2.txt"
                        << "installcontentG_update.txt" << "installcontentH.txt")
                << deletedComponentResources;

        /*********** Update packages with AutoDependOn **********/
        componentResources.clear();
        componentResources.append(ComponentResource("componentA", "1.0.0content.txt"));
        componentResources.append(ComponentResource("componentB", "1.0.0content.txt"));
        componentResources.append(ComponentResource("componentD", "1.0.0content.txt"));
        componentResources.append(ComponentResource("componentE", "1.0.0content.txt"));
        componentResources.append(ComponentResource("componentG", "1.0.0content.txt"));

        componentResourcesAfterUpdate.clear();
        componentResourcesAfterUpdate.append(ComponentResource("componentA", "1.0.0content.txt"));
        componentResourcesAfterUpdate.append(ComponentResource("componentB", "2.0.0content.txt"));
        componentResourcesAfterUpdate.append(ComponentResource("componentD", "2.0.0content.txt"));//AutodepenOn componentA,componentB
        componentResourcesAfterUpdate.append(ComponentResource("componentE", "1.0.0content.txt"));//ForcedInstall, not updated without user selection
        componentResourcesAfterUpdate.append(ComponentResource("componentG", "1.0.0content.txt"));

        deletedComponentResources.clear();
        deletedComponentResources.append(ComponentResource("componentB", "1.0.0content.txt"));
        deletedComponentResources.append(ComponentResource("componentD", "1.0.0content.txt"));

        QTest::newRow("Update packages with AutoDependOn")
                << ":///data/installPackagesRepository"
                << (QStringList()<< "componentA"  << "componentB" << "componentE" << "componentG")
                << PackageManagerCore::Success
                << componentResources
                << (QStringList() <<  "components.xml" << "installcontent.txt" << "installcontentA.txt"
                        << "installcontentD.txt" << "installcontentB.txt" << "installcontentE.txt"
                        << "installcontentG.txt")
                << ":///data/installPackagesRepositoryUpdate"
                << (QStringList() << "componentB")
                << PackageManagerCore::Success
                << componentResourcesAfterUpdate
                << (QStringList() <<  "components.xml" << "installcontent.txt" << "installcontentA.txt"
                        << "installcontentD_update.txt" << "installcontentB_update.txt"
                        << "installcontentE.txt" << "installcontentG.txt")
                << deletedComponentResources;

        /*********** Update packages with replacements **********/
        componentResources.clear();
        componentResources.append(ComponentResource("qt.tools.qtcreator", "1.0.0content.txt"));
        componentResources.append(ComponentResource("qt.tools.qtcreator.enterprise.plugins", "1.0.0content.txt"));
        componentResources.append(ComponentResource("componentE", "1.0.0content.txt"));

        componentResourcesAfterUpdate.clear();
        componentResourcesAfterUpdate.append(ComponentResource("qt.tools.qtcreator", "2.0.0content.txt"));
        componentResourcesAfterUpdate.append(ComponentResource("qt.tools.qtcreator_gui", "2.0.0content.txt"));
        componentResourcesAfterUpdate.append(ComponentResource("qt.tools.qtcreator_gui.enterprise.plugins", "2.0.0content.txt"));
        componentResourcesAfterUpdate.append(ComponentResource("componentE", "1.0.0content.txt"));

        deletedComponentResources.clear();
        deletedComponentResources.append(ComponentResource("qt.tools.qtcreator.enterprise.plugins", "1.0.0content.txt"));

        QTest::newRow("Update packages with replacements")
            << ":///data/installPackagesRepository"
            << (QStringList()<< "qt.tools.qtcreator")
            << PackageManagerCore::Success
            << componentResources
            << (QStringList() << "components.xml" << "installcontentA.txt" << "installcontentE.txt" << "installcontentG.txt"
                              << "installcontent.txt" << "qtcreator.txt" << "plugins.txt")
            << ":///data/repositoryUpdateWithReplacements"
            << (QStringList() << "qt.tools.qtcreator")
            << PackageManagerCore::Success
            << componentResourcesAfterUpdate
            << (QStringList() << "components.xml" << "installcontentA.txt" << "installcontentE.txt" << "installcontentG.txt"
                              << "installcontent.txt" << "gui.txt" << "qtcreator2.txt" << "gui_plugins.txt")
            << deletedComponentResources;
    }

    void testUpdate()
    {
        QFETCH(QString, repository);
        QFETCH(QStringList, installComponents);
        QFETCH(PackageManagerCore::Status, status);
        QFETCH(ComponentResourceHash, componentResources);
        QFETCH(QStringList, installedFiles);
        QFETCH(QString, updateRepository);
        QFETCH(QStringList, updateComponents);
        QFETCH(PackageManagerCore::Status, updateStatus);
        QFETCH(ComponentResourceHash, componentResourcesAfterUpdate);
        QFETCH(QStringList, installedFilesAfterUpdate);
        QFETCH(ComponentResourceHash, deletedComponentResources);

        PackageManagerCore *core = PackageManager::getPackageManagerWithInit(m_installDir, repository);

        QCOMPARE(status, core->installSelectedComponentsSilently(QStringList() << installComponents));
        for (const ComponentResource &resource : componentResources) {
            VerifyInstaller::verifyInstallerResources(m_installDir, resource.first, resource.second);
        }
        VerifyInstaller::verifyFileExistence(m_installDir, installedFiles);

        core->commitSessionOperations();
        core->setPackageManager();
        setRepository(updateRepository, core);
        QCOMPARE(updateStatus, core->updateComponentsSilently(updateComponents));

        for (const ComponentResource &resource : componentResourcesAfterUpdate) {
            VerifyInstaller::verifyInstallerResources(m_installDir, resource.first, resource.second);
        }
        for (const ComponentResource &resource : deletedComponentResources) {
            VerifyInstaller::verifyInstallerResourceFileDeletion(m_installDir, resource.first, resource.second);
        }
        VerifyInstaller::verifyFileExistence(m_installDir, installedFilesAfterUpdate);
        delete core;
    }

    void testUpdateNoUpdatesForSelectedPackage()
    {
        QScopedPointer<PackageManagerCore> core(PackageManager::getPackageManagerWithInit
                (m_installDir, ":///data/installPackagesRepositoryUpdate"));
        // No updates available for component so nothing to do
        QCOMPARE(PackageManagerCore::Canceled, core->updateComponentsSilently(QStringList()
                << "componentInvalid"));
    }

    void init()
    {
        m_installDir = QInstaller::generateTemporaryFileName();
        QVERIFY(QDir().mkpath(m_installDir));
    }

    void cleanup()
    {
        QDir dir(m_installDir);
        QVERIFY(dir.removeRecursively());
    }

private:
    QString m_installDir;
};


QTEST_MAIN(tst_CommandLineUpdate)

#include "tst_commandlineupdate.moc"
