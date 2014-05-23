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
    }

    virtual void init() {}

    void callProtectedDelayedExecuteControlScript(int id)
    {
        executeControlScript(id);
    }
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

        m_component->setValue("AutoDependOn", "Script");
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
    }

    void testBrokenJSMethodConnect()
    {
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

        emiter.produceSignal();

        const QJSValue value = m_scriptEngine->evaluate("");
        QCOMPARE(value.isError(), true);
        QCOMPARE(value.toString(), QLatin1String("ReferenceError: foo is not defined"));
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

            setExpectedScriptOutput("\"isAutoDependOn - OK\"");
            bool returnIsAutoDependOn = m_component->isAutoDependOn(QSet<QString>());
            QCOMPARE(returnIsAutoDependOn, false);

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

    void loadSimpleAutoRunScript()
    {
        try {
            TestGui testGui(&m_core);
            setExpectedScriptOutput("Loaded control script \":///data/auto-install.qs\" ");
            testGui.loadControlScript(":///data/auto-install.qs");
            QCOMPARE(m_core.value("GuiTestValue"), QString("hello"));

            // show event calls automatically the first callback which does not exist
            setExpectedScriptOutput("Control script callback \"IntroductionPageCallback\" does not exist. ");
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
