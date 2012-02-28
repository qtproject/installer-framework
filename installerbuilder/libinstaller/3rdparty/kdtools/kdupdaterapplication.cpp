/****************************************************************************
** Copyright (C) 2001-2010 Klaralvdalens Datakonsult AB.  All rights reserved.
**
** This file is part of the KD Tools library.
**
** Licensees holding valid commercial KD Tools licenses may use this file in
** accordance with the KD Tools Commercial License Agreement provided with
** the Software.
**
**
** This file may be distributed and/or modified under the terms of the
** GNU Lesser General Public License version 2 and version 3 as published by the
** Free Software Foundation and appearing in the file LICENSE.LGPL included.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
** Contact info@kdab.com if any conditions of this licensing are not
** clear to you.
**
**********************************************************************/

#include "kdupdaterapplication.h"
#include "kdupdaterpackagesinfo.h"
#include "kdupdaterupdatesourcesinfo.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QSettings>

using namespace KDUpdater;

/*!
   \defgroup kdupdater KD Updater
   \since_l 2.1

   "KD Updater" is a library from KDAB that helps in enabling automatic updates for your applications.
   All classes belonging to the "KD Updater" library are defined in the \ref KDUpdater namespace.

   TODO: this comes from the former mainpage:
KD Updater is a tool to automatically detect, retrieve, install and activate updates to software
applications and libraries. It is intended to be used with Qt based applications, and developed
against the Qt 4 series. It is a library that users link to their application. It uses only accepted
standard protocols, and does not require any other 3rd party libraries that are not shipped with
Qt.

KD Updater is generic in that it is not developed for one specific application. The first version is
experimental. If it proves successful and useful, it will be integrated into KDAB's KD Tools
package. It is part of KDAB's strategy to provide functionality missing in Qt that is required for
medium-to-large scale software systems.
*/

/*!
   \namespace KDUpdater
*/

ConfigurationInterface::~ConfigurationInterface()
{
}

namespace {

class DefaultConfigImpl : public ConfigurationInterface
{
public:
    QVariant value(const QString &key) const
    {
        QSettings settings;
        settings.beginGroup(QLatin1String("KDUpdater"));
        return settings.value(key);
    }

    void setValue(const QString &key, const QVariant &value)
    {
        QSettings settings;
        settings.beginGroup(QLatin1String("KDUpdater"));
        settings.setValue(key, value);
    }
};

} // namespace anon

/*!
   \class KDUpdater::Application kdupdaterapplication.h KDUpdaterApplication
   \ingroup kdupdater
   \brief This class represents an application that can be updated.

   A KDUpdater application is an application that needs to interact with one or more update servers and
   downloads/installs updates This class helps in describing an application in terms of:
   \li application Directory
   \li packages XML file name and its corresponding KDUpdater::PackagesInfo object
   \li update Sources XML file name and its corresponding KDUpdater::UpdateSourcesInfo object

   User can also retrieve some informations from this class:
   \li application name
   \li application version
   \li compat level
*/

struct Application::ApplicationData
{
    explicit ApplicationData(ConfigurationInterface *config) :
        packagesInfo(0),
        updateSourcesInfo(0),
        configurationInterface(config ? config : new DefaultConfigImpl)
    {
        const QStringList oldFiles = configurationInterface->value(QLatin1String("FilesForDelayedDeletion")).toStringList();
        Q_FOREACH(const QString &i, oldFiles) { //TODO this should happen asnyc and report errors, I guess
            QFile f(i);
            if (f.exists() && !f.remove()) {
                qWarning("Could not delete file %s: %s", qPrintable(i), qPrintable(f.errorString()));
                filesForDelayedDeletion << i; // try again next time
            }
        }
        configurationInterface->setValue(QLatin1String("FilesForDelayedDeletion"), filesForDelayedDeletion);
    }

    ~ApplicationData()
    {
        delete packagesInfo;
        delete updateSourcesInfo;
        delete configurationInterface;
    }
   
    static Application *instance;

    QString applicationDirectory;
    PackagesInfo *packagesInfo;
    UpdateSourcesInfo *updateSourcesInfo;
    QStringList filesForDelayedDeletion;
    ConfigurationInterface *configurationInterface;
};

Application *Application::ApplicationData::instance = 0;

