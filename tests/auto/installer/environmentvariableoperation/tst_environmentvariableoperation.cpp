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

#include <environmentvariablesoperation.h>
#include <environment.h>
#include <packagemanagercore.h>

#include <QSettings>
#include <QTest>

using namespace QInstaller;
using namespace KDUpdater;

class tst_environmentvariableoperation : public QObject
{
    Q_OBJECT

private:
    void installFromCLI(const QString &repository)
    {
        QString installDir = QInstaller::generateTemporaryFileName();
        QVERIFY(QDir().mkpath(installDir));
        QScopedPointer<PackageManagerCore> core(PackageManager::getPackageManagerWithInit
                (installDir, repository));
        core->installDefaultComponentsSilently();

        QVERIFY(m_settings->value("IFW_UNIT_TEST_LOCAL").toString().isEmpty());

        // Persistent is in settings in Windows platform only, otherwise it is written to local env.
#ifdef Q_OS_WIN
        QCOMPARE(m_settings->value("IFW_UNIT_TEST_PERSISTENT").toString(), QLatin1String("IFW_UNIT_TEST_PERSISTENT_VALUE"));
#else
        QCOMPARE(Environment::instance().value("IFW_UNIT_TEST_PERSISTENT"), QLatin1String("IFW_UNIT_TEST_PERSISTENT_VALUE"));
#endif
        QCOMPARE(Environment::instance().value("IFW_UNIT_TEST_LOCAL"), QLatin1String("IFW_UNIT_TEST_VALUE"));

        core->commitSessionOperations();
        core->setPackageManager();
        core->uninstallComponentsSilently(QStringList() << "A");
        QVERIFY(m_settings->value("IFW_UNIT_TEST_PERSISTENT").toString().isEmpty());
        QVERIFY(Environment::instance().value("IFW_UNIT_TEST_LOCAL").isEmpty());
    }

private slots:
    void initTestCase()
    {
        m_key = "IFW_TestKey";
        m_value = "IFW_TestValue";
        m_oldValue = "IFW_TestOldValue";
        m_settings = new QSettings("HKEY_CURRENT_USER\\Environment", QSettings::NativeFormat);
    }

    void cleanup()
    {
        m_settings->remove(m_key);
        m_settings->remove("IFW_UNIT_TEST_PERSISTENT"); //Added from script
    }

    void testPersistentNonSystem()
    {
    #ifndef Q_OS_WIN
        QSKIP("This operation only works on Windows");
    #endif
        EnvironmentVariableOperation op(nullptr);
        op.setArguments( QStringList() << m_key
                        << m_value
                        << QLatin1String("true")
                        << QLatin1String("false"));
        const bool ok = op.performOperation();

        QVERIFY2(ok, qPrintable(op.errorString()));

        QCOMPARE(m_value, m_settings->value(m_key).toString());

        QVERIFY(op.undoOperation());
        QVERIFY(m_settings->value(m_key).toString().isEmpty());
    }

    void testNonPersistentNonSystem()
    {
        EnvironmentVariableOperation op(nullptr);
        op.setArguments( QStringList() << m_key
                        << m_value
                        << QLatin1String("false")
                        << QLatin1String("false"));
        const bool ok = op.performOperation();

        QVERIFY2(ok, qPrintable(op.errorString()));

        //Make sure it is not written to env variable
        QString comp = QString::fromLocal8Bit(qgetenv(qPrintable(m_key)));
        QVERIFY(comp.isEmpty());

        QCOMPARE(m_value, Environment::instance().value(m_key));
    }

    void testPersistentNonSystemOldValue()
    {
    #ifndef Q_OS_WIN
        QSKIP("This operation only works on Windows");
    #endif
        m_settings->setValue(m_key, m_oldValue);

        EnvironmentVariableOperation op(nullptr);
        op.setArguments( QStringList() << m_key
                        << m_value
                        << QLatin1String("true")
                        << QLatin1String("false"));
        const bool ok = op.performOperation();

        QVERIFY2(ok, qPrintable(op.errorString()));

        QCOMPARE(m_value, m_settings->value(m_key).toString());

        QVERIFY(op.undoOperation());
        QCOMPARE(m_settings->value(m_key).toString(), m_oldValue);
        m_settings->remove(m_key);
    }

    void testNonPersistentNonSystemOldValue()
    {
        m_settings->setValue(m_key, m_oldValue);
        Environment::instance().setTemporaryValue(m_key, m_oldValue);
        EnvironmentVariableOperation op(nullptr);
        op.setArguments( QStringList() << m_key
                        << m_value
                        << QLatin1String("false")
                        << QLatin1String("false"));
        const bool ok = op.performOperation();

        QVERIFY2(ok, qPrintable(op.errorString()));

        QVERIFY(op.undoOperation());
        QCOMPARE(m_oldValue, Environment::instance().value(m_key));
    }

    void testEnvVariableFromScript()
    {
        installFromCLI(":///data/repository");
    }

    void testEnvVariableFromSXML()
    {
        installFromCLI(":///data/xmloperationrepository");
    }

private:
    QSettings *m_settings;
    QString m_key;
    QString m_value;
    QString m_oldValue;
};

QTEST_MAIN(tst_environmentvariableoperation)

#include "tst_environmentvariableoperation.moc"

