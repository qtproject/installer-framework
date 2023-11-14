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

#include <component.h>
#include <errors.h>
#include <updateoperation.h>
#include <updateoperationfactory.h>
#include <packagemanagercore.h>
#include <packagemanagergui.h>
#include <scriptengine.h>

#include <../unicodeexecutable/stringdata.h>

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

class EmptyArgOperation : public KDUpdater::UpdateOperation
{
public:
    explicit EmptyArgOperation(QInstaller::PackageManagerCore *core)
        : KDUpdater::UpdateOperation(core)
    {
        setName("EmptyArg");
    }

    void backup() {}
    bool performOperation() {
        return true;
    }
    bool undoOperation() {
        return true;
    }
    bool testOperation() {
        return true;
    }
};


// -- tst_ScriptEngine

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
        Component *component = new Component(&m_core);
        component->setValue(scName, "component.test.addOperation");
        m_core.appendRootComponent(component);

        m_scriptEngine = m_core.componentScriptEngine();
        m_applicatonDirPath = qApp->applicationDirPath();
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
        QCOMPARE(global.hasProperty(QLatin1String("QDesktopServices")), true);
        QCOMPARE(global.property(QLatin1String("QDesktopServices"))
            .hasProperty(QLatin1String("openUrl")), true);
        QCOMPARE(global.property(QLatin1String("QDesktopServices"))
            .hasProperty(QLatin1String("displayName")), true);
        QCOMPARE(global.property(QLatin1String("QDesktopServices"))
            .hasProperty(QLatin1String("storageLocation")), true);
        QCOMPARE(global.property(QLatin1String("QDesktopServices"))
                 .hasProperty(QLatin1String("findFiles")), true);

        QCOMPARE(global.hasProperty(QLatin1String("buttons")), true);
        QCOMPARE(global.hasProperty(QLatin1String("QInstaller")), true);
        QCOMPARE(global.hasProperty(QLatin1String("QMessageBox")), true);

        QCOMPARE(global.hasProperty(QLatin1String("gui")), true);
        QCOMPARE(global.hasProperty(QLatin1String("qsTr")), true);

        QCOMPARE(global.hasProperty(QLatin1String("systemInfo")), true);
        QJSValue sinfo = global.property(QLatin1String("systemInfo"));
        QCOMPARE(sinfo.property(QLatin1String("currentCpuArchitecture")).toString(),
                 QSysInfo::currentCpuArchitecture());
        QCOMPARE(sinfo.property(QLatin1String("buildCpuArchitecture")).toString(),
                 QSysInfo::buildCpuArchitecture());
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
        setExpectedScriptOutput("function receive()");

#if QT_VERSION >= QT_VERSION_CHECK(5,12,0)
        QTest::ignoreMessage(QtWarningMsg, "<Unknown File>:38: ReferenceError: foo is not defined");
#else
        QTest::ignoreMessage(QtWarningMsg, ":38: ReferenceError: foo is not defined");
