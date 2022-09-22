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

#include <packagemanagercore.h>
#include <component.h>
#include <componentmodel.h>

#include <QTest>
#include <QRegularExpression>

class tst_TreeName : public QObject
{
    Q_OBJECT

private slots:
    void moveToRoot();
    void moveToSubItem();
    void dependencyToMovedItem();
    void autodependOnMovedItem();

    void moveToExistingAllowUnstableComponents();
    void moveToExistingItemNoUnstableComponents();
    void moveWithChildrenChildConflictsAllowUnstable();
    void moveWithChildrenChildConflictsNoUnstable();

    void moveToRootWithChildren();
    void moveToSubItemWithChildren();
    void moveToAvailableParentItemWithChilren();
    void dependencyToMovedSubItem();
    void autoDependOnMovedSubItem();

    void replaceComponentWithTreeName();
    void replaceComponentWithTreeNameMoveChildren();

    void remotePackageConflictsLocal();

    void init();
    void cleanup();

private:
    QString m_installDir;

};

void tst_TreeName::moveToRoot()
{
    // componentB.sub1.sub1 moved from sub item to root (BSub1Sub1ToRoot)
    QScopedPointer<PackageManagerCore> core(PackageManager::getPackageManagerWithInit
            (m_installDir, ":///data/repository"));
    QCOMPARE(PackageManagerCore::Success, core->installSelectedComponentsSilently(QStringList() << "componentB.sub1.sub1"));
    QList<Component*> installedComponents = core->orderedComponentsToInstall();

    QCOMPARE(installedComponents.count(), 1);
    QCOMPARE(installedComponents.at(0)->name(), "componentB.sub1.sub1");
    QCOMPARE(installedComponents.at(0)->treeName(), "BSub1Sub1ToRoot");
    QVERIFY(core->componentByName("componentB.sub1.sub1") != 0);
    QVERIFY(core->componentByName("BSub1Sub1ToRoot") == 0);
    VerifyInstaller::verifyInstallerResources(m_installDir, "componentB.sub1.sub1", "1.0.0content.txt");
    VerifyInstaller::verifyFileExistence(m_installDir, QStringList() << "components.xml"
            << "componentBSub1Sub1.txt");
}

void tst_TreeName::moveToSubItem()
{
    // componentB.sub1.sub2 moved under componentC (componentC.sub1)
    QScopedPointer<PackageManagerCore> core(PackageManager::getPackageManagerWithInit
            (m_installDir, ":///data/repository"));

    QCOMPARE(PackageManagerCore::Success, core->installSelectedComponentsSilently(QStringList() << "componentC"));
    VerifyInstaller::verifyInstallerResources(m_installDir, "componentB.sub1.sub2", "1.0.0content.txt");
    VerifyInstaller::verifyInstallerResources(m_installDir, "componentC", "1.0.0content.txt");
    VerifyInstaller::verifyFileExistence(m_installDir, QStringList() << "components.xml"
            << "componentBSub1Sub2.txt" << "componentC.txt");
}

void tst_TreeName::dependencyToMovedItem()
{
    // componentA depends on componentB.sub2 which is moved to root
    QScopedPointer<PackageManagerCore> core(PackageManager::getPackageManagerWithInit
            (m_installDir, ":///data/repository"));
    QCOMPARE(PackageManagerCore::Success, core->installSelectedComponentsSilently(QStringList() << "componentA"));

    VerifyInstaller::verifyInstallerResources(m_installDir, "componentA", "1.0.0content.txt");
    VerifyInstaller::verifyInstallerResources(m_installDir, "componentA.sub1", "1.0.0content.txt");
    VerifyInstaller::verifyInstallerResources(m_installDir, "componentB.sub2", "1.0.0content.txt");
    VerifyInstaller::verifyFileExistence(m_installDir, QStringList() << "components.xml"
            << "componentA.txt" << "componentASub1.txt" << "componentBSub2.txt");
}

