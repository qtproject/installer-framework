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
#include "settings.h"

#include "errors.h"
#include "qinstallerglobal.h"
#include "repository.h"
#include "repositorycategory.h"
#include "globals.h"
#include "fileutils.h"

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QStringList>
#include <QtCore/QStandardPaths>
#include <QtCore/QUuid>
#include <QtGui/QFontMetrics>
#include <QtWidgets/QApplication>

#include <QRegularExpression>
#include <QXmlStreamReader>

using namespace QInstaller;

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::Settings
    \internal
*/

/*!
    \typedef QInstaller::RepoHash

    Synonym for QHash<QString, QPair<Repository, Repository> >. Describes a repository
    update with the supported key strings being \e{replace}, \e{remove}, and \e{add}.
*/

static const QLatin1String scInstallerApplicationIcon("InstallerApplicationIcon");
static const QLatin1String scInstallerWindowIcon("InstallerWindowIcon");
static const QLatin1String scPrefix("Prefix");
static const QLatin1String scProductUrl("ProductUrl");
static const QLatin1String scAdminTargetDir("AdminTargetDir");
static const QLatin1String scMaintenanceToolName("MaintenanceToolName");
static const QLatin1String scUserRepositories("UserRepositories");
static const QLatin1String scTmpRepositories("TemporaryRepositories");
static const QLatin1String scMaintenanceToolIniFile("MaintenanceToolIniFile");
static const QLatin1String scMaintenanceToolAlias("MaintenanceToolAlias");
static const QLatin1String scDependsOnLocalInstallerBinary("DependsOnLocalInstallerBinary");
static const QLatin1String scTranslations("Translations");
static const QLatin1String scCreateLocalRepository("CreateLocalRepository");
static const QLatin1String scInstallActionColumnVisible("InstallActionColumnVisible");

static const QLatin1String scFtpProxy("FtpProxy");
static const QLatin1String scHttpProxy("HttpProxy");
static const QLatin1String scProxyType("ProxyType");

static const QLatin1String scLocalCachePath("LocalCachePath");

const char scControlScript[] = "ControlScript";

template <typename T>
static QSet<T> variantListToSet(const QVariantList &list)
{
    QSet<T> set;
    foreach (const QVariant &variant, list)
        set.insert(variant.value<T>());
    return set;
}

static void raiseError(QXmlStreamReader &reader, const QString &error, Settings::ParseMode parseMode)
{
    if (parseMode == Settings::StrictParseMode) {
        reader.raiseError(error);
    } else {
        QFile *xmlFile = qobject_cast<QFile*>(reader.device());
        if (xmlFile) {
            qCWarning(QInstaller::lcInstallerInstallLog).noquote().nospace()
                    << "Ignoring following settings reader error in " << xmlFile->fileName()
                                 << ", line " << reader.lineNumber() << ", column " << reader.columnNumber()
                                 << ": " << error;
        } else {
            qCWarning(QInstaller::lcInstallerInstallLog) << "Ignoring following settings reader error: "
                << qPrintable(error);
        }
    }
}

static QStringList readArgumentAttributes(QXmlStreamReader &reader, Settings::ParseMode parseMode,
                                          const QString &tagName, bool lc = false)
{
    QStringList arguments;

    while (QXmlStreamReader::TokenType token = reader.readNext()) {
        switch (token) {
            case QXmlStreamReader::StartElement: {
                if (!reader.attributes().isEmpty()) {
                    raiseError(reader, QString::fromLatin1("Unexpected attribute for element \"%1\".")
                        .arg(reader.name().toString()), parseMode);
                    return arguments;
                } else {
                    if (reader.name().toString() == tagName) {
                        (lc) ? arguments.append(reader.readElementText().toLower()) :
                               arguments.append(reader.readElementText());
                    } else {
                        raiseError(reader, QString::fromLatin1("Unexpected element \"%1\".").arg(reader.name()
                            .toString()), parseMode);
                        return arguments;
                    }
                }
            }
            break;
            case QXmlStreamReader::Characters: {
                if (reader.isWhitespace())
                    continue;
                arguments.append(reader.text().toString().split(QRegularExpression(QLatin1String("\\s+")),
                    Qt::SkipEmptyParts));
            }
            break;
            case QXmlStreamReader::EndElement: {
                return arguments;
            }
            default:
            break;
        }
    }
    return arguments;
}

