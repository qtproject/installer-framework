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

#include "kdupdaterupdatefinder.h"
#include "kdupdaterapplication.h"
#include "kdupdaterupdatesourcesinfo.h"
#include "kdupdaterpackagesinfo.h"
#include "kdupdaterupdate.h"
#include "kdupdaterfiledownloader_p.h"
#include "kdupdaterfiledownloaderfactory.h"
#include "kdupdaterupdatesinfo_p.h"

#include <QCoreApplication>
#include <QDebug>

using namespace KDUpdater;

/*!
   \ingroup kdupdater
   \class KDUpdater::UpdateFinder kdupdaterupdatefinder KDUpdaterUpdateFinder
   \brief Finds updates applicable for a \ref KDUpdater::Application

   The KDUpdater::UpdateFinder class helps in searching for updates and installing them on the application. The
   class basically processes the application's \ref KDUpdater::PackagesInfo and the UpdateXMLs it aggregates
   from all the update sources described in KDUpdater::UpdateSourcesInfo and populates a list of
   \ref KDUpdater::Update objects. This list can then be passed to \ref KDUpdater::UpdateInstaller for
   actually downloading and installing the updates.


   Usage:
   \code
   KDUpdater::UpdateFinder updateFinder( application );
   QProgressDialog finderProgressDlg;

   QObject::connect( &updateFinder, SIGNAL(progressValue(int)),
   &finderProgressDlg, SLOT(setValue(int)));
   QObject::connect( &updateFinder, SIGNAL(computeUpdatesCompleted()),
   &finderProgressDlg, SLOT(accept()));
   QObject::connect( &updateFinder, SIGNAL(computeUpdatesCanceled()),
   &finderProgressDlg, SLOT(reject()));

   QObject::connect( &finderProgressDlg, SIGNAL(canceled()),
   &updateFinder, SLOT(cancelComputeUpdates()));

   updateFinder.run();
   finderProgressDlg.exec();

   // Control comes here after update finding is done or canceled.

   QList<KDUpdater::Update*> updates = updateFinder.updates();
   KDUpdater::UpdateInstaller updateInstaller;
   updateInstaller.installUpdates( updates );

\endcode
*/


//
// Private
//
class UpdateFinder::Private
{
public:
    Private(UpdateFinder *qq) :
        q(qq),
        application(0),
        updateType(PackageUpdate)
    {}

    ~Private()
    {
        qDeleteAll(updates);
        qDeleteAll(updatesInfoList);
        qDeleteAll(updateXmlFDList);
    }
    
    UpdateFinder *q;
    Application *application;
    QList<Update *> updates;
    UpdateTypes updateType;

    // Temporary structure that notes down information about updates.
    bool cancel;
    int downloadCompleteCount;
    QList<UpdateSourceInfo> updateSourceInfoList;
    QList<UpdatesInfo *> updatesInfoList;
    QList<FileDownloader *> updateXmlFDList;

    void clear();
    void computeUpdates();
    void cancelComputeUpdates();
    bool downloadUpdateXMLFiles();
    bool computeApplicableUpdates();

    QList<UpdateInfo> applicableUpdates(UpdatesInfo *updatesInfo,
                                        bool addNewPackages = false);
    void createUpdateObjects(const UpdateSourceInfo &sourceInfo,
                             const QList<UpdateInfo> &updateInfoList);
    bool checkForUpdatePriority(const UpdateSourceInfo &sourceInfo,
                                const UpdateInfo &updateInfo);
    int pickUpdateFileInfo(const QList<UpdateFileInfo> &updateFiles);
    void slotDownloadDone();
};


static int computeProgressPercentage(int min, int max, int percent)
{
    return min + qint64(max-min) * percent / 100;
}

static int computePercent(int done, int total)
{
    return total ? done * Q_INT64_C(100) / total : 0 ;
}

/*!
   \internal

   Releases all internal resources consumed while downloading and computing updates.
*/
void UpdateFinder::Private::clear()
{
    qDeleteAll(updates);
    updates.clear();
    qDeleteAll(updatesInfoList);
    updatesInfoList.clear();
    qDeleteAll(updateXmlFDList);
    updateXmlFDList.clear();
    updateSourceInfoList.clear();
    downloadCompleteCount = 0;
}

