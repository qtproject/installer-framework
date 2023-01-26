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
#include "settings.h"

#include <QTest>
#include <QtCore/QLocale>
#include <QRegularExpression>

using namespace KDUpdater;
using namespace QInstaller;

#define EXPECTED_COUNTS_COMPONENTS_VISIBLE 10
#define EXPECTED_COUNT_ROOT_COMPONENTS 7
#define EXPECTED_COUNT_INVALID_SCRIPT 7
#define EXPECTED_COUNT_MISSING_DEPENDENCY 8

static const char componentInvalidScript[] = "componentinvalidscript";
static const char componentB[] = "componentb";

static const char componentC[] = "componentc";
static const char componentCSubnodeDependsOnInvalidScriptComponent[] = "componentc.subnode";
static const char componentCSubnodeDependsOnInvalidScriptComponentSub[] = "componentc.subnode.sub";

static const char componentMissingDependency[] = "componentmissingdependency";
static const char componentDependsOnMissingDependency[] = "componentd";

static const char componentMissingContent[] = "componentmissingcontent";
static const char componentMissingContentSub[] = "componentmissingcontent.sub";
static const char componentDependsOnMissingContentSub[] = "componentE";

static const QMap<QString, QString> rootComponentDisplayNames = {
    {"", QLatin1String("The root component")},
    {"ru_ru", QString::fromUtf8("Корневая компонента")},
    {"de_de", QString::fromUtf8("Wurzel Komponente")}
};

using namespace QInstaller;

class tst_BrokenInstaller : public QObject
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

        m_checkedComponentsWithBrokenScript << componentB << componentMissingDependency << componentDependsOnMissingDependency << componentMissingContent
                                            << componentMissingContentSub << componentDependsOnMissingContentSub;
        m_uncheckedComponentsWithBrokenScript << componentInvalidScript << componentCSubnodeDependsOnInvalidScriptComponent
                                              << componentCSubnodeDependsOnInvalidScriptComponentSub << componentC;

        m_checkedComponentsWithMissingDependency << componentB << componentMissingContent << componentMissingContentSub << componentDependsOnMissingContentSub
                                                << componentInvalidScript << componentCSubnodeDependsOnInvalidScriptComponent
                                                << componentCSubnodeDependsOnInvalidScriptComponentSub << componentC;
        m_uncheckedComponentsWithMissingDependency << componentMissingDependency << componentDependsOnMissingDependency;

    }

    void testNameToIndexAndIndexToName()
    {
        PackageManagerCore core;
        QList<Component*> rootComponents = loadComponents(core);

        ComponentModel *model = new ComponentModel(1, &core);
        model->reset(rootComponents);
        // all names should be resolvable
        QStringList all;
        all << m_checkedComponentsWithBrokenScript << m_uncheckedComponentsWithBrokenScript << m_partiallyCheckedComponentsWithBrokenScript;
        foreach (const QString &name, all) {
            QVERIFY(model->indexFromComponentName(name).isValid());
            QVERIFY(model->componentFromIndex(model->indexFromComponentName(name)) != 0);
            QCOMPARE(model->componentFromIndex(model->indexFromComponentName(name))->name(), name);
        }
    }

    void testInvalidScript()
    {
        PackageManagerCore core;
        QList<Component*> components = loadComponents(core);

        core.settings().setAllowUnstableComponents(true);
        ComponentModel *model = new ComponentModel(1, &core);
        Component *invalidScriptComponent = core.componentByName("componentinvalidscript");

        // Using regexp to ignore the warning message as the message contains
        // object address which we don't know
        const QString debugMessage =  QString("Exception while loading the component script");
        const QRegularExpression re(debugMessage);
        QTest::ignoreMessage(QtWarningMsg, re);
        invalidScriptComponent->evaluateComponentScript(":///data/broken_script.qs");

        model->reset(components);
        testModelState(model, m_checkedComponentsWithBrokenScript, m_partiallyCheckedComponentsWithBrokenScript, m_uncheckedComponentsWithBrokenScript);
    }

    void testMissingDependency()
    {
        PackageManagerCore core;
        QList<Component*> components = loadComponents(core);

        core.settings().setAllowUnstableComponents(true);
        ComponentModel *model = new ComponentModel(1, &core);

        Component *invalidComponent = core.componentByName("componentmissingdependency");
        invalidComponent->addDependency("missingcomponent");

        Component *componentDependingOnMissingDependency = core.componentByName("componentd");
        componentDependingOnMissingDependency->addDependency("componentmissingdependency");

        core.recalculateAllComponents();
        model->reset(components);

        testModelState(model, m_checkedComponentsWithMissingDependency, m_partiallyCheckedComponentsWithBrokenScript, m_uncheckedComponentsWithMissingDependency);
    }

private:

    QList<Component*> loadComponents(PackageManagerCore &core)
    {
        UpdatesInfo updatesInfo;
        updatesInfo.setFileName(":///data/updates.xml");
        updatesInfo.parseFile();
        const QList<UpdateInfo> updateInfos = updatesInfo.updatesInfo();

        QList <Component*> components;
        UpdateInfo info;
        for (int i = 0; i < updateInfos.count(); i++) {
            info = updateInfos.at(i);
            Component *component = new Component(&core);

            // we need at least these to be able to test the model
            component->setValue("Name", info.data.value("Name").toString());
            component->setValue("Virtual", info.data.value("Virtual").toString());
            component->setValue("DisplayName", info.data.value("DisplayName").toString());
            component->setValue("Checkable", info.data.value("Checkable").toString());
            component->setValue("Dependencies", info.data.value("Dependencies").toString());
            component->setCheckState(Qt::Checked);
            QString forced = info.data.value("ForcedInstallation", scFalse).toString().toLower();
            if (core.noForceInstallation())
                forced = scFalse;
            component->setValue("ForcedInstallation", forced);
            if (forced == scTrue) {
                component->setEnabled(false);
                component->setCheckable(false);
            }
            //If name does not contain dot it is root component
            qDebug()<<"component name "<<component->name();
            if (!component->name().contains('.')) {
                core.appendRootComponent(component);
            } else { //Contains dot, is previous components child component
                components.at(i-1)->appendComponent(component);
            }
            components.append(component);
        }
        return components;
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

private:
    QStringList m_checkedComponentsWithBrokenScript;
    QStringList m_uncheckedComponentsWithBrokenScript;
    QStringList m_partiallyCheckedComponentsWithBrokenScript;

    QStringList m_checkedComponentsWithMissingDependency;
    QStringList m_uncheckedComponentsWithMissingDependency;
    QStringList m_partiallyCheckedComponentsWithMissingDependency;
};
Q_DECLARE_OPERATORS_FOR_FLAGS(tst_BrokenInstaller::Options)

QTEST_MAIN(tst_BrokenInstaller)

#include "tst_brokeninstaller.moc"