static QMap<QString, QVariant> readProductImages(QXmlStreamReader &reader)
{
    QMap<QString, QVariant> productImages;
    while (reader.readNextStartElement()) {
        if (reader.name() == QLatin1String("ProductImage")) {
            QString key = QString();
            QString value = QString();
            while (reader.readNextStartElement()) {
                if (reader.name() == QLatin1String("Image")) {
                    key = reader.readElementText();
                } else if (reader.name() == QLatin1String("Url")) {
                    value = reader.readElementText();
                }
            }
            productImages.insert(key, value);
        }
    }
    return productImages;
}

static QSet<Repository> readRepositories(QXmlStreamReader &reader, bool isDefault, Settings::ParseMode parseMode,
                                         QString *displayName = nullptr, bool *preselected = nullptr,
                                         QString *tooltip = nullptr)
{
    QSet<Repository> set;
    while (reader.readNextStartElement()) {
        if (reader.name() == QLatin1String("DisplayName"))  {
            //remote repository can have also displayname. Needed when creating archive repositories
            *displayName = reader.readElementText();
        } else if (reader.name() == QLatin1String("Repository")) {
            Repository repo(QString(), isDefault);
            while (reader.readNextStartElement()) {
                if (reader.name() == QLatin1String("Url")) {
                    repo.setUrl(reader.readElementText());
                } else if (reader.name() == QLatin1String("Username")) {
                    repo.setUsername(reader.readElementText());
                } else if (reader.name() == QLatin1String("Password")) {
                    repo.setPassword(reader.readElementText());
                } else if (reader.name() == QLatin1String("DisplayName")) {
                    repo.setDisplayName(reader.readElementText());
                } else if (reader.name() == QLatin1String("Enabled")) {
                    repo.setEnabled(bool(reader.readElementText().toInt()));
                } else {
                    raiseError(reader, QString::fromLatin1("Unexpected element \"%1\".").arg(reader.name()
                        .toString()), parseMode);
                }

                if (!reader.attributes().isEmpty()) {
                    raiseError(reader, QString::fromLatin1("Unexpected attribute for element \"%1\".")
                        .arg(reader.name().toString()), parseMode);
                }
            }
            if (displayName && !displayName->isEmpty())
                repo.setCategoryName(*displayName);
            set.insert(repo);
        } else if (reader.name() == QLatin1String("Tooltip")) {
            *tooltip = reader.readElementText();
        }  else if (reader.name() == QLatin1String("Preselected")) {
            *preselected = (reader.readElementText() == QLatin1String("true") ? true : false);
        } else {
            raiseError(reader, QString::fromLatin1("Unexpected element \"%1\".").arg(reader.name().toString()),
                parseMode);
        }

        if (!reader.attributes().isEmpty()) {
            raiseError(reader, QString::fromLatin1("Unexpected attribute for element \"%1\".").arg(reader
                .name().toString()), parseMode);
        }
    }
    return set;
}

static QSet<RepositoryCategory> readRepositoryCategories(QXmlStreamReader &reader, bool isDefault, Settings::ParseMode parseMode,
                                                         QString *repositoryCategoryName)
{
    QSet<RepositoryCategory> archiveSet;
    while (reader.readNextStartElement()) {
        if (reader.name() == QLatin1String("RemoteRepositories")) {
            RepositoryCategory archiveRepo;
            QString displayName;
            QString tooltip;
            bool preselected = false;
            archiveRepo.setRepositories(readRepositories(reader, isDefault, parseMode,
                                                         &displayName, &preselected, &tooltip));
            archiveRepo.setDisplayName(displayName);
            archiveRepo.setTooltip(tooltip);
            archiveRepo.setEnabled(preselected);
            archiveSet.insert(archiveRepo);
        } else if (reader.name() == QLatin1String("RepositoryCategoryDisplayname")) {
            *repositoryCategoryName = reader.readElementText();
        }
    }
    return archiveSet;
}

// -- Settings::Private