/*!
   \internal

   This method computes the updates that can be applied on the application by
   studying the application's \ref KDUpdater::PackagesInfo object and the UpdateXML files
   from each of the update sources described in \ref KDUpdater::UpdateSourcesInfo.

   This function can take a long time to complete. The following signals are emitted
   during the execution of this function

   The function creates \ref KDUpdater::Update objects on the stack. All KDUpdater::Update objects
   are made children of the application associated with this finder.

   The update sources are fetched from the \ref KDUpdater::UpdateSourcesInfo object associated with
   the application. Package information is extracted from the \ref KDUpdater::PackagesInfo object
   associated with the application.

   \note Each time this function is called, all the previously computed updates are discarded
   and its resources are freed.
*/
void UpdateFinder::Private::computeUpdates()
{
    // Computing updates is done in two stages
    // 1. Downloading Update XML files from all the update sources
    // 2. Matching updates with Package XML and figuring out available updates

    cancel = false;
    clear();

    // First do some quick sanity checks on the packages info
    PackagesInfo *packages = application->packagesInfo();
    if (!packages) {
        q->reportError(tr("Could not access the package information of this application."));
        return;
    }
    if (!packages->isValid()) {
        q->reportError(packages->errorString());
        return;
    }

    // Now do some quick sanity checks on the update sources info
    UpdateSourcesInfo *sources = application->updateSourcesInfo();
    if (!sources) {
        q->reportError(tr("Could not access the update sources information of this application."));
        return;
    }
    if (!sources->isValid()) {
        q->reportError(sources->errorString());
        return;
    }

    // Now we can start...

    // Step 1: 0 - 49 percent
    if (!downloadUpdateXMLFiles() || cancel) {
        clear();
        return;
    }

    // Step 2: 50 - 100 percent
    if (!computeApplicableUpdates() || cancel) {
        clear();
        return;
    }

    // All done
    q->reportProgress(100, tr("%1 updates found.").arg(updates.count()));
    q->reportDone();
}

/*!
   \internal

   Cancels the computation of updates.

   \sa \ref computeUpdates()
*/
void UpdateFinder::Private::cancelComputeUpdates()
{
    cancel = true;
}

/*!
   \internal

   This function downloads Updates.xml from all the update sources. A single application can potentially
   have several update sources, hence we need to be asynchronous in downloading updates from different
   sources.

   The function basically does this for each update source:
   a) Create a KDUpdater::FileDownloader and KDUpdater::UpdatesInfo for each update
   b) Triggers the download of Updates.xml from each file downloader.
   c) The downloadCompleted(), downloadCanceled() and downloadAborted() signals are connected
   in each of the downloaders. Once all the downloads are complete and/or aborted, the next stage
   would be done.

   The function gets into an event loop until all the downloads are complete.
*/
bool UpdateFinder::Private::downloadUpdateXMLFiles()
{
    if (!application)
        return false;

    UpdateSourcesInfo *updateSources = application->updateSourcesInfo();
    if (!updateSources )
        return false;

    // Create FileDownloader and UpdatesInfo for each update
    for (int i = 0; i < updateSources->updateSourceInfoCount(); i++) {
        UpdateSourceInfo info = updateSources->updateSourceInfo(i);
        QUrl updateXmlUrl = QString::fromLatin1("%1/Updates.xml").arg(info.url.toString());

        FileDownloader *downloader = FileDownloaderFactory::instance().create(updateXmlUrl.scheme(), q);
        if (!downloader)
            continue;

        downloader->setUrl(updateXmlUrl);
        downloader->setAutoRemoveDownloadedFile(true);

        UpdatesInfo *updatesInfo = new UpdatesInfo;
        updateSourceInfoList.append(info);
        updateXmlFDList.append(downloader);
        updatesInfoList.append(updatesInfo);

        connect(downloader, SIGNAL(downloadCompleted()),
                q, SLOT(slotDownloadDone()));
        connect(downloader, SIGNAL(downloadCanceled()),
                q, SLOT(slotDownloadDone()));
        connect(downloader, SIGNAL(downloadAborted(QString)),
                q, SLOT(slotDownloadDone()));
    }

    // Trigger download of Updates.xml file
    downloadCompleteCount = 0;
    for (int i = 0; i < updateXmlFDList.count(); i++) {
        FileDownloader *downloader = updateXmlFDList.at(i);
        downloader->download();
    }

    // Wait until all downloaders have completed their downloads.
    while (true) {
        QCoreApplication::processEvents();
        if (cancel)
            return false;
        if (downloadCompleteCount == updateXmlFDList.count())
            break;

        int pc = computePercent(downloadCompleteCount, updateXmlFDList.count());
        q->reportProgress(pc, tr("Downloading Updates.xml from update sources."));
    }

    // All the downloaders have now either downloaded or aborted the
    // download of update XML files.

    // Let's now get rid of update sources whose Updates.xml could not be downloaded
    for (int i = 0; i < updateXmlFDList.count(); i++) {
        FileDownloader *downloader = updateXmlFDList.at(i);
        if (downloader->isDownloaded())
            continue;

        UpdateSourceInfo info = updateSourceInfoList.at(i);
        QString msg = tr("Could not download updates from %1 ('%2')").arg(info.name, info.url.toString());
        q->reportError(msg);

        delete updatesInfoList[i];
        delete downloader;
        updateXmlFDList.removeAt(i);
        updatesInfoList.removeAt(i);
        updateSourceInfoList.removeAt(i);
        --i;
    }

    if (updatesInfoList.isEmpty())
        return false;

    // Lets parse the downloaded update XML files and get rid of the downloaders.
    for (int i = 0; i < updateXmlFDList.count(); i++) {
        FileDownloader *downloader = updateXmlFDList.at(i);
        UpdatesInfo *updatesInfo = updatesInfoList.at(i);

        updatesInfo->setFileName(downloader->downloadedFileName());

        if (!updatesInfo->isValid()) {
            QString msg = updatesInfo->errorString();
            q->reportError(msg);

            delete updatesInfoList[i];
            delete downloader;
            updateXmlFDList.removeAt(i);
            updatesInfoList.removeAt(i);
            --i;
        }
    }
    qDeleteAll(updateXmlFDList);
    updateXmlFDList.clear();

    if (updatesInfoList.isEmpty())
        return false;

    q->reportProgress(49, tr("Updates.xml file(s) downloaded from update sources."));
    return true;
}

