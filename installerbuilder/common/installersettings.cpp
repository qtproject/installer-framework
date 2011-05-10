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
#include "installersettings.h"

#include "common/errors.h"
#include "common/repository.h"

#include <QtCore/QFileInfo>
#include <QtCore/QStringList>

#include <QtXml/QDomDocument>

using namespace QInstaller;


// -- InstallerSettings::Private

class InstallerSettings::Private : public QSharedData
{
public:
    QString prefix;
    QString logo;
    QString logoSmall;
    QString title;
    QString maintenanceTitle;
    QString name;
    QString version;
    QString publisher;
    QString url;
    QString watermark;
    QString background;
    QString runProgram;
    QString runProgramDescription;
    QString startMenuDir;
    QString targetDir;
    QString adminTargetDir;
    QString icon;
    QString removeTargetDir;
    QString uninstallerName;
    QString uninstallerIniFile;
    QString configurationFileName;
    QSet<Repository> m_repositories;
    QSet<Repository> m_userRepositories;
    QStringList certificateFiles;
    QByteArray privateKey;
    QByteArray publicKey;

    QString makeAbsolutePath(const QString &p) const
    {
        if (QFileInfo(p).isAbsolute())
            return p;
        return prefix + QLatin1String("/") + p;
    }
};


static QString readChild(const QDomElement &el, const QString &child, const QString &defValue = QString())
{
    const QDomNodeList list = el.elementsByTagName(child);
    if (list.size() > 1)
        throw Error(QObject::tr("Multiple %1 elements found, but only one allowed.").arg(child));
    return list.isEmpty() ? defValue : list.at(0).toElement().text();
}

#if 0
static QStringList readChildList(const QDomElement &el, const QString &child)
{
    const QDomNodeList list = el.elementsByTagName(child);
    QStringList res;
    for (int i = 0; i < list.size(); ++i)
        res += list.at(i).toElement().text();
    return res;
}
#endif

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


// -- InstallerSettings

InstallerSettings::InstallerSettings()
    : d(new Private)
{
}

InstallerSettings::~InstallerSettings()
{
}

InstallerSettings::InstallerSettings(const InstallerSettings &other)
    : d(other.d)
{
}

InstallerSettings& InstallerSettings::operator=(const InstallerSettings &other)
{
    InstallerSettings copy(other);
    std::swap(d, copy.d);
    return *this;
}

/*!
    @throws QInstallerError
*/
InstallerSettings InstallerSettings::fromFileAndPrefix(const QString &path, const QString &prefix)
{
    QDomDocument doc;
    QFile file(path);
    QFile overrideConfig(QLatin1String(":/overrideconfig.xml"));

    if (overrideConfig.exists())
        file.setFileName(overrideConfig.fileName());

    if (!file.open(QIODevice::ReadOnly))
        throw Error(tr("Could not open settings file %1 for reading: %2").arg(path, file.errorString()));

    QString msg;
    int line = 0;
    int col = 0;
    if (!doc.setContent(&file, &msg, &line, &col)) {
        throw Error(tr("Could not parse settings file %1: %2:%3: %4").arg(path, QString::number(line),
            QString::number(col), msg));
    }

    const QDomElement root = doc.documentElement();
    if (root.tagName() != QLatin1String("Installer"))
        throw Error(tr("%1 is not valid: Installer root node expected"));

    InstallerSettings s;
    s.d->prefix = prefix;
    s.d->logo = readChild(root, QLatin1String("Logo"));
    s.d->logoSmall = readChild(root, QLatin1String("LogoSmall"));
    s.d->title = readChild(root, QLatin1String("Title"));
    s.d->maintenanceTitle = readChild(root, QLatin1String("MaintenanceTitle"));
    s.d->name = readChild(root, QLatin1String("Name"));
    s.d->version = readChild(root, QLatin1String("Version"));
    s.d->publisher = readChild(root, QLatin1String("Publisher"));
    s.d->url = readChild(root, QLatin1String("ProductUrl"));
    s.d->watermark = readChild(root, QLatin1String("Watermark"));
    s.d->background = readChild(root, QLatin1String("Background"));
    s.d->runProgram = readChild(root, QLatin1String("RunProgram"));
    s.d->runProgramDescription = readChild(root, QLatin1String("RunProgramDescription"));
    s.d->startMenuDir = readChild(root, QLatin1String("StartMenuDir"));
    s.d->targetDir = readChild(root, QLatin1String("TargetDir"));
    s.d->adminTargetDir = readChild(root, QLatin1String("AdminTargetDir"));
    s.d->icon = readChild(root, QLatin1String("Icon"));
    s.d->removeTargetDir = readChild(root, QLatin1String("RemoveTargetDir"), QLatin1String("true"));
    s.d->uninstallerName = readChild(root, QLatin1String("UninstallerName"), QLatin1String("uninstall"));
    s.d->uninstallerIniFile = readChild(root, QLatin1String("UninstallerIniFile"), s.d->uninstallerName
        + QLatin1String(".ini"));
    s.d->privateKey = splitTrimmed(readChild(root, QLatin1String("PrivateKey"))).toLatin1();
    s.d->publicKey = splitTrimmed(readChild(root, QLatin1String("PublicKey"))).toLatin1();
    s.d->configurationFileName = readChild(root, QLatin1String("TargetConfigurationFile"),
        QLatin1String("components.xml"));

    const QDomNodeList reposTags = root.elementsByTagName(QLatin1String("RemoteRepositories"));
    for (int a = 0; a < reposTags.count(); ++a) {
        const QDomNodeList repos = reposTags.at(a).toElement().elementsByTagName(QLatin1String("Repository"));
        for (int i = 0; i < repos.size(); ++i) {
            Repository r;
            const QDomNodeList children = repos.at(i).childNodes();
            for (int j = 0; j < children.size(); ++j) {
                if (children.at(j).toElement().tagName() == QLatin1String("Url"))
                    r.setUrl(children.at(j).toElement().text());
            }
            s.d->m_repositories.insert(r);
        }
    }

    const QDomNodeList certs = root.elementsByTagName(QLatin1String("SigningCertificate"));
    for (int i=0; i < certs.size(); ++i) {
        const QString str = certs.at(i).toElement().text();
        if (str.isEmpty())
            continue;
        s.d->certificateFiles.push_back(s.d->makeAbsolutePath(str));
    }
    return s;
}

