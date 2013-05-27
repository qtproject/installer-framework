#include "component.h"
#include "componentmodel.h"

#include "kdupdaterupdatesinfo_p.h"
#include "kdupdaterupdatesourcesinfo.h"

#include "packagemanagercore.h"

#include <QTest>

using namespace KDUpdater;
using namespace QInstaller;

#define EXPECTED_COUNT_VIRTUALS_VISIBLE 8
#define EXPECTED_COUNT_VIRTUALS_INVISIBLE 7

static const char vendorProduct[] = "com.vendor.product";
static const char vendorSecondProduct[] = "com.vendor.second.product";
static const char vendorSecondProductSub[] = "com.vendor.second.product.sub";
static const char vendorSecondProductSub1[] = "com.vendor.second.product.sub1";
static const char vendorSecondProductVirtual[] = "com.vendor.second.product.virtual";
static const char vendorSecondProductSubnode[] = "com.vendor.second.product.subnode";
static const char vendorSecondProductSubnodeSub[] = "com.vendor.second.product.subnode.sub";
static const char vendorThirdProductVirtual[] = "com.vendor.third.product.virtual";

class tst_ComponentModel : public QObject
{
    Q_OBJECT

public:
    enum Option {
        NoFlags = 0x00,
        VirtualsVisible = 0x01,
        NoForcedInstallation = 0x02
    };
    Q_DECLARE_FLAGS(Options, Option);

private slots:
    void initTestCase()
    {
        m_defaultChecked << vendorProduct << vendorSecondProductSub;
        m_defaultPartially << vendorSecondProduct;
        m_defaultUnchecked << vendorSecondProductSub1 << vendorSecondProductSubnode
            << vendorSecondProductSubnodeSub;
    }

    void testNameToIndexAndIndexToName()
    {
        setPackageManagerOptions(NoFlags);

        QList<Component*> rootComponents = loadComponents();
        testComponentsLoaded(rootComponents);

        // setup the model with 1 column
        ComponentModel model(1, &m_core);
        model.setRootComponents(rootComponents);

        // all names should be resolvable, virtual components are not indexed if they are not visible
        QStringList all;
        all << m_defaultChecked << m_defaultPartially << m_defaultUnchecked;
        foreach (const QString &name, all) {
            QVERIFY(model.indexFromComponentName(name).isValid());
            QVERIFY(model.componentFromIndex(model.indexFromComponentName(name)) != 0);
            QCOMPARE(model.componentFromIndex(model.indexFromComponentName(name))->name(), name);
        }

        foreach (Component *const component, rootComponents)
            delete component;
    }

    void testNameToIndexAndIndexToNameVirtualsVisible()
    {
        setPackageManagerOptions(VirtualsVisible);

        QList<Component*> rootComponents = loadComponents();
        testComponentsLoaded(rootComponents);

        // setup the model with 1 column
        ComponentModel model(1, &m_core);
        model.setRootComponents(rootComponents);

        // all names should be resolvable, including virtual components
        QStringList all;
        all << m_defaultChecked << m_defaultPartially << m_defaultUnchecked << vendorSecondProductVirtual
            << vendorThirdProductVirtual;
        foreach (const QString &name, all) {
            QVERIFY(model.indexFromComponentName(name).isValid());
            QVERIFY(model.componentFromIndex(model.indexFromComponentName(name)) != 0);
            QCOMPARE(model.componentFromIndex(model.indexFromComponentName(name))->name(), name);
        }

        foreach (Component *const component, rootComponents)
            delete component;
    }

    void testDefault()
    {
        setPackageManagerOptions(NoFlags);

        QList<Component*> rootComponents = loadComponents();
        testComponentsLoaded(rootComponents);

        // setup the model with 1 column
        ComponentModel model(1, &m_core);
        model.setRootComponents(rootComponents);
        testDefaultInheritedModelBehavior(&model, 1);

        QCOMPARE(model.core(), &m_core);
        QCOMPARE(model.checkedState(), ComponentModel::DefaultChecked);
        testModelState(&model, m_defaultChecked, m_defaultPartially, m_defaultUnchecked);

        foreach (Component *const component, rootComponents)
            delete component;
    }

