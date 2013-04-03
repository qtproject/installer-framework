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
        FakeStopProcessForUpdateOperation op;
        op.setValue(QLatin1String("installer"), QVariant::fromValue(&m_core));

        QVERIFY(op.testOperation());
        QVERIFY(op.performOperation());
        QVERIFY(!op.undoOperation());

        QCOMPARE(UpdateOperation::Error(op.error()), UpdateOperation::InvalidArguments);
        QCOMPARE(op.errorString(), QString("Number of arguments does not match: one is required"));
    }

    void testMissingPackageManagerCore()
    {
        FakeStopProcessForUpdateOperation op;
        op.setArguments(QStringList() << QFileInfo(QCoreApplication::applicationFilePath()).fileName());

        QVERIFY(op.testOperation());
        QVERIFY(op.performOperation());
        QVERIFY(!op.undoOperation());

        QCOMPARE(UpdateOperation::Error(op.error()), UpdateOperation::UserDefinedError);
        QCOMPARE(op.errorString(), QString("Could not get package manager core."));
    }

    void testRunningApplication()
    {
        const QString app = QFileInfo(QCoreApplication::applicationFilePath()).fileName();

        FakeStopProcessForUpdateOperation op;
        op.setArguments(QStringList() << app);
        op.setValue(QLatin1String("installer"), QVariant::fromValue(&m_core));

        QVERIFY(op.testOperation());
        QVERIFY(op.performOperation());
        QVERIFY(!op.undoOperation());

        QCOMPARE(UpdateOperation::Error(op.error()), UpdateOperation::UserDefinedError);
        QCOMPARE(op.errorString(), QString::fromLatin1("This process should be stopped before "
            "continuing: %1").arg(app));
    }

    void testRunningNonApplication()
    {
        FakeStopProcessForUpdateOperation op;
        op.setArguments(QStringList() << "dummy.exe");
        op.setValue(QLatin1String("installer"), QVariant::fromValue(&m_core));

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
