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

#include "repository.h"
#include "repositorycategory.h"
#include "settings.h"

#include <QDataStream>
#include <QString>
#include <QTest>

using namespace QInstaller;

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

        QHash<QString, QPair<Repository, Repository>> update;

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
};

QTEST_MAIN(tst_Repository)

#include "tst_repository.moc"