/*!
   \internal

   This function runs through all the KDUpdater::UpdatesInfo objects created during
   the downloadUpdateXMLFiles() method and compares it with the data contained in
   KDUpdater::PackagesInfo. Thereby figures out whether an update is applicable for
   this application or not.
*/
bool UpdateFinder::Private::computeApplicableUpdates()
{
    if (updateType & CompatUpdate) {
        UpdateInfo compatUpdateInfo;
        UpdateSourceInfo compatUpdateSourceInfo;

        // Required compat level
        int reqCompatLevel = application->compatLevel() + 1;

        q->reportProgress(60, tr("Looking for compatibility update..."));

        // We are only interested in compat updates.
        for (int i = 0; i < updatesInfoList.count(); i++) {
            UpdatesInfo *info = updatesInfoList.at(i);
            UpdateSourceInfo updateSource = updateSourceInfoList.at(i);

            // If we already have a compat update, just check if the source currently being
            // considered has a higher priority or not.
            if (compatUpdateInfo.data.contains(QLatin1String("CompatLevel")) && updateSource.priority < compatUpdateSourceInfo.priority)
                continue;

            // Let's look for compat updates that provide compat level one-higher than
            // the application's current compat level.
            QList<UpdateInfo> updatesInfo = info->updatesInfo(CompatUpdate, reqCompatLevel);

            if (updatesInfo.count() == 0)
                continue;

            compatUpdateInfo = updatesInfo.at(0);
            compatUpdateSourceInfo = updateSource;
        }

        bool found = compatUpdateInfo.data.contains(QLatin1String("CompatLevel"));
        if (found) {
            q->reportProgress(80, tr("Found compatibility update."));

            // Create an update for this compat update.
            // Pick a update file based on arch and OS.
            int pickUpdateFileIndex = pickUpdateFileInfo(compatUpdateInfo.updateFiles);
            if (pickUpdateFileIndex < 0) {
                q->reportError(tr("Compatibility update for the required architecture and hardware configuration was "
                                  "not found."));
                q->reportProgress(100, tr("Compatibility update not found."));
                return false;
            }

            UpdateFileInfo fileInfo = compatUpdateInfo.updateFiles.at(pickUpdateFileIndex);

            // Create an update for this entry
            QUrl url = QString::fromLatin1("%1/%2").arg( compatUpdateSourceInfo.url.toString(), fileInfo.fileName);
            Update *update = q->constructUpdate(application, compatUpdateSourceInfo, CompatUpdate,
                                                url, compatUpdateInfo.data, fileInfo.compressedSize,
                                                fileInfo.uncompressedSize, fileInfo.sha1sum);

            // Register the update
            updates.append(update);

            // Done
            q->reportProgress(100, tr("Compatibility update found."));
        } else {
            q->reportProgress(100, tr("No compatibility updates found."));
        }
    }
    if (updateType & PackageUpdate) {
        // We are looking for normal updates, not compat ones.
        for (int i = 0; i < updatesInfoList.count(); i++) {
            // Fetch updates applicable to this application.
            UpdatesInfo *info = updatesInfoList.at(i);
            QList<UpdateInfo> updates = applicableUpdates(info , updateType & NewPackage);
            if (!updates.count())
                continue;

            if (cancel)
                return false;
            UpdateSourceInfo updateSource = updateSourceInfoList.at(i);

            // Create Update objects for updates that have a valid
            // UpdateFile
            createUpdateObjects(updateSource, updates);
            if (cancel)
                return false;

            // Report progress
            int pc = computePercent(i, updatesInfoList.count());
            pc = computeProgressPercentage(51, 100, pc);
            q->reportProgress(pc, tr("Computing applicable updates."));
        }
    }

    q->reportProgress(99, tr("Application updates computed."));
    return true;
}

