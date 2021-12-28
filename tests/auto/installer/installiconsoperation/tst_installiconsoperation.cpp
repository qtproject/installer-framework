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
#include "../shared/verifyinstaller.h"

#include <installiconsoperation.h>

#include <packagemanagercore.h>
#include <component.h>

#include <QTest>

using namespace KDUpdater;
using namespace QInstaller;

class tst_installiconsoperation : public QObject
{
    Q_OBJECT
private:
    void installFromCLI(const QString &repository)
    {
        QString installDir = QInstaller::generateTemporaryFileName();
        QVERIFY(QDir().mkpath(installDir));
        QScopedPointer<PackageManagerCore> core(PackageManager::getPackageManagerWithInit
                (installDir, repository));
        core->installDefaultComponentsSilently();

        InstallIconsOperation *installIconsOp = nullptr;
        OperationList operations = core->componentByName("A")->operations();
        foreach (Operation *op, operations) {
            if (op->name() == QLatin1String("InstallIcons"))
                installIconsOp = dynamic_cast<InstallIconsOperation *>(op);
        }
        QVERIFY(installIconsOp);

        // As the original directory containing icons will be deleted by the operation,
        // we will use a copy with the exact same contents.
        QFileInfo fakeSourceInfo(installDir + "/icons_copy/test");
        QMap<QString, QByteArray> fakeSourceMap;
        VerifyInstaller::addToFileMap(QDir(fakeSourceInfo.absoluteFilePath()), fakeSourceInfo, fakeSourceMap);

        QFileInfo destinationInfo(installIconsOp->value("directory").toString() + "/test");
        QMap<QString, QByteArray> destinationMap;
        VerifyInstaller::addToFileMap(QDir(destinationInfo.absoluteFilePath()), destinationInfo, destinationMap);

        QVERIFY(fakeSourceMap == destinationMap);

        core->setPackageManager();
        core->commitSessionOperations();
        core->uninstallComponentsSilently(QStringList() << "A");
        QVERIFY(!destinationInfo.exists());

        QDir dir(installDir);
        QVERIFY(dir.removeRecursively());
    }

private slots:
    void initTestCase()
    {
        m_testIconSourcePath = qApp->applicationDirPath() + "/icons";
        m_testIconSubdirectoryPath = m_testIconSourcePath + "/test";

        m_testIconFilePaths.append(m_testIconSubdirectoryPath
            + QDir::separator() + "vendor-icon1.png");
        m_testIconFilePaths.append(m_testIconSubdirectoryPath
            + QDir::separator() + "vendor-icon2.png");
        m_testIconFilePaths.append(m_testIconSubdirectoryPath
            + QDir::separator() + "vendor-icon3.png");
    }

    void testMissingArguments()
    {
        InstallIconsOperation op(nullptr);

        op.backup();
        QVERIFY(op.testOperation());
        QVERIFY(!op.performOperation());

        QCOMPARE(UpdateOperation::Error(op.error()), UpdateOperation::InvalidArguments);
        QCOMPARE(op.errorString(), QString("Invalid arguments in InstallIcons: "
                                           "0 arguments given, 1 or 2 arguments expected "
                                           "in the form: <source path> [vendor prefix]."));

        op.setArguments(QStringList() << "");
        QVERIFY(!op.performOperation());

        QCOMPARE(UpdateOperation::Error(op.error()), UpdateOperation::InvalidArguments);
        QCOMPARE(op.errorString(), QString("Invalid Argument: source directory must not be empty."));
    }

    void testInstallIconsFromScript()
    {
       installFromCLI(":///data/repository");
    }

    void testInstallIconsFromXML()
    {
       installFromCLI(":///data/xmloperationrepository");
    }

