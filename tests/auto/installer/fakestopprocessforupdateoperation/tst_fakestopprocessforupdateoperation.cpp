#include "fakestopprocessforupdateoperation.h"
#include "packagemanagercore.h"

#include <QFileInfo>
#include <QString>
#include <QTest>

using namespace KDUpdater;
using namespace QInstaller;

class tst_FakeStopProcessForUpdateOperation : public QObject
{
    Q_OBJECT

private slots:
    void testMissingArgument()
    {
        FakeStopProcessForUpdateOperation op(&m_core);

        QVERIFY(op.testOperation());
        QVERIFY(op.performOperation());
        QVERIFY(!op.undoOperation());

        QCOMPARE(UpdateOperation::Error(op.error()), UpdateOperation::InvalidArguments);
        QCOMPARE(op.errorString(), QString("Invalid arguments in FakeStopProcessForUpdate: "
                                           "0 arguments given, exactly 1 arguments expected."));
    }

    void testMissingPackageManagerCore()
    {
        FakeStopProcessForUpdateOperation op(nullptr);
        op.setArguments(QStringList() << QFileInfo(QCoreApplication::applicationFilePath()).fileName());

        QVERIFY(op.testOperation());
        QVERIFY(op.performOperation());
        QVERIFY(!op.undoOperation());

        QCOMPARE(UpdateOperation::Error(op.error()), UpdateOperation::UserDefinedError);
        QCOMPARE(op.errorString(), QString("Cannot get package manager core."));
    }

    void testRunningApplication()
    {
        const QString app = QFileInfo(QCoreApplication::applicationFilePath()).fileName();

        FakeStopProcessForUpdateOperation op(&m_core);
        op.setArguments(QStringList() << app);

        QVERIFY(op.testOperation());
        QVERIFY(op.performOperation());
        QVERIFY(!op.undoOperation());

        QCOMPARE(UpdateOperation::Error(op.error()), UpdateOperation::UserDefinedError);
        QCOMPARE(op.errorString(), QString::fromLatin1("This process should be stopped before "
            "continuing: %1").arg(app));
    }

    void testRunningNonApplication()
    {
        FakeStopProcessForUpdateOperation op(&m_core);
        op.setArguments(QStringList() << "dummy.exe");

        QVERIFY(op.testOperation());
        QVERIFY(op.performOperation());
        QVERIFY(op.undoOperation());

        QCOMPARE(op.errorString(), QString());
        QCOMPARE(UpdateOperation::Error(op.error()), UpdateOperation::NoError);
    }

private:
    PackageManagerCore m_core;
};

QTEST_MAIN(tst_FakeStopProcessForUpdateOperation)

#include "tst_fakestopprocessforupdateoperation.moc"
