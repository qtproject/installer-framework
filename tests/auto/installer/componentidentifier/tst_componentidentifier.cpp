/**************************************************************************
**
** Copyright (C) 2017 Konstantin Podsvirov <konstantin@podsvirov.pro>
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

#include <component.h>
#include <packagemanagercore.h>

#include <QTest>

using namespace QInstaller;

class NamedComponent : public Component
{
public:
    NamedComponent(PackageManagerCore *core, const QString &name)
        : Component(core)
    {
        setValue(scName, name);
        setValue(scVersion, QLatin1String("1.0.0"));
    }

    NamedComponent(PackageManagerCore *core, const QString &name, const QString &version)
        : Component(core)
    {
        setValue(scName, name);
        setValue(scVersion, version);
    }

};

class tst_ComponentIdentifier : public QObject
{
    Q_OBJECT

private slots:
    void testPackageManagerCoreSetterGetter_data();
    void testPackageManagerCoreSetterGetter();

    void testComponentDependencies();
};

void tst_ComponentIdentifier::testPackageManagerCoreSetterGetter_data()
{
    const char *tag = 0;

    QTest::addColumn<PackageManagerCore *>("core");
    QTest::addColumn<QStringList>("requirements");
    QTest::addColumn<QList<Component *> >("expectedResult");

    PackageManagerCore *core = 0;
    QStringList req; // requirements;
    QList<Component *> res; // expectedResult;

    tag = "Alphabetic name";
    core = 0; req.clear(); res.clear();
    {
        core = new PackageManagerCore();
        core->setPackageManager();

        NamedComponent *componentRoot = new NamedComponent(core, QLatin1String("root"));
        core->appendRootComponent(componentRoot);

        req << QLatin1String("root");
        res << componentRoot;
    }
    QTest::newRow(tag) << core << req << res;

    tag = "Alphabetic name, dash (-) and numbered version";
    core = 0; req.clear(); res.clear();
    {
        core = new PackageManagerCore();
        core->setPackageManager();

        NamedComponent *componentRoot = new NamedComponent(core,
                                                           QLatin1String("root"),
                                                           QLatin1String("1.0.1"));
        core->appendRootComponent(componentRoot);

        req << QLatin1String("root-1.0.1");
        res << componentRoot;

        req << QLatin1String("root->1");
        res << componentRoot;

        req << QLatin1String("root-<2");
        res << componentRoot;

        req << QLatin1String("root-");
        res << componentRoot;
    }
    QTest::newRow(tag) << core << req << res;

    tag = "Alphabetic name, colon (:) and numbered version";
    core = 0; req.clear(); res.clear();
    {
        core = new PackageManagerCore();
        core->setPackageManager();

        NamedComponent *componentRoot = new NamedComponent(core,
                                                           QLatin1String("root"),
                                                           QLatin1String("1.0.1"));
        core->appendRootComponent(componentRoot);

        req << QLatin1String("root:1.0.1");
        res << componentRoot;

        req << QLatin1String("root:>1");
        res << componentRoot;

        req << QLatin1String("root:<2");
        res << componentRoot;

        req << QLatin1String("root:");
        res << componentRoot;
    }
    QTest::newRow(tag) << core << req << res;

    tag = "Kebab-case name, dash (-) and numbered version";
    core = 0; req.clear(); res.clear();
    {
        core = new PackageManagerCore();
        core->setPackageManager();

        NamedComponent *componentSdk = new NamedComponent(core,
                                                          QLatin1String("org.qt-project.sdk.qt"),
                                                          QLatin1String("4.5"));
        NamedComponent *componentQt = new NamedComponent(core,
                                                         QLatin1String("org.qt"),
                                                         QLatin1String("project.sdk.qt->=4.5"));
        componentQt->appendComponent(componentSdk);
        core->appendRootComponent(componentQt);

        // this example has been present for many years in
        // the PackageManagerCore::componentByName() method documentation,
        // but it does not work as expected
        // in this case, name is "org.qt" and version is "project.sdk.qt->=4.5"
        req << QLatin1String("org.qt-project.sdk.qt->=4.5");
        res << componentQt; // and you expected componentSdk?
    }
    QTest::newRow(tag) << core << req << res;

    tag = "Kebab-case name, colon (:) and numbered version";
    core = 0; req.clear(); res.clear();
    {
        core = new PackageManagerCore();
        core->setPackageManager();

        NamedComponent *componentSdk = new NamedComponent(core,
                                                           QLatin1String("org.qt-project.sdk.qt"),
                                                           QLatin1String("4.5"));
        core->appendRootComponent(componentSdk);


        req << QLatin1String("org.qt-project.sdk.qt:>=4.5");
        res << componentSdk; // work as expected

        req << QLatin1String("org.qt-project.sdk.qt");
        res << 0;

        // we should fix names with dash (-) while support dash (-) as separator
        req << PackageManagerCore::checkableName(QLatin1String("org.qt-project.sdk.qt"));
        res << componentSdk;
    }
    QTest::newRow(tag) << core << req << res;
}

void tst_ComponentIdentifier::testPackageManagerCoreSetterGetter()
{
    QFETCH(PackageManagerCore *, core);
    QFETCH(QStringList, requirements);
    QFETCH(QList<Component *>, expectedResult);

    QCOMPARE(requirements.count(), expectedResult.count());
    for (int i = 0; i < requirements.count(); ++i) {
        QCOMPARE(core->componentByName(requirements.at(i)), expectedResult.at(i));
    }

    delete core;
}

void tst_ComponentIdentifier::testComponentDependencies()
{
    PackageManagerCore *core = new PackageManagerCore();
    core->setPackageManager();

    Component *componentA = new NamedComponent(core, "A");
    Component *componentB = new NamedComponent(core, "B");
    Component *componentC = new NamedComponent(core, "component-C");
    Component *componentD = new NamedComponent(core, "D-backward-compatibility");
    Component *componentE = new NamedComponent(core, "E");

    componentA->addDependency("B-1.0.0");
    componentA->addDependency("component-C:1.0.0");
    componentA->addDependency("D-backward-compatibility:");

    componentE->addAutoDependOn("B-1.0.0");
    componentE->addAutoDependOn("component-C:1.0.0");
    componentE->addAutoDependOn("D-backward-compatibility:");

    core->appendRootComponent(componentA);
    core->appendRootComponent(componentB);
    core->appendRootComponent(componentC);
    core->appendRootComponent(componentD);
    core->appendRootComponent(componentE);

    QList<Component*> expectedResult = QList<Component*>()
            << componentB << componentC << componentD;

    QList<Component*> dependencies;
    foreach (const QString &identifier, componentA->dependencies()) {
        dependencies.append(core->componentByName(identifier));
    }

    QCOMPARE(expectedResult, dependencies);

    QList<Component*> autoDependOn;
    foreach (const QString &identifier, componentE->autoDependencies()) {
        autoDependOn.append(core->componentByName(identifier));
    }

    QCOMPARE(expectedResult, autoDependOn);

    delete core;
}

QTEST_MAIN(tst_ComponentIdentifier)

#include "tst_componentidentifier.moc"
