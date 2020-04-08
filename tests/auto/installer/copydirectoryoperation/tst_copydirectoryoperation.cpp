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

#include <fileutils.h>
#include <copydirectoryoperation.h>
#include <binarycontent.h>
#include <packagemanagercore.h>
#include <settings.h>
#include <init.h>

#include <QObject>
#include <QDir>
#include <QFile>
#include <QCryptographicHash>
#include <QTest>

using namespace KDUpdater;
using namespace QInstaller;

class tst_copydirectoryoperation : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        m_sourcePath = qApp->applicationDirPath() + QDir::toNativeSeparators("/source/");
        m_destinationPath = generateTemporaryFileName() + QDir::toNativeSeparators("_dest/");
    }

    void init()
    {
        QDir sourceDir;
        QVERIFY(sourceDir.mkpath(m_sourcePath));

        QDir destinationDir;
        QVERIFY(destinationDir.mkpath(m_destinationPath));
    }

    void cleanup()
    {
        QVERIFY(QDir(m_sourcePath).removeRecursively());
        QVERIFY(QDir(m_destinationPath).removeRecursively());
    }

    void testInvalidArguments()
    {
        CopyDirectoryOperation op(nullptr);

        QVERIFY(op.testOperation());
        op.backup();

        QVERIFY(!op.performOperation());

        QCOMPARE(UpdateOperation::Error(op.error()), UpdateOperation::InvalidArguments);
        QCOMPARE(op.errorString(), QString("Invalid arguments in CopyDirectory: "
                                           "0 arguments given, 2 or 3 arguments expected in the form: "
                                           "<source> <target> [\"forceOverwrite\"]."));

        op.setArguments(QStringList() << "" << "" << "overwrite");
        QVERIFY(!op.performOperation());

        QCOMPARE(UpdateOperation::Error(op.error()), UpdateOperation::InvalidArguments);
        QCOMPARE(op.errorString(), QString("Invalid argument in CopyDirectory: "
                                           "Third argument needs to be forceOverwrite, "
                                           "if specified."));

        op.setArguments(QStringList() << "" << "");
        QVERIFY(!op.performOperation());

        QCOMPARE(UpdateOperation::Error(op.error()), UpdateOperation::InvalidArguments);
        QCOMPARE(op.errorString(), QString("Invalid argument in CopyDirectory: "
                                           "Directory \"\" is invalid."));
    }

    void testCopyDirectoryWithUndo()
    {
        QStringList fileEntries;
        fileEntries << "file1" << "file2" << ".hidden1" << ".hidden2";

        // Populate source directory
        foreach (const QString &entry, fileEntries) {
            QFile file(m_sourcePath + entry);
            QVERIFY(file.open(QFileDevice::ReadWrite));
            file.close();
        }
        CopyDirectoryOperation op(nullptr);

        op.setArguments(QStringList() << m_sourcePath << m_destinationPath);
        QVERIFY2(op.performOperation(), op.errorString().toLatin1());

        foreach (const QString &entry, fileEntries)
            QVERIFY(QFile(m_destinationPath + entry).exists());

        QVERIFY2(op.undoOperation(), op.errorString().toLatin1());
        // Undo will delete the empty destination directory here
        QVERIFY(!QFileInfo(m_destinationPath).exists());
    }

    void testCopyDirectoryOverwrite()
    {
        QFile file(m_sourcePath + "file");
        QVERIFY(file.open(QFileDevice::ReadWrite));
        file.close();

        file.setFileName(m_destinationPath + "file");
        QVERIFY(file.open(QFileDevice::ReadWrite));
        file.close();

        CopyDirectoryOperation op(nullptr);

        op.setArguments(QStringList() << m_sourcePath << m_destinationPath);
        QVERIFY(!op.performOperation());

        QCOMPARE(UpdateOperation::Error(op.error()), UpdateOperation::UserDefinedError);

        op.setArguments(QStringList() << m_sourcePath << m_destinationPath << "forceOverwrite");
        QVERIFY2(op.performOperation(), op.errorString().toLatin1());
    }

    void testPerformingFromCLI()
    {
        QInstaller::init(); //This will eat debug output
        PackageManagerCore *core = new PackageManagerCore(BinaryContent::MagicInstallerMarker, QList<OperationBlob> ());
        core->setAllowedRunningProcesses(QStringList() << QCoreApplication::applicationFilePath());
        QSet<Repository> repoList;
        Repository repo = Repository::fromUserInput(":///data/repository");
        repoList.insert(repo);
        core->settings().setDefaultRepositories(repoList);

        QString installDir = QInstaller::generateTemporaryFileName();
        QDir().mkpath(installDir);
        core->setValue(scTargetDir, installDir);
        core->installDefaultComponentsSilently();

        // Matches path in component install script
        QFileInfo targetInfo(installDir + QDir::toNativeSeparators("/directory"));
        QMap<QString, QByteArray> targetMap;
        addToFileMap(QDir(targetInfo.absoluteFilePath()), targetInfo, targetMap);

        QFileInfo destinationInfo(installDir + QDir::toNativeSeparators("/destination/directory"));
        QMap<QString, QByteArray> destinationMap;
        addToFileMap(QDir(destinationInfo.absoluteFilePath()), destinationInfo, destinationMap);

        QVERIFY(targetMap == destinationMap);

        core->setPackageManager();
        core->commitSessionOperations();
        core->uninstallComponentsSilently(QStringList() << "A");
        QVERIFY(!destinationInfo.exists());

        QDir dir(installDir);
        QVERIFY(dir.removeRecursively());
        core->deleteLater();
    }

private:
    void addToFileMap(const QDir &baseDir, const QFileInfo &fileInfo, QMap<QString, QByteArray> &map)
    {
        QDir directory(fileInfo.absoluteFilePath());
        directory.setFilter(QDir::NoDotAndDotDot | QDir::NoSymLinks | QDir::AllDirs | QDir::Files);
        QFileInfoList fileInfoList = directory.entryInfoList();

        foreach (const QFileInfo &info, fileInfoList) {
            if (info.isDir()) {
                map.insert(baseDir.relativeFilePath(info.filePath()), QByteArray());
                addToFileMap(baseDir, info, map);
            } else {
                QCryptographicHash hash(QCryptographicHash::Sha1);
                QFile file(info.absoluteFilePath());
                QVERIFY(file.open(QIODevice::ReadOnly));
                QVERIFY(hash.addData(&file));
                map.insert(baseDir.relativeFilePath(info.filePath()), hash.result().toHex());
                file.close();
            }
        }
    }

    QString m_sourcePath;
    QString m_destinationPath;
};

QTEST_MAIN(tst_copydirectoryoperation)

#include "tst_copydirectoryoperation.moc"
