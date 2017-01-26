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

#include "kdupdaterupdatefinder.h"
#include "kdupdaterapplication.h"
#include "kdupdaterupdatesourcesinfo.h"
#include "kdupdaterpackagesinfo.h"
#include "kdupdaterupdate.h"
#include "kdupdaterfiledownloader.h"
#include "kdupdaterfiledownloaderfactory.h"
#include "kdupdaterupdatesinfo_p.h"

#include "fileutils.h"
#include "globals.h"

#include <QCoreApplication>
#include <QFileInfo>

using namespace KDUpdater;

/*!
    \inmodule kdupdater
    \class KDUpdater::UpdateFinder
    \brief The UpdaterFinder class finds updates applicable for a KDUpdater::Application.

    The KDUpdater::UpdateFinder class helps in searching for updates and installing them on the
    application. The class basically processes the application's KDUpdater::PackagesInfo and the
    UpdateXMLs it aggregates from all the update sources described in KDUpdater::UpdateSourcesInfo
    and populates a list of KDUpdater::Update objects.
*/

//
// Private
//
class UpdateFinder::Private
{
public:
    enum struct Resolution {
        AddPackage,
        KeepExisting,
        RemoveExisting
    };

    Private(UpdateFinder *qq)
        : q(qq)
        , application(0)
        , downloadCompleteCount(0)
        , m_downloadsToComplete(0)
    {}

    ~Private()
    {
        clear();
    }

    struct Data {
        Data()
            : downloader(0) {}
        Data(const UpdateSourceInfo &i, FileDownloader *d = 0)
            : info(i), downloader(d) {}

        UpdateSourceInfo info;
        FileDownloader *downloader;
    };
    UpdateFinder *q;
    Application *application;
    QHash<QString, Update *> updates;

    // Temporary structure that notes down information about updates.
    bool cancel;
    int downloadCompleteCount;
    int m_downloadsToComplete;
    QHash<UpdatesInfo *, Data> m_updatesInfoList;

    void clear();
    void computeUpdates();
    void cancelComputeUpdates();
    bool downloadUpdateXMLFiles();
    bool computeApplicableUpdates();

    QList<UpdateInfo> applicableUpdates(UpdatesInfo *updatesInfo);
    void createUpdateObjects(const UpdateSourceInfo &sourceInfo, const QList<UpdateInfo> &updateInfoList);
    Resolution checkPriorityAndVersion(const UpdateSourceInfo &sourceInfo, const QVariantHash &data) const;
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

    const QList<Data> values = m_updatesInfoList.values();
    foreach (const Data &data, values)
        delete data.downloader;

    qDeleteAll(m_updatesInfoList.keys());
    m_updatesInfoList.clear();

    downloadCompleteCount = 0;
    m_downloadsToComplete = 0;
}

/*!
   \internal

   This method computes the updates that can be applied on the application by
   studying the application's KDUpdater::PackagesInfo object and the UpdateXML files
   from each of the update sources described in KDUpdater::UpdateSourcesInfo.

   This function can take a long time to complete. The following signals are emitted
   during the execution of this function

   The function creates KDUpdater::Update objects on the stack. All KDUpdater::Update objects
   are made children of the application associated with this finder.

   The update sources are fetched from the KDUpdater::UpdateSourcesInfo object associated with
   the application. Package information is extracted from the KDUpdater::PackagesInfo object
   associated with the application.

   \note Each time this function is called, all the previously computed updates are discarded
   and its resources are freed.
*/
void UpdateFinder::Private::computeUpdates()
{
    // Computing updates is done in two stages
    // 1. Downloading Update XML files from all the update sources
    // 2. Matching updates with Package XML and figuring out available updates

    clear();
    cancel = false;

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
    q->reportProgress(100, tr("%n update(s) found.", "", updates.count()));
    q->reportDone();
}

/*!
   \internal

   Cancels the computation of updates.

   \sa computeUpdates()
*/
void UpdateFinder::Private::cancelComputeUpdates()
{
    cancel = true;
}

