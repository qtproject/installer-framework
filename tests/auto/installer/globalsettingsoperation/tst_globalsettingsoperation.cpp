/**************************************************************************
**
** Copyright (C) 2020 The Qt Company Ltd.
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

#include <globalsettingsoperation.h>
#include <environment.h>
#include <packagemanagercore.h>

#include <QSettings>
#include <QTest>

using namespace QInstaller;
using namespace KDUpdater;

class tst_globalsettingsoperation : public QObject
{
    Q_OBJECT

private:
    void cleanSettings()
    {
        QSettings testSettings("QtProject", "QtProject.QtIfwTest");
        testSettings.setValue("QtIfwTestKey", "");
    }


private slots:
    void initTestCase()
    {
        cleanSettings();
    }

    void cleanupTestCase()
    {
        cleanSettings();
    }

    void setGlobalSettingsValue()
    {
        GlobalSettingsOperation settingsOperation(nullptr);
        settingsOperation.setArguments(QStringList() <<  "QtProject" << "QtProject.QtIfwTest" << "QtIfwTestKey" << "QtIfwTestValue");
        settingsOperation.backup();
        QVERIFY2(settingsOperation.performOperation(), settingsOperation.errorString().toLatin1());

        QSettings testSettings("QtProject", "QtProject.QtIfwTest");
        QCOMPARE("QtIfwTestValue", testSettings.value("QtIfwTestKey"));
        QVERIFY2(settingsOperation.undoOperation(), settingsOperation.errorString().toLatin1());
        QCOMPARE("", testSettings.value("QtIfwTestKey"));
    }

    void setGlobalSettingsValueNoUndo()
    {

        GlobalSettingsOperation settingsOperation(nullptr);
        settingsOperation.setArguments(QStringList() <<  "QtProject" << "QtProject.QtIfwTest" << "QtIfwTestKey" << "QtIfwTestValue" << "UNDOOPERATION" << "");
        settingsOperation.backup();
        QVERIFY2(settingsOperation.performOperation(), settingsOperation.errorString().toLatin1());

        QSettings testSettings("QtProject", "QtProject.QtIfwTest");
        QCOMPARE("QtIfwTestValue", testSettings.value("QtIfwTestKey"));
        QVERIFY2(settingsOperation.undoOperation(), settingsOperation.errorString().toLatin1());
        QCOMPARE("QtIfwTestValue", testSettings.value("QtIfwTestKey"));
    }
};

QTEST_MAIN(tst_globalsettingsoperation)

#include "tst_globalsettingsoperation.moc"

