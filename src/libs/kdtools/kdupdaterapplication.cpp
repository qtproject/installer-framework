/****************************************************************************
**
** Copyright (C) 2013 Klaralvdalens Datakonsult AB (KDAB)
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "kdupdaterapplication.h"
#include "kdupdaterpackagesinfo.h"
#include "kdupdaterupdatesourcesinfo.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>

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

/*!
   \class KDUpdater::Application kdupdaterapplication.h KDUpdaterApplication
   \ingroup kdupdater
   \brief This class represents an application that can be updated.

   A KDUpdater application is an application that needs to interact with one or more update servers and
   downloads/installs updates This class helps in describing an application in terms of:
   \li application Directory
   \li packages XML file name and its corresponding KDUpdater::PackagesInfo object
   \li update Sources XML file name and its corresponding KDUpdater::UpdateSourcesInfo object

   User can also retrieve some information from this class:
   \li application name
   \li application version
*/

struct Application::ApplicationData
{
    explicit ApplicationData(ConfigurationInterface *config) :
        packagesInfo(0),
        updateSourcesInfo(0),
        configurationInterface(config ? config : new ConfigurationInterface)
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
 Returns a previously created Application instance.
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
