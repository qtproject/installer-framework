/**************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
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
    void moveToExistingItem();

    void init();
    void cleanup();

private:
    QString m_installDir;

};

void tst_TreeName::moveToRoot()
{
    // componentB.sub1.sub1 moved from sub item to root (BSub1Sub1ToRoot)
    PackageManagerCore *core = PackageManager::getPackageManagerWithInit
            (m_installDir, ":///data/repository");
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
    PackageManagerCore *core = PackageManager::getPackageManagerWithInit
            (m_installDir, ":///data/repository");

    QCOMPARE(PackageManagerCore::Success, core->installSelectedComponentsSilently(QStringList() << "componentC"));
    VerifyInstaller::verifyInstallerResources(m_installDir, "componentB.sub1.sub2", "1.0.0content.txt");
    VerifyInstaller::verifyInstallerResources(m_installDir, "componentC", "1.0.0content.txt");
    VerifyInstaller::verifyFileExistence(m_installDir, QStringList() << "components.xml"
            << "componentBSub1Sub2.txt" << "componentC.txt");
}

void tst_TreeName::dependencyToMovedItem()
{
    // componentA depends on componentB.sub2 which is moved to root
    PackageManagerCore *core = PackageManager::getPackageManagerWithInit
            (m_installDir, ":///data/repository");
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
    PackageManagerCore *core = PackageManager::getPackageManagerWithInit
            (m_installDir, ":///data/repository");
    QCOMPARE(PackageManagerCore::Success, core->installSelectedComponentsSilently(QStringList() << "componentA.sub2"));
    VerifyInstaller::verifyInstallerResources(m_installDir, "componentA.sub2", "1.0.0content.txt");
    VerifyInstaller::verifyInstallerResources(m_installDir, "componentD", "1.0.0content.txt");
    VerifyInstaller::verifyFileExistence(m_installDir, QStringList() << "components.xml"
            << "componentASub2.txt" << "componentD.txt");
}

void tst_TreeName::moveToExistingItem()
{
    PackageManagerCore *core = PackageManager::getPackageManagerWithInit
            (m_installDir, ":///data/invalid_repository");
    QCOMPARE(PackageManagerCore::Failure, core->installSelectedComponentsSilently(QStringList() << "componentA"));
    QCOMPARE(core->error(), "Cannot register component! Component with identifier componentA.sub1 already exists.");
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