class Settings::Private : public QSharedData
{
public:
    Private()
        : m_replacementRepos(false)
    {}

    QMultiHash<QString, QVariant> m_data;
    bool m_replacementRepos;

    QString absolutePathFromKey(const QString &key, const QString &suffix = QString()) const
    {
        const QString value = m_data.value(key).toString();
        if (value.isEmpty())
            return QString();

        const QString path = value + suffix;
        if (QFileInfo(path).isAbsolute())
            return path;
        return m_data.value(scPrefix).toString() + QLatin1String("/") + path;
    }
};


// -- Settings

Settings::Settings()
    : d(new Private)
{
}

Settings::~Settings()
{
}

Settings::Settings(const Settings &other)
    : d(other.d)
{
}

Settings& Settings::operator=(const Settings &other)
{
    Settings copy(other);
    std::swap(d, copy.d);
    return *this;
}

/* static */
Settings Settings::fromFileAndPrefix(const QString &path, const QString &prefix, ParseMode parseMode)
{
    QFile file(path);
    QFile overrideConfig(QLatin1String(":/overrideconfig.xml"));

    if (overrideConfig.exists())
        file.setFileName(overrideConfig.fileName());

    if (!file.open(QIODevice::ReadOnly))
        throw Error(tr("Cannot open settings file %1 for reading: %2").arg(path, file.errorString()));

    QXmlStreamReader reader(&file);
    if (reader.readNextStartElement()) {
        if (reader.name() != QLatin1String("Installer")) {
            reader.raiseError(QString::fromLatin1("Unexpected element \"%1\" as root element.").arg(reader
                .name().toString()));
        }
    }
    QStringList elementList;
    elementList << scName << scVersion << scTitle << scPublisher << scProductUrl
                << scTargetDir << scAdminTargetDir
                << scInstallerApplicationIcon << scInstallerWindowIcon
                << scLogo << scWatermark << scBanner << scBackground << scPageListPixmap << scAliasDefinitionsFile
                << scStartMenuDir << scMaintenanceToolName << scMaintenanceToolIniFile << scMaintenanceToolAlias
                << scRemoveTargetDir << scLocalCacheDir << scPersistentLocalCache
                << scRunProgram << scRunProgramArguments << scRunProgramDescription
                << scDependsOnLocalInstallerBinary
                << scAllowSpaceInPath << scAllowNonAsciiCharacters << scAllowRepositoriesForOfflineInstaller
                << scDisableAuthorizationFallback << scDisableCommandLineInterface
                << scWizardStyle << scStyleSheet << scTitleColor
                << scWizardDefaultWidth << scWizardDefaultHeight << scWizardMinimumWidth << scWizardMinimumHeight
                << scWizardShowPageList << scProductImages
                << scRepositorySettingsPageVisible << scTargetConfigurationFile
                << scRemoteRepositories << scTranslations << scUrlQueryString << QLatin1String(scControlScript)
                << scCreateLocalRepository << scInstallActionColumnVisible << scSupportsModify << scAllowUnstableComponents
                << scSaveDefaultRepositories << scRepositoryCategories;

    Settings s;
    s.d->m_data.replace(scPrefix, prefix);
    while (reader.readNextStartElement()) {
        const QString name = reader.name().toString();
        if (!elementList.contains(name))
            raiseError(reader, QString::fromLatin1("Unexpected element \"%1\".").arg(name), parseMode);

        if (!reader.attributes().isEmpty()) {
            raiseError(reader, QString::fromLatin1("Unexpected attribute for element \"%1\".").arg(name),
                parseMode);
        }

        if (s.d->m_data.contains(name))
            reader.raiseError(QString::fromLatin1("Element \"%1\" has been defined before.").arg(name));

        if (name == scTranslations) {
            s.setTranslations(readArgumentAttributes(reader, parseMode, QLatin1String("Translation"), false));
        } else if (name == scRunProgramArguments) {
            s.setRunProgramArguments(readArgumentAttributes(reader, parseMode, QLatin1String("Argument")));
        } else if (name == scProductImages) {
            s.setProductImages(readProductImages(reader));
        } else if (name == scRemoteRepositories) {
            s.addDefaultRepositories(readRepositories(reader, true, parseMode));
        } else if (name == scRepositoryCategories) {
            QString repositoryCategoryName;
            s.addRepositoryCategories(readRepositoryCategories(reader, true, parseMode, &repositoryCategoryName));
            if (!repositoryCategoryName.isEmpty()) {
                s.setRepositoryCategoryDisplayName(repositoryCategoryName);
            }
        } else {
            s.d->m_data.replace(name, reader.readElementText(QXmlStreamReader::SkipChildElements));
        }
    }
    if (reader.error() != QXmlStreamReader::NoError) {
        throw Error(QString::fromLatin1("Error in %1, line %2, column %3: %4").arg(path).arg(reader
            .lineNumber()).arg(reader.columnNumber()).arg(reader.errorString()));
    }

    if (s.d->m_data.value(scName).isNull())
        throw Error(QString::fromLatin1("Missing or empty <Name> tag in %1.").arg(file.fileName()));
    if (s.d->m_data.value(scVersion).isNull())
        throw Error(QString::fromLatin1("Missing or empty <Version> tag in %1.").arg(file.fileName()));

    // Add some possible missing values
    if (!s.d->m_data.contains(scInstallerApplicationIcon))
        s.d->m_data.replace(scInstallerApplicationIcon, QLatin1String(":/installer"));
    if (!s.d->m_data.contains(scInstallerWindowIcon)) {
        s.d->m_data.replace(scInstallerWindowIcon,
                           QString(QLatin1String(":/installer") + s.systemIconSuffix()));
    }
    if (!s.d->m_data.contains(scRemoveTargetDir))
        s.d->m_data.replace(scRemoveTargetDir, scTrue);
    if (s.d->m_data.value(scMaintenanceToolName).toString().isEmpty()) {
        s.d->m_data.replace(scMaintenanceToolName,
            // TODO: Remove deprecated 'UninstallerName'.
            s.d->m_data.value(QLatin1String("UninstallerName"), QLatin1String("maintenancetool"))
            .toString());
    }

    if (s.d->m_data.value(scTargetConfigurationFile).toString().isEmpty())
        s.d->m_data.replace(scTargetConfigurationFile, QLatin1String("components.xml"));
    if (s.d->m_data.value(scMaintenanceToolIniFile).toString().isEmpty()) {
        s.d->m_data.replace(scMaintenanceToolIniFile,
            // TODO: Remove deprecated 'UninstallerIniFile'.
            s.d->m_data.value(QLatin1String("UninstallerIniFile"), QString(s.maintenanceToolName()
            + QLatin1String(".ini"))).toString());
    }
    if (!s.d->m_data.contains(scDependsOnLocalInstallerBinary))
        s.d->m_data.replace(scDependsOnLocalInstallerBinary, false);
    if (!s.d->m_data.contains(scRepositorySettingsPageVisible))
        s.d->m_data.replace(scRepositorySettingsPageVisible, true);
    if (!s.d->m_data.contains(scCreateLocalRepository))
        s.d->m_data.replace(scCreateLocalRepository, false);
    if (!s.d->m_data.contains(scInstallActionColumnVisible))
        s.d->m_data.replace(scInstallActionColumnVisible, false);
    if (!s.d->m_data.contains(scAllowUnstableComponents))
        s.d->m_data.replace(scAllowUnstableComponents, false);
    if (!s.d->m_data.contains(scSaveDefaultRepositories))
        s.d->m_data.replace(scSaveDefaultRepositories, true);
    return s;
}

