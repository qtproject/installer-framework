/**************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
**************************************************************************/

#include "init.h"
#include "registerfiletypeoperation.h"
#include "packagemanagercore.h"

#include <QDir>
#include <QObject>
#include <QTest>
#include <QFile>
#include <QTextStream>
#include <QSettings>
#include "qsettingswrapper.h"

using namespace KDUpdater;
using namespace QInstaller;

class tst_registerfiletypeoperation : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QInstaller::init();
        QString randomString = "";
        const QString possible = "abcdefghijklmnopqrstuvwxyz0123456789";
        qsrand(QTime::currentTime().msec());
        for (int i = 0; i < 5; i++) {
            int index = qrand() % possible.length();
            QChar nextChar = possible.at(index);
            randomString.append(nextChar);

        }
        m_fileType = randomString;

        m_command = m_core.environmentVariable("SystemRoot") + "\\notepad.exe";
        m_progId = "QtProject.QtInstallerFramework." + m_fileType;

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

        const QString settingsPath = QString::fromLatin1("HKEY_CURRENT_USER\\Software\\Classes\\");
        QSettings settings(settingsPath, QSettings::NativeFormat);


        QVERIFY(op.testOperation());
        QVERIFY(op.performOperation());

        QString defaultKey = "."+m_fileType+ "/Default";
        QString openWithProgIdkey = "." + m_fileType + "/OpenWithProgIds/" +m_progId;
        QString shellKey = m_progId + "/shell/Open/Command/Default/";
        QString shellAppkey = "/Applications/" + m_progId + "/shell/Open/Command/Default/";

        QCOMPARE(settings.value(defaultKey).toString(), m_progId);
        QCOMPARE(settings.value(openWithProgIdkey).toString(), QString());
        QCOMPARE(settings.value(shellKey).toString(), m_command);
        QCOMPARE(settings.value(shellAppkey).toString(), m_command);

        QVERIFY(op.undoOperation());

        //Test that values have been removed after undo operation
        QCOMPARE(settings.value(defaultKey).toString(), QString());
        QCOMPARE(settings.value(openWithProgIdkey).toString(), QString());
        QCOMPARE(settings.value(shellKey).toString(), QString());
        QCOMPARE(settings.value(shellAppkey).toString(), QString());
    }

private:
    QString m_fileType;
    QString m_command;
    QString m_progId;
    PackageManagerCore m_core;
};

QTEST_MAIN(tst_registerfiletypeoperation)

#include "tst_registerfiletypeoperation.moc"
