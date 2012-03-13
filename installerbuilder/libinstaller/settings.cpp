/**************************************************************************
**
** This file is part of Installer Framework
**
** Copyright (c) 2011-2012 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (qt-info@nokia.com)
**
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** Other Usage
**
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
** If you have questions regarding the use of this file, please contact
** Nokia at qt-info@nokia.com.
**
**************************************************************************/
#include "settings.h"

#include "errors.h"
#include "qinstallerglobal.h"
#include "repository.h"

#include <QtCore/QFileInfo>
#include <QtCore/QStringList>

#include <QtXml/QXmlStreamReader>

using namespace QInstaller;

static const QLatin1String scIcon("Icon");
static const QLatin1String scLogo("Logo");
static const QLatin1String scPages("Pages");
static const QLatin1String scPrefix("Prefix");
static const QLatin1String scLogoSmall("LogoSmall");
static const QLatin1String scWatermark("Watermark");
static const QLatin1String scProductUrl("ProductUrl");
static const QLatin1String scBackground("Background");
static const QLatin1String scAdminTargetDir("AdminTargetDir");
static const QLatin1String scUninstallerName("UninstallerName");
static const QLatin1String scUserRepositories("UserRepositories");
static const QLatin1String scTmpRepositories("TemporaryRepositories");
static const QLatin1String scUninstallerIniFile("UninstallerIniFile");
static const QLatin1String scRemoteRepositories("RemoteRepositories");
static const QLatin1String scSigningCertificate("SigningCertificate");

static const QLatin1String scFtpProxy("FtpProxy");
static const QLatin1String scHttpProxy("HttpProxy");
static const QLatin1String scProxyType("ProxyType");

template <typename T>
static QSet<T> variantListToSet(const QVariantList &list)
{
    QSet<T> set;
    foreach (const QVariant &variant, list)
        set.insert(variant.value<T>());
    return set;
}

static QSet<Repository> readRepositories(QXmlStreamReader &reader, bool isDefault)
{
    QSet<Repository> set;
    while (reader.readNextStartElement()) {
        if (reader.name() == QLatin1String("Repository")) {
            Repository repo(QString(), isDefault);
            while (reader.readNextStartElement()) {
                if (reader.name() == QLatin1String("Url"))
                    repo.setUrl(reader.readElementText());
                else if (reader.name() == QLatin1String("Username"))
                    repo.setUsername(reader.readElementText());
                else if (reader.name() == QLatin1String("Password"))
                    repo.setPassword(reader.readElementText());
                else if (reader.name() == QLatin1String("Enabled"))
                    repo.setEnabled(bool(reader.readElementText().toInt()));
                else
                    reader.skipCurrentElement();
            }
            set.insert(repo);
        } else {
            reader.skipCurrentElement();
        }
    }
    return set;
}

static QVariantHash readTitles(QXmlStreamReader &reader)
{
    QVariantHash hash;
    while (reader.readNextStartElement())
        hash.insert(reader.name().toString(), reader.readElementText(QXmlStreamReader::SkipChildElements));
    return hash;
}

static QHash<QString, QVariantHash> readPages(QXmlStreamReader &reader)
{
    QHash<QString, QVariantHash> hash;
    while (reader.readNextStartElement()) {
        if (reader.name() == QLatin1String("Page")) {
            QVariantHash pageElements;
            QString pageName = reader.attributes().value(QLatin1String("name")).toString();
            while (reader.readNextStartElement()) {
                const QString name = reader.name().toString();
                if (name == QLatin1String("Title") || name == QLatin1String("SubTitle")) {
                    pageElements.insert(name, readTitles(reader));
                } else {
                    pageElements.insert(name, reader.readElementText(QXmlStreamReader::SkipChildElements));
                }
            }
            hash.insert(pageName, pageElements);
        }
    }
    return hash;
}


// -- Settings::Private

class Settings::Private : public QSharedData
{
public:
    Private()
        : m_replacementRepos(false)
    {}

