/**************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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
#include <graph.h>
#include <installercalculator.h>
#include <uninstallercalculator.h>
#include <componentchecker.h>
#include <packagemanagercore.h>

#include <QTest>

using namespace QInstaller;

typedef QMap<Component *, QStringList> ComponentToStringList;

class Data {
public:
    Data() {}
    explicit Data(const QString &data)
        : m_data(data) {}
    inline uint qHash(const Data &test);
    QString data() const { return m_data; }
    bool operator==(const Data &rhs) const { return m_data == rhs.m_data; }
    const Data &operator=(const Data &rhs) { if (this != &rhs) { m_data = rhs.m_data; } return *this; }

private:
    QString m_data;
};
inline uint qHash(const Data &data)
{
    return qHash(data.data());
}

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

class tst_Solver : public QObject
{
    Q_OBJECT

private slots:
    // TODO: add failing cases
    void sortGraph()
    {
        Graph<QString> graph;
        graph.addNode("Hut");
        graph.addEdge("Jacke", "Shirt");
        graph.addEdge("Guertel", "Hose");
        graph.addEdge("Guertel", "Shirt");
        graph.addEdge("Shirt", "Socken");
        graph.addEdge("Socken", "Unterwaesche");
        graph.addEdge("Shirt", "Unterwaesche");
        graph.addEdges("Hose", QStringList() << "Unterwaesche" << "Socken");
        graph.addEdges("Krawatte", QStringList() << "Shirt" << "Hose" << "Guertel");
        graph.addEdges("Schuhe", QStringList() << "Socken" << "Unterwaesche" << "Hose");
        graph.addEdges("Jacke", QStringList() << "Hose" << "Shirt" << "Guertel" << "Schuhe" << "Krawatte");

        QList<QString> resolved = graph.sort();
        foreach (const QString &value, resolved)
            qDebug("%s", qPrintable(value));
    }

    void sortGraphReverse()
    {
        Graph<QString> graph;
        graph.addEdge("Krawatte", "Jacke");
        graph.addEdge("Guertel", "Jacke");
        graph.addEdge("Shirt", "Guertel");
        graph.addEdges("Shirt", QList<QString>() << "Krawatte" << "Schuhe");
        graph.addEdges("Hose", QList<QString>() << "Schuhe" << "Guertel" << "Shirt");
        graph.addEdges("Socken", QList<QString>() << "Schuhe" << "Hose");
        graph.addEdges("Unterwaesche", QList<QString>() << "Socken" << "Hose" << "Guertel" << "Shirt"
            << "Krawatte" << "Schuhe");

        QList<QString> resolved = graph.sortReverse();
        foreach (const QString &value, resolved)
            qDebug("%s", qPrintable(value));
    }

    void sortGraphCycle()
    {
        Data a("A"), b("B"), c("C"), d("D"), e("E");

        Graph<Data> graph;
        graph.addEdge(a, b);
        graph.addEdge(b, c);
        graph.addEdge(c, d);
        graph.addEdge(d, e);
        graph.addEdge(e, a);

        QList<Data> resolved = graph.sort();
        foreach (const Data &value, resolved)
            qDebug("%s", qPrintable(value.data()));

        QPair<Data, Data> cycle = graph.cycle();
        qDebug("Found cycle: %s", graph.hasCycle() ? "true" : "false");
        qDebug("(%s) has a indirect dependency on (%s).", qPrintable(cycle.second.data()),
            qPrintable(cycle.first.data()));
    }

    void resolveInstaller_data()
    {
        QTest::addColumn<PackageManagerCore *>("core");
        QTest::addColumn<QList<Component *> >("selectedComponents");
        QTest::addColumn<QList<Component *> >("expectedResult");
        QTest::addColumn<QList<int> >("installReason");

        PackageManagerCore *core = new PackageManagerCore();
        core->setPackageManager();
        NamedComponent *componentA = new NamedComponent(core, QLatin1String("A"));
        NamedComponent *componentAA = new NamedComponent(core, QLatin1String("A.A"));
        NamedComponent *componentAB = new NamedComponent(core, QLatin1String("A.B"));
        componentA->appendComponent(componentAA);
        componentA->appendComponent(componentAB);
        NamedComponent *componentB = new NamedComponent(core, QLatin1String("B"));
        componentB->addDependency(QLatin1String("A.B"));
        core->appendRootComponent(componentA);
        core->appendRootComponent(componentB);

        QTest::newRow("Installer resolved") << core
                    << (QList<Component *>() << componentB)
                    << (QList<Component *>() << componentAB << componentB)
                    << (QList<int>()
                        << InstallerCalculator::Dependent
                        << InstallerCalculator::Resolved);
    }

    void resolveInstaller()
    {
        QFETCH(PackageManagerCore *, core);
        QFETCH(QList<Component *> , selectedComponents);
        QFETCH(QList<Component *> , expectedResult);
        QFETCH(QList<int>, installReason);

        InstallerCalculator calc(core->components(PackageManagerCore::ComponentType::AllNoReplacements));
        calc.appendComponentsToInstall(selectedComponents);
        QList<Component *> result = calc.orderedComponentsToInstall();

        QCOMPARE(result.count(), expectedResult.count());
        for (int i = 0; i < result.count(); i++) {
            QCOMPARE(result.at(i), expectedResult.at(i));
            QCOMPARE((int)calc.installReasonType(result.at(i)), installReason.at(i));
        }
        delete core;
    }

    void resolveUninstaller_data()
    {
        QTest::addColumn<PackageManagerCore *>("core");
        QTest::addColumn<QList<Component *> >("selectedToUninstall");
        QTest::addColumn<QList<Component *> >("installedComponents");
        QTest::addColumn<QSet<Component *> >("expectedResult");

        PackageManagerCore *core = new PackageManagerCore();
        core->setPackageManager();
        NamedComponent *componentA = new NamedComponent(core, QLatin1String("A"));
        NamedComponent *componentAA = new NamedComponent(core, QLatin1String("A.A"));
        NamedComponent *componentAB = new NamedComponent(core, QLatin1String("A.B"));
        componentA->appendComponent(componentAA);
        componentA->appendComponent(componentAB);
        NamedComponent *componentB = new NamedComponent(core, QLatin1String("B"));
        componentB->addDependency(QLatin1String("A.B"));
        core->appendRootComponent(componentA);
        core->appendRootComponent(componentB);

        componentA->setInstalled();
        componentB->setInstalled();
        componentAB->setInstalled();

        QTest::newRow("Uninstaller resolved") << core
                    << (QList<Component *>() << componentAB)
                    << (QList<Component *>() << componentA << componentB)
                    << (QSet<Component *>() << componentAB << componentB);

        core = new PackageManagerCore();
        core->setPackageManager();
        NamedComponent *compA = new NamedComponent(core, QLatin1String("A"));
        NamedComponent *compB = new NamedComponent(core, QLatin1String("B"));
        NamedComponent *compC = new NamedComponent(core, QLatin1String("C"));
        compB->addDependency(QLatin1String("A"));
        compC->addDependency(QLatin1String("B"));
        core->appendRootComponent(compA);
        core->appendRootComponent(compB);
        core->appendRootComponent(compC);
        compA->setInstalled();
        compB->setInstalled();

        QTest::newRow("Cascade dependencies") << core
                    << (QList<Component *>() << compA)
                    << (QList<Component *>() << compB)
                    << (QSet<Component *>() << compA << compB);
    }

    void resolveUninstaller()
    {
        QFETCH(PackageManagerCore *, core);
        QFETCH(QList<Component *> , selectedToUninstall);
        QFETCH(QList<Component *> , installedComponents);
        QFETCH(QSet<Component *> , expectedResult);

        UninstallerCalculator calc(installedComponents);
        calc.appendComponentsToUninstall(selectedToUninstall);
        QSet<Component *> result = calc.componentsToUninstall();

        QCOMPARE(result.count(), expectedResult.count());
        QCOMPARE(result, expectedResult);
        delete core;
    }


    void checkComponent_data()
    {
        QTest::addColumn<QList<Component *> >("componentsToCheck");
        QTest::addColumn<ComponentToStringList>("expectedResult");

        PackageManagerCore *core = new PackageManagerCore();
        core->setPackageManager();

        NamedComponent *componentA = new NamedComponent(core, QLatin1String("A"));
        NamedComponent *componentB = new NamedComponent(core, QLatin1String("B"));

        componentB->setValue(QLatin1String("AutoDependOn"), QLatin1String("A"));
        componentB->setValue(QLatin1String("Default"), QLatin1String("true"));
        core->appendRootComponent(componentA);
        core->appendRootComponent(componentB);

        ComponentToStringList result;
        result[componentB].append(QLatin1String("Component B specifies \"Default\" property "
            "together with \"AutoDependOn\" list. This combination of states "
            "may not work properly."));

        QTest::newRow("AutoDepend and default")
                << (QList<Component *>() << componentA << componentB)
                << result;

        NamedComponent *componentC = new NamedComponent(core, QLatin1String("C"));
        NamedComponent *componentD = new NamedComponent(core, QLatin1String("D"));
        NamedComponent *componentE = new NamedComponent(core, QLatin1String("E"));

        componentD->setValue(QLatin1String("AutoDependOn"), QLatin1String("C"));
        componentE->addDependency(QLatin1String("D"));
        core->appendRootComponent(componentC);
        core->appendRootComponent(componentD);
        core->appendRootComponent(componentE);

        result.clear();
        result[componentD].append(QLatin1String("Other components depend on auto dependent "
            "component D. This may not work properly."));

        QTest::newRow("AutoDepend and dependency")
                << (QList<Component *>() << componentC << componentD << componentE)
                << result;

    }

    void checkComponent()
    {
        QFETCH(QList<Component *>, componentsToCheck);
        QFETCH(ComponentToStringList, expectedResult);

        foreach (Component *component, componentsToCheck) {
            const QStringList result = ComponentChecker::checkComponent(component);
            QCOMPARE(result, expectedResult.value(component));
        }
    }
};

QTEST_MAIN(tst_Solver)

#include "tst_solver.moc"
