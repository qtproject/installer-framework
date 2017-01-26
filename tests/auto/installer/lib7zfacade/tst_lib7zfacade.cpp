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

#include "init.h"
#include "lib7z_facade.h"

#include <QDir>
#include <QObject>
#include <QTemporaryFile>
#include <QTest>

class tst_lib7zfacade : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QInstaller::init();

        m_file.path = "valid";
        m_file.permissions = 0;
        m_file.compressedSize = 836;
        m_file.uncompressedSize = 5242880;
        m_file.isDirectory = false;
        m_file.archiveIndex = QPoint(0, 0);
        m_file.mtime = QDateTime(QDate::fromJulianDay(2456413), QTime(12, 50, 42));
    }

    void testIsSupportedArchive()
    {
        QCOMPARE(Lib7z::isSupportedArchive(":///data/valid.7z"), true);
        QCOMPARE(Lib7z::isSupportedArchive(":///data/invalid.7z"), false);

        {
            QFile file(":///data/valid.7z");
            QVERIFY(file.open(QIODevice::ReadOnly));
            QCOMPARE(Lib7z::isSupportedArchive(&file), true);
        }

        {
            QFile file(":///data/invalid.7z");
            QVERIFY(file.open(QIODevice::ReadOnly));
            QCOMPARE(Lib7z::isSupportedArchive(&file), false);
        }
    }

    void testListArchive()
    {
        // TODO: this should work without scope, there's a bug in Lib7z::OpenArchiveInfo
        //       caused by the fact that we keep a pointer to the device, not the devices target
        {
            QFile file(":///data/valid.7z");
            QVERIFY(file.open(QIODevice::ReadOnly));

            QVector<Lib7z::File> files = Lib7z::listArchive(&file);
            QCOMPARE(files.count(), 1);
#ifdef Q_OS_UNIX
            QSKIP("This test requires the time handling to be repaired first.");
#endif
            QCOMPARE(files.first(), m_file);
        }

        {
            try {
                QFile file(":///data/invalid.7z");
                QVERIFY(file.open(QIODevice::ReadOnly));
                QVector<Lib7z::File> files = Lib7z::listArchive(&file);
            } catch (const Lib7z::SevenZipException& e) {
                QCOMPARE(e.message(), QString("Could not open archive"));
            } catch (...) {
                QFAIL("Unexpected error during list archive!");
            }
        }
    }

    void testCreateArchive()
    {
        QTemporaryFile target;
        QVERIFY(target.open());

        try {
            // TODO: we do not get any information about success
            Lib7z::createArchive(&target, QStringList() << ":///data/invalid.7z");
        } catch (const Lib7z::SevenZipException& e) {
            QFAIL(e.message().toUtf8());
        } catch (...) {
            QFAIL("Unexpected error during create archive!");
        }
    }

    void testExtractArchive()
    {
        QFile source(":///data/valid.7z");
        QVERIFY(source.open(QIODevice::ReadOnly));

        try {
            // TODO: we do not get any information about success
            Lib7z::extractArchive(&source, QDir::tempPath());
        } catch (const Lib7z::SevenZipException& e) {
            QFAIL(e.message().toUtf8());
        } catch (...) {
            QFAIL("Unexpected error during extract archive!");
        }
    }

    void testExtractFileFromArchive()
    {
        QFile source(":///data/valid.7z");
        QVERIFY(source.open(QIODevice::ReadOnly));

        QTemporaryFile target;
        QVERIFY(target.open());

        try {
            // TODO: we do not get any information about success
            Lib7z::extractFileFromArchive(&source, m_file, &target);
        } catch (const Lib7z::SevenZipException& e) {
            QFAIL(e.message().toUtf8());
        } catch (...) {
            QFAIL("Unexpected error during extract file from archive!");
        }
    }

    void testExtractFileFromArchive2()
    {
        QFile source(":///data/valid.7z");
        QVERIFY(source.open(QIODevice::ReadOnly));

        try {
            // TODO: we do not get any information about success
            Lib7z::extractFileFromArchive(&source, m_file, QDir::tempPath());
        } catch (const Lib7z::SevenZipException& e) {
            QFAIL(e.message().toUtf8());
        } catch (...) {
            QFAIL("Unexpected error during extract file from archive!");
        }
    }

private:
    Lib7z::File m_file;
};

QTEST_MAIN(tst_lib7zfacade)

#include "tst_lib7zfacade.moc"