QString Settings::logo() const
{
    return d->absolutePathFromKey(scLogo);
}

QString Settings::title() const
{
    return d->m_data.value(scTitle).toString();
}

QString Settings::applicationName() const
{
    return d->m_data.value(scName).toString();
}

QString Settings::version() const
{
    return d->m_data.value(scVersion).toString();
}

QString Settings::publisher() const
{
    return d->m_data.value(scPublisher).toString();
}

QString Settings::url() const
{
    return d->m_data.value(scProductUrl).toString();
}

QString Settings::watermark() const
{
    return d->absolutePathFromKey(scWatermark);
}

QString Settings::banner() const
{
    return d->absolutePathFromKey(scBanner);
}

QString Settings::background() const
{
    return d->absolutePathFromKey(scBackground);
}

QString Settings::pageListPixmap() const
{
    return d->absolutePathFromKey(scPageListPixmap);
}

QString Settings::wizardStyle() const
{
    return d->m_data.value(scWizardStyle).toString();
}

QString Settings::styleSheet() const
{
    return d->absolutePathFromKey(scStyleSheet);
}

QString Settings::titleColor() const
{
    return d->m_data.value(scTitleColor).toString();
}

static int lengthToInt(const QVariant &variant)
{
    QString length = variant.toString().trimmed();
    if (length.endsWith(QLatin1String("em"), Qt::CaseInsensitive)) {
        length.chop(2);
        return qRound(length.toDouble() * QFontMetricsF(QApplication::font()).height());
    }
    if (length.endsWith(QLatin1String("ex"), Qt::CaseInsensitive)) {
        length.chop(2);
        return qRound(length.toDouble() * QFontMetricsF(QApplication::font()).xHeight());
    }
    if (length.endsWith(QLatin1String("px"), Qt::CaseInsensitive)) {
        length.chop(2);
    }
    return length.toInt();
}

