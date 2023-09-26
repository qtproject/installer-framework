/**************************************************************************
**
** Copyright (C) 2023 The Qt Company Ltd.
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

#include "repository.h"
#include "repositorycategory.h"
#include "settings.h"

#include <QDataStream>
#include <QString>
#include <QTest>

using namespace QInstaller;

typedef QList<QPair<QString, QString>> SettingsPairList;

class tst_Repository : public QObject
{
    Q_OBJECT

private slots:
    void testRepository()
    {
        Repository repo;
        QCOMPARE(repo.isValid(), false);
        QCOMPARE(repo.isDefault(), false);
        QCOMPARE(repo.isEnabled(), false);

        QCOMPARE(repo.url(), QUrl());
        QCOMPARE(repo.username(), QString());
        QCOMPARE(repo.password(), QString());

        repo.setUrl(QUrl("http://www.digia.com"));
        QCOMPARE(repo.isValid(), true);
        QCOMPARE(repo.url(), QUrl("http://www.digia.com"));

        repo.setEnabled(true);
        QCOMPARE(repo.isEnabled(), true);

        repo.setUsername("tester");
        QCOMPARE(repo.username(), QString("tester"));

        repo.setPassword("test");
        QCOMPARE(repo.password(), QString("test"));
    }

    void testRepositoryFromUrl()
    {
        Repository repo(QUrl("http://www.digia.com"), true);
        QCOMPARE(repo.isValid(), true);
        QCOMPARE(repo.isDefault(), true);
        QCOMPARE(repo.isEnabled(), true);

        QCOMPARE(repo.url(), QUrl("http://www.digia.com"));
        QCOMPARE(repo.username(), QString());
        QCOMPARE(repo.password(), QString());

        repo.setUrl(QUrl());
        QCOMPARE(repo.isValid(), false);

        repo.setEnabled(false);
        QCOMPARE(repo.isEnabled(), false);

        repo.setUsername("tester");
        QCOMPARE(repo.username(), QString("tester"));

        repo.setPassword("test");
        QCOMPARE(repo.password(), QString("test"));
    }

    void testRepositoryFromUserInput()
    {
        Repository repo = Repository::fromUserInput("ftp://tester:test@www.digia.com");
        QCOMPARE(repo.isValid(), true);
        QCOMPARE(repo.isDefault(), false);
        QCOMPARE(repo.isEnabled(), true);

        QCOMPARE(repo.url(), QUrl("ftp://www.digia.com"));
        QCOMPARE(repo.username(), QString("tester"));
        QCOMPARE(repo.password(), QString("test"));

        repo.setUrl(QUrl());
        QCOMPARE(repo.isValid(), false);

        repo.setEnabled(false);
        QCOMPARE(repo.isEnabled(), false);

        repo.setUsername("");
        QCOMPARE(repo.username(), QString());

        repo.setPassword("");
        QCOMPARE(repo.password(), QString());
    }

    void testRepositoryOperators()
    {
        Repository lhs, rhs;

        // operator==
        QVERIFY(lhs == rhs);

        // assignment operator
        rhs = Repository::fromUserInput("ftp://tester:test@www.digia.com");

        // operator!=
        QVERIFY(lhs != rhs);

        // copy constructor
        Repository clhs(rhs);

        // operator==
        QVERIFY(clhs == rhs);

        QByteArray ba1, ba2;
        QDataStream s1(&ba1, QIODevice::ReadWrite), s2(&ba2, QIODevice::ReadWrite);

        // QDatastream operator
        s1 << clhs; s2 << rhs;
        QCOMPARE(ba1, ba2);

        Repository r1, r2;
        s1 >> r1; s2 >> r2;
        QCOMPARE(r1, r2);
    }

    void testUpdateRepositoryCategories()
    {
        Settings settings;

        RepositoryCategory category;
        category.setEnabled(true);
        category.setDisplayName(QLatin1String("category"));

        Repository original(QUrl("http://example.com/"), true);
        original.setEnabled(true);
        category.addRepository(original);

        Repository replacement = original;
        replacement.setUsername(QLatin1String("user"));
        replacement.setPassword(QLatin1String("pass"));

        QSet<RepositoryCategory> categories;
        categories.insert(category);
        settings.setRepositoryCategories(categories);

        QMultiHash<QString, QPair<Repository, Repository>> update;

        // non-empty update
        update.insert(QLatin1String("replace"), qMakePair(original, replacement));
        QVERIFY(settings.updateRepositoryCategories(update) == Settings::UpdatesApplied);

        // verify that the values really updated
        QVERIFY(settings.repositoryCategories().values().at(0)
            .repositories().values().at(0).username() == "user");

        QVERIFY(settings.repositoryCategories().values().at(0)
            .repositories().values().at(0).password() == "pass");

        // empty update
        update.clear();
        QVERIFY(settings.updateRepositoryCategories(update) == Settings::NoUpdatesApplied);

        // non-matching repository update
        Repository nonMatching(QUrl("https://www.qt.io/"), true);
        nonMatching.setEnabled(true);

        update.insert(QLatin1String("replace"), qMakePair(nonMatching, replacement));
        QVERIFY(settings.updateRepositoryCategories(update) == Settings::NoUpdatesApplied);
    }

    void testCompressedRepositoryWithPriority()
    {
        // Compressed repository has higher priority than normal repository.
        // If the versions match, compressed repository is used instead of normal repository.
        QString installDir = QInstaller::generateTemporaryFileName();
        QVERIFY(QDir().mkpath(installDir));

        PackageManagerCore *core = PackageManager::getPackageManagerWithInit(installDir, ":///data/repository");
        core->setTemporaryRepositories(QStringList()
            << ":///data/compressedRepository/compressedRepository.7z", false, true);
        core->installSelectedComponentsSilently(QStringList() << "A");
        VerifyInstaller::verifyFileExistence(installDir, QStringList() << "components.xml" << "A_from_compressed.txt");

        QDir dir(installDir);
        QVERIFY(dir.removeRecursively());
        core->deleteLater();
    }

    void testPostLoadScriptFromRepository_data()
    {
        QTest::addColumn<QStringList>("installComponent");
        QTest::addColumn<bool>("repositoryPostLoadSetting");
        QTest::addColumn<SettingsPairList>("expectedSettingsBeforeInstall");
        QTest::addColumn<SettingsPairList>("expectedSettingsAfterInstall");
        QTest::addColumn<QStringList>("unexpectedSettings");

        // component A has postLoad = true in component.xml
        // component B has no postLoad attribute
        // component C has postLoad = false in component.xml

        SettingsPairList expectedSettingsListAfterInstall;
        expectedSettingsListAfterInstall.append(QPair<QString, QString>("componentAKey", "componentAValue"));

        SettingsPairList expectedSettingsListBeforeInstall;
        expectedSettingsListBeforeInstall.append(QPair<QString, QString>("componentBKey", "componentBValue"));
        expectedSettingsListBeforeInstall.append(QPair<QString, QString>("componentCKey", "componentCValue"));

        QTest::newRow("noRepoPostLoadComponentA")
                            << (QStringList() << "A")
                            << false
                            << expectedSettingsListBeforeInstall
                            << expectedSettingsListAfterInstall
                            << (QStringList());

        // Component B is installed so values from component A and component C should not be set
        expectedSettingsListAfterInstall.clear();
        expectedSettingsListAfterInstall.append(QPair<QString, QString>("componentBKey", "componentBValue"));
        QTest::newRow("noRepoPostLoadComponentB")
                            << (QStringList() << "B")
                            << false
                            << expectedSettingsListBeforeInstall
                            << expectedSettingsListAfterInstall
                            << (QStringList() << "componentAValue" << "componentCValue");

        // PostLoad is set to whole repository. Since only A is installed,
        // values from component B and component C values are not set
        expectedSettingsListBeforeInstall.clear();
        expectedSettingsListAfterInstall.clear();
        expectedSettingsListAfterInstall.append(QPair<QString, QString>("componentAKey", "componentAValue"));
        QTest::newRow("repoPostLoadComponentA") << (QStringList() << "A")
                            << true
                            << expectedSettingsListBeforeInstall
                            << expectedSettingsListAfterInstall
                            << (QStringList() << "componentBValue" << "componentCValue");

        // PostLoad is set to whole repository. Since only B is installed,
        // values from component C and component A are not set.
        expectedSettingsListAfterInstall.clear();
        expectedSettingsListAfterInstall.append(QPair<QString, QString>("componentBKey", "componentBValue"));
        QTest::newRow("repoPostLoadComponentB") << (QStringList() << "B")
                            << true
                            << expectedSettingsListBeforeInstall
                            << expectedSettingsListAfterInstall
                            << (QStringList() << "componentCValue" << "componentAValue");

        // PostLoad is set to whole repository. Since component C has its postload = false,
        // value is set to component C before install
        expectedSettingsListBeforeInstall.clear();
        expectedSettingsListAfterInstall.clear();
        expectedSettingsListBeforeInstall.append(QPair<QString, QString>("componentCKey", "componentCValue"));
        QTest::newRow("repoPostLoadComponentC") << (QStringList() << "C")
                            << true
                            << expectedSettingsListBeforeInstall
                            << expectedSettingsListAfterInstall
                            << (QStringList() << "componentAValue" << "componentBValue");
    }

    void testPostLoadScriptFromRepository()
    {
        QFETCH(QStringList, installComponent);
        QFETCH(bool, repositoryPostLoadSetting);
        QFETCH(SettingsPairList, expectedSettingsBeforeInstall);
        QFETCH(SettingsPairList, expectedSettingsAfterInstall);
        QFETCH(QStringList, unexpectedSettings);

        QString installDir = QInstaller::generateTemporaryFileName();
        QScopedPointer<PackageManagerCore> core(PackageManager::getPackageManagerWithInit(installDir));
        QSet<Repository> repoList;
        Repository repo = Repository::fromUserInput(":///data/repository");
        repo.setPostLoadComponentScript(repositoryPostLoadSetting);
        repoList.insert(repo);
        core->settings().setDefaultRepositories(repoList);
        QVERIFY(core->fetchRemotePackagesTree());

        for (const QPair<QString, QString> settingValue : expectedSettingsBeforeInstall)
            QCOMPARE(core->value(settingValue.first), settingValue.second);

        core->installSelectedComponentsSilently(installComponent);
        for (const QPair<QString, QString> settingValue : expectedSettingsAfterInstall)
            QCOMPARE(core->value(settingValue.first), settingValue.second);
        for (const QString unexpectedSetting : unexpectedSettings)
            QVERIFY2(!core->containsValue(unexpectedSetting), "Core contains unexpected value");
    }
};

QTEST_MAIN(tst_Repository)

#include "tst_repository.moc"
