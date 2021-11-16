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

#include <lib7z_facade.h>
#include <lib7zarchive.h>
#include <fileutils.h>

#include <QDir>
#include <QObject>
#include <QTemporaryFile>
#include <QTest>

using namespace QInstaller;

class tst_lib7zarchive : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        Lib7z::initSevenZ();

        m_file.path = "valid";
        m_file.compressedSize = 836;
        m_file.uncompressedSize = 5242880;
        m_file.isDirectory = false;
        m_file.archiveIndex = QPoint(0, 0);
        m_file.utcTime = QDateTime(QDate::fromJulianDay(2456413), QTime(10, 50, 42), Qt::UTC);
    }

    void testIsSupportedArchive()
    {
        Lib7zArchive archive(":///data/valid.7z");
        QVERIFY(archive.open(QIODevice::ReadOnly));
        QCOMPARE(archive.isSupported(), true);
        archive.close();

        archive.setFilename(":///data/invalid.7z");
        QVERIFY(archive.open(QIODevice::ReadOnly));
        QCOMPARE(archive.isSupported(), false);
    }

    void testListArchive()
    {

        Lib7zArchive archive(":///data/valid.7z");
        QVERIFY(archive.open(QIODevice::ReadOnly));

        QVector<ArchiveEntry> files = archive.list();
        QCOMPARE(files.count(), 1);
        QCOMPARE(files.first(), m_file);
        archive.close();

        archive.setFilename(":///data/invalid.7z");
        QVERIFY(archive.open(QIODevice::ReadOnly));
        files = archive.list();

        QVERIFY(files.isEmpty());
        QCOMPARE(archive.errorString(), QString("Cannot open archive \":///data/invalid.7z\"."));
    }

    void testCreateArchive()
    {
        QString path1 = tempSourceFile("Source File 1.");
        QString path2 = tempSourceFile("Source File 2.");

        QString filename = generateTemporaryFileName();
        Lib7zArchive target(filename);
        QVERIFY(target.open(QIODevice::ReadWrite));
        QVERIFY(target.create(QStringList() << path1 << path2));
        QCOMPARE(target.list().count(), 2);
        target.close();
        QVERIFY(QFile::remove(filename));


        path1 = tempSourceFile(
            "Source File 1.",
            QDir::tempPath() + "/temp file with spaces.XXXXXX"
        );
        path2 = tempSourceFile(
            "Source File 2.",
            QDir::tempPath() + "/temp file with spaces.XXXXXX"
        );

        filename = QDir::tempPath() + "/target file with spaces.XXXXXX";
        target.setFilename(filename);
        QVERIFY(target.open(QIODevice::ReadWrite));
        QVERIFY(target.create(QStringList() << path1 << path2));
        QCOMPARE(target.list().count(), 2);
        target.close();
        QVERIFY(QFile::remove(filename));
    }

    void testExtractArchive()
    {
        Lib7zArchive source(":///data/valid.7z");
        QVERIFY(source.open(QIODevice::ReadOnly));

        QVERIFY(source.extract(QDir::tempPath()));
        QCOMPARE(QFile::exists(QDir::tempPath() + QString("/valid")), true);
        QVERIFY(QFile::remove(QDir::tempPath() + QString("/valid")));
    }

private:
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

QTEST_MAIN(tst_lib7zarchive)

#include "tst_lib7zarchive.moc"
