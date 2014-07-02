/**************************************************************************
**
** Copyright (C) 2012-2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
**************************************************************************/
#include <init.h>
#include <packagemanagercore.h>
#include <consumeoutputoperation.h>
#include <qinstallerglobal.h>
#include <fileutils.h>
#include <qtpatchoperation.h>
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
#if defined(Q_OS_WIN)
        m_binSuffix = ".exe";
#endif
        // be aware that this would enable eating the qDebug output
        //QInstaller::init();
        // if we don't use the init method we need to register the in QtPatchOperation used
        // patch_file_lists manually
#if defined(QT_STATIC)
        Q_INIT_RESOURCE(patch_file_lists);
#endif

        m_fakeQtPath =  QDir::toNativeSeparators(qApp->applicationDirPath()) + QDir::separator()
            + "fakeQt" + QDir::separator();
        QVERIFY(QDir().mkpath(m_fakeQtPath + "bin"));

        QFile orgQmake(QUOTE(QMAKE_BINARY));
        // use lrelease filename which will be patched by the QtPatch operation, but it hides that this
        // fake Qt contains a qmake which would be used to get the patch paths (instead of using the qmake
        // output we want to use our installer value)
        m_binaryInFakeQt = m_fakeQtPath + "bin" + QDir::separator() + "lrelease" + m_binSuffix;

        QVERIFY2(orgQmake.copy(m_binaryInFakeQt), qPrintable(orgQmake.errorString()));
    }

    void testMissingArguments()
    {
        ConsumeOutputOperation operation;

        QVERIFY(operation.testOperation());
        QVERIFY(!operation.performOperation());
        // undo does nothing so this should be true

        QVERIFY(operation.undoOperation());

        QCOMPARE(UpdateOperation::Error(operation.error()), UpdateOperation::InvalidArguments);
        //qDebug() << operation.errorString();
        QString compareString("Invalid arguments in ConsumeOutput: 0 arguments given, at least 2 "
                    "expected(<to be saved installer key name>, <executable>, [argument1], [argument2], ...).");
        //qDebug() << compareString;
        QCOMPARE(operation.errorString(), compareString);
    }

    void testGetOutputFromQmake()
    {
        QString testOutput = getOutputFrom(QUOTE(QMAKE_BINARY), QStringList("-query"));

        ConsumeOutputOperation operation;
        operation.setValue(QLatin1String("installer"), QVariant::fromValue(&m_core));

        operation.setArguments(QStringList() << "testConsumeOutputKey" << QUOTE(QMAKE_BINARY) << "-query");
        QVERIFY2(operation.performOperation(), qPrintable(operation.errorString()));
        QCOMPARE(Operation::Error(operation.error()), Operation::NoError);

        QCOMPARE(m_core.value("testConsumeOutputKey"), testOutput);
    }

    void testPatchFakeQt()
    {
        return;
#if defined(Q_OS_WIN)
        QString patchType = "windows";
#elif defined(Q_OS_OSX)
        QString patchType = "mac";
#else
        QString patchType = "linux";
#endif

        QtPatchOperation patchOperation;
        patchOperation.setValue(QLatin1String("installer"), QVariant::fromValue(&m_core));

        // testConsumeOutputKey comes from testGetOutputFromQmake() before and it can not use qmake,
        // because it does not exist in the m_fakeQtPath
        patchOperation.setArguments(QStringList() << "QmakeOutputInstallerKey=testConsumeOutputKey"
            << patchType << m_fakeQtPath);
        QVERIFY2(patchOperation.performOperation(), qPrintable(patchOperation.errorString()));

        QString patchedBinaryOutput = getOutputFrom(m_binaryInFakeQt, QStringList("-query"));
        QVERIFY(getOutputFrom(QUOTE(QMAKE_BINARY), QStringList("-query")) != patchedBinaryOutput);
        QVERIFY(patchedBinaryOutput.contains(m_fakeQtPath));
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

        QObject::connect(&process, SIGNAL(finished(int, QProcess::ExitStatus)), &loop, SLOT(quit()));
        process.start(binary, arguments, QIODevice::ReadOnly);

        if (process.state() != QProcess::NotRunning)
            loop.exec();

        return process.readAllStandardOutput();
    }


    PackageManagerCore m_core;
    QString m_fakeQtPath;
    QString m_binaryInFakeQt;
    QString m_binSuffix;
};

QTEST_MAIN(tst_consumeoutputoperationtest)

#include "tst_consumeoutputoperationtest.moc"
