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
#include <packagemanagercore.h>

#include <QFile>
#include <QTest>

using namespace KDUpdater;
using namespace QInstaller;

class tst_appendfileoperation : public QObject
{
    Q_OBJECT

private:
    void installFromCLI(const QString &repository)
    {
        QString installDir = QInstaller::generateTemporaryFileName();
        QVERIFY(QDir().mkpath(installDir));
        PackageManagerCore *core = PackageManager::getPackageManagerWithInit
                (installDir, repository);
        core->installDefaultComponentsSilently();

        QFile file(installDir + QDir::separator() + "A.txt");
        QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
        QTextStream stream(&file);
        QCOMPARE(stream.readAll(), QLatin1String("Appended text: lorem ipsum"));
        file.close();

        core->setPackageManager();
        core->commitSessionOperations();
        // We cannot check the file contents here as it will be deleted on
        // undo Extract, but at least check that the uninstallation succeeds.
        QCOMPARE(PackageManagerCore::Success, core->uninstallComponentsSilently
                 (QStringList()<< "A"));

        QDir dir(installDir);
        QVERIFY(dir.removeRecursively());
        core->deleteLater();
    }

private slots:
    void initTestCase()
    {
        m_testFilePath = qApp->applicationDirPath() + QDir::toNativeSeparators("/test");
    }

    void testMissingArguments()
    {
        AppendFileOperation op;

        QVERIFY(op.testOperation());
        QVERIFY(!op.performOperation());

        QCOMPARE(UpdateOperation::Error(op.error()), UpdateOperation::InvalidArguments);
        QCOMPARE(op.errorString(), QString("Invalid arguments in AppendFile: "
                                           "0 arguments given, exactly 2 arguments expected."));

        op.setArguments(QStringList() << "" << "");
        QTest::ignoreMessage(QtWarningMsg, "QFSFileEngine::open: No file name specified");
        QTest::ignoreMessage(QtWarningMsg, "QFile::rename: Empty or null file name");
        QVERIFY(!op.performOperation());

        QCOMPARE(UpdateOperation::Error(op.error()), UpdateOperation::UserDefinedError);
        QCOMPARE(op.errorString(), QString("Cannot open file \"\" for writing: No file name specified"));
    }

    void testAppendText_data()
    {
        QTest::addColumn<QString>("source");
        QTest::addColumn<QString>("append");
        QTest::addColumn<QString>("expected");
        QTest::newRow("newline") << "Line1\nLine2\nLine3\n" << "AppendedText"
            << "Line1\nLine2\nLine3\nAppendedText";
        QTest::newRow("no newline") << "Lorem ipsum " << "dolore sit amet"
            << "Lorem ipsum dolore sit amet";
    }

    void testAppendText()
    {
        QFETCH(QString, source);
        QFETCH(QString, append);
        QFETCH(QString, expected);

        QFile file(m_testFilePath);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));

        QTextStream stream(&file);
        stream << source << Qt::flush;
        file.close();

        AppendFileOperation op;
        op.setArguments(QStringList() << m_testFilePath << append);

        op.backup();
        QVERIFY(QFileInfo(op.value("backupOfFile").toString()).exists());

        QVERIFY2(op.performOperation(), op.errorString().toLatin1());
        QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
        QCOMPARE(stream.readAll(), expected);
        file.close();

        QVERIFY2(op.undoOperation(), op.errorString().toLatin1());
        QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
        QCOMPARE(stream.readAll(), source);
        file.close();

        QVERIFY(file.remove());
    }

    void testApppendFromScript()
    {
        installFromCLI(":///data/repository");
    }

    void testAppendFromComponentXML()
    {
        installFromCLI(":///data/xmloperationrepository");
    }

private:
    QString m_testFilePath;
};

QTEST_MAIN(tst_appendfileoperation)

#include "tst_appendfileoperation.moc"
