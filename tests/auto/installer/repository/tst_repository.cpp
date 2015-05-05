#include "repository.h"

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
};

QTEST_MAIN(tst_Repository)

#include "tst_repository.moc"
