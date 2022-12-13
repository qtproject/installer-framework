/**************************************************************************
**
** Copyright (C) 2022 The Qt Company Ltd.
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

#include "concurrentoperationrunner.h"
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
    }

    void testConcurrentExtractWithCompetingData()
    {
        // Suppress warnings about already deleted installerResources file
        qInstallMessageHandler(silentTestMessageHandler);

        const QString testDirectory = generateTemporaryFileName()
            + "/subdir1/subdir2/subdir3/subdir4/subdir5/";

        QStringList created7zList;

        OperationList operations;
        for (int i = 0; i < 100; ++i) {
            ExtractArchiveOperation *op = new ExtractArchiveOperation(nullptr);
            // We add the same data multiple times, and extract to same directory.
            // Can't open the same archive multiple times however so it needs to
            // be copied to unique files.
            const QString new7zPath = generateTemporaryFileName() + ".7z";
            QFile old7z(":///data/subdirs.7z");
            QVERIFY(old7z.copy(new7zPath));

            op->setArguments(QStringList() << new7zPath << testDirectory);
            operations.append(op);
        }
        ConcurrentOperationRunner runner(&operations, Operation::Backup);

        const QHash<Operation *, bool> backupResults = runner.run();
        const OperationList backupOperations = backupResults.keys();

        for (auto *operation : backupOperations)
            QVERIFY2((backupResults.value(operation) && operation->error() == Operation::NoError),
                     operation->errorString().toLatin1());

        runner.setType(Operation::Perform);
        const QHash<Operation *, bool> results = runner.run();
        const OperationList performedOperations = results.keys();

        for (auto *operation : performedOperations)
            QVERIFY2((results.value(operation) && operation->error() == Operation::NoError),
                     operation->errorString().toLatin1());

        for (auto *operation : operations)
            QVERIFY(operation->undoOperation());

        qDeleteAll(operations);

        for (const QString &archive : created7zList)
            QFile::remove(archive);

        QDir().rmdir(testDirectory);
    }

    void testExtractArchiveFromXML()
    {
        m_testDirectory = QInstaller::generateTemporaryFileName();
        QVERIFY(QDir().mkpath(m_testDirectory));
        QVERIFY(QDir(m_testDirectory).exists());

        QScopedPointer<PackageManagerCore> core(PackageManager::getPackageManagerWithInit
                (m_testDirectory, ":///data/xmloperationrepository"));
        core->installDefaultComponentsSilently();

        QFile extractedFile(m_testDirectory + QDir::separator() + "FolderForContent/content.txt");
        QVERIFY(extractedFile.exists());
#ifdef IFW_LIBARCHIVE
        extractedFile.setFileName(m_testDirectory + QDir::separator() + "FolderForTarGzContent/content.txt");
        QVERIFY(extractedFile.exists());

        extractedFile.setFileName(m_testDirectory + QDir::separator() + "FolderForTarBz2Content/content.txt");
        QVERIFY(extractedFile.exists());

        extractedFile.setFileName(m_testDirectory + QDir::separator() + "FolderForTarXzContent/content.txt");
        QVERIFY(extractedFile.exists());

        extractedFile.setFileName(m_testDirectory + QDir::separator() + "FolderForZipContent/content.txt");
        QVERIFY(extractedFile.exists());
#endif
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

    void testInstallerBaseBinaryExtraction()
    {
        m_testDirectory = QInstaller::generateTemporaryFileName();
        QVERIFY(QDir().mkpath(m_testDirectory));
        QVERIFY(QDir(m_testDirectory).exists());

        QScopedPointer<PackageManagerCore> core(PackageManager::getPackageManagerWithInit
                                                (m_testDirectory, ":///data/installerbaserepository"));
        core->setInstallerBaseBinary(m_testDirectory + QDir::separator() + "testInstallerBase.txt");
        core->installDefaultComponentsSilently();

        QFile contentResourceFile(m_testDirectory + QDir::separator() + "installerResources" + QDir::separator() + "A" + QDir::separator() + "1.0.0content.txt");
        QVERIFY2(contentResourceFile.open(QIODevice::ReadOnly | QIODevice::Text), "Could not open content resource file for reading.");
        QTextStream fileStream(&contentResourceFile);
        QString line = fileStream.readLine();
        QVERIFY2(!line.isEmpty(), "Content not written to resource file.");
        contentResourceFile.close();

        QFile installerBaseResourceFile(m_testDirectory + QDir::separator() + "installerResources" + QDir::separator() + "A" + QDir::separator() + "1.0.0installerbase.txt");
        QVERIFY2(installerBaseResourceFile.open(QIODevice::ReadOnly | QIODevice::Text),  "Could not open installerbase resource file for reading.");
        QTextStream fileStream2(&installerBaseResourceFile);
        line = installerBaseResourceFile.readLine();
        QVERIFY2(line.isEmpty(), "Installerbase falsly written to resource file.");
        installerBaseResourceFile.close();

        core->setPackageManager();
        core->commitSessionOperations();

        QCOMPARE(PackageManagerCore::Success, core->uninstallComponentsSilently(QStringList() << "A"));
        QDir dir(m_testDirectory);
        QVERIFY(dir.removeRecursively());
    }

private:
    QString m_testDirectory;
};

QTEST_MAIN(tst_extractarchiveoperationtest)

#include "tst_extractarchiveoperationtest.moc"