    void testVirtualsVisible()
    {
        setPackageManagerOptions(VirtualsVisible);

        QList<Component*> rootComponents = loadComponents();
        testComponentsLoaded(rootComponents);

        // setup the model with 1 column
        ComponentModel model(1, &m_core);
        model.setRootComponents(rootComponents);
        testDefaultInheritedModelBehavior(&model, 1);

        QCOMPARE(model.checkedState(), ComponentModel::DefaultChecked);
        // the virtual components are not checked
        testModelState(&model, m_defaultChecked, m_defaultPartially, m_defaultUnchecked
             + QStringList(vendorSecondProductVirtual) << vendorThirdProductVirtual);

        foreach (Component *const component, rootComponents)
            delete component;
    }

    void testNoForcedInstallation()
    {
        setPackageManagerOptions(NoForcedInstallation);

        QList<Component*> rootComponents = loadComponents();
        testComponentsLoaded(rootComponents);

        // setup the model with 1 column
        ComponentModel model(1, &m_core);
        model.setRootComponents(rootComponents);
        testDefaultInheritedModelBehavior(&model, 1);

        QCOMPARE(model.checkedState(), ComponentModel::DefaultChecked);
        testModelState(&model, m_defaultChecked, m_defaultPartially, m_defaultUnchecked);

        foreach (Component *const component, rootComponents)
            delete component;
    }

    void testVirtualsVisibleNoForcedInstallation()
    {
        setPackageManagerOptions(Options(VirtualsVisible | NoForcedInstallation));

        QList<Component*> rootComponents = loadComponents();
        testComponentsLoaded(rootComponents);

        // setup the model with 1 column
        ComponentModel model(1, &m_core);
        model.setRootComponents(rootComponents);
        testDefaultInheritedModelBehavior(&model, 1);

        QCOMPARE(model.checkedState(), ComponentModel::DefaultChecked);
        // the virtual components are not checked
        testModelState(&model, m_defaultChecked, m_defaultPartially, m_defaultUnchecked
            + QStringList(vendorSecondProductVirtual) << vendorThirdProductVirtual);

        foreach (Component *const component, rootComponents)
            delete component;
    }

    void testSelect()
    {
        setPackageManagerOptions(NoFlags);

        QList<Component*> rootComponents = loadComponents();
        testComponentsLoaded(rootComponents);

        // setup the model with 1 column
        ComponentModel model(1, &m_core);
        model.setRootComponents(rootComponents);
        testDefaultInheritedModelBehavior(&model, 1);

        // select all possible components
        model.setCheckedState(ComponentModel::AllChecked);
        QCOMPARE(model.checkedState(), ComponentModel::AllChecked);
        testModelState(&model, m_defaultChecked + m_defaultPartially + m_defaultUnchecked, QStringList()
            , QStringList());

        // deselect all possible components
        // as the first root is a forced install, should result in partially checked state
        model.setCheckedState(ComponentModel::AllUnchecked);
        QCOMPARE(model.checkedState(), ComponentModel::PartiallyChecked);
        testModelState(&model, QStringList() << vendorProduct, QStringList(), m_defaultPartially
            + m_defaultUnchecked + QStringList(vendorSecondProductSub));

        // reset all possible components
        model.setCheckedState(ComponentModel::DefaultChecked);
        QCOMPARE(model.checkedState(), ComponentModel::DefaultChecked);
        testModelState(&model, m_defaultChecked, m_defaultPartially, m_defaultUnchecked);

        foreach (Component *const component, rootComponents)
            delete component;
    }

