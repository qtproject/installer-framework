/**************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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
#include "extractarchiveoperation.h"

#include <QDir>
#include <QObject>
#include <QTest>

using namespace KDUpdater;
using namespace QInstaller;

class tst_extractarchiveoperationtest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QInstaller::init();
    }

    void testMissingArguments()
    {
        ExtractArchiveOperation op;

        QVERIFY(op.testOperation());
        QVERIFY(!op.performOperation());
        //QVERIFY(!op.undoOperation());     Can't test for failure as we run into Q_ASSERT

        QCOMPARE(UpdateOperation::Error(op.error()), UpdateOperation::InvalidArguments);
        QCOMPARE(op.errorString(), QString("Invalid arguments in Extract: 0 arguments given, exactly 2 expected."));

    }

    void testExtractOperationValidFile()
    {
        ExtractArchiveOperation op;
        op.setArguments(QStringList() << ":///data/valid.7z" << QDir::tempPath());

        QVERIFY(op.testOperation());
        QVERIFY(op.performOperation());
        QVERIFY(op.undoOperation());
    }

    void testExtractOperationInvalidFile()
    {
        ExtractArchiveOperation op;
        op.setArguments(QStringList() << ":///data/invalid.7z" << QDir::tempPath());

        QVERIFY(op.testOperation());
        QVERIFY(!op.performOperation());
        QVERIFY(op.undoOperation());

        QCOMPARE(UpdateOperation::Error(op.error()), UpdateOperation::UserDefinedError);
        QCOMPARE(op.errorString(), QString("Error while extracting ':///data/invalid.7z': Could not open archive"));
    }
};

QTEST_MAIN(tst_extractarchiveoperationtest)

#include "tst_extractarchiveoperationtest.moc"
