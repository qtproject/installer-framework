#include "settings.h"
#include "errors.h"
#include "repository.h"

#include <QFile>
#include <QString>
#include <QTest>

using namespace QInstaller;

class tst_Settings : public QObject
{
    Q_OBJECT

private slots:
    void loadTutorialConfig();
    void loadFullConfig();
    void loadEmptyConfig();
    void loadNotExistingConfig();
    void loadMalformedConfig();
    void loadUnknownElementConfigInStrictParseMode();
    void loadUnknownElementConfigInRelaxedParseMode();
};

void tst_Settings::loadTutorialConfig()
{
    Settings settings = Settings::fromFileAndPrefix(":///data/tutorial_config.xml", ":///data");

    // specified values
    QCOMPARE(settings.applicationName(), QLatin1String("Your application"));
    QCOMPARE(settings.applicationVersion(), QLatin1String("1.2.3"));
    QCOMPARE(settings.title(), QLatin1String("Your application Installer"));
    QCOMPARE(settings.publisher(), QLatin1String("Your vendor"));
    QCOMPARE(settings.startMenuDir(), QLatin1String("Super App"));
    QCOMPARE(settings.targetDir(), QLatin1String("@rootDir@InstallationDirectory"));

    // default values
    QCOMPARE(settings.logo(), QLatin1String(":///data/"));
    QCOMPARE(settings.url(), QString());
    QCOMPARE(settings.watermark(), QLatin1String(":///data/"));
    QCOMPARE(settings.banner(), QLatin1String(":///data/"));
    QCOMPARE(settings.background(), QLatin1String(":///data/"));
#if defined(Q_OS_WIN)
    QCOMPARE(settings.icon(), QLatin1String(":/installer.ico"));
    QCOMPARE(settings.installerApplicationIcon(), QLatin1String(":/installer.ico"));
    QCOMPARE(settings.installerWindowIcon(), QLatin1String(":/installer.ico"));
#elif defined(Q_OS_MAC)
    QCOMPARE(settings.icon(), QLatin1String(":/installer.icns"));
    QCOMPARE(settings.installerApplicationIcon(), QLatin1String(":/installer.icns"));
    QCOMPARE(settings.installerWindowIcon(), QLatin1String(":/installer.icns"));
#else
    QCOMPARE(settings.icon(), QLatin1String(":/installer.png"));
    QCOMPARE(settings.installerApplicationIcon(), QLatin1String(":/installer.png"));
    QCOMPARE(settings.installerWindowIcon(), QLatin1String(":/installer.png"));
#endif
    QCOMPARE(settings.runProgram(), QString());
    QCOMPARE(settings.runProgramArguments(), QString());
    QCOMPARE(settings.runProgramDescription(), QString());
    QCOMPARE(settings.adminTargetDir(), QString());
    QCOMPARE(settings.removeTargetDir(), QLatin1String("true"));
    QCOMPARE(settings.uninstallerName(), QLatin1String("uninstall"));
    QCOMPARE(settings.uninstallerIniFile(), QLatin1String("uninstall.ini"));
    QCOMPARE(settings.configurationFileName(), QLatin1String("components.xml"));
    QCOMPARE(settings.dependsOnLocalInstallerBinary(), false);
    QCOMPARE(settings.repositorySettingsPageVisible(), true);
    QCOMPARE(settings.hasReplacementRepos(), false);
    QCOMPARE(settings.allowSpaceInPath(), false);
    QCOMPARE(settings.allowNonAsciiCharacters(), false);

    QCOMPARE(settings.hasReplacementRepos(), false);
    QCOMPARE(settings.repositories(), QSet<Repository>());
    QCOMPARE(settings.defaultRepositories(), QSet<Repository>());
    QCOMPARE(settings.temporaryRepositories(), QSet<Repository>());
    QCOMPARE(settings.userRepositories(), QSet<Repository>());

    QCOMPARE(settings.proxyType(), Settings::NoProxy);
    QCOMPARE(settings.ftpProxy(), QNetworkProxy());
    QCOMPARE(settings.httpProxy(), QNetworkProxy());

    QCOMPARE(settings.translations(), QStringList());
}

void tst_Settings::loadFullConfig()
{
    QTest::ignoreMessage(QtWarningMsg, "Deprecated element 'Icon'. ");
    Settings settings = Settings::fromFileAndPrefix(":///data/full_config.xml", ":///data");
}

void tst_Settings::loadEmptyConfig()
{
    QTest::ignoreMessage(QtDebugMsg, "create Error-Exception: \"Missing or empty <Name> tag in "
                         ":/data/empty_config.xml.\" ");
    try {
        Settings::fromFileAndPrefix(":/data/empty_config.xml", ":/data");
    } catch (const Error &error) {
        QCOMPARE(error.message(), QLatin1String("Missing or empty <Name> tag in :/data/empty_config.xml."));
        return;
    }
    QFAIL("No exception thrown");
}

void tst_Settings::loadNotExistingConfig()
{
    QTest::ignoreMessage(QtDebugMsg, "create Error-Exception: \"Could not open settings file "
                         ":/data/inexisting_config.xml for reading: "
                         "Unknown error\" ");
    try {
        Settings::fromFileAndPrefix(":/data/inexisting_config.xml", ":/data");
    } catch (const Error &error) {
        QCOMPARE(error.message(), QLatin1String("Could not open settings file "
                                                ":/data/inexisting_config.xml for reading: "
                                                "Unknown error"));
        return;
    }
    QFAIL("No exception thrown");
}

void tst_Settings::loadMalformedConfig()
{
    QTest::ignoreMessage(QtDebugMsg, "create Error-Exception: \"Error in :/data/malformed_config.xml, "
                         "line 9, column 0: Premature end of document.\" ");
    try {
        Settings::fromFileAndPrefix(":/data/malformed_config.xml", ":/data");
    } catch (const Error &error) {
        QCOMPARE(error.message(), QLatin1String("Error in :/data/malformed_config.xml, line 9, column 0: "
                                           "Premature end of document."));
        return;
    }
    QFAIL("No exception thrown");
}

void tst_Settings::loadUnknownElementConfigInStrictParseMode()
{
    QTest::ignoreMessage(QtDebugMsg, "create Error-Exception: \"Error in :/data/unknown_element_config.xml, "
        "line 5, column 13: Unexpected element 'unknown'.\" ");
    try {
        Settings::fromFileAndPrefix(":/data/unknown_element_config.xml", ":/data");
    } catch (const Error &error) {
        QCOMPARE(error.message(), QLatin1String("Error in :/data/unknown_element_config.xml, line 5, "
                                                "column 13: Unexpected element 'unknown'."));
        return;
    }
    QFAIL("No exception thrown");
}

void tst_Settings::loadUnknownElementConfigInRelaxedParseMode()
{
    QTest::ignoreMessage(QtWarningMsg, "\"Ignoring following settings reader error in "
        ":/data/unknown_element_config.xml, line 5, column 13: Unexpected element 'unknown'.\" ");
    try {
        Settings settings = Settings::fromFileAndPrefix(":/data/unknown_element_config.xml", ":/data",
            Settings::RelaxedParseMode);
        QCOMPARE(settings.title(), QLatin1String("Your application Installer"));
    } catch (const Error &error) {
        QFAIL(qPrintable(QString::fromLatin1("Got an exception in TolerantParseMode: %1").arg(error.message())));
    }
}

QTEST_MAIN(tst_Settings)

#include "tst_settings.moc"
