/**************************************************************************
**
** Copyright (C) 2020 The Qt Company Ltd.
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
#include <utils.h>
#include <packagemanagercore.h>

#include <QTest>

using namespace KDUpdater;
using namespace QInstaller;

class tst_copyoperationtest : public QObject
{
    Q_OBJECT

private:
    void installFromCLI(const QString &repository)
    {
        QString installDir = QInstaller::generateTemporaryFileName();
        QVERIFY(QDir().mkpath(installDir));
        PackageManagerCore *core = PackageManager::getPackageManagerWithInit(installDir, repository);
        core->installDefaultComponentsSilently();

        QFile copiedFile(installDir + QDir::separator() + "AnotherFolder/A.txt");
        QVERIFY(copiedFile.exists());
        QFile originalFile(installDir + QDir::separator() + "A.txt");
        QVERIFY(originalFile.exists());

        QByteArray destinationFileHash = QInstaller::calculateHash(copiedFile.fileName(), QCryptographicHash::Sha1);
        QByteArray testFileHash = QInstaller::calculateHash(originalFile.fileName(), QCryptographicHash::Sha1);
        QVERIFY(testFileHash == destinationFileHash);

        core->setPackageManager();
        core->commitSessionOperations();
        core->uninstallComponentsSilently(QStringList() << "A");
        QVERIFY(!copiedFile.exists());

        QDir dir(installDir);
        QVERIFY(dir.removeRecursively());
        core->deleteLater();
    }

private slots:
    void initTestCase()
    {
        //QInstaller::init();
        m_testDestinationPath = qApp->applicationDirPath() + QDir::toNativeSeparators("/test");
        m_testDestinationFilePath = QDir(m_testDestinationPath).absoluteFilePath(QFileInfo(
            qApp->applicationFilePath()).fileName());
        if (QDir(m_testDestinationPath).exists()) {
            QFAIL("Remove test folder first!");
        }
    }

    void testMissingArguments()
    {
        CopyOperation op;

        QVERIFY(op.testOperation());
        QVERIFY(!op.performOperation());

        QCOMPARE(UpdateOperation::Error(op.error()), UpdateOperation::InvalidArguments);
        QCOMPARE(op.errorString(), QString("Invalid arguments in Copy: 0 arguments given, "
            "2 to 4 arguments expected in the form: <source filename> <destination filename> [UNDOOPERATION, \"\"]."));
    }

    void testCopySomething_data()
    {
        QTest::addColumn<QString>("source");
        QTest::addColumn<QString>("destination");
        QTest::addColumn<bool>("overrideUndo");
        QTest::newRow("full path syntax") << qApp->applicationFilePath() << m_testDestinationFilePath << false;
        QTest::newRow("short destination syntax") << qApp->applicationFilePath() << m_testDestinationPath << false;
        QTest::newRow("short destination syntax with ending separator") << qApp->applicationFilePath()
            << m_testDestinationPath + QDir::separator() << false;
        QTest::newRow("override undo") << qApp->applicationFilePath() << m_testDestinationFilePath << true;
    }

    void testCopySomething()
    {
        QFETCH(QString, source);
        QFETCH(QString, destination);
        QFETCH(bool, overrideUndo);

        QVERIFY2(QFileInfo::exists(source), QString("Source file \"%1\" does not exist.").arg(source).toLatin1());
        CopyOperation *op = new CopyOperation();
        op->setArguments(QStringList() << source << destination);
        if (overrideUndo)
            op->setArguments(op->arguments() << QLatin1String("UNDOOPERATION"));
        op->backup();
        QVERIFY2(op->performOperation(), op->errorString().toLatin1());

        QVERIFY2(QFileInfo::exists(m_testDestinationFilePath), QString("Copying from \"%1\" to \"%2\" was "
            "not working: '%3' does not exist").arg(source, destination, m_testDestinationFilePath).toLatin1());
        QVERIFY2(op->undoOperation(), op->errorString().toLatin1());
        if (!overrideUndo) {
            QVERIFY2(!QFileInfo::exists(m_testDestinationFilePath), QString("Undo of copying from \"%1\" to "
                "\"%2\" was not working.").toLatin1());
        } else {
            QVERIFY(QFileInfo::exists(m_testDestinationFilePath));
        }
        QString backupFileName = op->value("backupOfExistingDestination").toString();
        delete op;
        QVERIFY(!QFileInfo::exists(backupFileName));
    }

    void testCopyIfDestinationExist_data()
    {
        testCopySomething_data();
    }

    void testCopyIfDestinationExist()
    {
        QFETCH(QString, source);
        QFETCH(QString, destination);

        QByteArray testString("This file is generated by QTest\n");
        QFile testFile(m_testDestinationFilePath);
        testFile.open(QIODevice::WriteOnly | QIODevice::Text);
        QTextStream out(&testFile);
        out << testString;
        testFile.close();

        QByteArray testFileHash = QInstaller::calculateHash(m_testDestinationFilePath, QCryptographicHash::Sha1);

        QVERIFY2(QFileInfo(source).exists(), QString("Source file \"%1\" does not exist.").arg(source).toLatin1());
        CopyOperation op;
        op.setArguments(QStringList() << source << destination);
        op.backup();
        QVERIFY2(!op.value("backupOfExistingDestination").toString().isEmpty(), "The CopyOperation didn't saved any backup.");
        QVERIFY2(op.performOperation(), op.errorString().toLatin1());

        // checking that perform did something
        QByteArray currentFileHash = QInstaller::calculateHash(m_testDestinationFilePath, QCryptographicHash::Sha1);
        QVERIFY(testFileHash != currentFileHash);

        QVERIFY2(QFileInfo(m_testDestinationFilePath).exists(), QString("Copying from \"%1\" to \"%2\" was "
            "not working: \"%3\" does not exist").arg(source, destination, m_testDestinationFilePath).toLatin1());

        // undo should replace the new one with the old backuped one
        QVERIFY2(op.undoOperation(), op.errorString().toLatin1());
        currentFileHash = QInstaller::calculateHash(m_testDestinationFilePath, QCryptographicHash::Sha1);
        QVERIFY(testFileHash == currentFileHash);
    }

    void testCopyFromScript()
    {
        installFromCLI(":///data/repository");
    }
    void testCopyFromComponentXML()
    {
        installFromCLI(":///data/xmloperationrepository");
    }

    void init()
    {
        QVERIFY2(!QFileInfo(m_testDestinationFilePath).exists(), QString("Destination \"%1\" should not exist "
            "to test the copy operation.").arg(m_testDestinationFilePath).toLatin1());
        QDir().mkpath(m_testDestinationPath);
    }

    void cleanup()
    {
        QFile(m_testDestinationFilePath).remove();
        QDir().rmpath(m_testDestinationPath);
    }
private:
    QString m_testDestinationPath;
    QString m_testDestinationFilePath;
};

QTEST_MAIN(tst_copyoperationtest)

#include "tst_copyoperationtest.moc"
