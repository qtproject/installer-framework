/**************************************************************************
**
** This file is part of Qt SDK**
**
** Copyright (c) 2011 Nokia Corporation and/or its subsidiary(-ies).*
**
** Contact:  Nokia Corporation qt-info@nokia.com**
**
** No Commercial Usage
**
** This file contains pre-release code and may not be distributed.
** You may use this file in accordance with the terms and conditions
** contained in the Technology Preview License Agreement accompanying
** this package.
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
** rights. These rights are described in the Nokia Qt LGPL Exception version
** 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** If you are unsure which license is appropriate for your use, please contact
** (qt-info@nokia.com).
**
**************************************************************************/
#include "settings.h"

#include "common/errors.h"
#include "common/repository.h"
#include "qinstallerglobal.h"

#include <QtCore/QFileInfo>
#include <QtCore/QStringList>

#include <QtXml/QXmlStreamReader>

using namespace QInstaller;


template <typename T>
static QSet<T> variantListToSet(const QVariantList &list)
{
    QSet<T> set;
    foreach (const QVariant &variant, list)
        set.insert(variant.value<T>());
    return set;
}

static QString splitTrimmed(const QString &string)
{
    if (string.isEmpty())
        return QString();

    const QStringList input = string.split(QRegExp(QLatin1String("\n|\r\n")));

    QStringList result;
    foreach (const QString &line, input)
        result.append(line.trimmed());
    result.append(QString());

    return result.join(QLatin1String("\n"));
}

static QList<Repository> readRemoteRepositories(QXmlStreamReader &reader)
{
    QSet<Repository> set;
    while (reader.readNextStartElement()) {
        if (reader.name() == QLatin1String("Repository")) {
            while (reader.readNextStartElement()) {
                if (reader.name() == QLatin1String("Url"))
                    set.insert(Repository(reader.readElementText()));
                else
                    reader.skipCurrentElement();
            }
        } else {
            reader.skipCurrentElement();
        }
    }
    return set.toList();
}


// -- Settings::Private

class Settings::Private : public QSharedData
{
public:
    QVariantHash m_data;

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

/*!
    @throws Error
*/
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
            throw Error(tr("%1 is not valid: Installer root node expected").arg(path));
    }

    QStringList blackList;
    blackList << scPrivateKey << scPublicKey << scRemoteRepositories << scSigningCertificate;

    Settings s;
    s.d->m_data.insert(scPrefix, prefix);
    while (reader.readNextStartElement()) {
        const QString name = reader.name().toString();
        if (blackList.contains(name)) {
            if (name == scPrivateKey || name == scPublicKey)
                s.d->m_data.insert(name, splitTrimmed(reader.readElementText()));

            if (name == scSigningCertificate)
                s.d->m_data.insertMulti(name, s.d->makeAbsolutePath(reader.readElementText()));

            if (name == scRemoteRepositories) {
                QList<Repository> repositories = readRemoteRepositories(reader);
                foreach (const Repository &repository, repositories)
                    s.d->m_data.insertMulti(scRepositories, QVariant().fromValue(repository));
            }
        } else {
            if (s.d->m_data.contains(name))
                throw Error(QObject::tr("Multiple %1 elements found, but only one allowed.").arg(name));
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

QString Settings::maintenanceTitle() const
{
    return d->m_data.value(scMaintenanceTitle).toString();
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

QByteArray Settings::privateKey() const
{
    return d->m_data.value(scPrivateKey).toByteArray();
}

QByteArray Settings::publicKey() const
{
    return d->m_data.value(scPublicKey).toByteArray();
}

QList<Repository> Settings::repositories() const
{
    return variantListToSet<Repository>(d->m_data.values(scRepositories)
        + d->m_data.values(scUserRepositories)).toList();
}

void Settings::setTemporaryRepositories(const QList<Repository> &repositories, bool replace)
{
    if (replace)
        d->m_data.remove(scRepositories);

    foreach (const Repository &repository, repositories.toSet())
        d->m_data.insertMulti(scRepositories, QVariant().fromValue(repository));
}

QList<Repository> Settings::userRepositories() const
{
    return variantListToSet<Repository>(d->m_data.values(scUserRepositories)).toList();
}

void Settings::addUserRepositories(const QList<Repository> &repositories)
{
    foreach (const Repository &repository, repositories.toSet())
        d->m_data.insertMulti(scUserRepositories, QVariant().fromValue(repository));
}

QVariant Settings::value(const QString &key, const QVariant &defaultValue)
{
    return d->m_data.value(key, defaultValue);
}

QVariantList Settings::values(const QString &key, const QVariantList &defaultValue)
{
    QVariantList list = d->m_data.values(key);
    return list.isEmpty() ? defaultValue : list;
}