int Settings::wizardDefaultWidth() const
{
    // Add a bit more sensible default width in case the page list widget is shown
    // as it can take quite a lot horizontal space. A vendor can always override
    // the default value.
    return lengthToInt(d->m_data.value(scWizardDefaultWidth, wizardShowPageList() ? 800 : 0));
}

int Settings::wizardDefaultHeight() const
{
    return lengthToInt(d->m_data.value(scWizardDefaultHeight));
}

int Settings::wizardMinimumWidth() const
{
    return lengthToInt(d->m_data.value(scWizardMinimumWidth));
}

int Settings::wizardMinimumHeight() const
{
    return lengthToInt(d->m_data.value(scWizardMinimumHeight));
}

bool Settings::wizardShowPageList() const
{
    return d->m_data.value(scWizardShowPageList, true).toBool();
}

QMap<QString, QVariant> Settings::productImages() const
{
    const QVariant variant = d->m_data.value(scProductImages);
    QMap<QString, QVariant> imagePaths;
    if (variant.canConvert<QVariantMap>())
        imagePaths = variant.toMap();
    return imagePaths;
}

void Settings::setProductImages(const QMap<QString, QVariant> &images)
{
    d->m_data.insert(scProductImages, QVariant::fromValue(images));
}

QString Settings::aliasDefinitionsFile() const
{
    return d->absolutePathFromKey(scAliasDefinitionsFile);
}

QString Settings::installerApplicationIcon() const
{
    return d->absolutePathFromKey(scInstallerApplicationIcon, systemIconSuffix());
}

QString Settings::installerWindowIcon() const
{
    return d->absolutePathFromKey(scInstallerWindowIcon);
}

QString Settings::systemIconSuffix() const
{
#if defined(Q_OS_MACOS)
    return QLatin1String(".icns");
#elif defined(Q_OS_WIN)
    return QLatin1String(".ico");
#endif
    return QLatin1String(".png");
}


QString Settings::removeTargetDir() const
{
    return d->m_data.value(scRemoveTargetDir).toString();
}

QString Settings::maintenanceToolName() const
{
    return d->m_data.value(scMaintenanceToolName).toString();
}

QString Settings::maintenanceToolIniFile() const
{
    return d->m_data.value(scMaintenanceToolIniFile).toString();
}

QString Settings::maintenanceToolAlias() const
{
    return d->m_data.value(scMaintenanceToolAlias).toString();
}

QString Settings::runProgram() const
{
    return d->m_data.value(scRunProgram).toString();
}

QStringList Settings::runProgramArguments() const
{
    const QVariant variant = d->m_data.value(scRunProgramArguments);
    if (variant.canConvert<QStringList>())
        return variant.value<QStringList>();
    return QStringList();
}

void Settings::setRunProgramArguments(const QStringList &arguments)
{
    d->m_data.replace(scRunProgramArguments, arguments);
}


QString Settings::runProgramDescription() const
{
    return d->m_data.value(scRunProgramDescription).toString();
}