    void testSelectVirtualsVisible()
    {
        setPackageManagerOptions(VirtualsVisible);

        QList<Component*> rootComponents = loadComponents();
        testComponentsLoaded(rootComponents);

        // setup the model with 1 column
        ComponentModel model(1, &m_core);
        model.setRootComponents(rootComponents);
        testDefaultInheritedModelBehavior(&model, 1);

        // select all possible components
        model.setCheckedState(ComponentModel::AllChecked);
        QCOMPARE(model.checkedState(), ComponentModel::AllChecked);
        testModelState(&model, m_defaultChecked + m_defaultPartially + m_defaultUnchecked
            + QStringList(vendorSecondProductVirtual) << vendorThirdProductVirtual, QStringList(),
            QStringList());

        // deselect all possible components
        // as the first root is a forced install, should result in partially checked state
        model.setCheckedState(ComponentModel::AllUnchecked);
        QCOMPARE(model.checkedState(), ComponentModel::PartiallyChecked);
        testModelState(&model, QStringList() << vendorProduct, QStringList(), m_defaultPartially
            + m_defaultUnchecked + QStringList(vendorSecondProductSub) << vendorSecondProductVirtual
            << vendorThirdProductVirtual);

        // reset all possible components
        model.setCheckedState(ComponentModel::DefaultChecked);
        QCOMPARE(model.checkedState(), ComponentModel::DefaultChecked);
        testModelState(&model, m_defaultChecked, m_defaultPartially, m_defaultUnchecked
            + QStringList(vendorSecondProductVirtual) << vendorThirdProductVirtual);

        foreach (Component *const component, rootComponents)
            delete component;
    }

    void testSelectNoForcedInstallation()
    {
        setPackageManagerOptions(NoForcedInstallation);

        QList<Component*> rootComponents = loadComponents();
        testComponentsLoaded(rootComponents);

        // setup the model with 1 column
        ComponentModel model(1, &m_core);
        model.setRootComponents(rootComponents);
        testDefaultInheritedModelBehavior(&model, 1);

        // select all possible components
        model.setCheckedState(ComponentModel::AllChecked);
        QCOMPARE(model.checkedState(), ComponentModel::AllChecked);
        testModelState(&model, m_defaultChecked + m_defaultPartially + m_defaultUnchecked, QStringList()
            , QStringList());

        // deselect all possible components
        model.setCheckedState(ComponentModel::AllUnchecked);
        QCOMPARE(model.checkedState(), ComponentModel::AllUnchecked);
        testModelState(&model, QStringList(), QStringList(), m_defaultPartially + m_defaultUnchecked
            + QStringList(vendorSecondProductSub) << vendorProduct);

        // reset all possible components
        model.setCheckedState(ComponentModel::DefaultChecked);
        QCOMPARE(model.checkedState(), ComponentModel::DefaultChecked);
        testModelState(&model, m_defaultChecked, m_defaultPartially, m_defaultUnchecked);

        foreach (Component *const component, rootComponents)
            delete component;
    }

    void testSelectVirtualsVisibleNoForcedInstallation()
    {
        setPackageManagerOptions(Options(VirtualsVisible | NoForcedInstallation));

        QList<Component*> rootComponents = loadComponents();
        testComponentsLoaded(rootComponents);

        // setup the model with 1 column
        ComponentModel model(1, &m_core);
        model.setRootComponents(rootComponents);
        testDefaultInheritedModelBehavior(&model, 1);

        // select all possible components
        model.setCheckedState(ComponentModel::AllChecked);
        QCOMPARE(model.checkedState(), ComponentModel::AllChecked);
        testModelState(&model, m_defaultChecked + m_defaultPartially + m_defaultUnchecked
            + QStringList(vendorSecondProductVirtual) << vendorThirdProductVirtual, QStringList(),
            QStringList());

        // deselect all possible components
        model.setCheckedState(ComponentModel::AllUnchecked);
        QCOMPARE(model.checkedState(), ComponentModel::AllUnchecked);
        testModelState(&model, QStringList(), QStringList(), m_defaultPartially + m_defaultUnchecked
            + QStringList(vendorSecondProductSub) << vendorSecondProductVirtual << vendorProduct
             << vendorThirdProductVirtual);

        // reset all possible components
        model.setCheckedState(ComponentModel::DefaultChecked);
        QCOMPARE(model.checkedState(), ComponentModel::DefaultChecked);
        testModelState(&model, m_defaultChecked, m_defaultPartially, m_defaultUnchecked
            + QStringList(vendorSecondProductVirtual) << vendorThirdProductVirtual);

        foreach (Component *const component, rootComponents)
            delete component;
    }

private:
    void setPackageManagerOptions(Options flags) const
    {
        m_core.setNoForceInstallation(flags.testFlag(NoForcedInstallation));
        m_core.setVirtualComponentsVisible(flags.testFlag(VirtualsVisible));
    }