QString InstallerSettings::maintenanceTitle() const
{
    return d->maintenanceTitle;
}

QString InstallerSettings::logo() const
{
    return d->makeAbsolutePath(d->logo);
}

QString InstallerSettings::logoSmall() const
{
    return d->makeAbsolutePath(d->logoSmall);
}

QString InstallerSettings::title() const
{
    return d->title;
}

QString InstallerSettings::applicationName() const
{
    return d->name;
}

QString InstallerSettings::applicationVersion() const
{
    return d->version;
}

QString InstallerSettings::publisher() const
{
    return d->publisher;
}

QString InstallerSettings::url() const
{
    return d->url;
}

QString InstallerSettings::watermark() const
{
    return d->makeAbsolutePath(d->watermark);
}

QString InstallerSettings::background() const
{
    return d->makeAbsolutePath(d->background);
}

QString InstallerSettings::icon() const
{
#if defined(Q_WS_MAC)
    return d->makeAbsolutePath(d->icon) + QLatin1String(".icns");
#elif defined(Q_WS_WIN)
    return d->makeAbsolutePath(d->icon) + QLatin1String(".ico");
#endif
    return d->makeAbsolutePath(d->icon) + QLatin1String(".png");
}

QString InstallerSettings::removeTargetDir() const
{
    return d->removeTargetDir;
}

QString InstallerSettings::uninstallerName() const
{
    if (d->uninstallerName.isEmpty())
        return QLatin1String("uninstall");
    return d->uninstallerName;
}

QString InstallerSettings::uninstallerIniFile() const
{
    return d->uninstallerIniFile;
}

QString InstallerSettings::runProgram() const
{
    return d->runProgram;
}

QString InstallerSettings::runProgramDescription() const
{
    return d->runProgramDescription;
}

QString InstallerSettings::startMenuDir() const
{
    return d->startMenuDir;
}

QString InstallerSettings::targetDir() const
{
    return d->targetDir;
}

QString InstallerSettings::adminTargetDir() const
{
    return d->adminTargetDir;
}

QString InstallerSettings::configurationFileName() const
{
    return d->configurationFileName;
}

QStringList InstallerSettings::certificateFiles() const
{
    return d->certificateFiles;
}

QByteArray InstallerSettings::privateKey() const
{
    return d->privateKey;
}

QByteArray InstallerSettings::publicKey() const
{
    return d->publicKey;
}

QList<Repository> InstallerSettings::repositories() const
{
    return (d->m_repositories + d->m_userRepositories).toList();
}

void InstallerSettings::setTemporaryRepositories(const QList<Repository> &repos, bool replace)
{
    if (replace)
        d->m_repositories = repos.toSet();
    else
        d->m_repositories.unite(repos.toSet());
}

QList<Repository> InstallerSettings::userRepositories() const
{
    return d->m_userRepositories.toList();
}

void InstallerSettings::addUserRepositories(const QList<Repository> &repositories)
{
    d->m_userRepositories.unite(repositories.toSet());
}