void tst_TreeName::autodependOnMovedItem()
{
    // componentD autodepends on componentA.sub2 which is moved to root
    QScopedPointer<PackageManagerCore> core(PackageManager::getPackageManagerWithInit
            (m_installDir, ":///data/repository"));
    QCOMPARE(PackageManagerCore::Success, core->installSelectedComponentsSilently(QStringList() << "componentA.sub2"));
    VerifyInstaller::verifyInstallerResources(m_installDir, "componentA.sub2", "1.0.0content.txt");
    VerifyInstaller::verifyInstallerResources(m_installDir, "componentD", "1.0.0content.txt");
    VerifyInstaller::verifyFileExistence(m_installDir, QStringList() << "components.xml"
            << "componentASub2.txt" << "componentD.txt");
}

void tst_TreeName::moveToExistingAllowUnstableComponents()
{
    QScopedPointer<PackageManagerCore> core(PackageManager::getPackageManagerWithInit
            (m_installDir, ":///data/invalid_repository"));
    core->settings().setAllowUnstableComponents(true);

    QCOMPARE(PackageManagerCore::Success, core->installSelectedComponentsSilently(QStringList() << "componentA"));
    QVERIFY(core->componentByName("componentB")->isUnstable());
}

void tst_TreeName::moveToExistingItemNoUnstableComponents()
{
    QScopedPointer<PackageManagerCore> core(PackageManager::getPackageManagerWithInit
            (m_installDir, ":///data/invalid_repository"));
    core->settings().setAllowUnstableComponents(false);

    QCOMPARE(PackageManagerCore::Success, core->installSelectedComponentsSilently(QStringList() << "componentA"));
    QVERIFY(!core->componentByName("componentB"));
}

void tst_TreeName::replaceComponentWithTreeName()
{
    QScopedPointer<PackageManagerCore> core(PackageManager::getPackageManagerWithInit
            (m_installDir, ":///data/repository"));

    QCOMPARE(PackageManagerCore::Success, core->installSelectedComponentsSilently(QStringList() << "componentF"));
    QVERIFY(core->componentByName("componentF")->value(scTreeName).isEmpty());
    QVERIFY(!core->componentByName("componentE"));
}

void tst_TreeName::replaceComponentWithTreeNameMoveChildren()
{
    QScopedPointer<PackageManagerCore> core(PackageManager::getPackageManagerWithInit
            (m_installDir, ":///data/repository_children"));

    QCOMPARE(PackageManagerCore::Success, core->installSelectedComponentsSilently(QStringList() << "componentM"));
    QVERIFY(core->componentByName("componentM")->value(scTreeName).isEmpty());
    QVERIFY(!core->componentByName("componentL"));

    Component *component1 = core->componentByName("componentL.sub1");
    QCOMPARE(component1->treeName(), component1->name());

    Component *component2 = core->componentByName("componentL.sub2");
    QCOMPARE(component2->treeName(), component2->name());
}

void tst_TreeName::moveWithChildrenChildConflictsAllowUnstable()
{
    QScopedPointer<PackageManagerCore> core(PackageManager::getPackageManagerWithInit
            (m_installDir, ":///data/invalid_repository"));
    core->settings().setAllowUnstableComponents(true);

    QCOMPARE(PackageManagerCore::Success, core->installSelectedComponentsSilently(QStringList() << "componentD"));
    Component *const component = core->componentByName("componentD.sub1");
    QVERIFY(component && component->isUnstable() && !component->isInstalled());
}

void tst_TreeName::moveWithChildrenChildConflictsNoUnstable()
{
    QScopedPointer<PackageManagerCore> core(PackageManager::getPackageManagerWithInit
            (m_installDir, ":///data/invalid_repository"));
    core->settings().setAllowUnstableComponents(false);

    QCOMPARE(PackageManagerCore::Success, core->installSelectedComponentsSilently(QStringList() << "componentD"));
    QVERIFY(!core->componentByName("componentD.sub1"));
}

