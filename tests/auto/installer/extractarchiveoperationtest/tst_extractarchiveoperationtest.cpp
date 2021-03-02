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

#include "../shared/packagemanager.h"

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

private:

private slots:
    void initTestCase()
    {
        QInstaller::init();
    }

    void testMissingArguments()
    {
        ExtractArchiveOperation op(nullptr);

        QVERIFY(op.testOperation());
        QVERIFY(!op.performOperation());
        //QVERIFY(!op.undoOperation());     Can't test for failure as we run into Q_ASSERT

        QCOMPARE(UpdateOperation::Error(op.error()), UpdateOperation::InvalidArguments);
        QCOMPARE(op.errorString(), QString("Invalid arguments in Extract: "
                                           "0 arguments given, exactly 2 arguments expected."));

    }

    void testExtractOperationValidFile()
    {
        ExtractArchiveOperation op(nullptr);
        op.setArguments(QStringList() << ":///data/valid.7z" << QDir::tempPath());

        QVERIFY(op.testOperation());
        QVERIFY(op.performOperation());
        QVERIFY(op.undoOperation());
    }

    void testExtractOperationInvalidFile()
    {
        ExtractArchiveOperation op(nullptr);
        op.setArguments(QStringList() << ":///data/invalid.7z" << QDir::tempPath());

        QVERIFY(op.testOperation());
        QVERIFY(!op.performOperation());
        QVERIFY(op.undoOperation());

        QCOMPARE(UpdateOperation::Error(op.error()), UpdateOperation::UserDefinedError);
        QCOMPARE(op.errorString(), QString("Cannot open archive \":///data/invalid.7z\" for reading: "));
    }

    void testExtractArchiveFromXML()
    {
        m_testDirectory = QInstaller::generateTemporaryFileName();
        QVERIFY(QDir().mkpath(m_testDirectory));
        QVERIFY(QDir(m_testDirectory).exists());

        PackageManagerCore *core = PackageManager::getPackageManagerWithInit
                (m_testDirectory, ":///data/xmloperationrepository");
        core->installDefaultComponentsSilently();

        QFile extractedFile(m_testDirectory + QDir::separator() + "FolderForContent/content.txt");
        QVERIFY(extractedFile.exists());

        extractedFile.setFileName(m_testDirectory + QDir::separator() + "FolderForAnotherContent/anothercontent.txt");
        QVERIFY(extractedFile.exists());

        extractedFile.setFileName(m_testDirectory + QDir::separator() + "FolderForDefault/default.txt");
        QVERIFY(extractedFile.exists());

        core->setPackageManager();
        core->commitSessionOperations();

        core->uninstallComponentsSilently(QStringList() << "A");
        QDir dir(m_testDirectory);
        QVERIFY(dir.removeRecursively());
    }

private:
    QString m_testDirectory;
};

QTEST_MAIN(tst_extractarchiveoperationtest)

#include "tst_extractarchiveoperationtest.moc"
