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

#include "init.h"
#include "updateoperations.h"

#include <QDir>
#include <QObject>
#include <QTest>
#include <QFile>
#include <QTextStream>

using namespace KDUpdater;
using namespace QInstaller;

class tst_rmdiroperationtest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
       QInstaller::init();
       QString path = QDir::current().path() + QDir::toNativeSeparators("/test");
       if (QDir(path).exists()) {
           QFAIL("Remove test folder first!");
       }
    }

    void testMissingArguments()
    {
        RmdirOperation op;

        QVERIFY(op.testOperation());
        QVERIFY(!op.performOperation());

        QCOMPARE(UpdateOperation::Error(op.error()), UpdateOperation::InvalidArguments);
        QCOMPARE(op.errorString(), QLatin1String("Invalid arguments in Rmdir: 0 arguments given, 1 to 3 "
            "arguments expected in the form: <file to remove> [UNDOOPERATION, \"\"]."));
    }

    void testRemoveDirectory_data()
    {
        QTest::addColumn<QString>("directory");
        QTest::addColumn<bool>("overrideUndo");
        QTest::newRow("/test") << "/test" << false;
        QTest::newRow("/test/test") << "/test/test" << false;
        QTest::newRow("/test/test/test") << "/test/test/test" << false;
        QTest::newRow("no undo") << "/test/test/test/test" << true;
    }

    void testRemoveDirectory()
    {
        QFETCH(QString, directory);
        QFETCH(bool, overrideUndo);

        QString path = QDir::current().path() + QDir::toNativeSeparators(directory);
        //Create first the directories utilizing MkdirOperation
        MkdirOperation op;
        op.setArguments(QStringList() << path);
        op.setArguments(op.arguments() << QLatin1String("UNDOOPERATION"));
        op.backup();
        QVERIFY2(op.performOperation(), op.errorString().toLatin1());
        QVERIFY2(QDir(path).exists(), path.toLatin1());

        RmdirOperation rmOp;
        rmOp.setArguments(QStringList() << path);
        if (overrideUndo)
            rmOp.setArguments(op.arguments() << QLatin1String("UNDOOPERATION"));
        rmOp.backup();
        rmOp.performOperation();
        QVERIFY2(!QDir(path).exists(), path.toLatin1());

        QVERIFY2(rmOp.undoOperation(), rmOp.errorString().toLatin1());
        if (overrideUndo) {
            QVERIFY2(!QDir(path).exists(), path.toLatin1());
        } else {
            QVERIFY2(QDir(path).exists(), path.toLatin1());
            QVERIFY(QDir(path).removeRecursively());
        }
    }
};

QTEST_MAIN(tst_rmdiroperationtest)

#include "tst_rmdiroperationtest.moc"
