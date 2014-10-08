/**************************************************************************
**
** Copyright (C) 2012-2013 Digia Plc and/or its subsidiary(-ies).
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

#include <component.h>
#include <graph.h>
#include <installercalculator.h>
#include <packagemanagercore.h>

#include <QTest>

using namespace QInstaller;

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

    void resolve_data()
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

        QTest::newRow("Simple resolved") << core
                    << (QList<Component *>() << componentB)
                    << (QList<Component *>() << componentAB << componentB)
                    << (QList<int>()
                        << InstallerCalculator::Dependent
                        << InstallerCalculator::Resolved);
    }

    void resolve()
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
};

QTEST_MAIN(tst_Solver)

#include "tst_solver.moc"
