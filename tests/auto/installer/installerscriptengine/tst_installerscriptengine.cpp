#include <component.h>
#include <errors.h>
#include <packagemanagercore.h>
#include <packagemanagergui.h>

#include <QTest>
#include <QSet>
#include <QFile>
#include <QString>
#include <QScriptEngine>

using namespace QInstaller;

// -- InstallerGui

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

    void callProtectedDelayedControlScriptExecution(int id)
    {
        delayedControlScriptExecution(id);
    }
};


class tst_InstallerScriptEngine : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        m_component = new Component(&m_core);
        // append the component to the packagemanager which deletes it at destructor
        // (it calls clearAllComponentLists which calls qDeleteAll(m_rootComponents);)
        m_core.appendRootComponent(m_component);

        m_component->setValue("AutoDependOn", "Script");
        m_component->setValue("Default", "Script");
        m_component->setValue(scName, "component.test.name");

        m_scriptEngine = m_component->scriptEngine();
    }

    void testScriptPrint()
    {
        setExpectedScriptOutput("test");
        m_scriptEngine->evaluate("print(\"test\");");
        if (m_scriptEngine->hasUncaughtException()) {
            QFAIL(qPrintable(QString::fromLatin1("installerScriptEngine hasUncaughtException:\n %1").arg(
                uncaughtExceptionString(m_scriptEngine))));
        }
    }

    void testExistingInstallerObject()
    {
        setExpectedScriptOutput("object");
        m_scriptEngine->evaluate("print(typeof installer)");
        if (m_scriptEngine->hasUncaughtException()) {
            QFAIL(qPrintable(QString::fromLatin1("installerScriptEngine hasUncaughtException:\n %1").arg(
                uncaughtExceptionString(m_scriptEngine))));
        }
    }

    void testComponentByName()
    {
        const QString printComponentNameScript = QString::fromLatin1("var correctComponent = " \
            "installer.componentByName('%1');\nprint(correctComponent.name);").arg(m_component->name());

        setExpectedScriptOutput("component.test.name");
        m_scriptEngine->evaluate(printComponentNameScript);
        if (m_scriptEngine->hasUncaughtException()) {
            QFAIL(qPrintable(QString::fromLatin1("installerScriptEngine hasUncaughtException:\n %1").arg(
                uncaughtExceptionString(m_scriptEngine))));
        }
    }

    void testComponentByWrongName()
    {
        const QString printComponentNameScript = QString::fromLatin1( "var brokenComponent = " \
            "installer.componentByName('%1');\nprint(brokenComponent.name);").arg("MyNotExistingComponentName");

        m_scriptEngine->evaluate(printComponentNameScript);
        QVERIFY(m_scriptEngine->hasUncaughtException());
    }

    void loadSimpleComponentScript()
    {
       try {
            // ignore retranslateUi which is called by loadComponentScript
            setExpectedScriptOutput("Component constructor - OK");
            setExpectedScriptOutput("retranslateUi - OK");
            m_component->loadComponentScript(":///data/component1.qs");

            setExpectedScriptOutput("retranslateUi - OK");
            m_component->languageChanged();

            setExpectedScriptOutput("createOperationsForPath - OK");
            m_component->createOperationsForPath(":///data/");

            setExpectedScriptOutput("createOperationsForArchive - OK");
            // ignore createOperationsForPath which is called by createOperationsForArchive
            setExpectedScriptOutput("createOperationsForPath - OK");
            m_component->createOperationsForArchive("test.7z");

            setExpectedScriptOutput("beginInstallation - OK");
            m_component->beginInstallation();

            setExpectedScriptOutput("createOperations - OK");
            m_component->createOperations();

            setExpectedScriptOutput("isAutoDependOn - OK");
            bool returnIsAutoDependOn = m_component->isAutoDependOn(QSet<QString>());
            QCOMPARE(returnIsAutoDependOn, false);

            setExpectedScriptOutput("isDefault - OK");
            bool returnIsDefault = m_component->isDefault();
            QCOMPARE(returnIsDefault, false);

        } catch (const Error &error) {
            QFAIL(qPrintable(error.message()));
        }

    }
    void loadBrokenComponentScript()
    {
        PackageManagerCore core;
        Component testComponent(&core);

        const QString debugMesssage(
            "create Error-Exception: \"Exception while loading the component script: :///data/component2.qs\n\n" \
            "ReferenceError: Can't find variable: broken\n\n" \
            "Backtrace:\n" \
                "\t<anonymous>()@:///data/component2.qs:5\" ");
        try {
            // ignore Output from script
            setExpectedScriptOutput("script function: Component");
            setExpectedScriptOutput(qPrintable(debugMesssage));
            testComponent.loadComponentScript(":///data/component2.qs");
        } catch (const Error &error) {
            QVERIFY2(debugMesssage.contains(error.message()), "There was some unexpected error.");
        }
    }

    void loadSimpleAutoRunScript()
    {
        TestGui testGui(&m_core);
        setExpectedScriptOutput("Loaded control script \":///data/auto-install.qs\" ");
        testGui.loadControlScript(":///data/auto-install.qs");
        QCOMPARE(m_core.value("GuiTestValue"), QString("hello"));

        testGui.show();
        // show event calls automatically the first callback which does not exist
        setExpectedScriptOutput("Control script callback \"IntroductionPageCallback\" does not exist. ");
        // give some time to the event triggering mechanism
        qApp->processEvents();

        setExpectedScriptOutput("Calling control script callback \"ComponentSelectionPageCallback\" ");
        // inside the auto-install script we are clicking the next button, with a not existing button
        QTest::ignoreMessage(QtWarningMsg, "Button with type:  \"unknown button\" not found! ");
        testGui.callProtectedDelayedControlScriptExecution(PackageManagerCore::ComponentSelection);

        setExpectedScriptOutput("Calling control script callback \"FinishedPageCallback\" ");
        setExpectedScriptOutput("FinishedPageCallback - OK");
        testGui.callProtectedDelayedControlScriptExecution(PackageManagerCore::InstallationFinished);
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
    QScriptEngine *m_scriptEngine;

};


QTEST_MAIN(tst_InstallerScriptEngine)

#include "tst_installerscriptengine.moc"
