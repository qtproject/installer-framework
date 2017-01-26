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
#include <errors.h>
#include <packagemanagercore.h>
#include <packagemanagergui.h>
#include <scriptengine.h>

#include <QTest>
#include <QSet>
#include <QFile>
#include <QString>

using namespace QInstaller;

class TestGui : public QInstaller::PackageManagerGui
{
    Q_OBJECT

public:
    explicit TestGui(QInstaller::PackageManagerCore *core)
    : PackageManagerGui(core, 0)
    {
        setPage(PackageManagerCore::Introduction, new IntroductionPage(core));
        setPage(PackageManagerCore::ComponentSelection, new ComponentSelectionPage(core));
        setPage(PackageManagerCore::InstallationFinished, new FinishedPage(core));

        foreach (const int id, pageIds()) {
            packageManagerCore()->controlScriptEngine()->addToGlobalObject(page(id));
            packageManagerCore()->componentScriptEngine()->addToGlobalObject(page(id));
        }
    }

    virtual void init() {}

    void callProtectedDelayedExecuteControlScript(int id)
    {
        executeControlScript(id);
    }
};

class DynamicPageGui : public PackageManagerGui
{
    Q_OBJECT

public:
    explicit DynamicPageGui(PackageManagerCore *core)
        : PackageManagerGui(core)
    {}

    void init() {
        m_widget = new QWidget;
        m_widget->setObjectName("Widget");
        QWidget *button = new QWidget(m_widget);
        button->setObjectName("Button");

        packageManagerCore()->wizardPageInsertionRequested(m_widget,
            PackageManagerCore::Introduction);
    }

    QWidget *widget() const { return m_widget; }

private:
    QWidget *m_widget;
};

class EnteringPage : public PackageManagerPage
{
    Q_OBJECT
    Q_PROPERTY(QStringList invocationOrder READ invocationOrder)
public:
    explicit EnteringPage(PackageManagerCore *core)
        : PackageManagerPage(core)
    {
        setObjectName(QLatin1String("EnteringPage"));
    }
    QStringList invocationOrder() const {
        return m_invocationOrder;
    }
public slots:
    Q_INVOKABLE void enteringInvoked() {
        m_invocationOrder << QLatin1String("Entering");
    }
    Q_INVOKABLE void callbackInvoked() {
        m_invocationOrder << QLatin1String("Callback");
    }

protected:
    void entering() {
        enteringInvoked();
    }
private:
    QStringList m_invocationOrder;
};

class EnteringGui : public PackageManagerGui
{
    Q_OBJECT
public:
    explicit EnteringGui(PackageManagerCore *core)
        : PackageManagerGui(core)
    {}

    EnteringPage *enteringPage() const {
        return m_enteringPage;
    }

    void init() {
        m_enteringPage = new EnteringPage(packageManagerCore());
        setPage(0, m_enteringPage);
    }
private:
    EnteringPage *m_enteringPage;
};

class EmitSignalObject : public QObject
{
    Q_OBJECT

public:
    EmitSignalObject() {}
    ~EmitSignalObject() {}
    void produceSignal() { emit emitted(); }
signals:
    void emitted();
};


