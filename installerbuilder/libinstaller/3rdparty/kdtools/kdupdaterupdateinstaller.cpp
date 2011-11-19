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

#include "kdupdaterupdateinstaller.h"
#include "kdupdaterpackagesinfo.h"
#include "kdupdaterapplication.h"
#include "kdupdaterupdate.h"
#include "kdupdaterupdateoperationfactory.h"
#include "kdupdaterupdateoperation.h"
#include "kdupdaterufuncompressor_p.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QDir>
#include <QDomDocument>
#include <QDomElement>
#include <QDate>

using namespace KDUpdater;

/*!
   \ingroup kdupdater
   \class KDUpdater::UpdateInstaller kdupdaterupdateinstaller.h KDUpdaterUpdateInstaller
   \brief Installs updates, given a list of \ref KDUpdater::Update objects

   This class installs updates, given a list of \ref KDUpdater::Update objects via the
   \ref installUpdates() method. To install the updates this class performs the following
   for each update

   \li Downloads the update files from its source
   \li Unpacks update files into a temporary directory
   \li Parses and executes UpdateInstructions.xml by making use of \ref KDUpdater::UpdateOperation
   objects sourced via \ref KDUpdater::UpdateOperationFactory

   \note All temporary files created during the installation of the update will be destroyed
   immediately after the installation is complete.
*/
class UpdateInstaller::Private
{
  public:
    Private(UpdateInstaller *qq) :
        q(qq)
    {}

    UpdateInstaller *q;

    Application *application;
    int updateDownloadDoneCount;
    int updateDownloadProgress;
    int totalUpdates;
    QStringList toRemoveDirs;

    int totalProgressPc;
    int currentProgressPc;
    QList<Update *> updates;

    void resolveArguments(QStringList &args);

    void removeDirectory(const QDir &dir);
    void slotUpdateDownloadProgress(int percent);
    void slotUpdateDownloadDone();
};

// next two are duplicated from kdupdaterupdatefinder.cpp:

static int computeProgressPercentage(int min, int max, int percent)
{
    return min + qint64(max-min) * percent / 100 ;
}

static int computePercent(int done, int total)
{
    return total ? done * Q_INT64_C(100) / total : 0 ;
}

/*!
   Constructs an instance of this class for the \ref KDUpdater::Application passed as
   parameter. Only updates meant for the specified application will be installed by this class.
*/
UpdateInstaller::UpdateInstaller(Application *application)
    : Task(QLatin1String("UpdateInstaller"), NoCapability, application),
      d(new Private(this))
{
    d->application = application;
}

/*!
   Destructor
*/
UpdateInstaller::~UpdateInstaller()
{
    delete d;
}

/*!
   Returns the update application for which the update is being installed
*/
Application *UpdateInstaller::application() const
{
    return d->application;
}

/*!
   Use this function to let the installer know what updates are to be installed. The
   updates are actually installed when the \ref start() method is called on this class.
*/
void UpdateInstaller::setUpdatesToInstall(const QList<Update *> &updates)
{
    d->updates = updates;
}

/*!
   Returns the updates that would be installed when the next time \ref the start) method is called.
*/
QList<Update *> UpdateInstaller::updatesToInstall() const
{
    return d->updates;
}