QString Settings::startMenuDir() const
{
    return d->m_data.value(scStartMenuDir).toString();
}

QString Settings::targetDir() const
{
    return d->m_data.value(scTargetDir).toString();
}

QString Settings::adminTargetDir() const
{
    return d->m_data.value(scAdminTargetDir).toString();
}

QString Settings::configurationFileName() const
{
    return d->m_data.value(scTargetConfigurationFile).toString();
}

bool Settings::createLocalRepository() const
{
    return d->m_data.value(scCreateLocalRepository).toBool();
}

bool Settings::installActionColumnVisible() const
{
    return d->m_data.value(scInstallActionColumnVisible, false).toBool();
}

bool Settings::allowSpaceInPath() const
{
    return d->m_data.value(scAllowSpaceInPath, true).toBool();
}

bool Settings::allowNonAsciiCharacters() const
{
    return d->m_data.value(scAllowNonAsciiCharacters, false).toBool();
}

bool Settings::allowRepositoriesForOfflineInstaller() const
{
    return d->m_data.value(scAllowRepositoriesForOfflineInstaller, true).toBool();
}

bool Settings::disableAuthorizationFallback() const
{
    return d->m_data.value(scDisableAuthorizationFallback, false).toBool();
}

bool Settings::disableCommandLineInterface() const
{
    return d->m_data.value(scDisableCommandLineInterface, false).toBool();
}

bool Settings::dependsOnLocalInstallerBinary() const
{
    return d->m_data.value(scDependsOnLocalInstallerBinary).toBool();
}

bool Settings::hasReplacementRepos() const
{
    return d->m_replacementRepos;
}

QSet<Repository> Settings::repositories() const
{
    if (d->m_replacementRepos)
        return variantListToSet<Repository>(d->m_data.values(scTmpRepositories));

    return variantListToSet<Repository>(d->m_data.values(scRepositories)
        + d->m_data.values(scUserRepositories) + d->m_data.values(scTmpRepositories));
}

QSet<Repository> Settings::defaultRepositories() const
{
    return variantListToSet<Repository>(d->m_data.values(scRepositories));
}

QSet<RepositoryCategory> Settings::repositoryCategories() const
{
    return variantListToSet<RepositoryCategory>(d->m_data.values(scRepositoryCategories));
}

QMap<QString, RepositoryCategory> Settings::organizedRepositoryCategories() const
{
    QSet<RepositoryCategory> categories = repositoryCategories();
    QMap<QString, RepositoryCategory> map;
    foreach (const RepositoryCategory &category, categories) {
        map.insert(category.displayname(), category);
    }
    return map;
}

void Settings::setDefaultRepositories(const QSet<Repository> &repositories)
{
    d->m_data.remove(scRepositories);
    addDefaultRepositories(repositories);
}

void Settings::addDefaultRepositories(const QSet<Repository> &repositories)
{
    foreach (const Repository &repository, repositories)
        d->m_data.insert(scRepositories, QVariant().fromValue(repository));
}

void Settings::setRepositoryCategories(const QSet<RepositoryCategory> &repositories)
{
    d->m_data.remove(scRepositoryCategories);
    addRepositoryCategories(repositories);
}

void Settings::addRepositoryCategories(const QSet<RepositoryCategory> &repositories)
{
    foreach (const RepositoryCategory &repository, repositories)
        d->m_data.insert(scRepositoryCategories, QVariant().fromValue(repository));
}

Settings::Update Settings::updateRepositoryCategories(const RepoHash &updates)
{
    if (updates.isEmpty())
        return Settings::NoUpdatesApplied;

    QSet<RepositoryCategory> categories = repositoryCategories();
    QList<RepositoryCategory> categoriesList = categories.values();
    QPair<Repository, Repository> updateValues = updates.value(QLatin1String("replace"));

    bool update = false;

    foreach (RepositoryCategory category, categoriesList) {
        QSet<Repository> repositories = category.repositories();
        if (repositories.contains(updateValues.first)) {
            update = true;
            repositories.remove(updateValues.first);
            repositories.insert(updateValues.second);
            category.setRepositories(repositories, true);
            categoriesList.replace(categoriesList.indexOf(category), category);
        }
    }
    if (update) {
        categories = QSet<RepositoryCategory>(categoriesList.begin(), categoriesList.end());
        setRepositoryCategories(categories);
    }
    return update ? Settings::UpdatesApplied : Settings::NoUpdatesApplied;
}