/*!
   \internal

   This function downloads Updates.xml from all the update sources except local files.
   A single application can potentially have several update sources, hence we need to be
   asynchronous in downloading updates from different sources.

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

    // create UpdatesInfo for each update source
    for (int i = 0; i < updateSources->updateSourceInfoCount(); i++) {
        const UpdateSourceInfo info = updateSources->updateSourceInfo(i);
        const QUrl url = QString::fromLatin1("%1/Updates.xml").arg(info.url.toString());

        if (url.scheme() != QLatin1String("resource") && url.scheme() != QLatin1String("file")) {
            // create FileDownloader (except for local files and resources)
            FileDownloader *downloader = FileDownloaderFactory::instance().create(url.scheme(), q);
            if (!downloader)
                break;

            downloader->setUrl(url);
            downloader->setAutoRemoveDownloadedFile(true);
            connect(downloader, SIGNAL(downloadCanceled()), q, SLOT(slotDownloadDone()));
            connect(downloader, SIGNAL(downloadCompleted()), q, SLOT(slotDownloadDone()));
            connect(downloader, SIGNAL(downloadAborted(QString)), q, SLOT(slotDownloadDone()));
            m_updatesInfoList.insert(new UpdatesInfo, Data(info, downloader));
        } else {
            UpdatesInfo *updatesInfo = new UpdatesInfo;
            updatesInfo->setFileName(QInstaller::pathFromUrl(url));
            m_updatesInfoList.insert(updatesInfo, Data(info));
        }
    }

    // Trigger download of Updates.xml file
    downloadCompleteCount = 0;
    m_downloadsToComplete = 0;
    foreach (const Data &data, m_updatesInfoList) {
        if (data.downloader) {
            m_downloadsToComplete++;
            data.downloader->download();
        }
    }

    // Wait until all downloaders have completed their downloads.
    while (true) {
        QCoreApplication::processEvents();
        if (cancel)
            return false;

        if (downloadCompleteCount == m_downloadsToComplete)
            break;

        q->reportProgress(computePercent(downloadCompleteCount, m_downloadsToComplete),
            tr("Downloading Updates.xml from update sources."));
    }

    // Setup the update info objects with the files from download.
    foreach (UpdatesInfo *updatesInfo, m_updatesInfoList.keys()) {
        const Data data = m_updatesInfoList.value(updatesInfo);
        if (data.downloader) {
            if (!data.downloader->isDownloaded()) {
                q->reportError(tr("Could not download update source %1 from ('%2')").arg(data.info
                    .name, data.info.url.toString()));
            } else {
                updatesInfo->setFileName(data.downloader->downloadedFileName());
            }
        }
    }

    // Remove all invalid update info objects.
    QMutableHashIterator<UpdatesInfo *, Data> it(m_updatesInfoList);
    while (it.hasNext()) {
        UpdatesInfo *info = it.next().key();
        if (info->isValid())
            continue;

        q->reportError(info->errorString());
        delete info;
        it.remove();
    }

    if (m_updatesInfoList.isEmpty())
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
    int i = 0;
    foreach (UpdatesInfo *updatesInfo, m_updatesInfoList.keys()) {
        // Fetch updates applicable to this application.
        QList<UpdateInfo> updates = applicableUpdates(updatesInfo);
        if (!updates.count())
            continue;

        if (cancel)
            return false;
        const UpdateSourceInfo updateSource = m_updatesInfoList.value(updatesInfo).info;

        // Create Update objects for updates that have a valid
        // UpdateFile
        createUpdateObjects(updateSource, updates);
        if (cancel)
            return false;

        // Report progress
        q->reportProgress(computeProgressPercentage(51, 100, computePercent(i,
            m_updatesInfoList.count())), tr("Computing applicable updates."));
        ++i;
    }

    q->reportProgress(99, tr("Application updates computed."));
    return true;
}

QList<UpdateInfo> UpdateFinder::Private::applicableUpdates(UpdatesInfo *updatesInfo)
{
    const QList<UpdateInfo> dummy;
    if (!updatesInfo || updatesInfo->updateInfoCount() == 0)
        return dummy;

    PackagesInfo *packages = this->application->packagesInfo();
    if (!packages)
        return dummy;

    // Check to see if the updates info contains updates for any application
    if (updatesInfo->applicationName() != QLatin1String("{AnyApplication}")) {
        // updatesInfo->applicationName() describes one application or a series of
        // application names separated by commas.
        QString appName = updatesInfo->applicationName();
        appName = appName.replace(QLatin1String( ", " ), QLatin1String( "," ));
        appName = appName.replace(QLatin1String( " ," ), QLatin1String( "," ));

        // Catch hold of app names contained updatesInfo->applicationName()
        // If the application appName isn't one of the app names, then the updates are not applicable.
        const QStringList apps = appName.split(QInstaller::commaRegExp(), QString::SkipEmptyParts);
        if (apps.indexOf(this->application->applicationName()) < 0)
            return dummy;
    }
    return updatesInfo->updatesInfo();
}

void UpdateFinder::Private::createUpdateObjects(const UpdateSourceInfo &sourceInfo,
    const QList<UpdateInfo> &updateInfoList)
{
    foreach (const UpdateInfo &info, updateInfoList) {
        const Resolution value = checkPriorityAndVersion(sourceInfo, info.data);
        if (value == Resolution::KeepExisting)
            continue;

        const QString name = info.data.value(QLatin1String("Name")).toString();
        if (value == Resolution::RemoveExisting)
            delete updates.take(name);

        // Create and register the update
        updates.insert(name, new Update(sourceInfo.priority, sourceInfo.url, info.data));
    }
}

/*
    If a package of the same name exists, always use the one with the higher
    version. If the new package has the same version but a higher
    priority, use the new new package, otherwise keep the already existing package.
*/
UpdateFinder::Private::Resolution UpdateFinder::Private::checkPriorityAndVersion(
    const UpdateSourceInfo &sourceInfo, const QVariantHash &newPackage) const
{
    const QString name = newPackage.value(QLatin1String("Name")).toString();
    if (Update *existingPackage = updates.value(name)) {
        // Bingo, package was previously found elsewhere.

        const int match = compareVersion(newPackage.value(QLatin1String("Version")).toString(),
            existingPackage->data(QLatin1String("Version")).toString());

        if (match > 0) {
            // new package has higher version, use
            qDebug() << QString::fromLatin1("Remove Package 'Name: %1, Version: %2, Source: %3' "
                "found a package with higher version 'Name: %4, Version: %5, Source: %6'")
                .arg(name, existingPackage->data(QLatin1String("Version")).toString(),
                    QFileInfo(existingPackage->sourceInfoUrl().toLocalFile()).fileName(),
                    name, newPackage.value(QLatin1String("Version")).toString(),
                    QFileInfo(sourceInfo.url.toLocalFile()).fileName());
            return Resolution::RemoveExisting;
        }

        if ((match == 0) && (sourceInfo.priority > existingPackage->priority())) {
            // new package version equals but priority is higher, use
            qDebug() << QString::fromLatin1("Remove Package 'Name: %1, Priority: %2, Source: %3' "
                "found a package with higher priority 'Name: %4, Priority: %5, Source: %6'")
                .arg(name, QString::number(existingPackage->priority()),
                    QFileInfo(existingPackage->sourceInfoUrl().toLocalFile()).fileName(),
                    name, QString::number(sourceInfo.priority),
                    QFileInfo(sourceInfo.url.toLocalFile()).fileName());
            return Resolution::RemoveExisting;
        }
        return Resolution::KeepExisting; // otherwise keep existing
    }
    return Resolution::AddPackage;
}