void tst_TreeName::moveToRootWithChildren()
{
    // componentF.subcomponent1 moved from sub item to root (FSub1ToRoot)
    QScopedPointer<PackageManagerCore> core(PackageManager::getPackageManagerWithInit
            (m_installDir, ":///data/repository_children"));
    QCOMPARE(PackageManagerCore::Success, core->installSelectedComponentsSilently(
             QStringList() << "componentF.subcomponent1"));
    const QList<Component*> installedComponents = core->orderedComponentsToInstall();

    QCOMPARE(installedComponents.count(), 3);

    Component const *parent = installedComponents.at(0);
    QCOMPARE(parent->name(), "componentF.subcomponent1");
    QCOMPARE(parent->treeName(), "FSub1ToRoot");
    QVERIFY(parent->value(scAutoTreeName).isEmpty());

    Component const *child1 = installedComponents.at(1);
    QCOMPARE(child1->name(), "componentF.subcomponent1.subsubcomponent1");
    QCOMPARE(child1->treeName(), "FSub1ToRoot.subsubcomponent1");
    QCOMPARE(child1->treeName(), child1->value(scAutoTreeName));

    Component const *child2 = installedComponents.at(2);
    QCOMPARE(child2->name(), "componentF.subcomponent1.subsubcomponent2");
    QCOMPARE(child2->treeName(), "FSub1ToRoot.subsubcomponent2");
    QCOMPARE(child2->treeName(), child2->value(scAutoTreeName));

    QVERIFY(core->componentByName("componentF.subcomponent1"));
    QVERIFY(!core->componentByName("FSub1ToRoot"));
    QVERIFY(core->componentByName("componentF.subcomponent1.subsubcomponent1"));
    QVERIFY(!core->componentByName("FSub1ToRoot.subsubcomponent1"));
    QVERIFY(core->componentByName("componentF.subcomponent1.subsubcomponent2"));
    QVERIFY(!core->componentByName("FSub1ToRoot.subsubcomponent2"));

    VerifyInstaller::verifyInstallerResources(m_installDir,
            "componentF.subcomponent1", "1.0.0content.txt");
    VerifyInstaller::verifyInstallerResources(m_installDir,
            "componentF.subcomponent1.subsubcomponent1", "1.0.0content.txt");
    VerifyInstaller::verifyInstallerResources(m_installDir,
            "componentF.subcomponent1.subsubcomponent2", "1.0.0content.txt");
    VerifyInstaller::verifyFileExistence(m_installDir, QStringList() << "components.xml"
            << "installcontentF_1.txt" << "installcontentF_1_1.txt" << "installcontentF_1_2.txt");
}

void tst_TreeName::moveToSubItemWithChildren()
{
    // componentF.subcomponent2 moved under componentE (componentE.sub1)
    QScopedPointer<PackageManagerCore> core(PackageManager::getPackageManagerWithInit
            (m_installDir, ":///data/repository_children"));
    QCOMPARE(PackageManagerCore::Success, core->installSelectedComponentsSilently(
             QStringList() << "componentF.subcomponent2"));
    const QList<Component*> installedComponents = core->orderedComponentsToInstall();

    QCOMPARE(installedComponents.count(), 4);

    Component const *parent = installedComponents.at(1);
    QCOMPARE(parent->name(), "componentF.subcomponent2");
    QCOMPARE(parent->treeName(), "componentE.sub1");
    QVERIFY(parent->value(scAutoTreeName).isEmpty());

    Component const *child1 = installedComponents.at(2);
    QCOMPARE(child1->name(), "componentF.subcomponent2.subsubcomponent1");
    QCOMPARE(child1->treeName(), "componentE.sub1.subsubcomponent1");
    QCOMPARE(child1->treeName(), child1->value(scAutoTreeName));

    Component const *child2 = installedComponents.at(3);
    QCOMPARE(child2->name(), "componentF.subcomponent2.subsubcomponent2");
    QCOMPARE(child2->treeName(), "componentE.sub1.subsubcomponent2");
    QCOMPARE(child2->treeName(), child2->value(scAutoTreeName));

    QVERIFY(core->componentByName("componentF.subcomponent2"));
    QVERIFY(!core->componentByName("componentE.sub1"));
    QVERIFY(core->componentByName("componentF.subcomponent2.subsubcomponent1"));
    QVERIFY(!core->componentByName("componentE.sub1.subsubcomponent1"));
    QVERIFY(core->componentByName("componentF.subcomponent2.subsubcomponent2"));
    QVERIFY(!core->componentByName("componentE.sub1.subsubcomponent2"));

    VerifyInstaller::verifyInstallerResources(m_installDir, "componentE", "1.0.0content.txt");
    VerifyInstaller::verifyInstallerResources(m_installDir,
            "componentF.subcomponent2", "1.0.0content.txt");
    VerifyInstaller::verifyInstallerResources(m_installDir,
            "componentF.subcomponent2.subsubcomponent1", "1.0.0content.txt");
    VerifyInstaller::verifyInstallerResources(m_installDir,
            "componentF.subcomponent2.subsubcomponent2", "1.0.0content.txt");
    VerifyInstaller::verifyFileExistence(m_installDir, QStringList() << "components.xml"
            << "installcontentE.txt" << "installcontentF_2.txt" << "installcontentF_2_1.txt"
            << "installcontentF_2_2.txt");
}

