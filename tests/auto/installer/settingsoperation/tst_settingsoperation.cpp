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
#include <utils.h>
#include <settingsoperation.h>
#include <packagemanagercore.h>
#include <settings.h>

#include <QTest>
#include <QSettings>

using namespace KDUpdater;
using namespace QInstaller;

class tst_settingsoperation : public QObject
{
    Q_OBJECT

private slots:
    // called before all tests
    void initTestCase()
    {
        m_testSettingsDirPath = qApp->applicationDirPath();
        m_testSettingsFilename = "test.ini";
        QSettings testSettings(QDir(m_testSettingsDirPath).filePath(m_testSettingsFilename),
            QSettings::IniFormat);
        m_cleanupFilePaths << QDir(m_testSettingsDirPath).filePath(m_testSettingsFilename);
        testSettings.setValue("testkey", "testvalue");
        testSettings.setValue("testcategory/categorytestkey", "categorytestvalue");
        testSettings.setValue("testcategory/categoryarrayvalue1", QStringList() << "value1" <<
            "value2" << "value3");
        testSettings.setValue("testcategory/categoryarrayvalue2", "value1,value2,value3");
        testSettings.setValue("testcategory/categoryarrayvalue3", "value1 ,value2, value3");
    }

    void testWrongArguments()
    {
        SettingsOperation noArgumentsOperation(nullptr);

        QVERIFY(noArgumentsOperation.testOperation());

        // operation should do nothing if there are no arguments
        QCOMPARE(noArgumentsOperation.performOperation(), false);

        QCOMPARE(UpdateOperation::Error(noArgumentsOperation.error()),
            UpdateOperation::InvalidArguments);
        QString compareString("Missing argument(s) \"path; method; key; value\" calling Settings "
            "with arguments \"\".");
        QCOMPARE(noArgumentsOperation.errorString(), compareString);

        // same for undo
        QCOMPARE(noArgumentsOperation.undoOperation(), false);

        SettingsOperation wrongMethodArgumentOperation(nullptr);
        wrongMethodArgumentOperation.setArguments(QStringList() << "path=first" << "method=second"
            << "key=third" << "value=fourth");

        QVERIFY(wrongMethodArgumentOperation.testOperation());

        // operation should do nothing if there are no arguments
        QCOMPARE(wrongMethodArgumentOperation.performOperation(), false);

        QCOMPARE(UpdateOperation::Error(wrongMethodArgumentOperation.error()),
            UpdateOperation::InvalidArguments);
        compareString = "Current method argument calling \"Settings\" with arguments \"path=first; "
            "method=second; key=third; value=fourth\" is not supported. Please use set, remove, "
            "add_array_value, or remove_array_value.";
        QCOMPARE(wrongMethodArgumentOperation.errorString(), compareString);

        // same for undo
        QCOMPARE(wrongMethodArgumentOperation.undoOperation(), false);
    }

    void setSettingsValue()
    {
        const QString verifyFilePath = createFilePath(QTest::currentTestFunction());
        const QString testFilePath = createFilePath(QString("_") + QTest::currentTestFunction());
        m_cleanupFilePaths << verifyFilePath << testFilePath;

        const QString key = "category/key";
        const QString value = "value";
        {
            QSettings testSettings(verifyFilePath, QSettings::IniFormat);
            testSettings.setValue(key, value);
        }

        SettingsOperation settingsOperation(nullptr);
        settingsOperation.setArguments(QStringList() << QString("path=%1").arg(testFilePath) <<
            "method=set" << QString("key=%1").arg(key) << QString("value=%1").arg(value));
        settingsOperation.backup();

        QVERIFY2(settingsOperation.performOperation(), settingsOperation.errorString().toLatin1());

        QVERIFY2(compareFiles(verifyFilePath, testFilePath), QString("\"%1\" and \"%2\" are different.")
            .arg(verifyFilePath, testFilePath).toLatin1());
    }

