/**************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
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

    QString comp = QString::fromLocal8Bit(qgetenv(qPrintable(key)));
    QCOMPARE(value, comp);
}

QTEST_MAIN(EnvironmentVariableTest)