    QVariantHash m_data;
    bool m_replacementRepos;

    QString makeAbsolutePath(const QString &path) const
    {
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
Settings Settings::fromFileAndPrefix(const QString &path, const QString &prefix)
{
    QFile file(path);
    QFile overrideConfig(QLatin1String(":/overrideconfig.xml"));

    if (overrideConfig.exists())
        file.setFileName(overrideConfig.fileName());

    if (!file.open(QIODevice::ReadOnly))
        throw Error(tr("Could not open settings file %1 for reading: %2").arg(path, file.errorString()));

    QXmlStreamReader reader(&file);
    if (reader.readNextStartElement()) {
        if (reader.name() != QLatin1String("Installer"))
            throw Error(tr("%1 is not valid: Installer root node expected.").arg(path));
    }

    QStringList blackList;
    blackList << scRemoteRepositories << scSigningCertificate << scPages;

    Settings s;
    s.d->m_data.insert(scPrefix, prefix);
    while (reader.readNextStartElement()) {
        const QString name = reader.name().toString();
        if (blackList.contains(name)) {
            if (name == scSigningCertificate)
                s.d->m_data.insertMulti(name, s.d->makeAbsolutePath(reader.readElementText()));

            if (name == scRemoteRepositories)
                s.addDefaultRepositories(readRepositories(reader, true));

            if (name == scPages) {
                QHash<QString, QVariantHash> pages = readPages(reader);
                const QStringList &keys = pages.keys();
                foreach (const QString &key, keys)
                    s.d->m_data.insert(key, pages.value(key));
            }
        } else {
            if (s.d->m_data.contains(name))
                throw Error(tr("Multiple %1 elements found, but only one allowed.").arg(name));
            s.d->m_data.insert(name, reader.readElementText(QXmlStreamReader::SkipChildElements));
        }
    }

    if (reader.error() != QXmlStreamReader::NoError) {
        throw Error(QString::fromLatin1("Xml parse error in %1: %2 Line: %3, Column: %4").arg(path)
            .arg(reader.errorString()).arg(reader.lineNumber()).arg(reader.columnNumber()));
    }

    // Add some possible missing values
    if (!s.d->m_data.contains(scRemoveTargetDir))
        s.d->m_data.insert(scRemoveTargetDir, scTrue);
    if (!s.d->m_data.contains(scUninstallerName))
        s.d->m_data.insert(scUninstallerName, QLatin1String("uninstall"));
    if (!s.d->m_data.contains(scTargetConfigurationFile))
        s.d->m_data.insert(scTargetConfigurationFile, QLatin1String("components.xml"));
    if (!s.d->m_data.contains(scUninstallerIniFile))
        s.d->m_data.insert(scUninstallerIniFile, s.uninstallerName() + QLatin1String(".ini"));

    return s;
}

QString Settings::logo() const
{
    return d->makeAbsolutePath(d->m_data.value(scLogo).toString());
}

QString Settings::logoSmall() const
{
    return d->makeAbsolutePath(d->m_data.value(scLogoSmall).toString());
}

QString Settings::title() const
{
    return d->m_data.value(scTitle).toString();
}

QString Settings::applicationName() const
{
    return d->m_data.value(scName).toString();
}

QString Settings::applicationVersion() const
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
    return d->makeAbsolutePath(d->m_data.value(scWatermark).toString());
}

QString Settings::background() const
{
    return d->makeAbsolutePath(d->m_data.value(scBackground).toString());
}

QString Settings::icon() const
{
    const QString icon = d->makeAbsolutePath(d->m_data.value(scIcon).toString());
#if defined(Q_WS_MAC)
    return icon + QLatin1String(".icns");
#elif defined(Q_WS_WIN)
    return icon + QLatin1String(".ico");
#endif
    return icon + QLatin1String(".png");
}

QString Settings::removeTargetDir() const
{
    return d->m_data.value(scRemoveTargetDir).toString();
}

QString Settings::uninstallerName() const
{
    return d->m_data.value(scUninstallerName).toString();
}

QString Settings::uninstallerIniFile() const
{
    return d->m_data.value(scUninstallerIniFile).toString();
}

QString Settings::runProgram() const
{
    return d->m_data.value(scRunProgram).toString();
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

QStringList Settings::certificateFiles() const
{
    return d->m_data.value(scSigningCertificate).toStringList();
}

bool Settings::allowNoneAsciiCharacters() const
{
    return d->m_data.value(scAllowNonAsciiCharacters).toBool();
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

void Settings::setDefaultRepositories(const QSet<Repository> &repositories)
{
    d->m_data.remove(scRepositories);
    addDefaultRepositories(repositories);
}

void Settings::addDefaultRepositories(const QSet<Repository> &repositories)
{
    foreach (const Repository &repository, repositories)
        d->m_data.insertMulti(scRepositories, QVariant().fromValue(repository));
}

Settings::Update
Settings::updateDefaultRepositories(const QHash<QString, QPair<Repository, Repository> > &updates)
{
    if (updates.isEmpty())
        return Settings::NoUpdatesApplied;

    QHash <QUrl, Repository> defaultRepos;
    foreach (const QVariant &variant, d->m_data.values(scRepositories)) {
        const Repository repository = variant.value<Repository>();
        defaultRepos.insert(repository.url(), repository);
    }

    bool update = false;
    QList<QPair<Repository, Repository> > values = updates.values(QLatin1String("replace"));
    for (int a = 0; a < values.count(); ++a) {
        const QPair<Repository, Repository> data = values.at(a);
        if (defaultRepos.contains(data.second.url())) {
            update = true;
            defaultRepos.remove(data.second.url());
            defaultRepos.insert(data.first.url(), data.first);
        }
    }

    values = updates.values(QLatin1String("remove"));
    for (int a = 0; a < values.count(); ++a) {
        const QPair<Repository, Repository> data = values.at(a);
        if (defaultRepos.contains(data.first.url())) {
            update = true;
            defaultRepos.remove(data.first.url());
        }
    }

    values = updates.values(QLatin1String("add"));
    for (int a = 0; a < values.count(); ++a) {
        const QPair<Repository, Repository> data = values.at(a);
        if (!defaultRepos.contains(data.first.url())) {
            update = true;
            defaultRepos.insert(data.first.url(), data.first);
        }
    }

    if (update)
        setDefaultRepositories(defaultRepos.values().toSet());
    return update ? Settings::UpdatesApplied : Settings::NoUpdatesApplied;
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
        d->m_data.insertMulti(scTmpRepositories, QVariant().fromValue(repository));
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
        d->m_data.insertMulti(scUserRepositories, QVariant().fromValue(repository));
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

QVariantHash Settings::titlesForPage(const QString &pageName) const
{
    const QVariantHash hash = d->m_data.value(pageName).toHash();
    const QVariant variant = hash.value(QLatin1String("Title"), QVariant());
    if (!variant.canConvert<QVariantHash>())
        return QVariantHash();
    return variant.value<QVariantHash>();
}

QVariantHash Settings::subTitlesForPage(const QString &pageName) const
{
    const QVariantHash hash = d->m_data.value(pageName).toHash();
    const QVariant variant = hash.value(QLatin1String("SubTitle"), QVariant());
    if (!variant.canConvert<QVariantHash>())
        return QVariantHash();
    return variant.value<QVariantHash>();
}

Settings::ProxyType Settings::proxyType() const
{
    return Settings::ProxyType(d->m_data.value(scProxyType, Settings::NoProxy).toInt());
}

void Settings::setProxyType(Settings::ProxyType type)
{
    d->m_data.insert(scProxyType, type);
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
    d->m_data.insert(scFtpProxy, QVariant::fromValue(proxy));
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
    d->m_data.insert(scHttpProxy, QVariant::fromValue(proxy));
}