    void undoSettingsValueInCreatedSubDirectories()
    {
        const QString testFilePath = createFilePath(QString("sub/directory/") +
            QTest::currentTestFunction());
        const QString key = "key";
        const QString value = "value";

        SettingsOperation settingsOperation(nullptr);
        settingsOperation.setArguments(QStringList() << QString("path=%1").arg(testFilePath) <<
            "method=set" << QString("key=%1").arg(key) << QString("value=%1").arg(value));
        settingsOperation.backup();

        QVERIFY2(settingsOperation.performOperation(), settingsOperation.errorString().toLatin1());
        QCOMPARE(QFile(testFilePath).exists(), true);
        QVERIFY2(settingsOperation.undoOperation(), settingsOperation.errorString().toLatin1());
        QCOMPARE(QFile(testFilePath).exists(), false);
        QCOMPARE(QDir(QFileInfo(testFilePath).absolutePath()).exists(), false);
    }

    void removeSettingsValue()
    {
        const QString testFilePath = createFilePath(QTest::currentTestFunction());
        QFile testFile(QDir(m_testSettingsDirPath).filePath(m_testSettingsFilename));
        QVERIFY2(testFile.copy(testFilePath), testFile.errorString().toLatin1());
        m_cleanupFilePaths << testFilePath;

        const QString key = "testkey";
        QString testValueString;
        {
            QSettings testSettings(testFilePath, QSettings::IniFormat);
            testValueString = testSettings.value(key).toString();
        }
        QCOMPARE(testValueString.isEmpty(), false);

        SettingsOperation settingsOperation(nullptr);
        settingsOperation.setArguments(QStringList() <<  QString("path=%1").arg(testFilePath) <<
            "method=remove" << QString("key=%1").arg(key));
        settingsOperation.backup();
        QVERIFY2(settingsOperation.performOperation(), settingsOperation.errorString().toLatin1());

        QVariant testValueVariant;
        {
            QSettings testSettings(testFilePath, QSettings::IniFormat);
            testValueVariant = testSettings.value(key);
        }
        QVERIFY(testValueVariant.isNull());
    }

    void addSettingsArrayValue()
    {
        const QString testFilePath = createFilePath(QTest::currentTestFunction());
        QFile contentFile(QDir(m_testSettingsDirPath).filePath(m_testSettingsFilename));
        QVERIFY2(contentFile.open(QIODevice::ReadOnly | QIODevice::Text), contentFile.errorString().toLatin1());

        QFile testFile(testFilePath);
        QVERIFY2(testFile.open(QIODevice::WriteOnly | QIODevice::Text), contentFile.errorString().toLatin1());
        m_cleanupFilePaths << testFilePath;

        QTextStream out(&testFile);

        QTextStream in(&contentFile);
        QString line = in.readLine();
        while (!line.isNull()) {
            // remove the " to have some maybe invalid data
            out << line.replace("\"", QLatin1String("")) << QLatin1String("\n");
            line = in.readLine();
        }
        testFile.close();

        QMap<QString, SettingsOperation*> testSettingsOperationMap;
        testSettingsOperationMap["testcategory/categoryarrayvalue1"] = new SettingsOperation(nullptr);
        testSettingsOperationMap["testcategory/categoryarrayvalue2"] = new SettingsOperation(nullptr);
        testSettingsOperationMap["testcategory/categoryarrayvalue3"] = new SettingsOperation(nullptr);
        testSettingsOperationMap["testcategory/categoryarrayvalue4"] = new SettingsOperation(nullptr);

        QMap<QString, SettingsOperation*>::iterator i = testSettingsOperationMap.begin();
        while (i != testSettingsOperationMap.end()) {
            i.value()->setArguments(QStringList() <<  QString("path=%1").arg(testFilePath) <<
                "method=add_array_value" << QString("key=%1").arg(i.key()) << "value=value4");
            i.value()->backup();
            QVERIFY2(i.value()->performOperation(), i.value()->errorString().toLatin1());
            ++i;
        }
        QStringList testKeys(testSettingsOperationMap.keys());
        {
            QSettings verifySettings(testFilePath, QSettings::IniFormat);
            QCOMPARE(verifySettings.value(testKeys.at(0)).isNull(), false);
            QCOMPARE(verifySettings.value(testKeys.at(0)), verifySettings.value(testKeys.at(1)));
            QCOMPARE(verifySettings.value(testKeys.at(1)), verifySettings.value(testKeys.at(2)));
            QCOMPARE(verifySettings.value(testKeys.at(3)).toString(), QLatin1String("value4"));
        }

        i = testSettingsOperationMap.begin();
        while (i != testSettingsOperationMap.end()) {
            i.value()->setArguments(QStringList() <<  QString("path=%1").arg(testFilePath) <<
                "method=add_array_value" << QString("key=%1").arg(i.key()) << "value=value4");
            QVERIFY2(i.value()->undoOperation(), i.value()->errorString().toLatin1());
            ++i;
        }

        {
            QStringList verifyStringList;
            verifyStringList << "value1" << "value2" << "value3";
            QVariant verifyUndoValue = verifyStringList;
            QSettings verifySettings(testFilePath, QSettings::IniFormat);
            QCOMPARE(verifySettings.value(testKeys.at(0)), verifyUndoValue);
            QCOMPARE(verifySettings.value(testKeys.at(1)), verifyUndoValue);
            QCOMPARE(verifySettings.value(testKeys.at(2)), verifyUndoValue);
            // checking the none array value is removed
            QVERIFY(verifySettings.value(testKeys.at(3)).isNull());
        }

    }