//
// UpdateFinder
//

/*!
   Constructs an update finder for the KDUpdater::Application specified by
   \a application.
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
   Returns a list of KDUpdater::Update objects. The update objects returned in this list
   are made children of the KDUpdater::Application object associated with this class.
*/
QList<Update *> UpdateFinder::updates() const
{
    return d->updates.values();
}

/*!
   \internal

   Implemented from KDUpdater::Task::doRun().
*/
void UpdateFinder::doRun()
{
    d->computeUpdates();
}

/*!
   \internal

   Implemented from KDUpdater::Task::doStop().
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

   Implemented from KDUpdater::Task::doStop().
*/
bool UpdateFinder::doPause()
{
    // Not a pausable task
    return false;
}

/*!
   \internal

   Implemented from KDUpdater::Task::doStop().
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

    int pc = computePercent(downloadCompleteCount, m_downloadsToComplete);
    pc = computeProgressPercentage(0, 45, pc);
    q->reportProgress( pc, tr("Downloading Updates.xml from update sources.") );
}


/*!
   \inmodule kdupdater

   This function compares two version strings \c v1 and \c v2 and returns
   -1, 0 or +1 based on the following rule

    \list
        \li Returns 0 if v1 == v2
        \li Returns -1 if v1 < v2
        \li Returns +1 if v1 > v2
    \endlist

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
