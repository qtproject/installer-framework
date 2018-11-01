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
#include <init.h>
#include <packagemanagercore.h>
#include <consumeoutputoperation.h>
#include <qinstallerglobal.h>
#include <fileutils.h>
#include <errors.h>

#include <QObject>
#include <QTest>
#include <QProcess>
#include <QDir>
#include <QEventLoop>
#include <QDebug>

#define QUOTE_(x) #x
#define QUOTE(x) QUOTE_(x)

using namespace KDUpdater;
using namespace QInstaller;

class tst_consumeoutputoperationtest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // be aware that this would enable eating the qDebug output
        //QInstaller::init();

        m_fakeQtPath =  QDir::toNativeSeparators(qApp->applicationDirPath()) + QDir::separator()
            + "fakeQt" + QDir::separator();
        QVERIFY(QDir().mkpath(m_fakeQtPath + "bin"));
    }

    void testMissingArguments()
    {
        ConsumeOutputOperation operation(nullptr);

        QVERIFY(operation.testOperation());
        QVERIFY(!operation.performOperation());
        // undo does nothing so this should be true

        QVERIFY(operation.undoOperation());

        QCOMPARE(UpdateOperation::Error(operation.error()), UpdateOperation::InvalidArguments);
        //qDebug() << operation.errorString();
        QString compareString("Invalid arguments in ConsumeOutput: 0 arguments given, at least 2 "
                              "arguments expected in the form: <to be saved installer key name> "
                              "<executable> [argument1] [argument2] [...].");
        //qDebug() << compareString;
        QCOMPARE(operation.errorString(), compareString);
    }

    void testGetOutputFromQmake()
    {
        QString testOutput = getOutputFrom(QUOTE(QMAKE_BINARY), QStringList("-query"));

        ConsumeOutputOperation operation(&m_core);

        operation.setArguments(QStringList() << "testConsumeOutputKey" << QUOTE(QMAKE_BINARY) << "-query");
        QVERIFY2(operation.performOperation(), qPrintable(operation.errorString()));
        QCOMPARE(Operation::Error(operation.error()), Operation::NoError);

        QCOMPARE(m_core.value("testConsumeOutputKey"), testOutput);
    }

    void cleanupTestCase()
    {
        try {
            removeDirectory(m_fakeQtPath);
        } catch (const QInstaller::Error &error) {
            QFAIL(qPrintable(error.message()));
        }
    }

private:
    QString getOutputFrom(const QString &binary, const QStringList &arguments = QStringList())
    {
        QEventLoop loop;
        QProcess process;

        QObject::connect(&process, SIGNAL(finished(int,QProcess::ExitStatus)), &loop, SLOT(quit()));
        process.start(binary, arguments, QIODevice::ReadOnly);

        if (process.state() != QProcess::NotRunning)
            loop.exec();

        return process.readAllStandardOutput();
    }


    PackageManagerCore m_core;
    QString m_fakeQtPath;
};

QTEST_MAIN(tst_consumeoutputoperationtest)

#include "tst_consumeoutputoperationtest.moc"