    void removeSettingsArrayValue()
    {
        const QString testFilePath = createFilePath(QTest::currentTestFunction());
        QFile contentFile(QDir(m_testSettingsDirPath).filePath(m_testSettingsFilename));
        QVERIFY2(contentFile.open(QIODevice::ReadOnly | QIODevice::Text), contentFile.errorString()
            .toLatin1());

        QFile testFile(testFilePath);
        QVERIFY2(testFile.open(QIODevice::WriteOnly | QIODevice::Text), contentFile.errorString()
            .toLatin1());
        m_cleanupFilePaths << testFilePath;

        QTextStream out(&testFile);

        QTextStream in(&contentFile);
        QString line = in.readLine();
        while (!line.isNull()) {
            // remove the " to have some maybe invalid data
            out << line.replace("\"", QLatin1String("")) << QLatin1String("\n");
            line = in.readLine();
        }
        testFile.close();

        QMap<QString, SettingsOperation*> testSettingsOperationMap;
        testSettingsOperationMap["testcategory/categoryarrayvalue1"] = new SettingsOperation(nullptr);
        testSettingsOperationMap["testcategory/categoryarrayvalue2"] = new SettingsOperation(nullptr);
        testSettingsOperationMap["testcategory/categoryarrayvalue3"] = new SettingsOperation(nullptr);

        QMap<QString, SettingsOperation*>::iterator i = testSettingsOperationMap.begin();
        while (i != testSettingsOperationMap.end()) {
            i.value()->setArguments(QStringList() <<  QString("path=%1").arg(testFilePath) <<
                "method=remove_array_value" << QString("key=%1").arg(i.key()) << "value=value3");
            i.value()->backup();
            QVERIFY2(i.value()->performOperation(), i.value()->errorString().toLatin1());
            ++i;
        }
        QStringList testKeys(testSettingsOperationMap.keys());
        {
            QSettings verifySettings(testFilePath, QSettings::IniFormat);
            QCOMPARE(verifySettings.value(testKeys.at(0)).isNull(), false);

            QStringList verifyFirstValue = verifySettings.value(testKeys.at(0)).toStringList();
            QCOMPARE(verifyFirstValue.contains(QLatin1String("value3")), false);
            QCOMPARE(verifySettings.value(testKeys.at(0)), verifySettings.value(testKeys.at(1)));
            QCOMPARE(verifySettings.value(testKeys.at(1)), verifySettings.value(testKeys.at(2)));
        }
    }

    void testPerformingFromCLI()
    {
        QString installDir = QInstaller::generateTemporaryFileName();
        QVERIFY(QDir().mkpath(installDir));
        PackageManagerCore *core = PackageManager::getPackageManagerWithInit
                (installDir, ":///data/repository");

        QSettings testSettings(QDir(m_testSettingsDirPath).filePath(m_testSettingsFilename),
            QSettings::IniFormat);
        QCOMPARE(testSettings.value("testcategory/categoryarrayvalue1").toStringList(),
                 QStringList() << "value1" << "value2" << "value3");

        core->installSelectedComponentsSilently(QStringList() << "A");

        QCOMPARE(testSettings.value("testcategory/categoryarrayvalue1").toStringList(),
                 QStringList() << "value1" << "value2" << "value3" << "valueFromScript");

        core->commitSessionOperations();
        core->setPackageManager();
        core->uninstallComponentsSilently(QStringList() << "A");

        QCOMPARE(testSettings.value("testcategory/categoryarrayvalue1").toStringList(),
                 QStringList() << "value1" << "value2" << "value3");
        QDir dir(installDir);
        QVERIFY(dir.removeRecursively());
        core->deleteLater();
    }

