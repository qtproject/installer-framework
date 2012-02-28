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

#include "kdupdaterupdate.h"
#include "kdupdaterapplication.h"
#include "kdupdaterupdatesourcesinfo.h"
#include "kdupdaterfiledownloader_p.h"
#include "kdupdaterfiledownloaderfactory.h"
#include "kdupdaterupdateoperations.h"
#include "kdupdaterupdateoperationfactory.h"

#include <QFile>

using namespace KDUpdater;

/*!
   \ingroup kdupdater
   \class KDUpdater::Update kdupdaterupdate.h KDUpdaterUpdate
   \brief Represents a single update

   The KDUpdater::Update class contains information and mechanisms to download one update. It is
   created by KDUpdater::UpdateFinder and is used by KDUpdater::UpdateInstaller to download the UpdateFile
   corresponding to the update.

   The class makes use of appropriate network protocols (HTTP, HTTPS, FTP, or Local File Copy) to
   download the UpdateFile.

   The constructor of the KDUpdater::Update class is made protected, because it can be instantiated only by
   KDUpdater::UpdateFinder (which is a friend class). The destructor however is public.
*/

struct Update::UpdateData
{
    UpdateData(Update *qq) :
        q(qq),
        application(0),
        compressedSize(0),
        uncompressedSize(0)
    {}

    Update *q;
    Application *application;
    UpdateSourceInfo sourceInfo;
    QMap<QString, QVariant> data;
    QUrl updateUrl;
    UpdateType type;
    QList<UpdateOperation *> operations;
    QByteArray sha1sum;

    quint64 compressedSize;
    quint64 uncompressedSize;

    FileDownloader *fileDownloader;
};


/*!
   \internal
*/
Update::Update(Application *application, const UpdateSourceInfo &sourceInfo,
               UpdateType type, const QUrl &updateUrl, const QMap<QString, QVariant> &data,
               quint64 compressedSize, quint64 uncompressedSize, const QByteArray &sha1sum)
    : Task(QLatin1String("Update"), Stoppable, application),
      d(new UpdateData(this))
{
    d->application = application;
    d->sourceInfo = sourceInfo;
    d->data = data;
    d->updateUrl = updateUrl;
    d->type = type;

    d->compressedSize = compressedSize;
    d->uncompressedSize = uncompressedSize;
    d->sha1sum = sha1sum;

    d->fileDownloader = FileDownloaderFactory::instance().create(updateUrl.scheme(), this);
    if (d->fileDownloader) {
        d->fileDownloader->setUrl(d->updateUrl);
        d->fileDownloader->setSha1Sum(d->sha1sum);
        connect(d->fileDownloader, SIGNAL(downloadProgress(double)), this, SLOT(downloadProgress(double)));
        connect(d->fileDownloader, SIGNAL(downloadCanceled()), this, SIGNAL(stopped()));
        connect(d->fileDownloader, SIGNAL(downloadCompleted()), this, SIGNAL(finished()));
    }

    switch (type) {
    case NewPackage:
    case PackageUpdate: {
        UpdateOperation *packageOperation = UpdateOperationFactory::instance().create(QLatin1String("UpdatePackage"));
        QStringList args;
        args << data.value(QLatin1String("Name")).toString()
             << data.value(QLatin1String("Version")).toString()
             << data.value(QLatin1String("ReleaseDate")).toString();
        packageOperation->setArguments(args);
        packageOperation->setApplication(application);
        d->operations.append(packageOperation);
        break;
    }
    case CompatUpdate: {
        UpdateOperation *compatOperation = UpdateOperationFactory::instance().create(QLatin1String("UpdateCompatLevel"));
        QStringList args;
        args << data.value(QLatin1String("CompatLevel")).toString();
        compatOperation->setArguments(args);
        compatOperation->setApplication(application);
        d->operations.append(compatOperation);
        break;
    }
    default:
        break;
    }
}

/*!
   Destructor
*/
Update::~Update()
{
    const QString fileName = this->downloadedFileName();
    if (!fileName.isEmpty())
        QFile::remove(fileName);
    qDeleteAll(d->operations);
    d->operations.clear();
    delete d;
}

/*!
   Returns the application for which this class is downloading the UpdateFile
*/
Application *Update::application() const
{
    return d->application;
}

/*!
   Returns the release date of the update downloaded by this class
*/
QDate Update::releaseDate() const
{
    return d->data.value(QLatin1String("ReleaseDate")).toDate();
}

/*!
   Returns data whose name is given in parameter, or an invalid QVariant if the data doesn't exist.
*/
QVariant Update::data(const QString &name, const QVariant &defaultValue) const
{
    if (d->data.contains(name))
        return d->data.value(name);
    return defaultValue;
}

/*!
   Returns the complete URL of the UpdateFile downloaded by this class.
*/
QUrl Update::updateUrl() const
{
    return d->updateUrl;
}

/*!
   Returns the update source info on which this update was created.
*/
UpdateSourceInfo Update::sourceInfo() const
{
    return d->sourceInfo;
}

/*!
 * Returns the type of update
 */
UpdateType Update::type() const
{
    return d->type;
}

/*!
   Returns true of the update can be downloaded, false otherwise. The function
   returns false if the URL scheme is not supported by this class.
*/
bool Update::canDownload() const
{
    return d->fileDownloader && d->fileDownloader->canDownload();
}

/*!
   Returns true of the update has been downloaded. If this function returns true
   the you can use the \ref downloadedFileName() method to get the complete name
   of the downloaded UpdateFile.

   \note: The downloaded UpdateFile will be deleted when this class is destroyed
*/
bool Update::isDownloaded() const
{
    return d->fileDownloader && d->fileDownloader->isDownloaded();
}

/*!
   Returns the name of the downloaded UpdateFile after the download is complete, ie
   when \ref isDownloaded() returns true.
*/
QString Update::downloadedFileName() const
{
    if (d->fileDownloader)
        return d->fileDownloader->downloadedFileName();

    return QString();
}

/*!
   \internal
*/
void Update::downloadProgress(double value)
{
    Q_ASSERT(value <= 1);
    reportProgress(value * 100, tr("Downloading update..."));
}

/*!
   \internal
*/
void Update::downloadCompleted()
{
    reportProgress(100, tr("Update downloaded"));
    reportDone();
}

/*!
   \internal
*/
void Update::downloadAborted(const QString &msg)
{
    reportError(msg);
}

/*!
   \internal
*/
void Update::doRun()
{
    if (d->fileDownloader)
        d->fileDownloader->download();
}

/*!
   \internal
*/
bool Update::doStop()
{
    if (d->fileDownloader)
        d->fileDownloader->cancelDownload();
    return true;
}

/*!
   \internal
*/
bool Update::doPause()
{
    return false;
}

/*!
   \internal
*/
bool Update::doResume()
{
    return false;
}

/*!
   Returns a list of operations needed by this update. For example, package update needs to change
   the package version, compat update needs to change the compat level...
 */
QList<UpdateOperation *> Update::operations() const
{
    return d->operations;
}

/*!
 * Returns the compressed size of this update's data file.
 */
quint64 Update::compressedSize() const
{
    return d->compressedSize;
}

/*!
 * Returns the uncompressed size of this update's data file.
 */
quint64 Update::uncompressedSize() const
{
    return d->uncompressedSize;
}
