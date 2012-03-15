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
#include "updatesettings.h"

#include "errors.h"
#include "repository.h"
#include "settings.h"

#include <QtCore/QDateTime>
#include <QtCore/QSettings>
#include <QtCore/QStringList>

using namespace QInstaller;

class UpdateSettings::Private
{
public:
    Private(UpdateSettings* qq)
        : q(qq) { }

private:
    UpdateSettings *const q;

public:
    QSettings &settings()
    {
        return externalSettings ? *externalSettings : internalSettings;
    }

    static void setExternalSettings(QSettings *settings)
    {
        externalSettings = settings;
    }

private:
    QSettings internalSettings;
    static QSettings *externalSettings;
};

QSettings *UpdateSettings::Private::externalSettings = 0;


// -- UpdateSettings

UpdateSettings::UpdateSettings()
    : d(new Private(this))
{
    d->settings().sync();
}

UpdateSettings::~UpdateSettings()
{
    d->settings().sync();
    delete d;
}

/* static */
void UpdateSettings::setSettingsSource(QSettings *settings)
{
    Private::setExternalSettings(settings);
}

int UpdateSettings::updateInterval() const
{
    return d->settings().value(QLatin1String("updatesettings/interval"), static_cast<int>(Weekly)).toInt();
}

void UpdateSettings::setUpdateInterval(int seconds)
{
    d->settings().setValue(QLatin1String("updatesettings/interval"), seconds);
}

QString UpdateSettings::lastResult() const
{
    return d->settings().value(QLatin1String("updatesettings/lastresult")).toString();
}

void UpdateSettings::setLastResult(const QString &lastResult)
{
    d->settings().setValue(QLatin1String("updatesettings/lastresult"), lastResult);
}

QDateTime UpdateSettings::lastCheck() const
{
    return d->settings().value(QLatin1String("updatesettings/lastcheck")).toDateTime();
}

void UpdateSettings::setLastCheck(const QDateTime &lastCheck)
{
    d->settings().setValue(QLatin1String("updatesettings/lastcheck"), lastCheck);
}

bool UpdateSettings::checkOnlyImportantUpdates() const
{
    return d->settings().value(QLatin1String("updatesettings/onlyimportant"), false).toBool();
}

void UpdateSettings::setCheckOnlyImportantUpdates(bool checkOnlyImportantUpdates)
{
    d->settings().setValue(QLatin1String("updatesettings/onlyimportant"), checkOnlyImportantUpdates);
}

QSet<Repository> UpdateSettings::repositories() const
{
    QSettings &settings = d->settings();
    const int count = settings.beginReadArray(QLatin1String("updatesettings/repositories"));

    QSet<Repository> result;
    for (int i = 0; i < count; ++i) {
        settings.setArrayIndex(i);
        result.insert(Repository(d->settings().value(QLatin1String("url")).toUrl(), false));
    }
    settings.endArray();

    try {
        if(result.isEmpty()) {
            result = Settings::fromFileAndPrefix(QLatin1String(":/metadata/installer-config/config.xml"),
                QLatin1String(":/metadata/installer-config/")).userRepositories();
        }
    } catch (const Error &error) {
        qDebug("Could not parse config: %s", qPrintable(error.message()));
    }
    return result;
}

void UpdateSettings::setRepositories(const QSet<Repository> &repositories)
{
    QSet<Repository>::ConstIterator it = repositories.constBegin();
    d->settings().beginWriteArray(QLatin1String("updatesettings/repositories"));
    for (int i = 0; i < repositories.count(); ++i, ++it) {
        d->settings().setArrayIndex(i);
        d->settings().setValue(QLatin1String("url"), (*it).url());
    }
    d->settings().endArray();
}