/*!
   \internal
*/
void UpdateInstaller::doRun()
{
    QList<Update *> &updates = d->updates;

    // First download all the updates
    d->updateDownloadDoneCount = 0;
    d->totalUpdates = updates.count();

    for (int i = 0; i < updates.count(); i++) {
        Update *update = updates[i];
        if (update->application() != d->application)
            continue;

        update->setProperty("_ProgressPc_", 0);
        connect(update, SIGNAL(progressValue(int)), this, SLOT(slotUpdateDownloadProgress(int)));
        connect(update, SIGNAL(finished()), this, SLOT(slotUpdateDownloadDone()));
        connect(update, SIGNAL(stopped()), this, SLOT(slotUpdateDownloadDone()));
        update->download();
    }

    d->totalProgressPc = updates.count() * 100;
    d->currentProgressPc = 0;

    // Wait until all updates have been downloaded
    while (d->updateDownloadDoneCount != updates.count()) {
        QCoreApplication::processEvents();

        // Normalized progress
        int progressPc = computePercent(d->currentProgressPc, d->totalProgressPc);

        // Bring the progress to within 50 percent
        progressPc = (progressPc>>1);

        // Report the progress
        reportProgress(progressPc, tr("Downloading updates..."));
    }

    // Global progress
    reportProgress(50, tr("Updates downloaded..."));

    // Save the current working directory of the application
    QDir oldCWD = QDir::current();

    int pcDiff = computePercent(1, updates.count());
    pcDiff = computeProgressPercentage(50, 95, pcDiff) - 50;

    // Now install one update after another.
    for (int i = 0; i < updates.count(); i++) {
        Update *update = updates[i];

        // Global progress
        QString msg = tr("Installing %1..").arg(update->name());
        int minPc = pcDiff*i + 50;
        int maxPc = minPc + pcDiff;
        reportProgress(minPc, msg);

        if (update->application() != d->application)
            continue;

        QDir::setCurrent(oldCWD.absolutePath());
        if (!installUpdate(update, minPc, maxPc)) {
            d->application->packagesInfo()->writeToDisk();
            return;
        }
    }

    d->application->packagesInfo()->writeToDisk();

    // Global progress
    reportProgress(95, tr("Finished installing updates. Now removing temporary files and directories.."));

    // Restore the current working directory of the application
    QDir::setCurrent(oldCWD.absolutePath());

    // Remove all the toRemoveDirs
    for (int i = 0; i < d->toRemoveDirs.count(); i++) {
        QDir dir(d->toRemoveDirs[i]);
        d->removeDirectory(dir);

        QString dirName = dir.dirName();
        dir.cdUp();
        dir.rmdir(dirName);
    }
    d->toRemoveDirs.clear();

    // Global progress
    reportProgress(100, tr("Removed temporary files and directories"));
    reportDone();
}

/*!
   \internal
*/
bool UpdateInstaller::doStop()
{
    return false;
}

/*!
   \internal
*/
bool UpdateInstaller::doPause()
{
    return false;
}

/*!
   \internal
*/
bool UpdateInstaller::doResume()
{
    return false;
}

bool UpdateInstaller::installUpdate(Update *update, int minPc, int maxPc)
{
    QString updateName = update->name();

    // Sanity checks
    if (!update->isDownloaded()) {
        QString msg = tr("Could not download update '%1'").arg(update->name());
        reportError(msg);
        return false;
    }

    // Step 1: Prepare a directory into which the UpdateFile will be unpacked.
    // If update file is C:/Users/PRASHA~1/AppData/Local/Temp/qt_temp.Hp1204 and
    // the applicationn name "MyApplication"
    // Then the directory would be %USERDIR%/AppData/Local/Temp/MyApplication_Update1
    static int count = 0;
    QString dirName = QString::fromLatin1("%1_Update%2").arg(d->application->applicationName(), QString::number(count++));
    QString updateFile = update->downloadedFileName();
    QFileInfo fi(updateFile);
    QDir dir(fi.absolutePath());
    dir.mkdir(dirName);
    dir.cd(dirName);
    d->toRemoveDirs << dir.absolutePath();

    // Step 2: Unpack the update file into the update directory
    UFUncompressor uncompressor;
    uncompressor.setFileName(updateFile);
    uncompressor.setDestination(dir.absolutePath());

    if (!uncompressor.uncompress()) {
        reportError(tr("Couldn't uncompress update: %1")
                    .arg(uncompressor.errorString()));
        return false;
    }

    // Step 3: Find out the directory in which UpdateInstructions.xml can be found
    QDir updateDir = dir;
    while (!updateDir.exists(QLatin1String("UpdateInstructions.xml"))) {
        QString path = updateDir.absolutePath();
        QFileInfoList fiList = updateDir.entryInfoList(QDir::Dirs|QDir::NoDotAndDotDot);
        if (!fiList.count()) { // || fiList.count() >= 2 )
            QString msg = tr("Could not find UpdateInstructions.xml for %1").arg(update->name());
            reportError(msg);
            return false;
        }
        updateDir.cd(fiList.first().fileName());
    }

    // Set the application's current working directory as updateDir
    QDir::setCurrent(updateDir.absolutePath());

    // Step 4: Now load the UpdateInstructions.xml file
    QDomDocument doc;
    QFile file(updateDir.absoluteFilePath(QLatin1String( "UpdateInstructions.xml" )));
    if (!file.open(QFile::ReadOnly)) {
        QString msg = tr("Could not read UpdateInstructions.xml of %1").arg(update->name());
        reportError(msg);
        return false;
    }
    if (!doc.setContent(&file)) {
        QString msg = tr("Could not read UpdateInstructions.xml of %1").arg(update->name());
        reportError(msg);
        return false;
    }

    // Now parse and execute update operations
    QDomNodeList operEList = doc.elementsByTagName(QLatin1String( "UpdateOperation" ));
    QString msg = tr("Installing %1").arg(updateName);

    for (int i = 0; i < operEList.count(); i++) {
        int pc = computePercent(i+1, operEList.count());
        pc = computeProgressPercentage(minPc, maxPc, pc);
        reportProgress(pc, msg);

        // Fetch the important XML elements in UpdateOperation
        QDomElement operE = operEList.at(i).toElement();
        QDomElement nameE = operE.firstChildElement(QLatin1String("Name"));
        QDomElement errorE = operE.firstChildElement(QLatin1String("OnError"));
        QDomElement argE = operE.firstChildElement(QLatin1String("Arg"));

        // Figure out information about the update operation to perform
        QString operName = nameE.text();
        QString onError = errorE.attribute(QLatin1String("Action"), QLatin1String("Abort"));
        QStringList args;
        while(!argE.isNull()) {
            args << argE.text();
            argE = argE.nextSiblingElement(QLatin1String("Arg"));
        }

        //QString operSignature = QString::fromLatin1("%1(%2)").arg(operName, args.join( QLatin1String( ", ") ) );

        // Fetch update operation
        UpdateOperation *const updateOperation = UpdateOperationFactory::instance().create(operName);
        if (!updateOperation) {
            QString errMsg = tr("Update operation %1 not supported").arg(operName);
            reportError(errMsg);

            if (onError == QLatin1String("Continue"))
                continue;

            if (onError == QLatin1String("Abort"))
                return false;

            if (onError == QLatin1String("AskUser")) {
                // TODO:
                continue;
            }
        }

        // Now resolve special fields in arguments
        d->resolveArguments(args);

        // Now set the arguments to the update operation and execute the update operation
        updateOperation->setArguments(args);
        updateOperation->setApplication(d->application);
        const bool success = updateOperation->performOperation();
        
        updateOperation->clear();

        if (!success) {
            QString errMsg = tr("Cannot execute '%1'").arg(updateOperation->operationCommand());
            reportError(errMsg);

            if (onError == QLatin1String("Continue"))
                continue;

            if (onError == QLatin1String("Abort"))
                return false;

            if (onError == QLatin1String("AskUser")) {
                // TODO:
                continue;
            }
        }
        delete updateOperation;
    }

    Q_FOREACH (UpdateOperation *updateOperation, update->operations())
        updateOperation->performOperation();

    msg = tr("Finished installing update %1").arg(update->name());
    reportProgress(maxPc, msg);
    return true;
}