#endif
        emiter.produceSignal();

        const QJSValue value = m_scriptEngine->evaluate("");
        QCOMPARE(value.isError(), false);
    }

    void testScriptPrint()
    {
        setExpectedScriptOutput("test");
        const QJSValue value = m_scriptEngine->evaluate("print(\"test\");");
        if (value.isError()) {
            QFAIL(qPrintable(QString::fromLatin1("ScriptEngine error:\n %1").arg(
                value.toString())));
        }
    }

    void testExistingInstallerObject()
    {
        setExpectedScriptOutput("object");
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

        setExpectedScriptOutput("component.test.name");
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

      setExpectedScriptOutput("component.test.name");
      const QJSValue value = m_scriptEngine->evaluate(script);
      if (value.isError()) {
        QFAIL(qPrintable(QString::fromLatin1("ScriptEngine error:\n %1").arg(
                           value.toString())));
      }
    }

    void testComponentsWithRegexp()
    {
      const QString script = QString::fromLatin1("var components = installer.components(\"component.test.addOperation\");"
                                                 "\n"
                                                 "for (i = 0; i < components.length; i++)"
                                                 "print(components[i].name);");

      setExpectedScriptOutput("component.test.addOperation");
      const QJSValue value = m_scriptEngine->evaluate(script);
      if (value.isError()) {
        QFAIL(qPrintable(QString::fromLatin1("ScriptEngine error:\n %1").arg(
                           value.toString())));
      }
    }

    void testFindFiles()
    {
        const QString expectedOutput = QString::fromLatin1("Found file %1/tst_scriptengine.moc").arg(m_applicatonDirPath);
        QByteArray array = expectedOutput.toLatin1();
        const char *c_str2 = array.data();

        setExpectedScriptOutput(c_str2);
        const QString script = QString::fromLatin1("var directory = \"C:/Qt/test\";"
                                                   "\n"
                                                   "var pattern = \"*.moc\";"
                                                   "\n"
                                                   "var fileArray = QDesktopServices.findFiles('%1', pattern)"
                                                   "\n"
                                                   "for (i = 0; i < fileArray.length; i++) {"
                                                   "print(\"Found file \"+fileArray[i]);"
                                                   "}").arg(m_applicatonDirPath);
        const QJSValue result = m_scriptEngine->evaluate(script);
        qDebug()<<result.isArray();
        qDebug()<<result.isObject();
        qDebug()<<result.isString();
        qDebug()<<result.isVariant();
        QCOMPARE(result.isError(), false);
    }

    void loadSimpleComponentScript_data()
    {
        QTest::addColumn<QString>("path");
        QTest::addColumn<bool>("postLoad");
        QTest::newRow("Pre component script") << ":///data/component1.qs" << false;
        QTest::newRow("Post component script") << ":///data/component1.qs" << true;
    }

    void loadSimpleComponentScript()
    {
        QFETCH(QString, path);
        QFETCH(bool, postLoad);

        try {
            // ignore retranslateUi which is called by evaluateComponentScript
            setExpectedScriptOutput("Component constructor - OK");
            setExpectedScriptOutput("retranslateUi - OK");
            m_component->evaluateComponentScript(path, postLoad);

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

            setExpectedScriptOutput("isDefault - OK");
            bool returnIsDefault = m_component->isDefault();
            QCOMPARE(returnIsDefault, false);

        } catch (const Error &error) {
            QFAIL(qPrintable(error.message()));
        }
    }

    void loadBrokenComponentScript_data()
    {
        QTest::addColumn<QString>("path");
        QTest::addColumn<bool>("postLoad");
        QTest::newRow("Pre component script") << ":///data/component2.qs" << false;
        QTest::newRow("Post component script") << ":///data/component2.qs" << true;
    }

    void loadBrokenComponentScript()
    {
        QFETCH(QString, path);
        QFETCH(bool, postLoad);

        Component *testComponent = new Component(&m_core);
        testComponent->setValue(scName, "broken.component");

        // m_core becomes the owner of testComponent, it will delete it in the destructor
        m_core.appendRootComponent(testComponent);

        try {
            // ignore Output from script
            setExpectedScriptOutput("script function: Component");
            testComponent->evaluateComponentScript(path, postLoad);
        } catch (const Error &error) {
            const QString debugMessage(
                QString("Exception while loading the component script \"%1\": "
                "ReferenceError: broken is not defined on line number: 33").arg(QDir::toNativeSeparators(":///data/component2.qs")));
            QVERIFY2(debugMessage.contains(error.message()), "(ReferenceError: broken is not defined)");
        }
    }

    void loadComponentUserInterfaces_data()
    {
        QTest::addColumn<QString>("path");
        QTest::addColumn<bool>("postLoad");
        QTest::newRow("Pre component script") << ":///data/userinterface.qs" << false;
        QTest::newRow("Post component script") << ":///data/userinterface.qs" << true;
    }

    void loadComponentUserInterfaces()
    {
        QFETCH(QString, path);
        QFETCH(bool, postLoad);
        try {
            setExpectedScriptOutput("checked: false");
            TestGui testGui(&m_core);
            m_component->loadUserInterfaces(QDir(":///data"), QStringList() << QLatin1String("form.ui"));
            m_component->evaluateComponentScript(path, postLoad);
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

            setExpectedScriptOutput("FinishedPageCallback - OK");
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

    void testInstallerExecuteEncodings_data()
    {
        QTest::addColumn<QString>("argumentsToInstallerExecute");
        QTest::addColumn<QString>("expectedOutput");
        QTest::addColumn<int>("expectedExitCode");

        QTest::newRow("default_encoding_ascii_output_exit_code_0")
            << QString::fromLatin1("['ascii', '0']") << QString::fromLatin1(asciiText) << 0;
        QTest::newRow("default_encoding_ascii_output_exit_code_52")
            << QString::fromLatin1("['ascii', '52']") << QString::fromLatin1(asciiText) << 52;

        QTest::newRow("latin1_encoding_ascii_output")
            << QString::fromLatin1("['ascii', '0'], '', 'latin1', 'latin1'") << QString::fromLatin1(asciiText) << 0;
        QTest::newRow("latin1_encoding_utf8_output")
            << QString::fromLatin1("['utf8', '0'], '', 'latin1', 'latin1'") << QString::fromLatin1(utf8Text) << 0;

        QTest::newRow("utf8_encoding_ascii_output")
            << QString::fromLatin1("['ascii', '0'], '', 'utf8', 'utf8'") << QString::fromUtf8(asciiText) << 0;
        QTest::newRow("utf8_encoding_utf8_output")
            << QString::fromLatin1("['utf8', '0'], '', 'utf8', 'utf8'") << QString::fromUtf8(utf8Text) << 0;
    }

    void testInstallerExecuteEncodings()
    {
        QString unicodeExecutableName = QLatin1String(BUILDDIR "/../unicodeexecutable/unicodeexecutable");
#if defined(Q_OS_WIN)
        unicodeExecutableName += QLatin1String(".exe");
#endif

        QFileInfo unicodeExecutable(unicodeExecutableName);
        if (!unicodeExecutable.isExecutable()) {
            QFAIL(qPrintable(QString::fromLatin1("ScriptEngine error: test program %1 is not executable")
                                .arg(unicodeExecutable.absoluteFilePath())));
            return;
        }

        const QString testProgramPath = unicodeExecutable.absoluteFilePath();

        QFETCH(QString, argumentsToInstallerExecute);
        QFETCH(QString, expectedOutput);
        QFETCH(int, expectedExitCode);

        QJSValue result = m_scriptEngine->evaluate(QString::fromLatin1("installer.execute('%1', %2);")
                                                   .arg(testProgramPath)
                                                   .arg(argumentsToInstallerExecute));
        QCOMPARE(result.isArray(), true);
        QCOMPARE(result.property(0).toString(), expectedOutput);
        QCOMPARE(result.property(1).toString(), QString::number(expectedExitCode));
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

    void testAddOperation_AddElevatedOperation()
    {
#if QT_VERSION < 0x50600
        QSKIP("Behavior changed from 5.6.0 onwards.");
#endif
        using namespace KDUpdater;
        UpdateOperationFactory &factory = UpdateOperationFactory::instance();
        factory.registerUpdateOperation<EmptyArgOperation>(QLatin1String("EmptyArg"));

        try {
            m_core.setPackageManager();
            Component *component = m_core.componentByName("component.test.addOperation");
            component->evaluateComponentScript(":///data/addOperation.qs");

            setExpectedScriptOutput("Component::createOperations()");
            component->createOperations();

            const OperationList operations = component->operations();
            QCOMPARE(operations.count(), 8);

            struct {
                const char* args[3];
                const char* operator[](int i) const {
                    return args[i];
                }
            } expectedArgs[] = {
                { "Arg", "Arg2", "" }, { "Arg", "", "Arg3" }, { "", "Arg2", "Arg3" }, { "Arg", "Arg2", "" },
                { "eArg", "eArg2", "" }, { "eArg", "", "eArg3" }, { "", "eArg2", "eArg3" }, { "eArg", "eArg2", "" }
            };

            for (int i = 0; i < operations.count(); ++i) {
                const QStringList arguments = operations[i]->arguments();
                QCOMPARE(arguments.count(), 3);
                for (int j = 0; j < 3; ++j)
                    QCOMPARE(arguments[j], QString(expectedArgs[i][j]));
            }
        } catch (const QInstaller::Error &error) {
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
    QString m_applicatonDirPath;

};


QTEST_MAIN(tst_ScriptEngine)

#include "tst_scriptengine.moc"
