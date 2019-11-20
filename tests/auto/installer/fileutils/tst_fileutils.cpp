/**************************************************************************
**
** Copyright (C) 2019 The Qt Company Ltd.
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

#include <qinstallerglobal.h>
#include <fileutils.h>

#include <QObject>
#include <QTest>
#include <QFile>
#include <QDir>

using namespace QInstaller;

class tst_fileutils : public QObject
{
    Q_OBJECT

private slots:
    void testSetDefaultFilePermissions()
    {
#if defined(Q_OS_WIN)
        QString fileName = QInstaller::generateTemporaryFileName();
        QFile testFile(fileName);

        QVERIFY(testFile.open(QIODevice::ReadWrite));
        QVERIFY(testFile.exists());
        testFile.close();

        // Set permissions containing none of the Write* flags, this will cause
        // the read-only flag to be set for the file
        QVERIFY(testFile.setPermissions(QFileDevice::ReadOwner | QFileDevice::ReadUser
            | QFileDevice::ReadGroup | QFileDevice::ReadOther));

        // Verify that the file cannot be opened for both reading and writing, i.e.
        // it should be read-only.
        QVERIFY(!testFile.open(QIODevice::ReadWrite));

        // Now try to remove the read-only flag
        QVERIFY(setDefaultFilePermissions(&testFile, DefaultFilePermissions::NonExecutable));

        // Verify that the file can now be opened for both reading and writing
        QVERIFY(testFile.open(QIODevice::ReadWrite));
        testFile.close();

        QVERIFY(testFile.setPermissions(QFileDevice::ReadOwner | QFileDevice::ReadUser
            | QFileDevice::ReadGroup | QFileDevice::ReadOther));

        // Check that the behavior is same with 'Executable' argument
        QVERIFY(setDefaultFilePermissions(&testFile, DefaultFilePermissions::Executable));

        QVERIFY(testFile.open(QIODevice::ReadWrite));
        testFile.close();

        QVERIFY(testFile.remove());
#elif defined(Q_OS_UNIX)
        // Need to include the "user" flags here as they will be returned
        // by QFile::permissions(). Same as owner permissions of the file.
        QFlags<QFileDevice::Permission> permissions(QFileDevice::ReadOwner
            | QFileDevice::WriteOwner | QFileDevice::ReadUser | QFileDevice::WriteUser
            | QFileDevice::ReadGroup | QFileDevice::ReadOther);

        QFlags<QFileDevice::Permission> exePermissions(permissions | QFileDevice::ExeOwner
            | QFileDevice::ExeUser | QFileDevice::ExeGroup | QFileDevice::ExeOther);

        QString fileName = QInstaller::generateTemporaryFileName();
        QFile testFile(fileName);

        const QString message = "Target \"%1\" does not exists.";

        // Test non-existing file
        QTest::ignoreMessage(QtWarningMsg, qPrintable(message.arg(fileName)));
        QVERIFY(!setDefaultFilePermissions(fileName, DefaultFilePermissions::NonExecutable));

        QTest::ignoreMessage(QtWarningMsg, qPrintable(message.arg(testFile.fileName())));
        QVERIFY(!setDefaultFilePermissions(&testFile, DefaultFilePermissions::NonExecutable));

        QVERIFY(testFile.open(QIODevice::ReadWrite));
        QVERIFY(testFile.exists());
        testFile.close();

        // Test with file name
        QVERIFY(setDefaultFilePermissions(fileName, DefaultFilePermissions::NonExecutable));
        QCOMPARE(QFile().permissions(fileName), permissions);

        QVERIFY(setDefaultFilePermissions(fileName, DefaultFilePermissions::Executable));
        QCOMPARE(QFile().permissions(fileName), exePermissions);

        // Test with QFile object
        QVERIFY(setDefaultFilePermissions(&testFile, DefaultFilePermissions::NonExecutable));
        QCOMPARE(QFile().permissions(fileName), permissions);

        QVERIFY(setDefaultFilePermissions(&testFile, DefaultFilePermissions::Executable));
        QCOMPARE(QFile().permissions(fileName), exePermissions);

        // Test with directory path
        QString testDir = QDir().tempPath() + QLatin1String("/testDir");
        QVERIFY(QDir().mkdir(testDir));

        QVERIFY(setDefaultFilePermissions(testDir, DefaultFilePermissions::Executable));
        QCOMPARE(QFile().permissions(testDir), exePermissions);

        QVERIFY(QDir().rmdir(testDir));
        QVERIFY(testFile.remove());
#endif
    }
};

QTEST_MAIN(tst_fileutils)

#include "tst_fileutils.moc"
