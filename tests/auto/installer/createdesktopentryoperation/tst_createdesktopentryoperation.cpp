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

#include <createdesktopentryoperation.h>

#include <packagemanagercore.h>
#include <binarycontent.h>
#include <settings.h>
#include <fileutils.h>
#include <init.h>
#include <component.h>

#include <QObject>
#include <QTest>
#include <QFile>

using namespace KDUpdater;
using namespace QInstaller;

class tst_createdesktopentryoperation : public QObject
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

        CreateDesktopEntryOperation *createDesktopEntryOp = nullptr;
        OperationList operations = core->componentByName("A")->operations();
        foreach (Operation *op, operations) {
            if (op->name() == QLatin1String("CreateDesktopEntry"))
                createDesktopEntryOp = dynamic_cast<CreateDesktopEntryOperation *>(op);
        }
        QVERIFY(createDesktopEntryOp);

        QString entryFileName = createDesktopEntryOp->absoluteFileName();
        QVERIFY(QFileInfo(entryFileName).exists());
        if (QFileInfo(createDesktopEntryOp->arguments().first()).isRelative()) {
            QStringList directories = QString::fromLocal8Bit(qgetenv("XDG_DATA_HOME"))
                .split(QLatin1Char(':'), Qt::SkipEmptyParts);
            // Default path if XDG_DATA_HOME is not set
            directories.append(QDir::home().absoluteFilePath(QLatin1String(".local/share")));
            // Default path if run as root
            directories.append(QLatin1String("/usr/local/share"));
            bool validPath = false;
            foreach (const QString &dir, directories) {
                // Desktop entry should be in one of the expected locations
                if (QFileInfo(entryFileName).absolutePath() == QDir(dir).absoluteFilePath("applications")) {
                    validPath = true;
                    break;
                }
            }
            QVERIFY(validPath);
        }
        core->setPackageManager();
        core->commitSessionOperations();
        core->uninstallComponentsSilently(QStringList() << "A");
        QVERIFY2(!QFileInfo(entryFileName).exists(), "Please make sure there "
            "does not exist a desktop entry with the same name.");

        QDir dir(installDir);
        QVERIFY(dir.removeRecursively());
        core->deleteLater();

    }

private slots:
    void initTestCase()
    {
        m_desktopEntryContents = "Type=Application\nName=Test\nVersion=1.0";
    }

    void testMissingArguments()
    {
        CreateDesktopEntryOperation op(nullptr);

        QVERIFY(op.testOperation());
        QVERIFY(!op.performOperation());

        QCOMPARE(UpdateOperation::Error(op.error()), UpdateOperation::InvalidArguments);
        QCOMPARE(op.errorString(), QString("Invalid arguments in CreateDesktopEntry: "
                                           "0 arguments given, exactly 2 arguments expected."));
    }

    void testCreateDesktopEntry_data()
    {
         QTest::addColumn<QString>("filename");
         QTest::newRow("relative") << "entry";
         QTest::newRow("absolute") << qApp->applicationDirPath() + "/entry";
    }

    void testCreateDesktopEntry()
    {
        QFETCH(QString, filename);

        CreateDesktopEntryOperation op(nullptr);
        op.setArguments(QStringList() << filename << m_desktopEntryContents);

        QVERIFY2(op.performOperation(), op.errorString().toLatin1());
        QVERIFY(QFileInfo().exists(op.absoluteFileName()));

        QVERIFY2(op.undoOperation(), op.errorString().toLatin1());
        QVERIFY(!QFileInfo().exists(op.absoluteFileName()));
    }

    void testBackupExistingEntry()
    {
        QString filename = qApp->applicationDirPath() + "/entry";

        CreateDesktopEntryOperation op(nullptr);
        op.setArguments(QStringList() << filename << m_desktopEntryContents);

        op.backup();
        QVERIFY(!op.hasValue("backupOfExistingDesktopEntry"));

        QFile existingEntry(filename);
        QVERIFY(existingEntry.open(QFileDevice::ReadWrite) && existingEntry.exists());
        existingEntry.close();

        op.backup();
        QVERIFY(op.hasValue("backupOfExistingDesktopEntry"));

        QVERIFY2(op.undoOperation(), op.errorString().toLatin1());
        QVERIFY(!QFileInfo().exists(op.value("backupOfExistingDesktopEntry").toString()));
        QVERIFY(QFileInfo().exists(filename));

        QVERIFY(QFile(filename).remove());
    }

    void testDesktopEntryFromScript()
    {
        installFromCLI(":///data/repository");
    }

    void testDesktopEntryFromXML()
    {
        installFromCLI(":///data/xmloperationrepository");
    }

private:
    QString m_desktopEntryContents;
};

QTEST_MAIN(tst_createdesktopentryoperation)

#include "tst_createdesktopentryoperation.moc"