QList<UpdateInfo> UpdateFinder::Private::applicableUpdates(UpdatesInfo *updatesInfo, bool addNewPackages)
{
    QList<UpdateInfo> retList;

    if (!updatesInfo || updatesInfo->updateInfoCount( PackageUpdate ) == 0)
        return retList;

    PackagesInfo *packages = this->application->packagesInfo();
    if (!packages)
        return retList;

    // Check to see if the updates info contains updates for any application
    bool anyApp = updatesInfo->applicationName() == QLatin1String("{AnyApplication}");
    int appNameIndex = -1;

    if (!anyApp) {
        // updatesInfo->applicationName() describes one application or a series of
        // application names separated by commas.
        QString appName = updatesInfo->applicationName();
        appName = appName.replace(QLatin1String( ", " ), QLatin1String( "," ));
        appName = appName.replace(QLatin1String( " ," ), QLatin1String( "," ));

        // Catch hold of app names contained updatesInfo->applicationName()
        QStringList apps = appName.split(QRegExp(QLatin1String("\\b(,|, )\\b")), QString::SkipEmptyParts);
        appNameIndex = apps.indexOf(this->application->applicationName());

        // If the application appName isn't one of the app names, then
        // the updates are not applicable.
        if (appNameIndex < 0)
            return retList;
    }

    // Check to see if version numbers match. This means that the version
    // number of the update should be greater than the version number of
    // the package that is currently installed.
    QList<UpdateInfo> updateList = updatesInfo->updatesInfo(PackageUpdate);
    for (int i = 0; i < updatesInfo->updateInfoCount(PackageUpdate); i++) {
        UpdateInfo updateInfo = updateList.at(i);
        if (!addNewPackages) {
            int pkgInfoIdx = packages->findPackageInfo( updateInfo.data.value(QLatin1String("Name")).toString());
            if (pkgInfoIdx < 0)
                continue;

            PackageInfo pkgInfo = packages->packageInfo(pkgInfoIdx);

            // First check to see if the update version is higher than package version
            QString updateVersion = updateInfo.data.value(QLatin1String("Version")).toString();
            QString pkgVersion = pkgInfo.version;
            if (KDUpdater::compareVersion(updateVersion, pkgVersion) <= 0)
                continue;

            // It is quite possible that we may have already installed the update.
            // Lets check the last update date of the package and the release date
            // of the update. This way we can compare and figure out if the update
            // has been installed or not.
            QDate pkgDate = pkgInfo.lastUpdateDate;
            QDate updateDate = updateInfo.data.value(QLatin1String("ReleaseDate")).toDate();
            if (pkgDate > updateDate)
                continue;
        }

        // Bingo, we found an update :-)
        retList.append(updateInfo);
    }

    return retList;
}

