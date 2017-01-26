/****************************************************************************
**
** Copyright (C) 2013 Klaralvdalens Datakonsult AB (KDAB)
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
****************************************************************************/

#include "kdupdaterapplication.h"
#include "kdupdaterpackagesinfo.h"
#include "kdupdaterupdatesourcesinfo.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>

using namespace KDUpdater;

/*!
    \inmodule kdupdater
    \namespace KDUpdater
    \brief The KDUpdater classes provide functions to automatically detect
    updates to applications, to retrieve them from external repositories, and to
    install them.

    KDUpdater classes are a fork of KDAB's general
    \l{http://docs.kdab.com/kdtools/2.2.2/group__kdupdater.html}{KDUpdater module}.
*/

/*!
    \class KDUpdater::ConfigurationInterface
    \inmodule kdupdater
    \brief The ConfigurationInterface class provides an interface for configuring
    an application.
*/

/*!
    \fn KDUpdater::ConfigurationInterface::~ConfigurationInterface()
    Destroys the configuration interface.
*/

/*!
    \fn KDUpdater::ConfigurationInterface::value(const QString &key) const
    Returns the value of the key \a key.
*/

/*!
    \fn KDUpdater::ConfigurationInterface::setValue(const QString &key, const QVariant &value)
    Sets the value \a value for the key \a key.
*/

/*!
    \class KDUpdater::Application
    \inmodule kdupdater
    \brief The Application class represents an application that can be updated.

    A KDUpdater application is an application that interacts with one or more update servers and
    downloads or installs updates. This class helps in describing an application in terms of:
    \list
        \li Application Directory
        \li Installation information XML file name and its corresponding
            KDUpdater::PackagesInfo object
        \li Update sources XML file name and its corresponding KDUpdater::UpdateSourcesInfo object
    \endlist

    User can also retrieve some information from this class:
    \list
        \li Application name
        \li Application version
    \endlist
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
    Constructs an application with the parent \a p and configuration class \a config.
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
    Destroys the application.
*/
Application::~Application()
{
    if (this == ApplicationData::instance)
        ApplicationData::instance = 0;
    delete d;
}

/*!
    Returns a previously created application instance.
*/
Application *Application::instance()
{
    return ApplicationData::instance;
}

/*!
    Sets the application directory path directory to \a dir. The installation information and
    update sources XML files found in the new application directory will be used.
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
    Returns the path to the application directory.
*/
QString Application::applicationDirectory() const
{
    return d->applicationDirectory;
}

/*!
    Returns the application name. By default, QCoreApplication::applicationName() is returned.
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
    Adds the \a name, \a title, \a description, \a url, and \a priority of the
    update source to this class.

    \sa KDUpdater::UpdateSourceInfo
    \sa KDUpdater::UpdateSourcesInfo
*/
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
    Sets the file name of the installation information XML file for this application to \a fileName.
    By default, this is assumed to be Packages.xml in the application directory.

   \sa KDUpdater::PackagesInfo::setFileName()
*/
void Application::setPackagesXMLFileName(const QString &fileName)
{
    d->packagesInfo->setFileName(fileName);
}

/*!
    Returns the installation information XML file name.
*/
QString Application::packagesXMLFileName() const
{
    return d->packagesInfo->fileName();
}

/*!
    Returns the KDUpdater::PackagesInfo object associated with this application.
*/
PackagesInfo* Application::packagesInfo() const
{
    return d->packagesInfo;
}

/*!
    Sets \a fileName as the file name of the update sources XML file for this
    application. By default, this is assumed to be UpdateSources.xml in the
    application directory.

   \sa KDUpdater::UpdateSourcesInfo::setFileName()
*/
void Application::setUpdateSourcesXMLFileName(const QString &fileName)
{
    d->updateSourcesInfo->setFileName(fileName);
}

/*!
    Returns the update sources XML file name.
*/
QString Application::updateSourcesXMLFileName() const
{
    return d->updateSourcesInfo->fileName();
}

/*!
    Returns the KDUpdater::UpdateSourcesInfo object associated with this application.
*/
UpdateSourcesInfo* Application::updateSourcesInfo() const
{
    return d->updateSourcesInfo;
}

/*!
    Prints the error code \a errorCode and error message specified by \a error.
*/
void Application::printError(int errorCode, const QString &error)
{
    qDebug() << errorCode << error;
}

/*!
    Returns a list of files that are scheduled for delayed deletion.
*/
QStringList Application::filesForDelayedDeletion() const
{
    return d->filesForDelayedDeletion;
}

/*!
    Schedules \a files for delayed deletion.
*/
void Application::addFilesForDelayedDeletion(const QStringList &files)
{
    d->filesForDelayedDeletion << files;
    d->configurationInterface->setValue(QLatin1String("FilesForDelayedDeletion"), d->filesForDelayedDeletion);
}
