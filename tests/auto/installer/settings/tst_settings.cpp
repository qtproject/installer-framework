#include <QTest>
#include "settings.h"
#include "errors.h"
#include "repository.h"

#include <QFile>

using namespace QInstaller;

class tst_Settings : public QObject
{
    Q_OBJECT

private slots:
    void loadConfig();
    void loadNotExistingConfig();
    void loadMalformedConfig();
};

void tst_Settings::loadConfig()
{
    Settings settings =
            Settings::fromFileAndPrefix(":///data/tutorial_config.xml", ":///data");

    // specified values
    QCOMPARE(settings.applicationName(), QLatin1String("Your application"));
    QCOMPARE(settings.applicationVersion(), QLatin1String("1.2.3"));
    QCOMPARE(settings.title(), QLatin1String("Your application Installer"));
    QCOMPARE(settings.publisher(), QLatin1String("Your vendor"));
    QCOMPARE(settings.startMenuDir(), QLatin1String("Super App"));
    QCOMPARE(settings.targetDir(), QLatin1String("@rootDir@InstallationDirectory"));

    // default values
    QCOMPARE(settings.logo(), QLatin1String(":///data/"));
    QCOMPARE(settings.logoSmall(), QLatin1String(":///data/"));
    QCOMPARE(settings.url(), QString());
    QCOMPARE(settings.watermark(), QLatin1String(":///data/"));
    QCOMPARE(settings.background(), QLatin1String(":///data/"));
    QCOMPARE(settings.icon(), QLatin1String(":/installer.png"));
    QCOMPARE(settings.runProgram(), QString());
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
    QCOMPARE(settings.certificateFiles(), QStringList());
    QCOMPARE(settings.allowNonAsciiCharacters(), false);

    QCOMPARE(settings.hasReplacementRepos(), false);
    QCOMPARE(settings.repositories(), QSet<Repository>());
    QCOMPARE(settings.defaultRepositories(), QSet<Repository>());
    QCOMPARE(settings.temporaryRepositories(), QSet<Repository>());
    QCOMPARE(settings.userRepositories(), QSet<Repository>());

    QCOMPARE(settings.proxyType(), Settings::NoProxy);
    QCOMPARE(settings.ftpProxy(), QNetworkProxy());
    QCOMPARE(settings.httpProxy(), QNetworkProxy());
}

void tst_Settings::loadNotExistingConfig()
{
    try {
        Settings::fromFileAndPrefix(":/data/inexisting_config.xml", ":/data");
    } catch (const Error &error) {
        QVERIFY(error.message() == ("Could not open settings file :/data/inexisting_config.xml for reading: Unknown error"));
        return;
    }
    QFAIL("No exception thrown");
}

void tst_Settings::loadMalformedConfig()
{
    try {
        Settings::fromFileAndPrefix(":/data/malformed_config.xml", ":/data");
    } catch (const Error &error) {
        QVERIFY(error.message().startsWith("Xml parse error"));
        return;
    }
    QFAIL("No exception thrown");
}

QTEST_MAIN(tst_Settings)

#include "tst_settings.moc"