void tst_TreeName::moveToAvailableParentItemWithChilren()
{
    // componentH moved to componentI
    QScopedPointer<PackageManagerCore> core(PackageManager::getPackageManagerWithInit
            (m_installDir, ":///data/repository_children"));
    QCOMPARE(PackageManagerCore::Success, core->installSelectedComponentsSilently(
             QStringList() << "componentH"));
    const QList<Component*> installedComponents = core->orderedComponentsToInstall();

    QCOMPARE(installedComponents.count(), 3);

    Component const *parent = installedComponents.at(0);
    QCOMPARE(parent->name(), "componentH");
    QCOMPARE(parent->treeName(), "componentI");
    QVERIFY(parent->value(scAutoTreeName).isEmpty());

    Component const *child1 = installedComponents.at(1);
    QCOMPARE(child1->name(), "componentH.subcomponent1");
    QCOMPARE(child1->treeName(), "componentI.subcomponent1");
    QCOMPARE(child1->treeName(), child1->value(scAutoTreeName));

    Component const *child2 = installedComponents.at(2);
    QCOMPARE(child2->name(), "componentI.subcomponent2");
    QCOMPARE(child2->treeName(), child2->name());
    QVERIFY(child2->value(scAutoTreeName).isEmpty());

    QVERIFY(core->componentByName("componentH"));
    QVERIFY(!core->componentByName("componentI"));
    QVERIFY(core->componentByName("componentH.subcomponent1"));
    QVERIFY(!core->componentByName("componentI.subcomponent1"));
    QVERIFY(core->componentByName("componentI.subcomponent2"));

    VerifyInstaller::verifyInstallerResources(m_installDir, "componentH", "1.0.0content.txt");
    VerifyInstaller::verifyInstallerResources(m_installDir,
            "componentH.subcomponent1", "1.0.0content.txt");
    VerifyInstaller::verifyInstallerResources(m_installDir,
            "componentI.subcomponent2", "1.0.0content.txt");
    VerifyInstaller::verifyFileExistence(m_installDir, QStringList() << "components.xml"
            << "installcontentH.txt" << "installcontentH_1.txt" << "installcontentI_2.txt");
}

void tst_TreeName::dependencyToMovedSubItem()
{
    // componentK has dependency to componentJ.subcomponent1, which has a parent with treename
    QScopedPointer<PackageManagerCore> core(PackageManager::getPackageManagerWithInit
            (m_installDir, ":///data/repository_children"));
    QCOMPARE(PackageManagerCore::Success, core->installSelectedComponentsSilently(
             QStringList() << "componentK"));
    const QList<Component*> installedComponents = core->orderedComponentsToInstall();

    QCOMPARE(installedComponents.count(), 2);

    Component const *component1 = installedComponents.at(0);
    QCOMPARE(component1->name(), "componentJ.subcomponent1");
    QCOMPARE(component1->treeName(), "componentJNew.subcomponent1");
    QCOMPARE(component1->treeName(), component1->value(scAutoTreeName));

    Component const *component2 = installedComponents.at(1);
    QCOMPARE(component2->name(), "componentK");
    QCOMPARE(component2->treeName(), component2->name());
    QVERIFY(component2->value(scAutoTreeName).isEmpty());

    VerifyInstaller::verifyInstallerResources(m_installDir,
            "componentJ.subcomponent1", "1.0.0content.txt");
    VerifyInstaller::verifyInstallerResources(m_installDir, "componentK", "1.0.0content.txt");
    VerifyInstaller::verifyFileExistence(m_installDir, QStringList() << "components.xml"
            << "installcontentJ_1.txt" << "installcontentK.txt");
}

