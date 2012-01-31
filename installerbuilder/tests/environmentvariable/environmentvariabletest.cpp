/**************************************************************************
**
** This file is part of Qt SDK**
**
** Copyright (c) 2012 Nokia Corporation and/or its subsidiary(-ies).*
**
** Contact:  Nokia Corporation qt-info@nokia.com**
**
** No Commercial Usage
**
** This file contains pre-release code and may not be distributed.
** You may use this file in accordance with the terms and conditions
** contained in the Technology Preview License Agreement accompanying
** this package.
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception version
** 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** If you are unsure which license is appropriate for your use, please contact
** (qt-info@nokia.com).
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
    QSKIP("This operation only works on Windows",SkipSingle);
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
    QSettings settings("HKEY_CURRENT_USER\\Environment", QSettings::NativeFormat);
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
    QSKIP("This operation only works on Windows",SkipSingle);
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

    QString comp = qgetenv(qPrintable(key));
    QCOMPARE(value, comp);
}

QTEST_MAIN(EnvironmentVariableTest)