static bool apply(const RepoHash &updates, QMultiHash<QUrl, Repository> *reposToUpdate)
{
    bool update = false;
    QList<QPair<Repository, Repository> > values = updates.values(QLatin1String("replace"));
    for (int a = 0; a < values.count(); ++a) {
        const QPair<Repository, Repository> data = values.at(a);
        if (reposToUpdate->contains(data.first.url())) {
            update = true;
            reposToUpdate->remove(data.first.url());
            reposToUpdate->insert(data.second.url(), data.second);
        }
    }

    values = updates.values(QLatin1String("remove"));
    for (int a = 0; a < values.count(); ++a) {
        const QPair<Repository, Repository> data = values.at(a);
        if (reposToUpdate->contains(data.first.url())) {
            update = true;
            reposToUpdate->remove(data.first.url());
        }
    }

    values = updates.values(QLatin1String("add"));
    for (int a = 0; a < values.count(); ++a) {
        const QPair<Repository, Repository> data = values.at(a);
        if (!reposToUpdate->contains(data.first.url())) {
            update = true;
            reposToUpdate->insert(data.first.url(), data.first);
        }
    }
    return update;
}

Settings::Update Settings::updateDefaultRepositories(const RepoHash &updates)
{
    if (updates.isEmpty())
        return Settings::NoUpdatesApplied;

    QMultiHash <QUrl, Repository> defaultRepos;
    foreach (const QVariant &variant, d->m_data.values(scRepositories)) {
        const Repository repository = variant.value<Repository>();
        defaultRepos.insert(repository.url(), repository);
    }

    const bool updated = apply(updates, &defaultRepos);
    if (updated) {
        const QList<Repository> repositoriesList = defaultRepos.values();
        setDefaultRepositories(QSet<Repository>(repositoriesList.begin(), repositoriesList.end()));
    }
    return updated ? Settings::UpdatesApplied : Settings::NoUpdatesApplied;
}

QSet<Repository> Settings::temporaryRepositories() const
{
    return variantListToSet<Repository>(d->m_data.values(scTmpRepositories));
}

void Settings::setTemporaryRepositories(const QSet<Repository> &repositories, bool replace)
{
    d->m_data.remove(scTmpRepositories);
    addTemporaryRepositories(repositories, replace);
}

void Settings::addTemporaryRepositories(const QSet<Repository> &repositories, bool replace)
{
    d->m_replacementRepos = replace;
    foreach (const Repository &repository, repositories)
        d->m_data.insert(scTmpRepositories, QVariant().fromValue(repository));
}

QSet<Repository> Settings::userRepositories() const
{
    return variantListToSet<Repository>(d->m_data.values(scUserRepositories));
}

void Settings::setUserRepositories(const QSet<Repository> &repositories)
{
    d->m_data.remove(scUserRepositories);
    addUserRepositories(repositories);
}

void Settings::addUserRepositories(const QSet<Repository> &repositories)
{
    foreach (const Repository &repository, repositories)
        d->m_data.insert(scUserRepositories, QVariant().fromValue(repository));
}

Settings::Update Settings::updateUserRepositories(const RepoHash &updates)
{
    if (updates.isEmpty())
        return Settings::NoUpdatesApplied;

    QMultiHash <QUrl, Repository> reposToUpdate;
    foreach (const QVariant &variant, d->m_data.values(scUserRepositories)) {
        const Repository repository = variant.value<Repository>();
        reposToUpdate.insert(repository.url(), repository);
    }

    const bool updated = apply(updates, &reposToUpdate);
    if (updated) {
        const QList<Repository> repositoriesList = reposToUpdate.values();
        setUserRepositories(QSet<Repository>(repositoriesList.begin(), repositoriesList.end()));
    }
    return updated ? Settings::UpdatesApplied : Settings::NoUpdatesApplied;
}

bool Settings::containsValue(const QString &key) const
{
    return d->m_data.contains(key);
}

