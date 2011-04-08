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
#include "downloadarchivesjob.h"

#include <QFile>
#include <QTimer>
#include <QMessageBox>
#include <QDebug>
#include <QTimerEvent>

#include <KDUpdater/FileDownloader>
#include <KDUpdater/FileDownloaderFactory>

#include "common/binaryformatenginehandler.h"
#include "common/utils.h"

#include "cryptosignatureverifier.h"
#include "qinstaller.h"
#include "qinstallercomponent.h"
#include "messageboxhandler.h"

using namespace QInstaller;
using namespace KDUpdater;

/*
TRANSLATOR QInstaller::DownloadArchivesJob
*/

/**
 * Creates a new DownloadArchivesJob with \a parent.
 */
DownloadArchivesJob::DownloadArchivesJob(const QByteArray& publicKey, Installer* parent )
    : KDJob( parent ),
      cancelled( false ),
      publicKey( publicKey ),
      downloader( 0 ),
      filesToDownload( 0 ),
      filesDownloaded( 0 ),
      parent( parent ),
      lastFileProgress(0),
      progressChangedTimerId(0)
{
    setCapabilities( Cancelable );
}

/**
 * Destroys the DownloadArchivesJob.
 * All temporary files get deleted.
 */
DownloadArchivesJob::~DownloadArchivesJob()
{
    for( QStringList::const_iterator it = temporaryFiles.constBegin(); it != temporaryFiles.constEnd(); ++it ) {
        QFile f( *it );
        if ( !f.remove() )
            qWarning( "Could not delete file %s: %s", qPrintable(*it), qPrintable(f.errorString()) );
    }
    if( downloader != 0 )
        downloader->deleteLater();
}

/**
 * Sets the archives to download. The first value of each pair contains the file name to register
 * the file in the installer's internal file system, the second one the source url.
 */
void DownloadArchivesJob::setArchivesToDownload( const QList< QPair< QString, QString > >& archs )
{
    filesToDownload = archs.count();
    archives = archs;
}

/**
 * \reimp
 */
void DownloadArchivesJob::doStart()
{
    filesDownloaded = 0;
    fetchNextArchiveHash();
}

/**
 * \reimp
 */
void DownloadArchivesJob::doCancel()
{
    cancelled = true;
    if( downloader != 0 )
        downloader->cancelDownload();
}

void DownloadArchivesJob::fetchNextArchiveHash()
{
    if ( parent->testChecksum() )
    {
        if( cancelled )
        {
            finishWithError( tr( "Canceled" ) );
            return;
        }

        if( archives.isEmpty() )
        {
            emitFinished();
            return;
        }

        if( downloader != 0 )
            downloader->deleteLater();

        QString hashUrl = archives.first().second;
        hashUrl += QLatin1String( ".sha1" );
        const QUrl url( hashUrl );
        const QUrl sigUrl = publicKey.isEmpty() ? QUrl() : QUrl( archives.first().second + QLatin1String( ".sig" ) );
        const CryptoSignatureVerifier verifier( publicKey );
        downloader = FileDownloaderFactory::instance().create( url.scheme(), publicKey.isEmpty() ? 0 : &verifier, sigUrl, this );

        if ( !downloader ) {
            archives.pop_front();
            const QString errMsg = tr("Scheme not supported: %1 (%2)").arg( url.scheme(), url.toString() );
            verbose() << errMsg << std::endl;
            emit outputTextChanged( errMsg );
            QMetaObject::invokeMethod( this, "fetchNextArchiveHash", Qt::QueuedConnection );
            return;
        }

        connect( downloader, SIGNAL( downloadCompleted() ), this, SLOT( finishedHashDownload() ), Qt::QueuedConnection );
        connect( downloader, SIGNAL( downloadCanceled() ), this, SLOT( downloadCanceled() ) );
        connect( downloader, SIGNAL( downloadAborted( QString ) ), this, SLOT( downloadFailed( QString ) ), Qt::QueuedConnection );
        //hashes are not registered as files - so we can't handle this as a normal progress
        //connect( downloader, SIGNAL( downloadProgress( double ) ), this, SLOT( emitDownloadProgress( double ) ) );
        downloader->setUrl( url );
        downloader->setAutoRemoveDownloadedFile( false );

        const QString comp = QFileInfo( QFileInfo( archives.first().first ).path() ).fileName();
        const Component* const component = parent->componentByName( comp );

        emit outputTextChanged( tr( "Downloading archive hash for component %1" ).arg( component->displayName() ) );
        downloader->download();
    }
    else
        QMetaObject::invokeMethod( this, "fetchNextArchive", Qt::QueuedConnection );
}

void DownloadArchivesJob::finishedHashDownload()
{
    Q_ASSERT( downloader != 0 );

    const QString tempFile = downloader->downloadedFileName();
    QFile sha1HashFile( tempFile );
    if ( sha1HashFile.open( QFile::ReadOnly ) )
        currentHash = sha1HashFile.readAll();
    else
        finishWithError( tr( "Downloading hash signature failed" ) );

    temporaryFiles.push_back( tempFile );

    fetchNextArchive();
}

/**
 * Fetches the next archive and registers it in the installer.
 */
