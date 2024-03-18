/**************************************************************************
**
** Copyright (C) 2024 The Qt Company Ltd.
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

#include <updateoperations.h>
#include <packagemanagercore.h>

#include <QFile>
#include <QTest>

using namespace KDUpdater;
using namespace QInstaller;

class tst_prependfileoperation : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        m_testFilePath = qApp->applicationDirPath() + QDir::toNativeSeparators("/test");
    }

    void testMissingArguments()
    {
        PrependFileOperation op;

        QVERIFY(op.testOperation());
        QVERIFY(!op.performOperation());

        QCOMPARE(UpdateOperation::Error(op.error()), UpdateOperation::InvalidArguments);
        QCOMPARE(op.errorString(), QString("Invalid arguments in PrependFile: 0 arguments given, 2 to 4 arguments expected in the form: <filename> <text to prepend> [UNDOOPERATION, \"\"]."));

        op.setArguments(QStringList() << "" << "");
        QTest::ignoreMessage(QtWarningMsg, "QFSFileEngine::open: No file name specified");
        QVERIFY(!op.performOperation());

        QCOMPARE(UpdateOperation::Error(op.error()), UpdateOperation::UserDefinedError);
        QCOMPARE(op.errorString(), QString("Cannot open file \"\" for reading: No file name specified"));
    }

    void testPrependText_data()
    {
        QTest::addColumn<QString>("source");
        QTest::addColumn<QString>("prepend");
        QTest::addColumn<QString>("expected");
        QTest::addColumn<bool>("overrideUndo");
        QTest::newRow("newline") << "Line1\nLine2\nLine3\n" << "PrependedText"
                                 << "PrependedTextLine1\nLine2\nLine3\n" << false;
        QTest::newRow("no newline") << "dolore sit amet" << "Lorem ipsum "
            << "Lorem ipsum dolore sit amet" << false;

        QTest::newRow("no undo")<< "dolore sit amet"  << "Lorem ipsum "
            << "Lorem ipsum dolore sit amet" << true;
    }

    void testPrependText()
    {
        QFETCH(QString, source);
        QFETCH(QString, prepend);
        QFETCH(QString, expected);
        QFETCH(bool, overrideUndo);

        QFile file(m_testFilePath);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));

        QTextStream stream(&file);
        stream << source << Qt::flush;
        file.close();

        PrependFileOperation *op = new PrependFileOperation();
        op->setArguments(QStringList() << m_testFilePath << prepend);
        if (overrideUndo)
            op->setArguments(op->arguments() << QLatin1String("UNDOOPERATION"));

        op->backup();
        QVERIFY(QFileInfo(op->value("backupOfFile").toString()).exists());

        QVERIFY2(op->performOperation(), op->errorString().toLatin1());
        QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
        QCOMPARE(stream.readAll(), expected);
        file.close();

        QVERIFY2(op->undoOperation(), op->errorString().toLatin1());
        QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
        if (overrideUndo)
            QCOMPARE(stream.readAll(), expected);
        else
            QCOMPARE(stream.readAll(), source);
        file.close();

        QVERIFY(file.remove());

        QString backupFileName = op->value("backupOfFile").toString();
        delete op;
        QVERIFY(!QFileInfo::exists(backupFileName));
    }

    void testPrependFromCLI_data()
    {
        QTest::addColumn<QString>("repository");
        QTest::addColumn<QString>("fileName");
        QTest::addColumn<QString>("componentName");
        QTest::addColumn<QString>("expected");

        QTest::newRow("operationFromScript")
            << (":///data/repository")
            << ("B.txt")
            << ("B")
            << ("Prepended text: lorem ipsum");

        QTest::newRow("operationFromXML")
            << (":///data/xmloperationrepository")
            << ("C.txt")
            << ("C")
            << ("Prepended text: lorem ipsum");
    }

    void testPrependFromCLI()
    {
        QFETCH(QString, repository);
        QFETCH(QString, fileName);
        QFETCH(QString, componentName);
        QFETCH(QString, expected);

        QString installDir = QInstaller::generateTemporaryFileName();
        QVERIFY(QDir().mkpath(installDir));
        QScopedPointer<PackageManagerCore> core(PackageManager::getPackageManagerWithInit
                                                (installDir, repository));
        core->installSelectedComponentsSilently(QStringList() << componentName);

        QFile file(installDir + QDir::separator() + fileName);
        QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
        QTextStream stream(&file);
        QCOMPARE(stream.readAll(), expected);
        file.close();

        core->setPackageManager();
        core->commitSessionOperations();
        // We cannot check the file contents here as it will be deleted on
        // undo Extract, but at least check that the uninstallation succeeds.
        QCOMPARE(PackageManagerCore::Success, core->uninstallComponentsSilently
                                              (QStringList()<< componentName));

        QDir dir(installDir);
        QVERIFY(dir.removeRecursively());
    }

private:
    QString m_testFilePath;
};

QTEST_MAIN(tst_prependfileoperation)

#include "tst_prependfileoperation.moc"