/*!
   \internal
*/
void UpdateInstaller::Private::slotUpdateDownloadProgress(int percent)
{
    // 0-49 percent progress is dedicated for the download of updates
    Update *update = qobject_cast<Update *>(q->sender());
    if (!update)
        return;

    int oldPc = update->property("_ProgressPc_").toInt();
    int diffPc = percent-oldPc;
    if (diffPc <= 0)
        return;

    currentProgressPc += diffPc;
    update->setProperty("_ProgressPc_", percent);
}

/*!
   \internal
*/
void UpdateInstaller::Private::slotUpdateDownloadDone()
{
    ++updateDownloadDoneCount;
}

void UpdateInstaller::Private::resolveArguments(QStringList& args)
{
    for (int i = 0; i < args.count(); i++) {
        QString arg = args[i];

        arg = arg.replace(QLatin1String("{APPDIR}"), application->applicationDirectory());
        arg = arg.replace(QLatin1String("{HOME}"), QDir::homePath());
        arg = arg.replace(QLatin1String("{APPNAME}"), application->applicationName());
        arg = arg.replace(QLatin1String("{APPVERSION}"), application->applicationVersion());
        arg = arg.replace(QLatin1String("{CURPATH}"), QDir::currentPath());
        arg = arg.replace(QLatin1String("{ROOT}"), QDir::rootPath());
        arg = arg.replace(QLatin1String("{TEMP}"), QDir::tempPath());

        args[i] = arg;
    }
}

void UpdateInstaller::Private::removeDirectory(const QDir &dir)
{
    QFileInfoList fiList = dir.entryInfoList(QDir::Dirs|QDir::Files|QDir::NoDotAndDotDot);
    if (!fiList.count())
        return;

    for (int i = 0; i < fiList.count(); i++) {
        QFileInfo fi = fiList[i];
        if (fi.isDir()) {
            QDir childDir = fi.absoluteFilePath();
            removeDirectory(childDir);
            dir.rmdir(childDir.dirName());
        } else if(fi.isFile()) {
            QFile::remove(fi.absoluteFilePath());
        }
    }
}

#include "moc_kdupdaterupdateinstaller.cpp"