    void testComponentsLoaded(const QList<Component *> &rootComponents) const
    {
        // we need to have three root components
        QCOMPARE(rootComponents.count(), 3);

        QList<Component*> components = rootComponents;
        foreach (Component *const component, rootComponents)
            components.append(component->childItems());

        // will differ between 6 and 7 components, (2 root nodes, 1 sub node, 3 non virtual and 1 virtual)
        QCOMPARE(components.count(), m_core.virtualComponentsVisible() ? EXPECTED_COUNT_VIRTUALS_VISIBLE
            : EXPECTED_COUNT_VIRTUALS_INVISIBLE);
    }

    void testDefaultInheritedModelBehavior(ComponentModel *model, int columnCount) const
    {
        // row count with invalid model index should return:
        if (m_core.virtualComponentsVisible())
            QCOMPARE(model->rowCount(), 3); // 3 (2 non virtual and 1 virtual root component)
        else
            QCOMPARE(model->rowCount(), 2); // 2 (the 2 non virtual root components)
        QCOMPARE(model->columnCount(), columnCount);

        const QModelIndex firstParent = model->indexFromComponentName(vendorProduct);
        const QModelIndex secondParent = model->indexFromComponentName(vendorSecondProduct);
        const QModelIndex thirdParent = model->indexFromComponentName(vendorThirdProductVirtual);

        // return invalid indexes, as they are the root components
        QCOMPARE(model->parent(firstParent), QModelIndex());
        QCOMPARE(model->parent(secondParent), QModelIndex());
        QCOMPARE(model->parent(thirdParent), QModelIndex());

        // test valid indexes
        QVERIFY(firstParent.isValid());
        QVERIFY(secondParent.isValid());
        QVERIFY(model->indexFromComponentName(vendorSecondProductSub).isValid());
        QVERIFY(model->indexFromComponentName(vendorSecondProductSub1).isValid());
        QVERIFY(model->indexFromComponentName(vendorSecondProductSubnode).isValid());
        QVERIFY(model->indexFromComponentName(vendorSecondProductSubnodeSub).isValid());

        // they should have the same parent
        QCOMPARE(model->parent(model->indexFromComponentName(vendorSecondProductSub)), secondParent);
        QCOMPARE(model->parent(model->indexFromComponentName(vendorSecondProductSub1)), secondParent);
        QCOMPARE(model->parent(model->indexFromComponentName(vendorSecondProductSubnode)), secondParent);

        const QModelIndex forthParent = model->indexFromComponentName(vendorSecondProductSubnode);
        QCOMPARE(model->parent(model->indexFromComponentName(vendorSecondProductSubnodeSub)), forthParent);

        // row count should be 0, as they have no children
        QCOMPARE(model->rowCount(firstParent), 0);
        QCOMPARE(model->rowCount(model->indexFromComponentName(vendorSecondProductSub)), 0);
        QCOMPARE(model->rowCount(model->indexFromComponentName(vendorSecondProductSub1)), 0);
        QCOMPARE(model->rowCount(model->indexFromComponentName(vendorSecondProductSubnodeSub)), 0);

        if (m_core.virtualComponentsVisible()) {
            // test valid index
            QVERIFY(thirdParent.isValid());
            QCOMPARE(model->rowCount(thirdParent), 0);

            QVERIFY(model->indexFromComponentName(vendorSecondProductVirtual).isValid());
            QCOMPARE(model->rowCount(model->indexFromComponentName(vendorSecondProductVirtual)), 0);
            QCOMPARE(model->parent(model->indexFromComponentName(vendorSecondProductVirtual)), secondParent);
            // row count should be 4, (2 standard, 1 virtual, 1 sub node)
            QCOMPARE(model->rowCount(secondParent), 4);
        } else {
            // test invalid index
            QVERIFY(!thirdParent.isValid());

            // row count should be 3, (2 standard, 1 sub node, omit the virtual)
            QCOMPARE(model->rowCount(secondParent), 3);
        }
        // row count should be 1, 1 sub node
        QCOMPARE(model->rowCount(model->indexFromComponentName(vendorSecondProductSubnode)), 1);
    }

