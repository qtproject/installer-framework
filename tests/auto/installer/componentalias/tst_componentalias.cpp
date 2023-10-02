/**************************************************************************
**
** Copyright (C) 2023 The Qt Company Ltd.
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
#include "../shared/verifyinstaller.h"

#include <packagemanagercore.h>
#include <componentalias.h>

#include <QTest>
#include <QLoggingCategory>

using namespace QInstaller;

class tst_ComponentAlias : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        m_installDir = QInstaller::generateTemporaryFileName();
        QVERIFY(QDir().mkpath(m_installDir));
    }

    void cleanup()
    {
        QDir dir(m_installDir);
        QVERIFY(dir.removeRecursively());
    }

    void testSearchAlias()
    {
        QString loggingRules = (QLatin1String("ifw.* = false\n"));

        QLoggingCategory::setFilterRules(loggingRules);
        QScopedPointer<PackageManagerCore> core(PackageManager::getPackageManager
            (m_installDir, ":///data/repository"));

        core->setCommandLineInstance(true);

        auto listMethod = &PackageManagerCore::listAvailableAliases;

        VerifyInstaller::verifyListPackagesMessage(core.get(), QLatin1String("\n"
            "Name: set-A\n"
            "Display name: Installation A\n"
            "Description: Installs component A\n"
            "Version: 1.0.0\n"
            "Components: A\n"
            "Required aliases: \n"
            "Optional components: \n"
            "Optional aliases: \n"
            "========================================\n"
            "Name: set-full\n"
            "Display name: Full installation\n"
            "Description: Installs all components\n"
            "Version: 1.0.0\n"
            "Components: C\n"
            "Required aliases: set-A,set-B\n"
            "Optional components: \n"
            "Optional aliases: \n"), listMethod, QString());

        VerifyInstaller::verifyListPackagesMessage(core.get(), QLatin1String("\n"
            "Name: set-A\n"
            "Display name: Installation A\n"
            "Description: Installs component A\n"
            "Version: 1.0.0\n"
            "Components: A\n"
            "Required aliases: \n"
            "Optional components: \n"
            "Optional aliases: \n"), listMethod, QLatin1String("A"));
    }

    void testAliasSourceWithPriority()
    {
        QString loggingRules = (QLatin1String("ifw.* = false\n"));

        QLoggingCategory::setFilterRules(loggingRules);
        QScopedPointer<PackageManagerCore> core(PackageManager::getPackageManager
            (m_installDir, ":///data/repository"));

        core->setCommandLineInstance(true);
        core->addAliasSource(AliasSource(AliasSource::SourceFileFormat::Xml,
            ":///data/aliases-priority.xml", 1));

        auto listMethod = &PackageManagerCore::listAvailableAliases;

        VerifyInstaller::verifyListPackagesMessage(core.get(), QLatin1String("\n"
            "Name: set-A\n"
            "Display name: Installation A (priority)\n"
            "Description: Installs component A\n"
            "Version: 1.0.0\n"
            "Components: A\n"
            "Required aliases: \n"
            "Optional components: \n"
            "Optional aliases: \n"
            "========================================\n"
            "Name: set-full\n"
            "Display name: Full installation (priority)\n"
            "Description: Installs all components\n"
            "Version: 1.0.0\n"
            "Components: C\n"
            "Required aliases: set-A,set-B\n"
            "Optional components: \n"
            "Optional aliases: \n"), listMethod, QString());
    }

    void testAliasSourceWithVersionCompare()
    {
        QString loggingRules = (QLatin1String("ifw.* = false\n"));

        QLoggingCategory::setFilterRules(loggingRules);
        QScopedPointer<PackageManagerCore> core(PackageManager::getPackageManager
            (m_installDir, ":///data/repository"));

        core->setCommandLineInstance(true);
        core->addAliasSource(AliasSource(AliasSource::SourceFileFormat::Xml,
            ":///data/aliases-versions.xml", -1));

        auto listMethod = &PackageManagerCore::listAvailableAliases;

        VerifyInstaller::verifyListPackagesMessage(core.get(), QLatin1String("\n"
            "Name: set-A\n"
            "Display name: Installation A (updated)\n"
            "Description: Installs component A\n"
            "Version: 2.0.0\n"
            "Components: A\n"
            "Required aliases: \n"
            "Optional components: \n"
            "Optional aliases: \n"
            "========================================\n"
            "Name: set-full\n"
            "Display name: Full installation (updated)\n"
            "Description: Installs all components\n"
            "Version: 3.0.0\n"
            "Components: C\n"
            "Required aliases: set-A,set-B\n"
            "Optional components: \n"
            "Optional aliases: \n"), listMethod, QString());
    }

    void testInstallAlias_data()
    {
        QTest::addColumn<AliasSource>("additionalSource");
        QTest::addColumn<QStringList>("selectedAliases");
        QTest::addColumn<PackageManagerCore::Status>("status");
        QTest::addColumn<QStringList>("installedComponents");

        QTest::newRow("Simple alias")
            << AliasSource()
            << (QStringList() << "set-A")
            << PackageManagerCore::Success
            << (QStringList() << "A");

        QTest::newRow("Alias with dependencies")
            << AliasSource()
            << (QStringList() << "set-full")
            << PackageManagerCore::Success
            << (QStringList() << "A" << "B" << "C" << "C.subcomponent" << "C.subcomponent.subcomponent");

        QTest::newRow("Alias with dependencies (JSON source)")
            << AliasSource(AliasSource::SourceFileFormat::Json, ":///data/aliases.json", -1)
            << (QStringList() << "set-full-json")
            << PackageManagerCore::Success
            << (QStringList() << "A" << "B" << "C" << "C.subcomponent" << "C.subcomponent.subcomponent");

        QTest::newRow("Alias with optional components (existent and non-existent)")
            << AliasSource(AliasSource::SourceFileFormat::Xml, ":///data/aliases-optional.xml", -1)
            << (QStringList() << "set-A")
            << PackageManagerCore::Success
            << (QStringList() << "A" << "B");

        QTest::newRow("Alias with optional aliases (existent and non-existent)")
            << AliasSource(AliasSource::SourceFileFormat::Xml, ":///data/aliases-optional.xml", -1)
            << (QStringList() << "set-full")
            << PackageManagerCore::Success
            << (QStringList() << "A" << "B");

        QTest::newRow("Alias with optional broken alias (will not install)")
            << AliasSource(AliasSource::SourceFileFormat::Xml, ":///data/aliases-optional.xml", -1)
            << (QStringList() << "set-optional-broken")
            << PackageManagerCore::Canceled
            << QStringList();
    }

    void testInstallAlias()
    {
        QFETCH(AliasSource, additionalSource);
        QFETCH(QStringList, selectedAliases);
        QFETCH(PackageManagerCore::Status, status);
        QFETCH(QStringList, installedComponents);

        QScopedPointer<PackageManagerCore> core(PackageManager::getPackageManagerWithInit
            (m_installDir, ":///data/repository"));

        core->setCommandLineInstance(true);

        if (!additionalSource.filename.isEmpty())
            core->addAliasSource(additionalSource);

        QCOMPARE(core->installSelectedComponentsSilently(selectedAliases), status);

        for (const QString &component : installedComponents)
            QVERIFY(core->componentByName(component)->isInstalled());
    }

    void testInstallAliasFails_data()
    {
        QTest::addColumn<QStringList>("selectedAliases");
        QTest::addColumn<PackageManagerCore::Status>("status");

        QTest::newRow("Virtual alias")
            << (QStringList() << "set-B")
            << PackageManagerCore::Canceled;

        QTest::newRow("Unstable alias")
            << (QStringList() << "set-D")
            << PackageManagerCore::Canceled;

        QTest::newRow("Nested reference to unstable alias")
            << (QStringList() << "set-F")
            << PackageManagerCore::Canceled;
    }

    void testInstallAliasFails()
    {
        QFETCH(QStringList, selectedAliases);
        QFETCH(PackageManagerCore::Status, status);

        QScopedPointer<PackageManagerCore> core(PackageManager::getPackageManagerWithInit
            (m_installDir, ":///data/repository"));

        core->setCommandLineInstance(true);

        QCOMPARE(status, core->installSelectedComponentsSilently(selectedAliases));
    }

private:
    QString m_installDir;
};

QTEST_GUILESS_MAIN(tst_ComponentAlias)

#include "tst_componentalias.moc"
