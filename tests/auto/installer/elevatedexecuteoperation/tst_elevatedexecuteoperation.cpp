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

#include <packagemanagercore.h>
#include <elevatedexecuteoperation.h>

#include <QTest>

#define QUOTE_(x) #x
#define QUOTE(x) QUOTE_(x)

using namespace QInstaller;

class tst_elevatedexecuteoperation : public QObject
{
    Q_OBJECT

private slots:
    void testExecuteOperation()
    {
        m_core.setValue(QLatin1String("QMAKE_BINARY"), QUOTE(QMAKE_BINARY));
        m_core.setValue(QLatin1String("QMAKE_BINARY_OLD"), QLatin1String("FAKE_QMAKE"));
        ElevatedExecuteOperation operation(&m_core);
        operation.setArguments(QStringList() << QLatin1String("UNDOEXECUTE") << QLatin1String("FAKE_QMAKE") << QLatin1String("-v"));

        QTest::ignoreMessage(QtDebugMsg, "\"FAKE_QMAKE\" started, arguments: \"-v\"");
        QString message =  "Failed to run undo operation \"Execute\" for component . Trying again with arguments %1, -v";
        QTest::ignoreMessage(QtDebugMsg, qPrintable(message.arg(QUOTE(QMAKE_BINARY))));
        message =  "\"%1\" started, arguments: \"-v\"";
        QTest::ignoreMessage(QtDebugMsg, qPrintable(message.arg(QUOTE(QMAKE_BINARY))));

        QCOMPARE(operation.undoOperation(), true);
        QCOMPARE(Operation::Error(operation.error()), Operation::NoError);
    }

private:
    PackageManagerCore m_core;
};

QTEST_MAIN(tst_elevatedexecuteoperation)

#include "tst_elevatedexecuteoperation.moc"