void DownloadArchivesJob::fetchNextArchive()
{
    if( cancelled )
    {
        finishWithError( tr( "Canceled" ) );
        return;
    }
    
    if( archives.isEmpty() )
    {
        emitFinished();
        return;
    }

    if( downloader != 0 )
        downloader->deleteLater();

    const QUrl url( archives.first().second );
    const QUrl sigUrl = publicKey.isEmpty() ? QUrl() : QUrl( archives.first().second + QLatin1String( ".sig" ) );
    const CryptoSignatureVerifier verifier( publicKey );
    downloader = FileDownloaderFactory::instance().create( url.scheme(), publicKey.isEmpty() ? 0 : &verifier, sigUrl, this );    

    if ( !downloader ) {
        archives.pop_front();
        const QString errMsg = tr("Scheme not supported: %1 (%2)").arg( url.scheme(), url.toString() );
        verbose() << errMsg;
        emit outputTextChanged( errMsg );
        QMetaObject::invokeMethod( this, "fetchNextArchive", Qt::QueuedConnection );
        return;
    }

    connect( downloader, SIGNAL( downloadCompleted() ), this, SLOT( registerFile() ), Qt::QueuedConnection );
    connect( downloader, SIGNAL( downloadCanceled() ), this, SLOT( downloadCanceled() ) );
    connect( downloader, SIGNAL( downloadAborted( QString ) ), this, SLOT( downloadFailed( QString ) ), Qt::QueuedConnection );
    connect( downloader, SIGNAL( downloadProgress( double ) ), this, SLOT( emitDownloadProgress( double ) ) );
    downloader->setUrl( url );
    downloader->setAutoRemoveDownloadedFile( false );
    
    const QString archive = QFileInfo( archives.first().first ).fileName();
    const QString comp = QFileInfo( QFileInfo( archives.first().first ).path() ).fileName();
    const Component* const component = parent->componentByName( comp );

    emit outputTextChanged( tr( "Downloading archive for component %1" ).arg( component->displayName() ) );
    emit progressChanged( double(filesDownloaded) / filesToDownload );
    downloader->download();
}


/**
 * Emits the global download progress during a single download in a lazy way(uses a timer to reduce to much processChanged).
 */
void DownloadArchivesJob::emitDownloadProgress( double progress )
{
    lastFileProgress = progress;
    if (!progressChangedTimerId) {
        progressChangedTimerId = startTimer(5);
    }
}

/**
 * this is used to reduce the progressChanged signals
 */
void DownloadArchivesJob::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == progressChangedTimerId) {
        killTimer(progressChangedTimerId);
        progressChangedTimerId = 0;
        emit progressChanged( ( double(filesDownloaded) + lastFileProgress ) / filesToDownload );
    }
}


/**
 * Registers the just downloaded file in the intaller's file system.
 */
void DownloadArchivesJob::registerFile()
{
    Q_ASSERT( downloader != 0 );
    ++filesDownloaded;
    if (progressChangedTimerId) {
        killTimer(progressChangedTimerId);
        progressChangedTimerId = 0;
        emit progressChanged( double(filesDownloaded) / filesToDownload );
    }

    const QString tempFile = downloader->downloadedFileName();
    QFile archiveFile( tempFile );
    if ( archiveFile.open( QFile::ReadOnly ) )
    {
        if ( parent->testChecksum() )
        {
            const QByteArray archiveHash = QCryptographicHash::hash( archiveFile.readAll(), QCryptographicHash::Sha1 ).toHex();
            if ( archiveHash != currentHash )
            {
                //TODO: Maybe we should try to download the file again automatically
                const QMessageBox::Button res = MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(),
                                                                            QLatin1String( "DownloadError" ),
                                                                            tr( "Download Error" ), tr( "Hash verification while downloading failed. This is a temporary error, please retry." ),
                                                                            QMessageBox::Retry | QMessageBox::Cancel, QMessageBox::Cancel );
                if ( res == QMessageBox::Cancel )
                {
                    finishWithError( tr( "Could not verify Hash") );
                    return;
                }
                else
                {
                    fetchNextArchiveHash();
                    return;
                }
            }
        }
    }
    else
    {
        finishWithError( tr( "Could not open %1" ).arg( tempFile ) );
    }
    const QString path = archives.first().first;
    archives.pop_front();
    temporaryFiles.push_back( tempFile );

    QInstallerCreator::BinaryFormatEngineHandler::instance()->registerArchive( path, tempFile );

    fetchNextArchiveHash();
}

void DownloadArchivesJob::downloadCanceled()
{
    emitFinishedWithError( Canceled, downloader->errorString() );
}

void DownloadArchivesJob::downloadFailed( const QString& error )
{
    const QMessageBox::StandardButton b = MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(),
                                                                      QLatin1String("archiveDownloadError"),
                                                                      tr("Download Error"),
                                                                      tr("Could not download archive %1: %2").arg( archives.first().second, error ),
                                                                      QMessageBox::Retry | QMessageBox::Cancel );
    if ( b == QMessageBox::Retry )
        QMetaObject::invokeMethod( this, "fetchNextArchive", Qt::QueuedConnection );
    else
        downloadCanceled();
}

void DownloadArchivesJob::finishWithError( const QString& error )
{
    const FileDownloader* const dl = dynamic_cast< const FileDownloader* >( sender() );
    if( dl != 0 )
        emitFinishedWithError( DownloadError, tr( "Could not fetch archives: %1\nError while loading %2" ).arg( error, dl->url().toString() ) );
    else
        emitFinishedWithError( DownloadError, tr( "Could not fetch archives: %1\nError while loading %2" ).arg( error, downloader->url().toString() ) );
}