    void testInstallIconsWithUndo()
    {
        QDir testIconDir(m_testIconSourcePath);
        QVERIFY(testIconDir.mkpath(m_testIconSubdirectoryPath));

        // Populate source directory
        foreach (const QString &filePath, m_testIconFilePaths) {
            QFile file(filePath);
            QVERIFY(file.open(QFileDevice::ReadWrite));
            file.close();
        }

        PackageManagerCore *core = new PackageManagerCore();
        InstallIconsOperation op(core);
        op.setArguments(QStringList() << m_testIconSourcePath);

        QFileInfo sourceInfo(m_testIconSubdirectoryPath);
        QMap<QString, QByteArray> sourceMap;
        VerifyInstaller::addToFileMap(QDir(sourceInfo.absoluteFilePath()), sourceInfo, sourceMap);

        QVERIFY2(op.performOperation(), op.errorString().toLatin1());
        QVERIFY(!QFileInfo().exists(m_testIconSourcePath));

        QFileInfo destinationInfo(op.value("directory").toString() + "/test");
        QMap<QString, QByteArray> destinationMap;
        VerifyInstaller::addToFileMap(QDir(destinationInfo.absoluteFilePath()), destinationInfo, destinationMap);

        QVERIFY2(op.undoOperation(), op.errorString().toLatin1());
        QVERIFY(!QFileInfo().exists(op.value("directory").toString() + "/test"));
        QVERIFY(QFileInfo().exists(m_testIconSourcePath));

        QVERIFY(sourceMap == destinationMap);
        QVERIFY(testIconDir.removeRecursively());

        core->deleteLater();
    }

    void testChangeVendorPrefix()
    {
        QDir testIconDir(m_testIconSourcePath);
        QVERIFY(testIconDir.mkpath(m_testIconSubdirectoryPath));

        QFile file(m_testIconFilePaths.first());
        QVERIFY(file.open(QFileDevice::ReadWrite));
        file.close();

        PackageManagerCore *core = new PackageManagerCore();
        InstallIconsOperation op(core);
        op.setArguments(QStringList() << m_testIconSourcePath << "testVendor");

        QVERIFY2(op.performOperation(), op.errorString().toLatin1());
        QVERIFY(!QFileInfo().exists(m_testIconSourcePath));
        QVERIFY(QFileInfo().exists(op.value("directory").toString() + "/test/testVendor-icon1.png"));

        QVERIFY2(op.undoOperation(), op.errorString().toLatin1());
        QVERIFY(!QFileInfo().exists(op.value("directory").toString() + "/test"));
        QVERIFY(QFileInfo().exists(m_testIconSourcePath));

        QVERIFY(testIconDir.removeRecursively());

        core->deleteLater();
    }

    void testValidTargetDirectory()
    {
        QDir testIconDir(m_testIconSourcePath);
        QVERIFY(testIconDir.mkpath(m_testIconSubdirectoryPath));

        PackageManagerCore *core = new PackageManagerCore();
        InstallIconsOperation op(core);
        op.setArguments(QStringList() << m_testIconSourcePath);

        QVERIFY2(op.performOperation(), op.errorString().toLatin1());
        QVERIFY(!QFileInfo().exists(m_testIconSourcePath));

        QString targetIconsDirectory = op.value("directory").toString();
        QVERIFY(QFileInfo(targetIconsDirectory).exists());
        QStringList directories = QString::fromLocal8Bit(qgetenv("XDG_DATA_HOME"))
            .split(QLatin1Char(':'), Qt::SkipEmptyParts);
        // Default path if XDG_DATA_HOME is not set
        directories.append(QDir::home().absoluteFilePath(QLatin1String(".local/share")));
        // Default path if run as root
        directories.append(QLatin1String("/usr/local/share"));
        bool validPath = false;
        foreach (const QString &dir, directories) {
            // Icon directory should be one of the expected locations
            if (targetIconsDirectory == QDir(dir).absoluteFilePath("icons")) {
                validPath = true;
                break;
            }
        }
        QVERIFY(validPath);

        QVERIFY2(op.undoOperation(), op.errorString().toLatin1());
        QVERIFY(!QFileInfo().exists(op.value("directory").toString() + "/test"));

        QVERIFY(testIconDir.removeRecursively());
        core->deleteLater();
    }

private:
    QString m_testIconSourcePath;
    QString m_testIconSubdirectoryPath;
    QStringList m_testIconFilePaths;
};

QTEST_MAIN(tst_installiconsoperation)

#include "tst_installiconsoperation.moc"