    void testPerformingFromCLIWithPlaceholders()
    {
        QString installDir = QInstaller::generateTemporaryFileName();
        QVERIFY(QDir().mkpath(installDir));
        PackageManagerCore *core = PackageManager::getPackageManagerWithInit
            (installDir, ":///data/repository");

        core->installSelectedComponentsSilently(QStringList() << "B");
        // Path is set in component constructor in install script
        const QString  settingsFile = core->value("SettingsPathFromVariable");
        QSettings testSettings(QDir(m_testSettingsDirPath).filePath(settingsFile),
                               QSettings::IniFormat);

        QCOMPARE(testSettings.value("testcategory/categoryarrayvalue1").toStringList(),
                 QStringList() << "ValueFromPlaceholder");

        core->commitSessionOperations();
        core->setPackageManager();
        core->uninstallComponentsSilently(QStringList() << "B");

        // Settings file is removed as it is empty
        QVERIFY(!QFile::exists(settingsFile));
        QDir dir(installDir);
        QVERIFY(dir.removeRecursively());
        core->deleteLater();
    }

    void testPerformingFromCLIWithSettingsFileMoved()
    {
        QString installDir = QInstaller::generateTemporaryFileName();
        QVERIFY(QDir().mkpath(installDir));
        PackageManagerCore *core = PackageManager::getPackageManagerWithInit
            (installDir, ":///data/repository");


        core->installSelectedComponentsSilently(QStringList() << "C");

        const QString settingsFileName = qApp->applicationDirPath() + "/oldTestSettings.ini";
        QSettings testSettings(QDir(m_testSettingsDirPath).filePath(settingsFileName),
                               QSettings::IniFormat);

        QCOMPARE(testSettings.value("testcategory/categoryarrayvalue1").toStringList(),
                 QStringList() << "valueFromScript");

        QFile settingsFile(settingsFileName);
        // Move the settings path to new location. Script has set values
        // ComponentCSettingsPath (@InstallerDirPath@/newTestSettings.ini) and
        // ComponentCSettingsPath_OLD (@InstallerDirPath@/oldTestSettings.ini) so
        // UNDO operation should delete the moved newTestSettings.ini file instead.
        QVERIFY2(settingsFile.rename(qApp->applicationDirPath() + "/newTestSettings.ini"), "Could not move settings file.");
        core->commitSessionOperations();
        core->setPackageManager();
        core->uninstallComponentsSilently(QStringList() << "C");

        // Settings file is removed in uninstall as it is empty
        QVERIFY2(!QFile::exists(qApp->applicationDirPath() + "/newTestSettings.ini"), "Settings file not deleted correctly");

        QDir dir(installDir);
        QVERIFY(dir.removeRecursively());
        core->deleteLater();
    }

    // called after all tests
    void cleanupTestCase()
    {
        foreach (const QString &filePath, m_cleanupFilePaths)
            QFile(filePath).remove();
    }
private:
    QString createFilePath(const QString &fileNamePrependix)
    {
        return QDir(m_testSettingsDirPath).filePath(QString(fileNamePrependix) + m_testSettingsFilename);
    }

    bool compareFiles(const QString &filePath1, const QString &filePath2)
    {
        if (!QFile::exists(filePath1) || !QFile::exists(filePath2))
            return false;
        const QByteArray fileHash1 = QInstaller::calculateHash(filePath1, QCryptographicHash::Sha1);
        const QByteArray fileHash2 = QInstaller::calculateHash(filePath2, QCryptographicHash::Sha1);
        return fileHash1 == fileHash2;
    }

    QString m_testSettingsFilename;
    QString m_testSettingsDirPath;
    QStringList m_cleanupFilePaths;
};

QTEST_MAIN(tst_settingsoperation)

#include "tst_settingsoperation.moc"
