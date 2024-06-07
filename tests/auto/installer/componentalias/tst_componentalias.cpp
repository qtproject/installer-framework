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
        m_fallbackRepositoryData = QLatin1String("========================================\n"
            "Name: set-optional-broken-component-from-fallback\n"
            "Display name: Installation (fetches optional component from fallback repository)\n"
            "Description: Installs component\n"
            "Version: 1.0.0\n"
            "Components: \n"
            "Required aliases: \n"
            "Optional components: UnstableComponent\n"
            "Optional aliases: \n"
            "========================================\n"
            "Name: set-optional-component-from-fallback\n"
            "Display name: Installation (fetches optional component from fallback repository)\n"
            "Description: Installs component\n"
            "Version: 1.0.0\nComponents: \n"
            "Required aliases: \n"
            "Optional components: A\n"
            "Optional aliases: \n"
            "========================================\n"
            "Name: set-required-broken-component-from-fallback\n"
            "Display name: Installation (fetches optional component from fallback repository)\n"
            "Description: Installs component\n"
            "Version: 1.0.0\n"
            "Components: UnstableComponent\n"
            "Required aliases: \n"
            "Optional components: \n"
            "Optional aliases: \n"
            "========================================\n"
            "Name: set-required-component-from-fallback\n"
            "Display name: Installation (fetches component from fallback repository)\n"
            "Description: Installs component\n"
            "Version: 1.0.0\nComponents: A\n"
            "Required aliases: \n"
            "Optional components: \n"
            "Optional aliases: \n");
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
            "Name: set-D\n"
            "Display name: Installation D (Unstable)\n"
            "Description: Installs missing component D\n"
            "Version: 1.0.0\nComponents: D\n"
            "Required aliases: \n"
            "Optional components: \n"
            "Optional aliases: \n"
            "========================================\n"
            "Name: set-E\n"
            "Display name: Installation E (Requires unstable)\n"
            "Description: Installs set E\nVersion: 1.0.0\nComponents: \n"
            "Required aliases: set-D\n"
            "Optional components: \n"
            "Optional aliases: \n"
            "========================================\n"
            "Name: set-F\n"
            "Display name: Installation F (Requires alias that refers another unstable)\n"
            "Description: Installs set F\n"
            "Version: 1.0.0\n"
            "Components: \n"
            "Required aliases: set-E\nOptional components: \n"
            "Optional aliases: \n"
            "========================================\n"
            "Name: set-full\n"
            "Display name: Full installation\n"
            "Description: Installs all components\n"
            "Version: 1.0.0\n"
            "Components: C\n"
            "Required aliases: set-A,set-B\n"
            "Optional components: \n"
            "Optional aliases: \n") + m_fallbackRepositoryData, listMethod, QString());

        VerifyInstaller::verifyListPackagesMessage(core.get(), QLatin1String("\n"
            "Name: set-A\n"
            "Display name: Installation A\n"
            "Description: Installs component A\n"
            "Version: 1.0.0\n"
            "Components: A\n"
            "Required aliases: \n"
            "Optional components: \n"
            "Optional aliases: \n"), listMethod, QLatin1String("-A"));
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
            "Name: set-D\n"
            "Display name: Installation D (Unstable) (priority)\n"
            "Description: Installs missing component D\n"
            "Version: 1.0.0\n"
            "Components: D\n"
            "Required aliases: \n"
            "Optional components: \n"
            "Optional aliases: \n"
            "========================================\n"
            "Name: set-E\n"
            "Display name: Installation E (Requires unstable)\n"
            "Description: Installs set E\n"
            "Version: 1.0.0\n"
            "Components: \n"
            "Required aliases: set-D\n"
            "Optional components: \n"
            "Optional aliases: \n"
            "========================================\n"
            "Name: set-F\n"
            "Display name: Installation F (Requires alias that refers another unstable)\n"
            "Description: Installs set F\n"
            "Version: 1.0.0\n"
            "Components: \n"
            "Required aliases: set-E\n"
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
            "Optional aliases: \n") + m_fallbackRepositoryData, listMethod, QString());
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
            "Name: set-D\n"
            "Display name: Installation D (Unstable) (updated)\n"
            "Description: Installs missing component D\n"
            "Version: 1.2.0\n"
            "Components: D\n"
            "Required aliases: \n"
            "Optional components: \n"
            "Optional aliases: \n"
            "========================================\n"
            "Name: set-E\n"
            "Display name: Installation E (Requires unstable)\n"
            "Description: Installs set E\n"
            "Version: 1.0.0\n"
            "Components: \n"
            "Required aliases: set-D\n"
            "Optional components: \n"
            "Optional aliases: \n"
            "========================================\n"
            "Name: set-F\n"
            "Display name: Installation F (Requires alias that refers another unstable)\n"
            "Description: Installs set F\n"
            "Version: 1.0.0\n"
            "Components: \n"
            "Required aliases: set-E\n"
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
            "Optional aliases: \n") + m_fallbackRepositoryData, listMethod, QString());
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

        QTest::newRow("Alias with optional broken alias (will install)")
            << AliasSource(AliasSource::SourceFileFormat::Xml, ":///data/aliases-optional.xml", -1)
            << (QStringList() << "set-optional-broken")
            << PackageManagerCore::Success
            << QStringList();

        QTest::newRow("Alias with optional broken component (will install)")
            << AliasSource(AliasSource::SourceFileFormat::Xml, ":///data/aliases-optional.xml", -1)
            << (QStringList() << "set-optional-broken-component")
            << PackageManagerCore::Success
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
        core->settings().setAllowUnstableComponents(true);
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

        QCOMPARE(core->installSelectedComponentsSilently(selectedAliases), status);
    }

    void testInstallAliasWithFallbackRepositories_data()
    {
        QTest::addColumn<QStringList>("selectedAliases");
        QTest::addColumn<PackageManagerCore::Status>("status");
        QTest::addColumn<QStringList>("installedComponents");

        QTest::newRow("Component from fallback")
            << (QStringList() << "set-required-component-from-fallback")
            << PackageManagerCore::Success
            << (QStringList() << "A");
        QTest::newRow("Optional component from fallback")
            << (QStringList() << "set-optional-component-from-fallback")
            << PackageManagerCore::Success
            << (QStringList() << "A");
        QTest::newRow("Optional broken component from fallback")
            << (QStringList() << "set-optional-broken-component-from-fallback")
            << PackageManagerCore::Success
            << (QStringList());
        QTest::newRow("Required broke component from fallback")
            << (QStringList() << "set-required-broken-component-from-fallback")
            << PackageManagerCore::Canceled
            << (QStringList());
    }

    void testInstallAliasWithFallbackRepositories()
    {
        QFETCH(QStringList, selectedAliases);
        QFETCH(PackageManagerCore::Status, status);
        QFETCH(QStringList, installedComponents);

        QScopedPointer<PackageManagerCore> core(PackageManager::getPackageManagerWithInit(m_installDir));

        core->setCommandLineInstance(true);
        core->settings().setAllowUnstableComponents(true);
        QSet<RepositoryCategory> repositoryCategories;
        RepositoryCategory repositoryCategory;
        repositoryCategory.setEnabled(false);
        repositoryCategory.addRepository(Repository::fromUserInput(":///data/repository"));
        repositoryCategories.insert(repositoryCategory);
        core->settings().addRepositoryCategories(repositoryCategories);
        QCOMPARE(core->installSelectedComponentsSilently(selectedAliases), status);

        for (const QString &component : installedComponents)
            QVERIFY(core->componentByName(component)->isInstalled());
    }

private:
    QString m_installDir;
    QString m_fallbackRepositoryData;
};

QTEST_GUILESS_MAIN(tst_ComponentAlias)

#include "tst_componentalias.moc"