    void testModelState(ComponentModel *model, const QStringList &checked, const QStringList &partially,
        const QStringList &unchecked) const
    {
        QCOMPARE(model->checked().count(), checked.count());
        QCOMPARE(model->partially().count(), partially.count());
        QCOMPARE(model->unchecked().count(), unchecked.count());

        // these components should have checked state
        foreach (Component *const component, model->checked())
            QVERIFY(checked.contains(component->name()));

        // these components should not have partially checked state
        foreach (Component *const component, model->partially())
            QVERIFY(partially.contains(component->name()));

        // these components should not have checked state
        foreach (Component *const component, model->unchecked())
            QVERIFY(unchecked.contains(component->name()));
    }

    QList<Component*> loadComponents() const
    {
        UpdatesInfo updatesInfo;
        updatesInfo.setFileName(":///data/updates.xml");
        const QList<UpdateInfo> updateInfos = updatesInfo.updatesInfo();

        QHash<QString, Component*> components;
        foreach (const UpdateInfo &info, updateInfos) {
            Component *component = new Component(const_cast<PackageManagerCore *>(&m_core));

            // we need at least these to be able to test the model
            component->setValue("Name", info.data.value("Name").toString());
            component->setValue("Default", info.data.value("Default").toString());
            component->setValue("Virtual", info.data.value("Virtual").toString());
            component->setValue("DisplayName", info.data.value("DisplayName").toString());

            QString forced = info.data.value("ForcedInstallation", scFalse).toString().toLower();
            if (m_core.noForceInstallation())
                forced = scFalse;
            component->setValue("ForcedInstallation", forced);
            if (forced == scTrue) {
                component->setEnabled(false);
                component->setCheckable(false);
                component->setCheckState(Qt::Checked);
            }
            components.insert(component->name(), component);
        }

        QList <Component*> rootComponents;
        foreach (QString id, components.keys()) {
            QInstaller::Component *component = components.value(id);
            while (!id.isEmpty() && component->parentComponent() == 0) {
                id = id.section(QLatin1Char('.'), 0, -2);
                if (components.contains(id))
                    components[id]->appendComponent(component);
            }
        }

        foreach (QInstaller::Component *component, components) {
            if (component->parentComponent() == 0)
                rootComponents.append(component);

            if (component->isCheckable() && component->isDefault() && (!component->isTristate()))
                component->setCheckState(Qt::Checked);
        }
        return rootComponents;
    }

private:
    PackageManagerCore m_core;

    QStringList m_defaultChecked;
    QStringList m_defaultPartially;
    QStringList m_defaultUnchecked;
};
Q_DECLARE_OPERATORS_FOR_FLAGS(tst_ComponentModel::Options)

QTEST_MAIN(tst_ComponentModel)

#include "tst_componentmodel.moc"