void UpdateFinder::Private::createUpdateObjects(const UpdateSourceInfo &sourceInfo, const QList<UpdateInfo> &updateInfoList)
{
    for (int i = 0; i < updateInfoList.count(); i++) {
        UpdateInfo info = updateInfoList.at(i);
        // Compat level checks
        if (info.data.contains(QLatin1String("RequiredCompatLevel")) &&
            info.data.value(QLatin1String("RequiredCompatLevel")).toInt() != application->compatLevel())
        {
            qDebug().nospace() << "Update \"" << info.data.value( QLatin1String( "Name" ) ).toString()
                               << "\" at \"" << sourceInfo.name << "\"(\"" << sourceInfo.url.toString()
                               << "\") requires a different compat level";
            continue; // Compatibility level mismatch
        }

        // If another update of the same name exists, then use the update coming from
        // a higher priority.
        if (!checkForUpdatePriority(sourceInfo, info)) {
            qDebug().nospace() << "Skipping Update \""
                               << info.data.value(QLatin1String("Name")).toString()
                               << "\" from \""
                               << sourceInfo.name
                               << "\"(\""
                               << sourceInfo.url.toString()
                               << "\") because an update with the same name was found from a higher priority location";

            continue;
        }

        // Pick a update file based on arch and OS.
        int pickUpdateFileIndex = this->pickUpdateFileInfo(info.updateFiles);
        if (pickUpdateFileIndex < 0)
            continue;

        UpdateFileInfo fileInfo = info.updateFiles.at(pickUpdateFileIndex);

        // Create an update for this entry
        QUrl url(QString::fromLatin1("%1/%2").arg( sourceInfo.url.toString(), fileInfo.fileName));
        Update *update = q->constructUpdate(application, sourceInfo, PackageUpdate, url, info.data, fileInfo.compressedSize, fileInfo.uncompressedSize, fileInfo.sha1sum);

        // Register the update
        this->updates.append(update);
    }
}

bool UpdateFinder::Private::checkForUpdatePriority(const UpdateSourceInfo &sourceInfo, const UpdateInfo &updateInfo)
{
    for (int i = 0; i < this->updates.count(); i++){
        Update *update = this->updates.at(i);
        if (update->data(QLatin1String("Name")).toString() != updateInfo.data.value(QLatin1String("Name")).toString())
            continue;

        // Bingo, update was previously found elsewhere.

        // If the existing update comes from a higher priority server, then cool :)
        if (update->sourceInfo().priority > sourceInfo.priority)
            return false;

        // If the existing update has a higher version number, keep it
        if (KDUpdater::compareVersion(update->data(QLatin1String("Version")).toString(),
                                      updateInfo.data.value(QLatin1String("Version")).toString()) > 0)
            return false;

        // Otherwise the old update must be deleted.
        this->updates.removeAll(update);
        delete update;

        return true;
    }

    // No update by that name was found, so what we have is a priority update.
    return true;
}

int UpdateFinder::Private::pickUpdateFileInfo(const QList<UpdateFileInfo> &updateFiles)
{
#ifdef Q_WS_MAC
    QString os = QLatin1String( "MacOSX" );
#endif
#ifdef Q_WS_WIN
    QString os = QLatin1String( "Windows" );
#endif
#ifdef Q_WS_X11
    QString os = QLatin1String( "Linux" );
#endif

    QString arch = QLatin1String( "i386" ); // only one architecture considered for now.

    for (int i = 0; i < updateFiles.count(); i++) {
        UpdateFileInfo fileInfo = updateFiles.at(i);

        if (fileInfo.arch != arch)
            continue;

        if (fileInfo.os != QLatin1String("Any") && fileInfo.os != os)
            continue;

        return i;
    }

    return -1;
}



//
// UpdateFinder
//

/*!
   Constructs a update finder for a given \ref KDUpdater::Application.
*/
UpdateFinder::UpdateFinder(Application *application)
    : Task(QLatin1String("UpdateFinder"), Stoppable, application),
      d(new Private(this))
{
    d->application = application;
}

/*!
   Destructor
*/
UpdateFinder::~UpdateFinder()
{
    delete d;
}

/*!
   Returns a pointer to the update application for which this function computes all
   the updates.
*/
Application *UpdateFinder::application() const
{
    return d->application;
}

/*!
   Returns a list of KDUpdater::Update objects. The update objects returned in this list
   are made children of the \ref KDUpdater::Application object associated with this class.
*/
QList<Update *> UpdateFinder::updates() const
{
    return d->updates;
}

/*!
   Looks only for a certain type of update. By default, only package update
*/
void UpdateFinder::setUpdateType(UpdateTypes type)
{
    d->updateType = type;
}

/*!
   Returns the type of updates searched
*/
UpdateTypes UpdateFinder::updateType() const
{
    return d->updateType;
}

/*!
   \internal

   Implemented from \ref KDUpdater::Task::doStart().
*/
void UpdateFinder::doRun()
{
    d->computeUpdates();
}

