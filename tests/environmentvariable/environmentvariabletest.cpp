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

#include "environmentvariabletest.h"
#include "environmentvariablesoperation.h"

#include "init.h"

#include <kdupdaterapplication.h>

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QStack>
#include <QSettings>

EnvironmentVariableTest::EnvironmentVariableTest()
{
    QInstaller::init();
}

void EnvironmentVariableTest::testPersistentNonSystem()
{
#ifndef Q_OS_WIN
    QSKIP("This operation only works on Windows");
#endif
    KDUpdater::Application app;
    QString key = QLatin1String("IFW_TestKey");
    QString value = QLatin1String("IFW_TestValue");
    QInstaller::EnvironmentVariableOperation op;
    op.setArguments( QStringList() << key
                    << value
                    << QLatin1String("true")
                    << QLatin1String("false"));
    const bool ok = op.performOperation();

    QVERIFY2(ok, qPrintable(op.errorString()));

    // Verify now...
    QSettings settings(QLatin1String("HKEY_CURRENT_USER\\Environment"), QSettings::NativeFormat);
    QVERIFY(value == settings.value(key).toString());

    // Remove the setting
    QEXPECT_FAIL("", "Undo Operation not implemented yet", Continue);
    QVERIFY(op.undoOperation());

    //QVERIFY(settings.value(key).toString().isEmpty());
    settings.remove(key);
}

void EnvironmentVariableTest::testNonPersistentNonSystem()
{
#ifndef Q_OS_WIN
    QSKIP("This operation only works on Windows");
#endif
    KDUpdater::Application app;
    QString key = QLatin1String("IFW_TestKey");
    QString value = QLatin1String("IFW_TestValue");
    QInstaller::EnvironmentVariableOperation op;
    op.setArguments( QStringList() << key
                    << value
                    << QLatin1String("false")
                    << QLatin1String("false"));
    const bool ok = op.performOperation();

    QVERIFY2(ok, qPrintable(op.errorString()));

    QString comp = QString::fromLocal8Bit(qgetenv(qPrintable(key)));
    QCOMPARE(value, comp);
}

QTEST_MAIN(EnvironmentVariableTest)
