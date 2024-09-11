/**************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
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

#include "../shared/verifyinstaller.h"

#include <libarchivearchive.h>
#include <fileutils.h>

#include <QDir>
#include <QObject>
#include <QTemporaryFile>
#include <QTest>

using namespace QInstaller;

class tst_libarchivearchive : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        m_file.path = "valid";
        m_file.permissions_mode = 0644;
        m_file.compressedSize = 0; // unused
        m_file.uncompressedSize = 5242880;
        m_file.isDirectory = false;
        m_file.archiveIndex = QPoint(0, 0);
        m_file.utcTime = QDateTime(QDate::fromJulianDay(2456413), QTime(10, 50, 42), Qt::UTC);
    }

    void testIsSupportedArchive_data()
    {
        archiveFilenamesTestData();
    }

    void testIsSupportedArchive()
    {
        QFETCH(QString, filename);

        LibArchiveArchive archive(filename);
        QVERIFY(archive.open(QIODevice::ReadOnly));
        QCOMPARE(archive.isSupported(), true);
    }

    void testListArchive_data()
    {
        archiveFilenamesTestData();
    }

    void testListArchive()
    {
        QFETCH(QString, filename);

        LibArchiveArchive archive(filename);
        QVERIFY(archive.open(QIODevice::ReadOnly));

        QVector<ArchiveEntry> files = archive.list();
        QCOMPARE(files.count(), 1);
        QCOMPARE(files.first(), m_file);
    }

    void testCreateArchive_data()
    {
        archiveSuffixesTestData();
    }

    void testCreateArchive()
    {
        QFETCH(QString, suffix);

        const QString path1 = tempSourceFile("Source File 1.");
        const QString path2 = tempSourceFile("Source File 2.");

        const QString filename = generateTemporaryFileName() + suffix;
        LibArchiveArchive target(filename);
        QVERIFY(target.open(QIODevice::WriteOnly));
        QVERIFY(target.create(QStringList() << path1 << path2));
        target.close();
        QVERIFY(target.open(QIODevice::ReadOnly));
        QCOMPARE(target.list().count(), 2);
        target.close();
        QVERIFY(QFile(filename).remove());
    }

    void testCreateArchiveWithGlobPattern_data()
    {
        archiveSuffixesTestData();
    }

    void testCreateArchiveWithGlobPattern()
    {
        QFETCH(QString, suffix);

        const QString baseDir(generateTemporaryFileName(QDir::tempPath() + "/tst_libarchivearchive.XXXXXX"));
        QVERIFY(QDir().mkpath(baseDir));

        const QString path1 = tempSourceFile(
            "Source File 1.",
            baseDir + "/file.XXXXXX"
        );
        const QString path2 = tempSourceFile(
            "Source File 2.",
            baseDir + "/file.XXXXXX"
        );

        const QString filename = generateTemporaryFileName() + suffix;
        LibArchiveArchive target(filename);
        QVERIFY(target.open(QIODevice::WriteOnly));
        QVERIFY(target.create(QStringList() << baseDir + "/*"));
        target.close();
        QVERIFY(target.open(QIODevice::ReadOnly));
        QCOMPARE(target.list().count(), 2);
        target.close();

        QVERIFY(QFile(filename).remove());
        QVERIFY(QDir(baseDir).removeRecursively());
    }

    void testCreateArchiveWithSpaces_data()
    {
        archiveSuffixesTestData();
    }

    void testCreateArchiveWithSpaces()
    {
        QFETCH(QString, suffix);

        const QString path1 = tempSourceFile(
            "Source File 1.",
            QDir::tempPath() + "/temp file with spaces.XXXXXX"
        );
        const QString path2 = tempSourceFile(
            "Source File 2.",
            QDir::tempPath() + "/temp file with spaces.XXXXXX"
        );

        const QString filename = QDir::tempPath() + "/target file with spaces" + suffix;
        LibArchiveArchive target(filename);
        target.setFilename(filename);
        QVERIFY(target.open(QIODevice::WriteOnly));
        QVERIFY(target.create(QStringList() << path1 << path2));
        target.close();
        QVERIFY(target.open(QIODevice::ReadOnly));
        QCOMPARE(target.list().count(), 2);
        target.close();
        QVERIFY(QFile(filename).remove());
    }

    void testExtractArchive_data()
    {
        archiveFilenamesTestData();
    }

    void testExtractArchive()
    {
        QFETCH(QString, filename);

        LibArchiveArchive source(filename);
        QVERIFY(source.open(QIODevice::ReadOnly));

        QVERIFY(source.extract(QDir::tempPath()));
        QCOMPARE(QFile::exists(QDir::tempPath() + QString("/valid")), true);
        QVERIFY(QFile(QDir::tempPath() + QString("/valid")).remove());
    }

    void testCreateExtractWithSymlink_data()
    {
        archiveSuffixesTestData();
    }

    void testCreateExtractWithSymlink()
    {
        QFETCH(QString, suffix);

        const QString workingDir = generateTemporaryFileName() + "/";
        const QString archiveName = workingDir + "archive" + suffix;
        const QString targetName = workingDir + "target/";
#ifdef Q_OS_WIN
        const QString linkName = workingDir + "link.lnk";
#else
        const QString linkName = workingDir + "link";
#endif

        QVERIFY(QDir().mkpath(targetName));

        QFile source(workingDir + "file");
        QVERIFY(source.open(QIODevice::ReadWrite));
        QVERIFY(source.write("Source File"));

        // Creates a shortcut on Windows, a symbolic link on Unix
        QVERIFY(QFile::link(source.fileName(), linkName));

        LibArchiveArchive archive(archiveName);
        QVERIFY(archive.open(QIODevice::WriteOnly));
        QVERIFY(archive.create(QStringList() << source.fileName() << linkName));
        QVERIFY(QFileInfo::exists(archiveName));
        archive.close();

        QVERIFY(archive.open(QIODevice::ReadOnly));
        QVERIFY(archive.extract(targetName));
        const QString sourceFilename = QFileInfo(source.fileName()).fileName();
        const QString linkFilename = QFileInfo(linkName).fileName();
        QVERIFY(QFileInfo::exists(targetName + sourceFilename));
        QVERIFY(QFileInfo::exists(targetName + linkFilename));

        VerifyInstaller::verifyFileContent(targetName + sourceFilename, source.readAll());
        const QString sourceFilePath = workingDir + sourceFilename;
        QCOMPARE(QFile::symLinkTarget(targetName + linkFilename), sourceFilePath);

        archive.close();

        QVERIFY(source.remove());
        QVERIFY(QFile::remove(archiveName));
        QVERIFY(QFile::remove(linkName));
        QVERIFY(QFile::remove(targetName + sourceFilename));
        QVERIFY(QFile::remove(targetName + linkFilename));

        removeDirectory(targetName, true);
        removeDirectory(workingDir, true);
    }

    void testCreateExtractWithUnicodePaths_data()
    {
        archiveSuffixesTestData();
    }

    void testCreateExtractWithUnicodePaths()
    {
        QFETCH(QString, suffix);

        const QString targetName = generateTemporaryFileName() + QDir::separator();
        const QString archiveName = QDir::tempPath() + "/test_archive" + suffix;

        const QString path1 = tempSourceFile(
            "Source File 1.",
            QDir::tempPath() + QString::fromUtf8("/测试文件.XXXXXX")
        );
        const QString path2 = tempSourceFile(
            "Source File 2.",
            QDir::tempPath() + QString::fromUtf8("/тестовый файл.XXXXXX")
        );
        const QString path3 = tempSourceFile(
            "Source File 3.",
            QDir::tempPath() + QString::fromUtf8("/ملف الاختبار.XXXXXX")
        );

        LibArchiveArchive archive(archiveName);
        archive.setFilename(archiveName);
        QVERIFY(archive.open(QIODevice::WriteOnly));
        QVERIFY(archive.create(QStringList() << path1 << path2 << path3));
        archive.close();
        QVERIFY(archive.open(QIODevice::ReadOnly));
        QCOMPARE(archive.list().count(), 3);

        QVERIFY(archive.extract(targetName));

        const QString targetPath1 = targetName + QFileInfo(path1).fileName();
        const QString targetPath2 = targetName + QFileInfo(path2).fileName();
        const QString targetPath3 = targetName + QFileInfo(path3).fileName();

        QVERIFY(QFileInfo::exists(targetPath1));
        QVERIFY(QFileInfo::exists(targetPath2));
        QVERIFY(QFileInfo::exists(targetPath3));

        archive.close();
        QVERIFY(QFile::remove(archiveName));
        QVERIFY(QDir(targetName).removeRecursively());
    }

private:
    void archiveFilenamesTestData()
    {
        QTest::addColumn<QString>("filename");
        QTest::newRow("ZIP archive") << ":///data/valid.zip";
        QTest::newRow("gzip compressed tar archive") << ":///data/valid.tar.gz";
        QTest::newRow("bzip2 compressed tar archive") << ":///data/valid.tar.bz2";
        QTest::newRow("xz compressed tar archive") << ":///data/valid.tar.xz";
        QTest::newRow("7zip archive") << ":///data/valid.7z";
        QTest::newRow("QBSP archive (7z)") << ":///data/valid.qbsp";
    }

    void archiveSuffixesTestData()
    {
        QTest::addColumn<QString>("suffix");
        QTest::newRow("ZIP archive") << ".zip";
        QTest::newRow("uncompressed tar archive") << ".tar";
        QTest::newRow("gzip compressed tar archive") << ".tar.gz";
        QTest::newRow("bzip2 compressed tar archive") << ".tar.bz2";
        QTest::newRow("xz compressed tar archive") << ".tar.xz";
        QTest::newRow("7z archive") << ".7z";
        QTest::newRow("QBSP archive") << ".qbsp";
    }

    QString tempSourceFile(const QByteArray &data, const QString &templateName = QString())
    {
        QTemporaryFile source;
        if (!templateName.isEmpty()) {
            source.setFileTemplate(templateName);
        }
        source.open();
        source.write(data);
        source.setAutoRemove(false);
        return source.fileName();
    }

private:
    ArchiveEntry m_file;
};

QTEST_MAIN(tst_libarchivearchive)

#include "tst_libarchivearchive.moc"
