/**************************************************************************
**
** Copyright (C) 2024 The Qt Company Ltd.
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

#include <component.h>
#include <packagemanagercore.h>

#include <QLoggingCategory>
#include <QTest>
#include <QRegularExpression>
#include <QSignalSpy>

using namespace QInstaller;

typedef QList<QPair<QString, QString> > ComponentResourceHash;
typedef QPair<QString, QString> ComponentResource;

class tst_CommandLineInstall : public QObject
{
    Q_OBJECT

public:
    void ignoreAvailablePackagesMissingMessages()
    {
        QTest::ignoreMessage(QtDebugMsg, QRegularExpression("Searching packages with regular expression:"));
        QTest::ignoreMessage(QtDebugMsg, "No matching packages found.");
    }

    void ignoreInstallPackageFailsMessages(const QRegularExpression &regExp)
    {
        QTest::ignoreMessage(QtDebugMsg, "Fetching latest update information...");
        QTest::ignoreMessage(QtDebugMsg, regExp);
    }

private slots:
    void testListAvailablePackages()
    {
        QString loggingRules = (QLatin1String("ifw.* = false\n"));

        QLoggingCategory::setFilterRules(loggingRules);
        QScopedPointer<PackageManagerCore> core(PackageManager::getPackageManager
                (m_installDir, ":///data/repository"));

        auto func = &PackageManagerCore::listAvailablePackages;
        VerifyInstaller::verifyListPackagesMessage(core.get(), QLatin1String("<?xml version=\"1.0\"?>\n"
            "<availablepackages>\n"
            "    <package name=\"AB\" displayname=\"AB\" version=\"1.0.2-1\"/>\n"
            "    <package name=\"A\" displayname=\"A\" version=\"1.0.2-1\"/>\n"
            "    <package name=\"B\" displayname=\"B\" version=\"1.0.0-1\"/>\n"
            "    <package name=\"C\" displayname=\"C\" version=\"1.0.0-1\"/>\n"
            "</availablepackages>\n"), func, QLatin1String("."), QHash<QString, QString>());

        VerifyInstaller::verifyListPackagesMessage(core.get(), QLatin1String("<?xml version=\"1.0\"?>\n"
            "<availablepackages>\n"
            "    <package name=\"AB\" displayname=\"AB\" version=\"1.0.2-1\"/>\n"
            "    <package name=\"A\" displayname=\"A\" version=\"1.0.2-1\"/>\n"
            "</availablepackages>\n"), func, QLatin1String("A"), QHash<QString, QString>());

        VerifyInstaller::verifyListPackagesMessage(core.get(), QLatin1String("<?xml version=\"1.0\"?>\n"
            "<availablepackages>\n"
            "    <package name=\"AB\" displayname=\"AB\" version=\"1.0.2-1\"/>\n"
            "    <package name=\"A\" displayname=\"A\" version=\"1.0.2-1\"/>\n"
            "</availablepackages>\n"), func, QLatin1String("A.*"), QHash<QString, QString>());

        VerifyInstaller::verifyListPackagesMessage(core.get(), QLatin1String("<?xml version=\"1.0\"?>\n"
            "<availablepackages>\n"
            "    <package name=\"B\" displayname=\"B\" version=\"1.0.0-1\"/>\n"
            "</availablepackages>\n"), func, QLatin1String("^B"), QHash<QString, QString>());

        VerifyInstaller::verifyListPackagesMessage(core.get(), QLatin1String("<?xml version=\"1.0\"?>\n"
            "<availablepackages>\n"
            "    <package name=\"B\" displayname=\"B\" version=\"1.0.0-1\"/>\n"
            "</availablepackages>\n"), func, QLatin1String("^B.*"), QHash<QString, QString>());

        VerifyInstaller::verifyListPackagesMessage(core.get(), QLatin1String("<?xml version=\"1.0\"?>\n"
            "<availablepackages>\n"
            "    <package name=\"C\" displayname=\"C\" version=\"1.0.0-1\"/>\n"
            "</availablepackages>\n"), func, QLatin1String("^C"), QHash<QString, QString>());

        // Test with filters
        QHash<QString, QString> searchHash {
            { "Version", "1.0.2" },
            { "DisplayName", "A" }
        };
        VerifyInstaller::verifyListPackagesMessage(core.get(), QLatin1String("<?xml version=\"1.0\"?>\n"
             "<availablepackages>\n"
             "    <package name=\"AB\" displayname=\"AB\" version=\"1.0.2-1\"/>\n"
             "    <package name=\"A\" displayname=\"A\" version=\"1.0.2-1\"/>\n"
             "</availablepackages>\n"), func, QString(), searchHash);

        searchHash.clear();
        searchHash.insert("Default", "false");
        VerifyInstaller::verifyListPackagesMessage(core.get(), QLatin1String("<?xml version=\"1.0\"?>\n"
             "<availablepackages>\n"
             "    <package name=\"B\" displayname=\"B\" version=\"1.0.0-1\"/>\n"
             "</availablepackages>\n"), func, QString(), searchHash);

        QLoggingCategory::setFilterRules("ifw.* = true\n");
        ignoreAvailablePackagesMissingMessages();
        core->listAvailablePackages(QLatin1String("C.virt"));

        ignoreAvailablePackagesMissingMessages();
        core->listAvailablePackages(QLatin1String("C.virt.subcomponent"));
    }

    void testInstallPackageFails()
    {
        QString loggingRules = (QLatin1String("ifw.* = false\n"
                                "ifw.installer.installlog = true\n"));

        QLoggingCategory::setFilterRules(loggingRules);
        QTest::ignoreMessage(QtDebugMsg, "Operations sanity check succeeded.");
        QTest::ignoreMessage(QtDebugMsg, QRegularExpression("Using metadata cache from "));
        QScopedPointer<PackageManagerCore> core(PackageManager::getPackageManager
                (m_installDir, ":///data/uninstallableComponentsRepository"));

        ignoreInstallPackageFailsMessages(QRegularExpression("Cannot install component A. Component "
                                                             "is installed only as automatic dependency to autoDep.\n"));
        QCOMPARE(PackageManagerCore::Canceled, core->installSelectedComponentsSilently(QStringList()
                << QLatin1String("A")));

        ignoreInstallPackageFailsMessages(QRegularExpression("Cannot install component AB. Component "
                                                             "is not checkable, meaning you have to select one of the subcomponents.\n"));
        QCOMPARE(PackageManagerCore::Canceled, core->installSelectedComponentsSilently(QStringList()
                << QLatin1String("AB")));

        ignoreInstallPackageFailsMessages(QRegularExpression("Cannot install B. Component is virtual.\n"));
        QCOMPARE(PackageManagerCore::Canceled, core->installSelectedComponentsSilently(QStringList()
                << QLatin1String("B")));

        ignoreInstallPackageFailsMessages(QRegularExpression("Cannot install B.subcomponent. Component "
                                                             "is a descendant of a virtual component B.\n"));
        QCOMPARE(PackageManagerCore::Canceled, core->installSelectedComponentsSilently(QStringList()
                << QLatin1String("B.subcomponent")));

        QCOMPARE(PackageManagerCore::Canceled, core->installSelectedComponentsSilently(QStringList()
                << QLatin1String("MissingComponent")));
        QCOMPARE(PackageManagerCore::Canceled, core->status());
    }

    void testUninstallPackageFails()
    {
        QString loggingRules = (QLatin1String("ifw.installer.installog = true\n"));
        PackageManagerCore core;
        core.setPackageManager();
        QLoggingCategory::setFilterRules(loggingRules);

        QVERIFY(QFile::copy(":/data/componentsFromInstallPackagesRepository.xml", m_installDir + "/components.xml"));

        core.setValue(scTargetDir, m_installDir);
        QTest::ignoreMessage(QtWarningMsg, "Cannot uninstall ForcedInstallation component componentE");
        QCOMPARE(PackageManagerCore::Success, core.uninstallComponentsSilently(QStringList()
                << "componentE"));

        QTest::ignoreMessage(QtWarningMsg, "Cannot uninstall component componentD because it is added as auto dependency to componentA,componentB");
        QCOMPARE(PackageManagerCore::Success, core.uninstallComponentsSilently(QStringList()
                << "componentD"));

        QTest::ignoreMessage(QtWarningMsg, "Cannot uninstall component MissingComponent. Component not found in install tree.");
        QCOMPARE(PackageManagerCore::Success, core.uninstallComponentsSilently(QStringList()
                << "MissingComponent"));

        QTest::ignoreMessage(QtWarningMsg, "Cannot uninstall virtual component componentH");
        QCOMPARE(PackageManagerCore::Success, core.uninstallComponentsSilently(QStringList()
                << "componentH"));

        QCOMPARE(PackageManagerCore::Success, core.status());
    }

    void testListInstalledPackages()
    {
        QString loggingRules = (QLatin1String("ifw.* = false\n"));
        PackageManagerCore core;
        core.setPackageManager();
        QLoggingCategory::setFilterRules(loggingRules);
        auto func = &PackageManagerCore::listInstalledPackages;

        const QString testDirectory = QInstaller::generateTemporaryFileName();
        QVERIFY(QDir().mkpath(testDirectory));
        QVERIFY(QFile::copy(":/data/components.xml", testDirectory + "/components.xml"));

        core.setValue(scTargetDir, testDirectory);

        VerifyInstaller::verifyListPackagesMessage(&core, QLatin1String("<?xml version=\"1.0\"?>\n"
            "<localpackages>\n"
            "    <package name=\"A\" displayname=\"A Title\" version=\"1.0.2-1\"/>\n"
            "    <package name=\"B\" displayname=\"B Title\" version=\"1.0.0-1\"/>\n"
            "</localpackages>\n"), func, QString());

        VerifyInstaller::verifyListPackagesMessage(&core, QLatin1String("<?xml version=\"1.0\"?>\n"
            "<localpackages>\n"
            "    <package name=\"A\" displayname=\"A Title\" version=\"1.0.2-1\"/>\n"
            "</localpackages>\n"), func, QLatin1String("A"));

        QDir dir(testDirectory);
        QVERIFY(dir.removeRecursively());
    }

    void testCategoryInstall_data()
    {
        QTest::addColumn<QString>("repository");
        QTest::addColumn<QStringList>("installComponents");
        QTest::addColumn<PackageManagerCore::Status>("status");


        QTest::newRow("Category installation")
            << ":///data/installPackagesRepository"
            << (QStringList() << "componentE")
            << PackageManagerCore::Success;
    }

    void testCategoryInstall()
    {
        QFETCH(QString, repository);
        QFETCH(QStringList, installComponents);
        QFETCH(PackageManagerCore::Status, status);


        QString loggingRules = (QLatin1String("ifw.* = true\n"));
        QLoggingCategory::setFilterRules(loggingRules);
        QScopedPointer<PackageManagerCore> core(PackageManager::getPackageManagerWithInit
            (m_installDir));

        RepositoryCategory category;
        category.setEnabled(false);
        category.setDisplayName(QLatin1String("category"));

        Repository repo = Repository::fromUserInput(repository);
        category.addRepository(repo);

        QSet<RepositoryCategory> categories;
        categories.insert(category);

        core->settings().addRepositoryCategories(categories);

        QSignalSpy spy(core.data(), &PackageManagerCore::statusChanged);
        QCOMPARE(core->installSelectedComponentsSilently(QStringList() << installComponents), status);

        QList<int> statusArguments;

        for (qsizetype i = 0; i < spy.size(); ++i) {
            QList<QVariant> tempList = spy.at(i);
            statusArguments << tempList.at(0).toInt();
        }

        QVERIFY(statusArguments.contains(PackageManagerCore::NoPackagesFound));
        QVERIFY(statusArguments.contains(PackageManagerCore::Success));
    }

    void testInstall_data()
    {
        QTest::addColumn<QString>("repository");
        QTest::addColumn<QStringList>("installComponents");
        QTest::addColumn<PackageManagerCore::Status>("status");
        QTest::addColumn<ComponentResourceHash>("componentResources");
        QTest::addColumn<QStringList >("installedFiles");

        /*********** Forced installation **********/
        ComponentResourceHash componentResources;
        componentResources.append(ComponentResource("componentA", "1.0.0content.txt"));
        componentResources.append(ComponentResource("componentE", "1.0.0content.txt")); //ForcedInstall
        componentResources.append(ComponentResource("componentG", "1.0.0content.txt")); //Depends on componentA

        QTest::newRow("Forced installation")
                << ":///data/installPackagesRepository"
                << (QStringList() << "componentE")
                << PackageManagerCore::Success
                << componentResources
                << (QStringList() << "components.xml" << "installcontent.txt"
                    << "installcontentA.txt" << "installcontentE.txt" << "installcontentG.txt");

        /*********** Simple installation **********/
        componentResources.clear();
        componentResources.append(ComponentResource("componentA", "1.0.0content.txt"));
        componentResources.append(ComponentResource("componentE", "1.0.0content.txt")); //ForcedInstall
        componentResources.append(ComponentResource("componentG", "1.0.0content.txt")); //Depends on componentA

        QTest::newRow("Simple installation")
                << ":///data/installPackagesRepository"
                << (QStringList() << "componentA")
                << PackageManagerCore::Success
                << componentResources
                << (QStringList() << "components.xml" << "installcontent.txt"
                    << "installcontentA.txt" << "installcontentE.txt" << "installcontentG.txt");

        /*********** Install with dependency **********/
        componentResources.clear();
        componentResources.append(ComponentResource("componentA", "1.0.0content.txt")); //Dependency for componentC
        componentResources.append(ComponentResource("componentB", "1.0.0content.txt")); //Dependency for componentC
        componentResources.append(ComponentResource("componentE", "1.0.0content.txt")); //ForcedInstall
        componentResources.append(ComponentResource("componentG", "1.0.0content.txt")); //Depends on componentA
        componentResources.append(ComponentResource("componentI", "1.0.0content.txt")); //Virtual, depends on componentC
        componentResources.append(ComponentResource("componentD", "1.0.0content.txt")); //Autodepend on componentA and componentB

        QTest::newRow("Install with dependency")
                << ":///data/installPackagesRepository"
                << (QStringList() << "componentC")
                << PackageManagerCore::Success
                << componentResources
                << (QStringList() << "components.xml" << "installcontentC.txt"
                        << "installcontent.txt" << "installcontentA.txt" << "installcontentB.txt"
                        << "installcontentD.txt"<< "installcontentE.txt" << "installcontentG.txt"
                        << "installcontentI.txt");

        /*********** Install with subcomponents **********/
        componentResources.clear();
        componentResources.append(ComponentResource("componentF.subcomponent2.subsubcomponent2", "1.0.0content.txt"));
        componentResources.append(ComponentResource("componentF.subcomponent2", "1.0.0content.txt"));
        componentResources.append(ComponentResource("componentF", "1.0.0content.txt"));
        componentResources.append(ComponentResource("componentA", "1.0.0content.txt")); //Dependency for componentG
        componentResources.append(ComponentResource("componentE", "1.0.0content.txt")); //ForcedInstall
        componentResources.append(ComponentResource("componentG", "1.0.0content.txt")); //Default install

        QTest::newRow("Install with subcomponents")
                << ":///data/installPackagesRepository"
                << (QStringList() << "componentF.subcomponent2.subsubcomponent2")
                << PackageManagerCore::Success
                << componentResources
                << (QStringList() << "components.xml" << "installcontentF.txt"
                            << "installcontentF_2.txt"  << "installcontentF_2_2.txt"
                            << "installcontent.txt" << "installcontentA.txt"
                            << "installcontentE.txt" << "installcontentG.txt");
    }

    void testInstall()
    {
        QFETCH(QString, repository);
        QFETCH(QStringList, installComponents);
        QFETCH(PackageManagerCore::Status, status);
        QFETCH(ComponentResourceHash, componentResources);
        QFETCH(QStringList, installedFiles);

        QScopedPointer<PackageManagerCore> core(PackageManager::getPackageManagerWithInit
                (m_installDir, repository));

        QCOMPARE(status, core->installSelectedComponentsSilently(QStringList() << installComponents));
        for (const ComponentResource &resource : componentResources)
            VerifyInstaller::verifyInstallerResources(m_installDir, resource.first, resource.second);
        VerifyInstaller::verifyFileExistence(m_installDir, installedFiles);
    }

    void testUninstall_data()
    {
        QTest::addColumn<QString>("repository");
        QTest::addColumn<QStringList>("installComponents");
        QTest::addColumn<PackageManagerCore::Status>("status");
        QTest::addColumn<ComponentResourceHash>("componentResources");
        QTest::addColumn<QStringList >("installedFiles");
        QTest::addColumn<QStringList>("uninstallComponents");
        QTest::addColumn<PackageManagerCore::Status>("uninstallStatus");
        QTest::addColumn<ComponentResourceHash>("componentResourcesAfterUninstall");
        QTest::addColumn<QStringList >("installedFilesAfterUninstall");
        QTest::addColumn<ComponentResourceHash >("deletedComponentResources");
        QTest::addColumn<bool >("virtualSetVisible");

        /*********** Uninstallation **********/
        ComponentResourceHash componentResourcesAfterUninstall;
        componentResourcesAfterUninstall.append(ComponentResource("componentE", "1.0.0content.txt")); //ForcedInstall

        ComponentResourceHash deletedComponentResources;
        deletedComponentResources.append(ComponentResource("componentA", "1.0.0content.txt"));
        deletedComponentResources.append(ComponentResource("componentG", "1.0.0content.txt"));

        ComponentResourceHash componentResourceAfterInstall;
        componentResourceAfterInstall << deletedComponentResources << componentResourcesAfterUninstall;

        QStringList filesAfterUninstall;
        filesAfterUninstall << "components.xml" << "installcontentE.txt";

        QStringList filesAfterInstall;
        filesAfterInstall << filesAfterUninstall << "installcontent.txt"
                          << "installcontentA.txt" << "installcontentG.txt";

        QTest::newRow("Test uninstall")
                << ":///data/installPackagesRepository"
                << (QStringList() << "componentA")
                << PackageManagerCore::Success
                << componentResourceAfterInstall
                << filesAfterInstall
                << (QStringList() << "componentA")
                << PackageManagerCore::Success
                << componentResourcesAfterUninstall
                << filesAfterUninstall
                << deletedComponentResources;

        /*********** Uninstall with dependency **********/
        componentResourcesAfterUninstall.clear();
        componentResourcesAfterUninstall.append(ComponentResource("componentA", "1.0.0content.txt"));
        componentResourcesAfterUninstall.append(ComponentResource("componentB", "1.0.0content.txt"));
        componentResourcesAfterUninstall.append(ComponentResource("componentE", "1.0.0content.txt"));
        componentResourcesAfterUninstall.append(ComponentResource("componentG", "1.0.0content.txt"));
        componentResourcesAfterUninstall.append(ComponentResource("componentD", "1.0.0content.txt"));

        deletedComponentResources.clear();
        deletedComponentResources.append(ComponentResource("componentC", "1.0.0content.txt")); //Unselected
        deletedComponentResources.append(ComponentResource("componentI", "1.0.0content.txt")); //Autodepends on componentC

        componentResourceAfterInstall.clear();
        componentResourceAfterInstall << deletedComponentResources << componentResourcesAfterUninstall;

        filesAfterUninstall.clear();
        filesAfterUninstall << "components.xml"
                           << "installcontent.txt" << "installcontentA.txt" << "installcontentB.txt"
                           << "installcontentD.txt"<< "installcontentE.txt" << "installcontentG.txt";

        filesAfterInstall.clear();
        filesAfterInstall << filesAfterUninstall << "installcontentC.txt"
                          << "installcontentI.txt";


        QTest::newRow("Uninstall with dependency")
                << ":///data/installPackagesRepository"
                << (QStringList() << "componentC")
                << PackageManagerCore::Success
                << componentResourceAfterInstall
                << filesAfterInstall
                << (QStringList() << "componentC")
                << PackageManagerCore::Success
                << componentResourcesAfterUninstall
                << filesAfterUninstall
                << deletedComponentResources;

        /*********** Uninstall with subcomponents **********/
        componentResourcesAfterUninstall.clear();
        componentResourcesAfterUninstall.append(ComponentResource("componentA", "1.0.0content.txt"));
        componentResourcesAfterUninstall.append(ComponentResource("componentE", "1.0.0content.txt"));
        componentResourcesAfterUninstall.append(ComponentResource("componentG", "1.0.0content.txt"));

        deletedComponentResources.clear();
        deletedComponentResources.append(ComponentResource("componentF.subcomponent2.subsubcomponent2", "1.0.0content.txt"));
        deletedComponentResources.append(ComponentResource("componentF.subcomponent2", "1.0.0content.txt"));
        deletedComponentResources.append(ComponentResource("componentF", "1.0.0content.txt"));

        componentResourceAfterInstall.clear();
        componentResourceAfterInstall << deletedComponentResources << componentResourcesAfterUninstall;

        filesAfterInstall.clear();
        filesAfterInstall  << "components.xml" << "installcontentF.txt"
                           << "installcontentF_2.txt"  << "installcontentF_2_2.txt"
                           << "installcontent.txt" << "installcontentA.txt"
                           << "installcontentE.txt" << "installcontentG.txt";

        filesAfterUninstall.clear();
        filesAfterUninstall << "components.xml"
                            << "installcontent.txt" << "installcontentA.txt"
                            << "installcontentE.txt" << "installcontentG.txt";

        QTest::newRow("Uninstall with subcomponents")
                << ":///data/installPackagesRepository"
                << (QStringList() << "componentF.subcomponent2.subsubcomponent2")
                << PackageManagerCore::Success
                << componentResourceAfterInstall
                << filesAfterInstall
                << (QStringList() << "componentF.subcomponent2")
                << PackageManagerCore::Success
                << componentResourcesAfterUninstall
                << filesAfterUninstall
                << deletedComponentResources;

        /*********** Uninstall forced packages **********/
        componentResourceAfterInstall.clear();
        componentResourceAfterInstall.append(ComponentResource("componentA", "1.0.0content.txt")); //ComponentG depends
        componentResourceAfterInstall.append(ComponentResource("componentE", "1.0.0content.txt")); //ForcedInstall
        componentResourceAfterInstall.append(ComponentResource("componentG", "1.0.0content.txt")); //Default

        componentResourcesAfterUninstall.clear();
        componentResourcesAfterUninstall = componentResourceAfterInstall; //forced packages cannot be uninstalled

        deletedComponentResources.clear();

        filesAfterInstall.clear();
        filesAfterInstall << "components.xml" << "installcontent.txt"
                           << "installcontentA.txt" << "installcontentE.txt" << "installcontentG.txt";

        filesAfterUninstall.clear();
        filesAfterUninstall = filesAfterInstall;

        QTest::newRow("Uninstall forced packages")
                << ":///data/installPackagesRepository"
                << (QStringList() << "componentG")
                << PackageManagerCore::Success
                << componentResourceAfterInstall
                << filesAfterInstall
                << (QStringList() << "componentE")
                << PackageManagerCore::Success
                << componentResourceAfterInstall
                << filesAfterUninstall
                << deletedComponentResources;

        /*********** Uninstall autodependency packages **********/
        componentResourceAfterInstall.clear();
        componentResourceAfterInstall.append(ComponentResource("componentA", "1.0.0content.txt")); //ComponentG depends
        componentResourceAfterInstall.append(ComponentResource("componentB", "1.0.0content.txt")); //Dependency for componentC
        componentResourceAfterInstall.append(ComponentResource("componentD", "1.0.0content.txt")); //Autodepend on componentA and componentB
        componentResourceAfterInstall.append(ComponentResource("componentE", "1.0.0content.txt")); //ForcedInstall
        componentResourceAfterInstall.append(ComponentResource("componentG", "1.0.0content.txt")); //Default

        componentResourcesAfterUninstall.clear();
        componentResourcesAfterUninstall = componentResourceAfterInstall; //autodependency packages cannot be uninstalled

        deletedComponentResources.clear();

        filesAfterInstall.clear();
        filesAfterInstall  << "components.xml" << "installcontent.txt"
                           << "installcontentA.txt" << "installcontentE.txt" << "installcontentG.txt"
                           << "installcontentB.txt" << "installcontentD.txt";
        filesAfterUninstall.clear();
        filesAfterUninstall = filesAfterInstall;

        QTest::newRow("Uninstall autodependency packages")
                << ":///data/installPackagesRepository"
                << (QStringList() << "componentA" << "componentB")
                << PackageManagerCore::Success
                << componentResourceAfterInstall
                << filesAfterInstall
                << (QStringList() << "componentD")
                << PackageManagerCore::Success
                << componentResourceAfterInstall
                << filesAfterUninstall
                << deletedComponentResources;
    }

    void testUninstall()
    {
        QFETCH(QString, repository);
        QFETCH(QStringList, installComponents);
        QFETCH(PackageManagerCore::Status, status);
        QFETCH(ComponentResourceHash, componentResources);
        QFETCH(QStringList, installedFiles);
        QFETCH(QStringList, uninstallComponents);
        QFETCH(PackageManagerCore::Status, uninstallStatus);
        QFETCH(ComponentResourceHash, componentResourcesAfterUninstall);
        QFETCH(QStringList, installedFilesAfterUninstall);
        QFETCH(ComponentResourceHash, deletedComponentResources);

        QScopedPointer<PackageManagerCore> core(PackageManager::getPackageManagerWithInit
                (m_installDir, repository));

        QCOMPARE(status, core->installSelectedComponentsSilently(QStringList() << installComponents));
        for (const ComponentResource &resource : componentResources)
            VerifyInstaller::verifyInstallerResources(m_installDir, resource.first, resource.second);
        VerifyInstaller::verifyFileExistence(m_installDir, installedFiles);
        core->commitSessionOperations();
        core->setPackageManager();

        QCOMPARE(uninstallStatus, core->uninstallComponentsSilently(QStringList()
                        << uninstallComponents));
        QCOMPARE(uninstallStatus, core->status());
        for (const ComponentResource &resource : componentResourcesAfterUninstall)
            VerifyInstaller::verifyInstallerResources(m_installDir, resource.first, resource.second);
        for (const ComponentResource &resource : deletedComponentResources)
            VerifyInstaller::verifyInstallerResourceFileDeletion(m_installDir, resource.first, resource.second);
        VerifyInstaller::verifyFileExistence(m_installDir, installedFilesAfterUninstall);
    }

    void addToExistingInstall_data()
    {
        QTest::addColumn<QString>("repository");
        QTest::addColumn<QStringList>("installComponents");
        QTest::addColumn<PackageManagerCore::Status>("status");
        QTest::addColumn<ComponentResourceHash>("componentResources");
        QTest::addColumn<QStringList >("installedFiles");
        QTest::addColumn<QString>("newRepository");
        QTest::addColumn<QStringList>("installNewComponents");
        QTest::addColumn<PackageManagerCore::Status>("reinstallStatus");
        QTest::addColumn<ComponentResourceHash>("componentResourcesAfterReinstall");
        QTest::addColumn<QStringList >("installedFilesAfterReinstall");
        QTest::addColumn<ComponentResourceHash >("deletedComponentResources");

        /*********** New dependency in repository **********/
        ComponentResourceHash componentResourceAfterInstall;
        componentResourceAfterInstall.append(ComponentResource("componentA", "1.0.0content.txt")); //ComponentC depends on
        componentResourceAfterInstall.append(ComponentResource("componentB", "1.0.0content.txt")); //ComponentC depends on
        componentResourceAfterInstall.append(ComponentResource("componentC", "1.0.0content.txt")); //Selected
        componentResourceAfterInstall.append(ComponentResource("componentD", "1.0.0content.txt")); //Autodepend on componentA,componentB
        componentResourceAfterInstall.append(ComponentResource("componentE", "1.0.0content.txt")); //Forced
        componentResourceAfterInstall.append(ComponentResource("componentG", "1.0.0content.txt")); //Default
        componentResourceAfterInstall.append(ComponentResource("componentI", "1.0.0content.txt")); //Autodepend componentC

        ComponentResourceHash componentResourcesAfterReinstall;
        componentResourcesAfterReinstall = componentResourceAfterInstall;
        componentResourcesAfterReinstall.append(ComponentResource("componentH", "1.0.0content.txt"));

        ComponentResourceHash deletedComponentResources;
        // New dependency is added in repository from componentA to componentF, check that it is not installed
        deletedComponentResources.append(ComponentResource("componentF", "1.0.0content.txt"));

        QStringList filesAfterInstall;
        filesAfterInstall << "components.xml"
                          << "installcontent.txt" << "installcontentA.txt" << "installcontentB.txt"
                          << "installcontentC.txt" << "installcontentD.txt" << "installcontentE.txt"
                          << "installcontentG.txt" << "installcontentI.txt";

        QStringList filesAfterReinstall;
        filesAfterReinstall = filesAfterInstall;
        filesAfterReinstall << "installcontentH.txt";

        QTest::newRow("New dependency in repository")
                << ":///data/installPackagesRepository"
                << (QStringList() << "componentC")
                << PackageManagerCore::Success
                << componentResourceAfterInstall
                << filesAfterInstall
                << ":///data/installPackagesDependencyChanged"
                << (QStringList() << "componentH")
                << PackageManagerCore::Success
                << componentResourcesAfterReinstall
                << filesAfterReinstall
                << deletedComponentResources;
    }

    void addToExistingInstall()
    {
        QFETCH(QString, repository);
        QFETCH(QStringList, installComponents);
        QFETCH(PackageManagerCore::Status, status);
        QFETCH(ComponentResourceHash, componentResources);
        QFETCH(QStringList, installedFiles);
        QFETCH(QString, newRepository);
        QFETCH(QStringList, installNewComponents);
        QFETCH(PackageManagerCore::Status, reinstallStatus);
        QFETCH(ComponentResourceHash, componentResourcesAfterReinstall);
        QFETCH(QStringList, installedFilesAfterReinstall);
        QFETCH(ComponentResourceHash, deletedComponentResources);

        QScopedPointer<PackageManagerCore> core(PackageManager::getPackageManagerWithInit
                (m_installDir, repository));
        QCOMPARE(status, core->installSelectedComponentsSilently(installComponents));
        QCOMPARE(status, core->status());

        for (const ComponentResource &resource : componentResources)
            VerifyInstaller::verifyInstallerResources(m_installDir, resource.first, resource.second);
        VerifyInstaller::verifyFileExistence(m_installDir, installedFiles);

        core->reset();
        core->cancelMetaInfoJob(); //Call cancel to reset metadata so that update repositories are fetched

        QSet<Repository> repoList;
        Repository repo = Repository::fromUserInput(newRepository);
        repoList.insert(repo);
        core->settings().setDefaultRepositories(repoList);

        QCOMPARE(reinstallStatus, core->installSelectedComponentsSilently(installNewComponents));

        for (const ComponentResource &resource : deletedComponentResources)
            VerifyInstaller::verifyInstallerResourceFileDeletion(m_installDir, resource.first, resource.second);
        for (const ComponentResource &resource : componentResourcesAfterReinstall)
            VerifyInstaller::verifyInstallerResources(m_installDir, resource.first, resource.second);
        VerifyInstaller::verifyFileExistence(m_installDir, installedFilesAfterReinstall);
    }

    void testInstallDefaultPackagesSilently()
    {
        QScopedPointer<PackageManagerCore> core(PackageManager::getPackageManagerWithInit
                (m_installDir, ":///data/installPackagesRepository"));
        QCOMPARE(PackageManagerCore::Success, core->installDefaultComponentsSilently());
        QCOMPARE(PackageManagerCore::Success, core->status());
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentA", "1.0.0content.txt"); //Dependency for componentG
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentE", "1.0.0content.txt"); //ForcedInstall
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentG", "1.0.0content.txt"); //Default
        VerifyInstaller::verifyFileExistence(m_installDir, QStringList() << "components.xml" << "installcontent.txt"
                            << "installcontentA.txt" << "installcontentE.txt" << "installcontentG.txt");
    }


    void testNoDefaultInstallations()
    {
        QScopedPointer<PackageManagerCore> core(PackageManager::getPackageManagerWithInit
                (m_installDir, ":///data/installPackagesRepository"));
        core->setNoDefaultInstallation(true);
        QCOMPARE(PackageManagerCore::Success, core->installDefaultComponentsSilently());
        QCOMPARE(PackageManagerCore::Success, core->status());
        VerifyInstaller::verifyFileExistence(m_installDir, QStringList() << "components.xml"
                            << "installcontentE.txt");
        core->setNoDefaultInstallation(false);
    }

    void testRemoveAllSilently()
    {
        QScopedPointer<PackageManagerCore> core(PackageManager::getPackageManagerWithInit
                (m_installDir, ":///data/installPackagesRepository"));
        QCOMPARE(PackageManagerCore::Success, core->installSelectedComponentsSilently(QStringList()
            << QLatin1String("componentA")));
        VerifyInstaller::verifyFileExistence(m_installDir, QStringList() << "components.xml" << "installcontentE.txt"
                            << "installcontentA.txt" << "installcontent.txt" << "installcontentG.txt");

        core->commitSessionOperations();
        core->setUninstaller();
        QCOMPARE(PackageManagerCore::Success, core->removeInstallationSilently());
        QCOMPARE(PackageManagerCore::Success, core->status());
        VerifyInstaller::verifyInstallerResourcesDeletion(m_installDir, "componentA");
        VerifyInstaller::verifyInstallerResourcesDeletion(m_installDir, "componentE");
        VerifyInstaller::verifyInstallerResourcesDeletion(m_installDir, "componentG");

        // On Windows we have to settle for the resources check above as maintenance
        // tool (if it would exists) and target directory are only removed later via
        // started VBScript process. On Unix platforms the target directory should
        // be removed in PackageManagerCorePrivate::runUninstaller().
#if defined(Q_OS_UNIX)
        QVERIFY(!QDir(m_installDir).exists());
#endif
    }

    void testUninstallVirtualSetVisibleSilently()
    {
        QScopedPointer<PackageManagerCore> core(PackageManager::getPackageManagerWithInit
                (m_installDir, ":///data/installPackagesRepository"));
        core->setVirtualComponentsVisible(true);
        QCOMPARE(PackageManagerCore::Success, core->installSelectedComponentsSilently(QStringList()
                <<"componentH"));
        QCOMPARE(PackageManagerCore::Success, core->status());
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentH", "1.0.0content.txt");

        core->commitSessionOperations();
        core->setPackageManager();
        QCOMPARE(PackageManagerCore::Success, core->uninstallComponentsSilently(QStringList()
                << "componentH"));
        QCOMPARE(PackageManagerCore::Success, core->status());
        VerifyInstaller::verifyInstallerResourcesDeletion(m_installDir, "componentH");
    }

    void testFileQuery()
    {
        QScopedPointer<PackageManagerCore> core(PackageManager::getPackageManagerWithInit(m_installDir,
                                    ":///data/filequeryrepository"));
        core->setCommandLineInstance(true);
        core->setFileDialogAutomaticAnswer("ValidDirectory", m_installDir);

        QString testFile = qApp->applicationDirPath() + QDir::toNativeSeparators("/test");
        QFile file(testFile);
        QVERIFY(file.open(QIODevice::WriteOnly));
        core->setFileDialogAutomaticAnswer("ValidFile", testFile);

        //File dialog launched without ID
        core->setFileDialogAutomaticAnswer("GetExistingDirectory", m_installDir);
        core->setFileDialogAutomaticAnswer("GetExistingFile", testFile);

        QCOMPARE(PackageManagerCore::Success, core->installDefaultComponentsSilently());
        QCOMPARE(PackageManagerCore::Success, core->status());

        QVERIFY(core->containsFileDialogAutomaticAnswer("ValidFile"));
        core->removeFileDialogAutomaticAnswer("ValidFile");
        QVERIFY(!core->containsFileDialogAutomaticAnswer("ValidFile"));

        QVERIFY(file.remove());
    }

    void testPostScript()
    {
        QScopedPointer<PackageManagerCore> core(PackageManager::getPackageManagerWithInit
                                                (m_installDir, ":///data/installPackagesRepository"));
        QCOMPARE(PackageManagerCore::Success, core->installSelectedComponentsSilently(QStringList() << QLatin1String("componentJ")));
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentJ", "1.0.0content.txt"); //Selected
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentA", "1.0.0content.txt"); //Dependency for componentG
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentE", "1.0.0content.txt"); //ForcedInstall
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentG", "1.0.0content.txt"); //Default

        //componentJ is extracted to "extractToAnotherPath" -folder in post install script
        bool fileExists = QFileInfo::exists(m_installDir + QDir::separator() + "extractToAnotherPath" + QDir::separator() + "installcontentJ.txt");
        QVERIFY2(fileExists, QString("File \"%1\" does not exist.").arg("installcontentJ.txt").toLatin1());
    }

    void init()
    {
        m_installDir = QInstaller::generateTemporaryFileName();
        QVERIFY(QDir().mkpath(m_installDir));
    }

    void initTestCase()
    {
        qSetGlobalQHashSeed(0); //Ensures the dom document deterministic behavior
    }

    void cleanup()
    {
        QDir dir(m_installDir);
        QVERIFY(dir.removeRecursively());
    }

private:
    QString m_installDir;
};


QTEST_MAIN(tst_CommandLineInstall)

#include "tst_commandlineinstall.moc"