QVariant Settings::value(const QString &key, const QVariant &defaultValue) const
{
    return d->m_data.value(key, defaultValue);
}

QVariantList Settings::values(const QString &key, const QVariantList &defaultValue) const
{
    QVariantList list = d->m_data.values(key);
    return list.isEmpty() ? defaultValue : list;
}

bool Settings::repositorySettingsPageVisible() const
{
    return d->m_data.value(scRepositorySettingsPageVisible, true).toBool();
}

void Settings::setRepositorySettingsPageVisible(bool visible)
{
    d->m_data.replace(scRepositorySettingsPageVisible, visible);
}

bool Settings::persistentLocalCache() const
{
    return d->m_data.value(scPersistentLocalCache, true).toBool();
}

void Settings::setPersistentLocalCache(bool enable)
{
    d->m_data.replace(scPersistentLocalCache, enable);
}

QString Settings::localCacheDir() const
{
    const QString fallback = QLatin1String("qt-installer-framework") + QDir::separator()
        + QUuid::createUuidV3(QUuid(), applicationName()).toString(QUuid::WithoutBraces);
    return d->m_data.value(scLocalCacheDir, fallback).toString();
}

void Settings::setLocalCacheDir(const QString &dir)
{
    d->m_data.replace(scLocalCacheDir, dir);
}

QString Settings::localCachePath() const
{
    const QString fallback = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation)
        + QDir::separator() + localCacheDir();
    return d->m_data.value(scLocalCachePath, fallback).toString();
}

void Settings::setLocalCachePath(const QString &path)
{
    d->m_data.replace(scLocalCachePath, path);
}

Settings::ProxyType Settings::proxyType() const
{
    return Settings::ProxyType(d->m_data.value(scProxyType, Settings::NoProxy).toInt());
}

void Settings::setProxyType(Settings::ProxyType type)
{
    d->m_data.replace(scProxyType, type);
}

QNetworkProxy Settings::ftpProxy() const
{
    const QVariant variant = d->m_data.value(scFtpProxy);
    if (variant.canConvert<QNetworkProxy>())
        return variant.value<QNetworkProxy>();
    return QNetworkProxy();
}

void Settings::setFtpProxy(const QNetworkProxy &proxy)
{
    d->m_data.replace(scFtpProxy, QVariant::fromValue(proxy));
}

QNetworkProxy Settings::httpProxy() const
{
    const QVariant variant = d->m_data.value(scHttpProxy);
    if (variant.canConvert<QNetworkProxy>())
        return variant.value<QNetworkProxy>();
    return QNetworkProxy();
}

void Settings::setHttpProxy(const QNetworkProxy &proxy)
{
    d->m_data.replace(scHttpProxy, QVariant::fromValue(proxy));
}

QStringList Settings::translations() const
{
    const QVariant variant = d->m_data.value(scTranslations);
    if (variant.canConvert<QStringList>())
        return variant.value<QStringList>();
    return QStringList();
}

void Settings::setTranslations(const QStringList &translations)
{
    d->m_data.replace(scTranslations, translations);
}

QString Settings::controlScript() const
{
    return d->m_data.value(QLatin1String(scControlScript)).toString();
}

bool Settings::supportsModify() const
{
    return d->m_data.value(scSupportsModify, true).toBool();
}

bool Settings::allowUnstableComponents() const
{
    return d->m_data.value(scAllowUnstableComponents, true).toBool();
}

void Settings::setAllowUnstableComponents(bool allow)
{
    d->m_data.replace(scAllowUnstableComponents, allow);
}

bool Settings::saveDefaultRepositories() const
{
    return d->m_data.value(scSaveDefaultRepositories, true).toBool();
}

void Settings::setSaveDefaultRepositories(bool save)
{
    d->m_data.replace(scSaveDefaultRepositories, save);
}

QString Settings::repositoryCategoryDisplayName() const
{
    QString displayName = d->m_data.value(QLatin1String(scRepositoryCategoryDisplayName)).toString();
    return displayName.isEmpty() ? tr("Categories") : displayName;
}

void Settings::setRepositoryCategoryDisplayName(const QString& name)
{
    d->m_data.replace(scRepositoryCategoryDisplayName, name);
}