class tst_ScriptEngine : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        m_component = new Component(&m_core);
        // append the component to the package manager which deletes it at destructor
        // (it calls clearAllComponentLists which calls qDeleteAll(m_rootComponents);)
        m_core.appendRootComponent(m_component);

        m_component->setValue("Default", "Script");
        m_component->setValue(scName, "component.test.name");

        m_scriptEngine = m_core.componentScriptEngine();
    }

    void testDefaultScriptEngineValues()
    {
        const QJSValue global = m_scriptEngine->globalObject();
        QCOMPARE(global.hasProperty(QLatin1String("print")), true);

        QCOMPARE(global.hasProperty(QLatin1String("installer")), true);
        QCOMPARE(global.property(QLatin1String("installer"))
            .hasProperty(QLatin1String("componentByName")), true);
        QCOMPARE(global.property(QLatin1String("installer"))
            .hasProperty(QLatin1String("components")), true);

        QCOMPARE(global.hasProperty(QLatin1String("console")), true);
        QCOMPARE(global.property(QLatin1String("console"))
            .hasProperty(QLatin1String("log")), true);

        QCOMPARE(global.hasProperty(QLatin1String("QFileDialog")), true);
        QCOMPARE(global.property(QLatin1String("QFileDialog"))
            .hasProperty(QLatin1String("getExistingDirectory")), true);
        QCOMPARE(global.property(QLatin1String("QFileDialog"))
            .hasProperty(QLatin1String("getOpenFileName")), true);

        QCOMPARE(global.hasProperty(QLatin1String("InstallerProxy")), true);
        QCOMPARE(global.property(QLatin1String("InstallerProxy"))
            .hasProperty(QLatin1String("componentByName")), true);
        QCOMPARE(global.property(QLatin1String("InstallerProxy"))
                 .hasProperty(QLatin1String("components")), true);

        QCOMPARE(global.hasProperty(QLatin1String("QDesktopServices")), true);
        QCOMPARE(global.property(QLatin1String("QDesktopServices"))
            .hasProperty(QLatin1String("openUrl")), true);
        QCOMPARE(global.property(QLatin1String("QDesktopServices"))
            .hasProperty(QLatin1String("displayName")), true);
        QCOMPARE(global.property(QLatin1String("QDesktopServices"))
            .hasProperty(QLatin1String("storageLocation")), true);

        QCOMPARE(global.hasProperty(QLatin1String("buttons")), true);
        QCOMPARE(global.hasProperty(QLatin1String("QInstaller")), true);
        QCOMPARE(global.hasProperty(QLatin1String("QMessageBox")), true);

        QCOMPARE(global.hasProperty(QLatin1String("gui")), true);
        QCOMPARE(global.hasProperty(QLatin1String("qsTr")), true);

        QCOMPARE(global.hasProperty(QLatin1String("systemInfo")), true);
        QJSValue sinfo = global.property(QLatin1String("systemInfo"));
        QCOMPARE(sinfo.property(QLatin1String("currentCpuArchitecture")).toString(),
                 QSysInfo::currentCpuArchitecture());
        QCOMPARE(sinfo.property(QLatin1String("kernelType")).toString(), QSysInfo::kernelType());
        QCOMPARE(sinfo.property(QLatin1String("kernelVersion")).toString(),
                 QSysInfo::kernelVersion());
        QCOMPARE(sinfo.property(QLatin1String("productType")).toString(), QSysInfo::productType());
        QCOMPARE(sinfo.property(QLatin1String("productVersion")).toString(),
                 QSysInfo::productVersion());
        QCOMPARE(sinfo.property(QLatin1String("prettyProductName")).toString(),
                 QSysInfo::prettyProductName());
    }

    void testBrokenJSMethodConnect()
    {
#if QT_VERSION <= 0x50400
        QSKIP("Behavior changed from 5.4.1 onwards");
#endif
        EmitSignalObject emiter;
        m_scriptEngine->globalObject().setProperty(QLatin1String("emiter"),
            m_scriptEngine->newQObject(&emiter));

        QJSValue context = m_scriptEngine->loadInContext(QLatin1String("BrokenConnect"),
            ":///data/broken_connect.qs");

        if (context.isError()) {
            QFAIL(qPrintable(QString::fromLatin1("ScriptEngine error:\n %1").arg(
                context.toString())));
        }
        QCOMPARE(context.isError(), false);

        // ignore Output from script
        setExpectedScriptOutput("\"function receive()\"");

        QTest::ignoreMessage(QtWarningMsg, ":10: ReferenceError: foo is not defined");
        emiter.produceSignal();

        const QJSValue value = m_scriptEngine->evaluate("");
        QCOMPARE(value.isError(), false);
    }

    void testScriptPrint()
    {
        setExpectedScriptOutput("\"test\"");
        const QJSValue value = m_scriptEngine->evaluate("print(\"test\");");
        if (value.isError()) {
            QFAIL(qPrintable(QString::fromLatin1("ScriptEngine error:\n %1").arg(
                value.toString())));
        }
    }

    void testExistingInstallerObject()
    {
        setExpectedScriptOutput("\"object\"");
        const QJSValue value = m_scriptEngine->evaluate("print(typeof installer)");
        if (value.isError()) {
            QFAIL(qPrintable(QString::fromLatin1("ScriptEngine error:\n %1").arg(
                value.toString())));
        }
    }

    void testComponentByName()
    {
        const QString script = QString::fromLatin1("var component = installer.componentByName('%1');"
            "\n"
            "print(component.name);").arg(m_component->name());

        setExpectedScriptOutput("\"component.test.name\"");
        const QJSValue value = m_scriptEngine->evaluate(script);
        if (value.isError()) {
            QFAIL(qPrintable(QString::fromLatin1("ScriptEngine error:\n %1").arg(
                value.toString())));
        }
    }

    void testComponentByWrongName()
    {
        const QString script = QString::fromLatin1( "var component = installer.componentByName('%1');"
            "\n"
            "print(brokenComponent.name);").arg("NotExistingComponentName");

        const QJSValue value = m_scriptEngine->evaluate(script);
        QCOMPARE(value.isError(), true);
    }

    void testComponents()
    {
      const QString script = QString::fromLatin1("var components = installer.components();"
                                                 "\n"
                                                 "print(components[0].name);");

      setExpectedScriptOutput("\"component.test.name\"");
      const QJSValue value = m_scriptEngine->evaluate(script);
      if (value.isError()) {
        QFAIL(qPrintable(QString::fromLatin1("ScriptEngine error:\n %1").arg(
                           value.toString())));
      }
    }

    void loadSimpleComponentScript()
    {
       try {
            // ignore retranslateUi which is called by loadComponentScript
            setExpectedScriptOutput("\"Component constructor - OK\"");
            setExpectedScriptOutput("\"retranslateUi - OK\"");
            m_component->loadComponentScript(":///data/component1.qs");

            setExpectedScriptOutput("\"retranslateUi - OK\"");
            m_component->languageChanged();

            setExpectedScriptOutput("\"createOperationsForPath - OK\"");
            m_component->createOperationsForPath(":///data/");

            setExpectedScriptOutput("\"createOperationsForArchive - OK\"");
            // ignore createOperationsForPath which is called by createOperationsForArchive
            setExpectedScriptOutput("\"createOperationsForPath - OK\"");
            m_component->createOperationsForArchive("test.7z");

            setExpectedScriptOutput("\"beginInstallation - OK\"");
            m_component->beginInstallation();

            setExpectedScriptOutput("\"createOperations - OK\"");
            m_component->createOperations();

            setExpectedScriptOutput("\"isDefault - OK\"");
            bool returnIsDefault = m_component->isDefault();
            QCOMPARE(returnIsDefault, false);

        } catch (const Error &error) {
            QFAIL(qPrintable(error.message()));
        }
    }

    void loadBrokenComponentScript()
    {
        Component *testComponent = new Component(&m_core);
        testComponent->setValue(scName, "broken.component");

        // m_core becomes the owner of testComponent, it will delete it in the destructor
        m_core.appendRootComponent(testComponent);

        const QString debugMesssage(
            "create Error-Exception: \"Exception while loading the component script '"
            ":///data/component2.qs'. (ReferenceError: broken is not defined)\"");
        try {
            // ignore Output from script
            setExpectedScriptOutput("\"script function: Component\"");
            setExpectedScriptOutput(qPrintable(debugMesssage));
            testComponent->loadComponentScript(":///data/component2.qs");
        } catch (const Error &error) {
            QVERIFY2(debugMesssage.contains(error.message()), "(ReferenceError: broken is not defined)");
        }
    }

    void loadComponentUserInterfaces()
    {
       try {
            setExpectedScriptOutput("\"checked: false\"");
            m_component->loadUserInterfaces(QDir(":///data"), QStringList() << QLatin1String("form.ui"));
            m_component->loadComponentScript(":///data/userinterface.qs");
        } catch (const Error &error) {
            QFAIL(qPrintable(error.message()));
        }
    }

    void loadSimpleAutoRunScript()
    {
        try {
            TestGui testGui(&m_core);
            setExpectedScriptOutput("Loaded control script \":///data/auto-install.qs\" ");
            testGui.loadControlScript(":///data/auto-install.qs");
            QCOMPARE(m_core.value("GuiTestValue"), QString("hello"));

            testGui.show();

            // inside the auto-install script we are clicking the next button, with a not existing button
            QTest::ignoreMessage(QtWarningMsg, "Button with type:  \"unknown button\" not found! ");
            testGui.callProtectedDelayedExecuteControlScript(PackageManagerCore::ComponentSelection);

            setExpectedScriptOutput("\"FinishedPageCallback - OK\"");
            testGui.callProtectedDelayedExecuteControlScript(PackageManagerCore::InstallationFinished);
        } catch (const Error &error) {
            QFAIL(qPrintable(error.message()));
        }
    }

    void testDynamicPage()
    {
        DynamicPageGui gui(&m_core);
        gui.init();

        setExpectedScriptOutput("Loaded control script \":///data/dynamicpage.qs\" ");
        gui.loadControlScript(":///data/dynamicpage.qs");

        gui.callControlScriptMethod("ReadAndSetValues");

        QCOMPARE(m_core.value("DynamicWidget.final"), QString("false"));
        QCOMPARE(gui.widget()->property("final").toString(), QString("false"));
        QCOMPARE(m_core.value("DynamicWidget.commit"), QString("false"));
        QCOMPARE(gui.widget()->property("commit").toString(), QString("false"));
        QCOMPARE(m_core.value("DynamicWidget.complete"), QString("true"));
        QCOMPARE(gui.widget()->property("complete").toString(), QString("true"));

        gui.widget()->setProperty("final", true);
        gui.widget()->setProperty("commit", true);
        gui.widget()->setProperty("complete", false);

        gui.callControlScriptMethod("ReadAndSetValues");

        QCOMPARE(m_core.value("DynamicWidget.final"), QString("true"));
        QCOMPARE(gui.widget()->property("final").toString(), QString("true"));
        QCOMPARE(m_core.value("DynamicWidget.commit"), QString("true"));
        QCOMPARE(gui.widget()->property("commit").toString(), QString("true"));
        QCOMPARE(m_core.value("DynamicWidget.complete"), QString("false"));
        QCOMPARE(gui.widget()->property("complete").toString(), QString("false"));

        gui.callControlScriptMethod("UpdateAndSetValues");

        QCOMPARE(m_core.value("DynamicWidget.final"), QString("false"));
        QCOMPARE(gui.widget()->property("final").toString(), QString("false"));
        QCOMPARE(m_core.value("DynamicWidget.commit"), QString("false"));
        QCOMPARE(gui.widget()->property("commit").toString(), QString("false"));
        QCOMPARE(m_core.value("DynamicWidget.complete"), QString("true"));
        QCOMPARE(gui.widget()->property("complete").toString(), QString("true"));
    }

    void checkEnteringCalledBeforePageCallback()
    {
        EnteringGui gui(&m_core);
        gui.init();
        setExpectedScriptOutput("Loaded control script \":///data/enteringpage.qs\" ");
        gui.loadControlScript(":///data/enteringpage.qs");
        gui.show();

        EnteringPage *enteringPage = gui.enteringPage();

        QStringList expectedOrder;
        expectedOrder << QLatin1String("Entering") << QLatin1String("Callback");
        QCOMPARE(enteringPage->invocationOrder(), expectedOrder);
    }

private:
    void setExpectedScriptOutput(const char *message)
    {
        // Using setExpectedScriptOutput(...); inside the test method
        // as a simple test that the scripts are called.
        QTest::ignoreMessage(QtDebugMsg, message);
    }

    PackageManagerCore m_core;
    Component *m_component;
    ScriptEngine *m_scriptEngine;

};


QTEST_MAIN(tst_ScriptEngine)

#include "tst_scriptengine.moc"
