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

#include <registerfiletypeoperation.h>
#include <packagemanagercore.h>

#include <QDir>
#include <QObject>
#include <QTest>
#include <QFile>
#include <QTextStream>
#include <QSettings>
#include <QtGlobal>
#include <QRandomGenerator>

#include "qsettingswrapper.h"

using namespace KDUpdater;
using namespace QInstaller;

class tst_registerfiletypeoperation : public QObject
{
    Q_OBJECT

private:
    void verifySettings()
    {
        QCOMPARE(m_settings->value(m_defaultKey).toString(), m_progId);
        QCOMPARE(m_settings->value(m_openWithProgIdkey).toString(), QString());
        QCOMPARE(m_settings->value(m_shellKey).toString(), m_command);
        QCOMPARE(m_settings->value(m_shellAppkey).toString(), m_command);
    }

    void verifySettingsCleaned()
    {
         //Test that values have been removed after undo operation
        QCOMPARE(m_settings->value(m_defaultKey).toString(), QString());
        QCOMPARE(m_settings->value(m_openWithProgIdkey).toString(), QString());
        QCOMPARE(m_settings->value(m_shellKey).toString(), QString());
        QCOMPARE(m_settings->value(m_shellAppkey).toString(), QString());
    }

    void clearSettings()
    {
        m_settings->setValue(m_defaultKey, QString());
        m_settings->setValue(m_openWithProgIdkey, QString());
        m_settings->setValue(m_shellKey, QString());
        m_settings->setValue(m_shellAppkey, QString());
    }

private slots:
    void initTestCase()
    {
        QInstaller::init();
        QString randomString = "";
        const QString possible = "abcdefghijklmnopqrstuvwxyz0123456789";
        for (int i = 0; i < 5; i++) {
            int index = QRandomGenerator::global()->generate() % possible.length();
            QChar nextChar = possible.at(index);
            randomString.append(nextChar);

        }
        m_fileType = randomString;

        m_command = m_core.environmentVariable("SystemRoot") + "\\notepad.exe";
        m_progId = "QtProject.QtInstallerFramework." + m_fileType;
        qputenv("ifw_random_filetype", m_fileType.toUtf8());
        qputenv("ifw_random_programid", m_progId.toUtf8());

        const QString settingsPath = QString::fromLatin1("HKEY_CURRENT_USER\\Software\\Classes\\");
        m_settings = new QSettings(settingsPath, QSettings::NativeFormat);
        m_defaultKey = "." + m_fileType + "/Default";
        m_openWithProgIdkey = "." + m_fileType + "/OpenWithProgIds/" + m_progId;
        m_shellKey = m_progId + "/shell/Open/Command/Default/";
        m_shellAppkey = "/Applications/" + m_progId + "/shell/Open/Command/Default/";
    }

    void cleanupTestCase()
    {
        qunsetenv("ifw_random_filetype");
        qunsetenv("ifw_random_programid");
        delete m_settings;
    }

    void testMissingArguments()
    {
        RegisterFileTypeOperation op(&m_core);

        QVERIFY(op.testOperation());
        QVERIFY(!op.performOperation());

        QCOMPARE(UpdateOperation::Error(op.error()), UpdateOperation::InvalidArguments);
        QCOMPARE(op.errorString(), QString("Invalid arguments in RegisterFileType: 0 arguments given, "
                                      "2 to 5 arguments expected in the form: <extension> <command> [description [contentType [icon]]]."));

    }
    void testRegisterFileType()
    {
        RegisterFileTypeOperation op(&m_core);
        op.setArguments(QStringList() << m_fileType << m_command << "test filetype" <<
                                       "text/plain" << 0 << "ProgId="+m_progId);
        QVERIFY(op.testOperation());
        QVERIFY(op.performOperation());

        verifySettings();
        QVERIFY(op.undoOperation());
        verifySettingsCleaned();
    }

    void testRegisterFileTypeNoUndo()
    {
        RegisterFileTypeOperation op(&m_core);
        op.setArguments(QStringList() << m_fileType << m_command << "test filetype" <<
                                       "text/plain" << 0 << "ProgId="+m_progId << "UNDOOPERATION" << "");
        QVERIFY(op.testOperation());
        QVERIFY(op.performOperation());

        verifySettings();
        QVERIFY(op.undoOperation());
        verifySettings();

        //Clear so it does not pollute settings
        clearSettings();
    }

    void testPerformingFromCLI()
    {
        QString installDir = QInstaller::generateTemporaryFileName();
        QVERIFY(QDir().mkpath(installDir));
        PackageManagerCore *core = PackageManager::getPackageManagerWithInit
                (installDir, ":///data/repository");

        core->installDefaultComponentsSilently();
        verifySettings();

        core->commitSessionOperations();
        core->setPackageManager();
        core->uninstallComponentsSilently(QStringList() << "A");
        verifySettingsCleaned();

        QDir dir(installDir);
        QVERIFY(dir.removeRecursively());
        core->deleteLater();
    }

private:
    QString m_fileType;
    QString m_command;
    QString m_progId;
    PackageManagerCore m_core;
    QSettings *m_settings;
    QString m_defaultKey = "."+m_fileType+ "/Default";
    QString m_openWithProgIdkey = "." + m_fileType + "/OpenWithProgIds/" +m_progId;
    QString m_shellKey = m_progId + "/shell/Open/Command/Default/";
    QString m_shellAppkey = "/Applications/" + m_progId + "/shell/Open/Command/Default/";
};

QTEST_MAIN(tst_registerfiletypeoperation)

#include "tst_registerfiletypeoperation.moc"
