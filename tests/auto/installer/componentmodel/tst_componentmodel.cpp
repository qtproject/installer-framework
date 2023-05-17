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

#include "component.h"
#include "componentmodel.h"
#include "updatesinfo_p.h"
#include "packagemanagercore.h"

#include <QTest>
#include <QtCore/QLocale>

using namespace KDUpdater;
using namespace QInstaller;

#define EXPECTED_COUNT_VIRTUALS_VISIBLE 12
#define EXPECTED_COUNT_VIRTUALS_INVISIBLE 11

static const char vendorProduct[] = "com.vendor.product";
static const char vendorSecondProduct[] = "com.vendor.second.product";
static const char vendorSecondProductSub[] = "com.vendor.second.product.sub";
static const char vendorSecondProductSub1[] = "com.vendor.second.product.sub1";
static const char vendorSecondProductVirtual[] = "com.vendor.second.product.virtual";
static const char vendorSecondProductSubnode[] = "com.vendor.second.product.subnode";
static const char vendorSecondProductSubnodeSub[] = "com.vendor.second.product.subnode.sub";
static const char vendorThirdProductVirtual[] = "com.vendor.third.product.virtual";
static const char vendorFourthProductCheckable[] = "com.vendor.fourth.product.checkable";
static const char vendorFifthProductNonCheckable[] = "com.vendor.fifth.product.noncheckable";
static const char vendorFifthProductSub[] = "com.vendor.fifth.product.noncheckable.sub";
static const char vendorFifthProductSubWithTreeName[] = "moved_with_treename";

static const QMap<QString, QString> rootComponentDisplayNames = {
    {"", QLatin1String("The root component")},
    {"ru_ru", QString::fromUtf8("Корневая компонента")},
    {"de_de", QString::fromUtf8("Wurzel Komponente")}
};

static const QMap<QString, QString> rootComponentDescriptions = {
    {"", QLatin1String("Install this example.")},
    {"ru_ru", QString::fromUtf8("Установите этот пример.")},
    {"de_de", QString::fromUtf8("Installieren Sie dieses Beispiel.")}
};

class tst_ComponentModel : public QObject
{
    Q_OBJECT

public:
    enum Option {
        NoFlags = 0x00,
        VirtualsVisible = 0x01,
        NoForcedInstallation = 0x02,
        NoDefaultInstallation = 0x04
    };
    Q_DECLARE_FLAGS(Options, Option);

private slots:
    void initTestCase()
    {
        m_defaultChecked << vendorProduct << vendorSecondProductSub;
        m_defaultPartially << vendorSecondProduct;
        m_defaultUnchecked << vendorSecondProductSub1 << vendorSecondProductSubnode
            << vendorSecondProductSubnodeSub << vendorFourthProductCheckable
            << vendorFifthProductSub << vendorFifthProductSubWithTreeName;
        m_uncheckable << vendorFifthProductNonCheckable;
    }