/*!
   Constructor of the Application class. The class will be constructed and configured to
   assume the application directory to be the directory in which the application exists. The
   application name is assumed to be QCoreApplication::applicationName()
*/
Application::Application(ConfigurationInterface* config, QObject* p) : QObject(p)
{
    d = new Application::ApplicationData( config );
    d->packagesInfo = new PackagesInfo(this);
    d->updateSourcesInfo = new UpdateSourcesInfo(this);

    setApplicationDirectory( QCoreApplication::applicationDirPath() );

    ApplicationData::instance = this;
}

/*!
   Destructor
*/
Application::~Application()
{
    if (this == ApplicationData::instance)
        ApplicationData::instance = 0;
    delete d;
}

/*!
 Returns a previousle created Application instance.
 */
Application *Application::instance()
{
    return ApplicationData::instance;
}

/*!
   Changes the applicationDirPath directory to \c dir. Packages.xml and UpdateSources.xml found in the new
   application directory will be used.
*/
void Application::setApplicationDirectory(const QString &dir)
{
    if (d->applicationDirectory == dir)
        return;

    QDir dirObj(dir);

    // FIXME: Perhaps we should check whether dir exists on the local file system or not
    d->applicationDirectory = dirObj.absolutePath();
    setPackagesXMLFileName(QString::fromLatin1("%1/Packages.xml").arg(dir));
    setUpdateSourcesXMLFileName(QString::fromLatin1("%1/UpdateSources.xml").arg(dir));
}

/*!
   Returns path to the application directory.
*/
QString Application::applicationDirectory() const
{
    return d->applicationDirectory;
}

/*!
   Returns the application name.
*/
QString Application::applicationName() const
{
    if (d->packagesInfo->isValid())
        return d->packagesInfo->applicationName();

    return QCoreApplication::applicationName();
}

/*!
   Returns the application version.
*/
QString Application::applicationVersion() const
{
    if (d->packagesInfo->isValid())
        return d->packagesInfo->applicationVersion();

    return QString();
}

/*!
   Returns the compat level that this application is in.
*/
int Application::compatLevel() const
{
    if (d->packagesInfo->isValid())
        return d->packagesInfo->compatLevel();

    return -1;
}

void Application::addUpdateSource(const QString &name, const QString &title,
                                  const QString &description, const QUrl &url, int priority)
{
    UpdateSourceInfo info;
    info.name = name;
    info.title = title;
    info.description = description;
    info.url = url;
    info.priority = priority;
    d->updateSourcesInfo->addUpdateSourceInfo(info);
}


/*!
   Sets the file name of the Package XML file for this application. By default this is assumed to be
   Packages.xml in the application directory.

   \sa KDUpdater::PackagesInfo::setFileName()
*/
void Application::setPackagesXMLFileName(const QString &fileName)
{
    d->packagesInfo->setFileName(fileName);
}

/*!
   Returns the Package XML file name.
*/
QString Application::packagesXMLFileName() const
{
    return d->packagesInfo->fileName();
}

/*!
   Returns the \ref PackagesInfo object associated with this application.
*/
PackagesInfo* Application::packagesInfo() const
{
    return d->packagesInfo;
}

/*!
   Sets the file name of the Package XML file for this application. By default this is assumed to be
   Packages.xml in the application directory.

   \sa KDUpdater::UpdateSourcesInfo::setFileName()
*/
void Application::setUpdateSourcesXMLFileName(const QString &fileName)
{
    d->updateSourcesInfo->setFileName(fileName);
}

/*!
   Returns the Update Sources XML file name.
*/
QString Application::updateSourcesXMLFileName() const
{
    return d->updateSourcesInfo->fileName();
}

/*!
   Returns the \ref UpdateSourcesInfo object associated with this application.
*/
UpdateSourcesInfo* Application::updateSourcesInfo() const
{
    return d->updateSourcesInfo;
}
    
void Application::printError(int errorCode, const QString &error)
{
    qDebug() << errorCode << error;
}

QStringList Application::filesForDelayedDeletion() const
{
    return d->filesForDelayedDeletion;
}

void Application::addFilesForDelayedDeletion(const QStringList &files)
{
    d->filesForDelayedDeletion << files;
    d->configurationInterface->setValue(QLatin1String("FilesForDelayedDeletion"), d->filesForDelayedDeletion);
}