/*!
   \internal

   Implemented form \ref KDUpdater::Task::doStop()
*/
bool UpdateFinder::doStop()
{
    d->cancelComputeUpdates();

    // Wait until the cancel has actually happened, and then return.
    // Thinking of using QMutex for this. Frank/Till any suggestions?

    return true;
}

/*!
   \internal

   Implemented form \ref KDUpdater::Task::doStop()
*/
bool UpdateFinder::doPause()
{
    // Not a pausable task
    return false;
}

/*!
   \internal

   Implemented form \ref KDUpdater::Task::doStop()
*/
bool UpdateFinder::doResume()
{
    // Not a pausable task, hence it is not resumable as well
    return false;
}

/*!
   \internal
*/
void UpdateFinder::Private::slotDownloadDone()
{
    ++downloadCompleteCount;

    int pc = computePercent(downloadCompleteCount, updateXmlFDList.count());
    pc = computeProgressPercentage(0, 45, pc);
    q->reportProgress( pc, tr("Downloading Updates.xml from update sources.") );
}

/*!
   \internal
 */
Update *UpdateFinder::constructUpdate(Application *application, const UpdateSourceInfo &sourceInfo,
    UpdateType type, const QUrl &updateUrl, const QMap<QString, QVariant> &data,
    quint64 compressedSize, quint64 uncompressedSize, const QByteArray &sha1sum )
{
    return new Update(application, sourceInfo, type, updateUrl, data, compressedSize, uncompressedSize, sha1sum);
}


/*!
   \ingroup kdupdater

   This function compares two version strings \c v1 and \c v2 and returns
   -1, 0 or +1 based on the following rule

   \li Returns 0 if v1 == v2
   \li Returns -1 if v1 < v2
   \li Returns +1 if v1 > v2

   The function is very similar to \c strcmp(), except that it works on version strings.

   Example:
   \code

   KDUpdater::compareVersion("2.0", "2.1"); // Returns -1
   KDUpdater::compareVersion("2.1", "2.0"); // Returns +1
   KDUpdater::compareVersion("2.0", "2.0"); // Returns 0
   KDUpdater::compareVersion("2.1", "2.1"); // Returns 0

   KDUpdater::compareVersion("2.0", "2.x"); // Returns 0
   KDUpdater::compareVersion("2.x", "2.0"); // Returns 0

   KDUpdater::compareVersion("2.0.12.4", "2.1.10.4"); // Returns -1
   KDUpdater::compareVersion("2.0.12.x", "2.0.x");    // Returns 0
   KDUpdater::compareVersion("2.1.12.x", "2.0.x");    // Returns +1
   KDUpdater::compareVersion("2.1.12.x", "2.x");      // Returns 0
   KDUpdater::compareVersion("2.x", "2.1.12.x");      // Returns 0

   \endcode
*/
int KDUpdater::compareVersion(const QString &v1, const QString &v2)
{
    // For tests refer VersionCompareFnTest testcase.

    // Check for equality
    if (v1 == v2)
        return 0;

    // Split version numbers across "."
    const QStringList v1_comps = v1.split(QRegExp(QLatin1String( "\\.|-")));
    const QStringList v2_comps = v2.split(QRegExp(QLatin1String( "\\.|-")));

    // Check each component of the version
    int index = 0;
    while (true) {
        if (index == v1_comps.count() && index < v2_comps.count())
            return -1;
        if (index < v1_comps.count() && index == v2_comps.count())
            return +1;
        if (index >= v1_comps.count() || index >= v2_comps.count())
            break;

        bool v1_ok, v2_ok;
        int v1_comp = v1_comps[index].toInt(&v1_ok);
        int v2_comp = v2_comps[index].toInt(&v2_ok);

        if (!v1_ok) {
            if (v1_comps[index] == QLatin1String("x"))
                return 0;
        }
        if (!v2_ok) {
            if (v2_comps[index] == QLatin1String("x"))
                return 0;
        }
        if (!v1_ok && !v2_ok)
            return v1_comps[index].compare(v2_comps[index]);

        if (v1_comp < v2_comp)
            return -1;

        if (v1_comp > v2_comp)
            return +1;

        // v1_comp == v2_comp
        ++index;
    }

    if (index < v2_comps.count())
        return +1;

    if (index < v1_comps.count())
        return -1;

    // Controversial return. I hope this never happens.
    return 0;
}

#include "moc_kdupdaterupdatefinder.cpp"