    void testNameToIndexAndIndexToName()
    {
        setPackageManagerOptions(NoFlags);

        QList<Component*> rootComponents = loadComponents();
        testComponentsLoaded(rootComponents);

        // setup the model with 1 column
        ComponentModel model(1, &m_core);
        model.reset(rootComponents);

        // all names should be resolvable, virtual components are not indexed if they are not visible
        QStringList all;
        all << m_defaultChecked << m_defaultPartially << m_defaultUnchecked << m_uncheckable;
        foreach (const QString &name, all) {
            QVERIFY(model.indexFromComponentName(name).isValid());
            QVERIFY(model.componentFromIndex(model.indexFromComponentName(name)) != 0);
            QCOMPARE(model.componentFromIndex(model.indexFromComponentName(name))->treeName(), name);
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
        model.reset(rootComponents);

        // all names should be resolvable, including virtual components
        QStringList all;
        all << m_defaultChecked << m_defaultPartially << m_defaultUnchecked << m_uncheckable
            << vendorSecondProductVirtual << vendorThirdProductVirtual;
        foreach (const QString &name, all) {
            QVERIFY(model.indexFromComponentName(name).isValid());
            QVERIFY(model.componentFromIndex(model.indexFromComponentName(name)) != 0);
            QCOMPARE(model.componentFromIndex(model.indexFromComponentName(name))->treeName(), name);
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
        model.reset(rootComponents);
        testDefaultInheritedModelBehavior(&model, 1);

        QCOMPARE(model.core(), &m_core);
        QCOMPARE(model.checkedState(), ComponentModel::DefaultChecked);
        testModelState(&model, m_defaultChecked, m_defaultPartially, m_defaultUnchecked
            + m_uncheckable);

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
        model.reset(rootComponents);
        testDefaultInheritedModelBehavior(&model, 1);

        QCOMPARE(model.checkedState(), ComponentModel::DefaultChecked);
        // the virtual and non-checkable components are not checked
        testModelState(&model, m_defaultChecked, m_defaultPartially,
            m_defaultUnchecked + m_uncheckable
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
        model.reset(rootComponents);
        testDefaultInheritedModelBehavior(&model, 1);

        QCOMPARE(model.checkedState(), ComponentModel::DefaultChecked);
        testModelState(&model, m_defaultChecked, m_defaultPartially, m_defaultUnchecked
            + m_uncheckable);

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
        model.reset(rootComponents);
        testDefaultInheritedModelBehavior(&model, 1);

        QCOMPARE(model.checkedState(), ComponentModel::DefaultChecked);
        // the virtual components are not checked
        testModelState(&model, m_defaultChecked, m_defaultPartially,
            m_defaultUnchecked + m_uncheckable
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
        model.reset(rootComponents);
        testDefaultInheritedModelBehavior(&model, 1);

        // select all possible components.
        // Also uncheckable is checked as that is only 'visually' uncheckedable.
        model.setCheckedState(ComponentModel::AllChecked);
        QCOMPARE(model.checkedState(), ComponentModel::AllChecked);
        testModelState(&model, m_defaultChecked + m_defaultPartially + m_defaultUnchecked + m_uncheckable,
            QStringList(), QStringList());

        // deselect all possible components
        // as the first root is a forced install, should result in partially checked state
        model.setCheckedState(ComponentModel::AllUnchecked);
        QCOMPARE(model.checkedState(), ComponentModel::PartiallyChecked);
        testModelState(&model, QStringList() << vendorProduct, QStringList(), m_defaultPartially
            + m_defaultUnchecked + QStringList(vendorSecondProductSub) + m_uncheckable);

        // reset all possible components
        model.setCheckedState(ComponentModel::DefaultChecked);
        QCOMPARE(model.checkedState(), ComponentModel::DefaultChecked);
        testModelState(&model, m_defaultChecked, m_defaultPartially,
            m_defaultUnchecked + m_uncheckable);

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
        model.reset(rootComponents);
        testDefaultInheritedModelBehavior(&model, 1);

        // select all possible components.
        model.setCheckedState(ComponentModel::AllChecked);
        QCOMPARE(model.checkedState(), ComponentModel::AllChecked);
        testModelState(&model, m_defaultChecked + m_defaultPartially + m_defaultUnchecked + m_uncheckable
            + QStringList(vendorSecondProductVirtual) << vendorThirdProductVirtual, QStringList(),
            QStringList());

        // deselect all possible components
        // as the first root is a forced install, should result in partially checked state
        model.setCheckedState(ComponentModel::AllUnchecked);
        QCOMPARE(model.checkedState(), ComponentModel::PartiallyChecked);
        testModelState(&model, QStringList() << vendorProduct, QStringList(), m_defaultPartially
            + m_defaultUnchecked + m_uncheckable + QStringList(vendorSecondProductSub)
             << vendorSecondProductVirtual << vendorThirdProductVirtual);

        // reset all possible components
        model.setCheckedState(ComponentModel::DefaultChecked);
        QCOMPARE(model.checkedState(), ComponentModel::DefaultChecked);
        testModelState(&model, m_defaultChecked, m_defaultPartially,
            m_defaultUnchecked + m_uncheckable
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
        model.reset(rootComponents);
        testDefaultInheritedModelBehavior(&model, 1);

        // select all possible components.
        model.setCheckedState(ComponentModel::AllChecked);
        QCOMPARE(model.checkedState(), ComponentModel::AllChecked);
        testModelState(&model, m_defaultChecked + m_defaultPartially + m_defaultUnchecked+ m_uncheckable,
            QStringList(), QStringList());

        // deselect all possible components
        model.setCheckedState(ComponentModel::AllUnchecked);
        QCOMPARE(model.checkedState(), ComponentModel::AllUnchecked);
        testModelState(&model, QStringList(), QStringList(), m_defaultPartially
            + m_defaultUnchecked + m_uncheckable
            + QStringList(vendorSecondProductSub) << vendorProduct);

        // reset all possible components
        model.setCheckedState(ComponentModel::DefaultChecked);
        QCOMPARE(model.checkedState(), ComponentModel::DefaultChecked);
        testModelState(&model, m_defaultChecked, m_defaultPartially,
            m_defaultUnchecked + m_uncheckable);

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
        model.reset(rootComponents);
        testDefaultInheritedModelBehavior(&model, 1);

        // select all possible components.
        model.setCheckedState(ComponentModel::AllChecked);
        QCOMPARE(model.checkedState(), ComponentModel::AllChecked);
        testModelState(&model, m_defaultChecked + m_defaultPartially + m_defaultUnchecked +m_uncheckable
            + QStringList(vendorSecondProductVirtual) << vendorThirdProductVirtual, QStringList(),
            QStringList());

        // deselect all possible components
        model.setCheckedState(ComponentModel::AllUnchecked);
        QCOMPARE(model.checkedState(), ComponentModel::AllUnchecked);
        testModelState(&model, QStringList(), QStringList(), m_defaultPartially
            + m_defaultUnchecked + m_uncheckable + QStringList(vendorSecondProductSub)
            << vendorSecondProductVirtual << vendorProduct << vendorThirdProductVirtual);

        // reset all possible components
        model.setCheckedState(ComponentModel::DefaultChecked);
        QCOMPARE(model.checkedState(), ComponentModel::DefaultChecked);
        testModelState(&model, m_defaultChecked, m_defaultPartially, m_defaultUnchecked
            + m_uncheckable + QStringList(vendorSecondProductVirtual)
            << vendorThirdProductVirtual);

        foreach (Component *const component, rootComponents)
            delete component;
    }

    void testComponentsLocalization()
    {
        QStringList localesToTest = { "en_US", "ru_RU", "de_DE", "fr_FR" };
        foreach (const QString &localeToTest, localesToTest) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            QLocale::setDefault(QLocale(localeToTest));
#else
            QLocale::setDefault(localeToTest);
#endif
            QString expectedName = rootComponentDisplayNames.contains(localeToTest.toLower())
                ? rootComponentDisplayNames[localeToTest.toLower()]
                : rootComponentDisplayNames[QString()];

            QString expectedDescription = rootComponentDescriptions.contains(localeToTest.toLower())
                                       ? rootComponentDescriptions[localeToTest.toLower()]
                                       : rootComponentDescriptions[QString()];

            setPackageManagerOptions(NoFlags);

            QList<Component*> rootComponents = loadComponents();
            testComponentsLoaded(rootComponents);

            // setup the model with 1 column
            ComponentModel model(1, &m_core);
            model.reset(rootComponents);

            const QModelIndex root = model.indexFromComponentName(vendorProduct);
            QCOMPARE(model.data(root, Qt::DisplayRole).toString(), expectedName);

            Component *comp = model.componentFromIndex(root);
            QCOMPARE(comp->value("Description"), expectedDescription);

            qDeleteAll(rootComponents);
        }
    }

    void testNoDefaultInstallation()
    {
        setPackageManagerOptions(NoDefaultInstallation);

        QList<Component*> rootComponents = loadComponents();
        testComponentsLoaded(rootComponents);

        // setup the model with 1 column
        ComponentModel model(1, &m_core);
        model.reset(rootComponents);
        testDefaultInheritedModelBehavior(&model, 1);

        model.setCheckedState(ComponentModel::DefaultChecked);
        QCOMPARE(model.checkedState(), ComponentModel::DefaultChecked);
        testModelState(&model, QStringList() << vendorProduct, QStringList(), m_defaultUnchecked
            + m_uncheckable + m_defaultPartially + QStringList() << vendorSecondProductSub);
    }

private:
    void setPackageManagerOptions(Options flags) const
    {
        m_core.setNoForceInstallation(flags.testFlag(NoForcedInstallation));
        m_core.setVirtualComponentsVisible(flags.testFlag(VirtualsVisible));
        m_core.setNoDefaultInstallation(flags.testFlag(NoDefaultInstallation));
    }

    void testComponentsLoaded(const QList<Component *> &rootComponents) const
    {
        // we need to have six root components
        QCOMPARE(rootComponents.count(), 6);

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
            QCOMPARE(model->rowCount(), 6); // 6 (5 non virtual and 1 virtual root component)
        else
            QCOMPARE(model->rowCount(), 5); // 5 (the 5 non virtual root components)
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
            QVERIFY(checked.contains(component->treeName()));

        // these components should not have partially checked state
        foreach (Component *const component, model->partially())
            QVERIFY(partially.contains(component->treeName()));

        // these components should not have checked state
        foreach (Component *const component, model->unchecked())
            QVERIFY(unchecked.contains(component->treeName()));
    }

    QList<Component*> loadComponents() const
    {
        UpdatesInfo updatesInfo;
        updatesInfo.setFileName(":///data/updates.xml");
        updatesInfo.parseFile();
        const QList<UpdateInfo> updateInfos = updatesInfo.updatesInfo();

        QHash<QString, Component*> components;
        foreach (const UpdateInfo &info, updateInfos) {
            Component *component = new Component(const_cast<PackageManagerCore *>(&m_core));

            // we need at least these to be able to test the model
            component->setValue("Name", info.data.value("Name").toString());
            component->setValue("TreeName", info.data.value("TreeName").value<QPair<QString, bool>>().first);
            QString isDefault = info.data.value("Default").toString();
            if (m_core.noDefaultInstallation())
                isDefault = scFalse;
            component->setValue("Default", isDefault);
            component->setValue("Virtual", info.data.value("Virtual").toString());
            component->setValue("DisplayName", info.data.value("DisplayName").toString());
            component->setValue("Checkable", info.data.value("Checkable").toString());
            component->setValue("Description", info.data.value("Description").toString());

            QString forced = info.data.value("ForcedInstallation", scFalse).toString().toLower();
            if (m_core.noForceInstallation())
                forced = scFalse;
            component->setValue("ForcedInstallation", forced);
            if (forced == scTrue) {
                component->setEnabled(false);
                component->setCheckable(false);
                component->setCheckState(Qt::Checked);
            }
            components.insert(component->treeName(), component);
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
    QStringList m_uncheckable;
    QStringList m_defaultNoChecked;
};
Q_DECLARE_OPERATORS_FOR_FLAGS(tst_ComponentModel::Options)

QTEST_MAIN(tst_ComponentModel)

#include "tst_componentmodel.moc"