void tst_TreeName::autoDependOnMovedSubItem()
{
    // componentB auto-depends on componentA.subcomponent1, which has a parent with treename
    QScopedPointer<PackageManagerCore> core(PackageManager::getPackageManagerWithInit
            (m_installDir, ":///data/repository_children"));
    QCOMPARE(PackageManagerCore::Success, core->installSelectedComponentsSilently(
             QStringList() << "componentA.subcomponent1"));
    const QList<Component*> installedComponents = core->orderedComponentsToInstall();

    QCOMPARE(installedComponents.count(), 3);

    Component const *component1 = installedComponents.at(0);
    QCOMPARE(component1->name(), "componentA");
    QCOMPARE(component1->treeName(), "componentANew");
    QVERIFY(component1->value(scAutoTreeName).isEmpty());

    Component const *component2 = installedComponents.at(1);
    QCOMPARE(component2->name(), "componentA.subcomponent1");
    QCOMPARE(component2->treeName(), "componentANew.subcomponent1");
    QCOMPARE(component2->treeName(), component2->value(scAutoTreeName));

    Component const *component3 = installedComponents.at(2);
    QCOMPARE(component3->name(), "componentB");
    QCOMPARE(component3->treeName(), component3->name());

    VerifyInstaller::verifyInstallerResources(m_installDir, "componentA", "1.0.0content.txt");
    VerifyInstaller::verifyInstallerResources(m_installDir,
            "componentA.subcomponent1", "1.0.0content.txt");
    VerifyInstaller::verifyInstallerResources(m_installDir, "componentB", "1.0.0content.txt");
    VerifyInstaller::verifyFileExistence(m_installDir, QStringList() << "components.xml"
            << "installcontentA.txt" << "installcontentA_1.txt" << "installcontentB.txt");
}

void tst_TreeName::remotePackageConflictsLocal()
{
    const QString packageHubFile = qApp->applicationDirPath() + QDir::separator() + "components.xml";
    QFile::remove(packageHubFile);
    QVERIFY(QFile::copy(":///data/components.xml", packageHubFile));
    // For some reason Windows sets the read-only flag when we copy the resource..
    QVERIFY(setDefaultFilePermissions(packageHubFile, DefaultFilePermissions::NonExecutable));

    QHash<QString, QString> params;
    params.insert(scTargetDir, qApp->applicationDirPath());
    PackageManagerCore core(BinaryContent::MagicPackageManagerMarker, QList<OperationBlob>(),
        QString(), QString(), Protocol::DefaultAuthorizationKey, Protocol::Mode::Production, params);

    core.settings().setAllowUnstableComponents(true);
    core.settings().setDefaultRepositories(QSet<Repository>()
        << Repository::fromUserInput(":///data/repository"));

    QVERIFY(core.fetchRemotePackagesTree());
    {
        // Remote treename conflicts with local name
        Component *const local = core.componentByName("ASub2ToRoot");
        QVERIFY(local && local->isInstalled());

        Component *const remote = core.componentByName("componentA.sub2");
        QVERIFY(remote && remote->isUnstable());
        QCOMPARE(remote->treeName(), remote->name());
    }
    {
        // Remote treename conflicts with local treename
        Component *const local = core.componentByName("B");
        QVERIFY(local && local->isInstalled() && local->treeName() == "BSub2ToRoot");

        Component *const remote = core.componentByName("componentB.sub2");
        QVERIFY(remote && remote->isUnstable());
        QCOMPARE(remote->treeName(), remote->name());
    }
    {
        // Remote name conflicts with local treename
        Component *const local = core.componentByName("C");
        QVERIFY(local && local->isInstalled() && local->treeName() == "componentA");

        Component *const remote = core.componentByName("componentA");
        QVERIFY(!remote);
    }
    {
        // Component has a treename in local but not in remote, add with local treename
        Component *const component = core.componentByName("componentD");
        QVERIFY(component && component->isInstalled() && component->treeName() == "componentDNew");
    }
    {
        // Component has different treename in local and remote, add with local treename
        Component *const component = core.componentByName("componentB.sub1.sub2");
        QVERIFY(component && component->isInstalled() && component->treeName() == "componentC.sub2");
    }
    QVERIFY(QFile::remove(packageHubFile));
}

void tst_TreeName::init()
{
    m_installDir = QInstaller::generateTemporaryFileName();
    QVERIFY(QDir().mkpath(m_installDir));
}

void tst_TreeName::cleanup()
{
    QDir dir(m_installDir);
    QVERIFY(dir.removeRecursively());
}

QTEST_MAIN(tst_TreeName)

#include "tst_treename.moc"
