/****************************************************************************
**
** Copyright (C) 2013 Klaralvdalens Datakonsult AB (KDAB)
** Copyright (C) 2023 The Qt Company Ltd.
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

#include "filedownloader_p.h"
#include "filedownloaderfactory.h"
#include "ui_authenticationdialog.h"

#include "fileutils.h"
#include "utils.h"
#include "loggingutils.h"

#include <QDialog>
#include <QDir>
#include <QFile>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkProxyFactory>
#include <QPointer>
#include <QUrl>
#include <QTemporaryFile>
#include <QFileInfo>
#include <QThreadPool>
#include <QDebug>
#include <QSslError>
#include <QBasicTimer>
#include <QTimerEvent>
#include <QLoggingCategory>
#include <globals.h>
#include <QHostInfo>

using namespace KDUpdater;
using namespace QInstaller;

static double calcProgress(qint64 done, qint64 total)
{
    return total ? (double(done) / double(total)) : 0;
}


// -- KDUpdater::FileDownloader

/*!
    \inmodule kdupdater
    \class KDUpdater::FileDownloader
    \brief The FileDownloader class is the base class for file downloaders used in KDUpdater.

    File downloaders are used by the KDUpdater::Update class to download update files. Each
    subclass of FileDownloader can download files from a specific category of sources (such as
    \c local, \c ftp, \c http).

    This is an internal class, not a part of the public API. Currently we have the
    following subclasses of FileDownloader:
    \list
        \li HttpDownloader to download files over FTP, HTTP, or HTTPS if Qt is built with SSL.
        \li LocalFileDownloader to copy files from the local file system.
        \li ResourceFileDownloader to download resource files.
    \endlist
*/

/*!
    \property KDUpdater::FileDownloader::autoRemoveDownloadedFile
    \brief Whether the downloaded file should be automatically removed after it
    is downloaded and the class goes out of scope.
*/

/*!
    \property KDUpdater::FileDownloader::url
    \brief The URL to download files from.
*/

/*!
    \property KDUpdater::FileDownloader::scheme
    \brief The scheme to use for downloading files.
    */

/*!
    \fn KDUpdater::FileDownloader::authenticatorChanged(const QAuthenticator &authenticator)
    This signal is emitted when the authenticator changes to \a authenticator.
*/

/*!
    \fn KDUpdater::FileDownloader::canDownload() const = 0
    Returns \c true if the file exists and is readable.
*/

/*!
    \fn KDUpdater::FileDownloader::clone(QObject *parent=0) const = 0
    Clones the local file downloader and assigns it the parent \a parent.
*/

/*!
    \fn KDUpdater::FileDownloader::downloadCanceled()
    This signal is emitted if downloading a file is canceled.
*/

/*!
    \fn KDUpdater::FileDownloader::downloadedFileName() const = 0
    Returns the file name of the downloaded file.
*/

/*!
    \fn KDUpdater::FileDownloader::downloadProgress(double progress)
    This signal is emitted with the current download \a progress.
*/

/*!
    \fn KDUpdater::FileDownloader::downloadProgress(qint64 bytesReceived, qint64 bytesToReceive)
    This signal is emitted with the download progress as the number of received bytes,
    \a bytesReceived, and the total size of the file to download, \a bytesToReceive.
*/

/*!
    \fn KDUpdater::FileDownloader::downloadSpeed(qint64 bytesPerSecond)
    This signal is emitted with the download speed in bytes per second as \a bytesPerSecond.
*/

/*!
    \fn KDUpdater::FileDownloader::downloadStarted()
    This signal is emitted when downloading a file starts.
*/

/*!
    \fn KDUpdater::FileDownloader::downloadStatus(const QString &status)
    This signal is emitted with textual representation of the current download \a status in the
    following format: "100 MiB of 150 MiB - (DAYS) (HOURS) (MINUTES) (SECONDS) remaining".
*/

/*!
    \fn KDUpdater::FileDownloader::downloadCompleted()
    This signal is emitted when downloading a file ends.
*/

/*!
    \fn KDUpdater::FileDownloader::downloadAborted(const QString &errorMessage)
    This signal is emitted when downloading a file is aborted with \a errorMessage.
*/

/*!
    \fn KDUpdater::FileDownloader::estimatedDownloadTime(int seconds)
    This signal is emitted with the estimated download time in \a seconds.
*/

/*!
    \fn KDUpdater::FileDownloader::isDownloaded() const = 0
    Returns \c true if the file is downloaded.
*/

/*!
    \fn KDUpdater::FileDownloader::onError() = 0
    Closes the destination file if an error occurs during copying and stops
    the download speed timer.
*/

/*!
    \fn KDUpdater::FileDownloader::onSuccess() = 0
    Closes the destination file after it has been successfully copied and stops
    the download speed timer.
*/

/*!
    \fn KDUpdater::FileDownloader::setDownloadedFileName(const QString &name) = 0
    Sets the file name of the downloaded file to \a name.
*/

struct KDUpdater::FileDownloader::Private
{
    Private()
        : m_hash(QCryptographicHash::Sha1)
        , m_assumedSha1Sum("")
        , autoRemove(true)
        , followRedirect(false)
        , m_speedTimerInterval(100)
        , m_downloadDeadlineTimerInterval(30000)
        , m_downloadPaused(false)
        , m_downloadResumed(false)
        , m_bytesReceived(0)
        , m_bytesToReceive(0)
        , m_bytesBeforeResume(0)
        , m_totalBytesBeforeResume(0)
        , m_currentSpeedBin(0)
        , m_sampleIndex(0)
        , m_downloadSpeed(0)
        , m_factory(0)
        , m_ignoreSslErrors(false)
    {
        memset(m_samples, 0, sizeof(m_samples));
    }

    ~Private()
    {
        delete m_factory;
    }

    QUrl url;
    QString scheme;

    QCryptographicHash m_hash;
    QByteArray m_assumedSha1Sum;

    QString errorString;
    bool autoRemove;
    bool followRedirect;

    QBasicTimer m_speedIntervalTimer;
    int m_speedTimerInterval;

    QBasicTimer m_downloadDeadlineTimer;
    int m_downloadDeadlineTimerInterval;
    bool m_downloadPaused;
    bool m_downloadResumed;

    qint64 m_bytesReceived;
    qint64 m_bytesToReceive;
    qint64 m_bytesBeforeResume;
    qint64 m_totalBytesBeforeResume;

    mutable qint64 m_samples[50];
    mutable qint64 m_currentSpeedBin;
    mutable quint32 m_sampleIndex;
    mutable qint64 m_downloadSpeed;

    QAuthenticator m_authenticator;
    FileDownloaderProxyFactory *m_factory;
    bool m_ignoreSslErrors;
};

/*!
    Creates a file downloader with the scheme \a scheme and parent \a parent.
*/
KDUpdater::FileDownloader::FileDownloader(const QString &scheme, QObject *parent)
    : QObject(parent)
    , d(new Private)
{
    d->scheme = scheme;
}

/*!
    Destroys the file downloader.
*/
KDUpdater::FileDownloader::~FileDownloader()
{
    delete d;
}

void KDUpdater::FileDownloader::setUrl(const QUrl &url)
{
    d->url = url;
}

QUrl KDUpdater::FileDownloader::url() const
{
    return d->url;
}

/*!
    Returns the SHA-1 checksum of the downloaded file.
*/
QByteArray KDUpdater::FileDownloader::sha1Sum() const
{
    return d->m_hash.result();
}

/*!
    Returns the assumed SHA-1 checksum of the file to download.
*/
QByteArray KDUpdater::FileDownloader::assumedSha1Sum() const
{
    return d->m_assumedSha1Sum;
}

/*!
    Sets the assumed SHA-1 checksum of the file to download to \a sum.
*/
void KDUpdater::FileDownloader::setAssumedSha1Sum(const QByteArray &sum)
{
    d->m_assumedSha1Sum = sum;
}

/*!
    Returns an error message.
*/
QString FileDownloader::errorString() const
{
    return d->errorString;
}

/*!
    Sets the human readable description of the last error that occurred to \a error. Emits the
    downloadStatus() and downloadAborted() signals.
*/
void FileDownloader::setDownloadAborted(const QString &error)
{
    d->errorString = error;
    emit downloadStatus(error);
    emit downloadAborted(error);
}

/*!
    Sets the download status to \c completed and displays a status message.

    If an assumed SHA-1 checksum is set and the actual calculated checksum does not match it, sets
    the status to \c error. If no SHA-1 is assumed, no check is performed, and status is set to
    \c success.

    Emits the downloadCompleted() and downloadStatus() signals on success.
*/
void KDUpdater::FileDownloader::setDownloadCompleted()
{
    if (d->m_assumedSha1Sum.isEmpty() || (d->m_assumedSha1Sum == sha1Sum())) {
        onSuccess();
        emit downloadCompleted();
        emit downloadStatus(tr("Download finished."));
    } else {
        onError();
        setDownloadAborted(tr("Cryptographic hashes do not match."));
    }
}

/*!
    Emits the downloadCanceled() and downloadStatus() signals.
*/
void KDUpdater::FileDownloader::setDownloadCanceled()
{
    emit downloadCanceled();
    emit downloadStatus(tr("Download canceled."));
}

QString KDUpdater::FileDownloader::scheme() const
{
    return d->scheme;
}

void KDUpdater::FileDownloader::setScheme(const QString &scheme)
{
    d->scheme = scheme;
}

void KDUpdater::FileDownloader::setAutoRemoveDownloadedFile(bool val)
{
    d->autoRemove = val;
}

/*!
    Determines that redirects should be followed if \a val is \c true.
*/
void KDUpdater::FileDownloader::setFollowRedirects(bool val)
{
    d->followRedirect = val;
}

/*!
    Returns whether redirects should be followed.
*/
bool KDUpdater::FileDownloader::followRedirects() const
{
    return d->followRedirect;
}

bool KDUpdater::FileDownloader::isAutoRemoveDownloadedFile() const
{
    return d->autoRemove;
}

/*!
    Downloads files.
*/
void KDUpdater::FileDownloader::download()
{
    QMetaObject::invokeMethod(this, "doDownload", Qt::QueuedConnection);
}

/*!
    Cancels file download.
*/
void KDUpdater::FileDownloader::cancelDownload()
{
    // Do nothing
}

/*!
    Starts the download speed timer.
*/
void KDUpdater::FileDownloader::runDownloadSpeedTimer()
{
    if (!d->m_speedIntervalTimer.isActive())
        d->m_speedIntervalTimer.start(d->m_speedTimerInterval, this);
}

/*!
    Stops the download speed timer.
*/
void KDUpdater::FileDownloader::stopDownloadSpeedTimer()
{
    d->m_speedIntervalTimer.stop();
}

/*!
    Restarts the download deadline timer.
*/
void KDUpdater::FileDownloader::runDownloadDeadlineTimer()
{
  stopDownloadDeadlineTimer();
  d->m_downloadDeadlineTimer.start(d->m_downloadDeadlineTimerInterval, this);
}

/*!
    Stops the download deadline timer.
*/
void KDUpdater::FileDownloader::stopDownloadDeadlineTimer()
{
    d->m_downloadDeadlineTimer.stop();
}

/*!
    Sets the download into a \a paused state.
*/
void KDUpdater::FileDownloader::setDownloadPaused(bool paused)
{
    d->m_downloadPaused = paused;
}

/*!
    Returns the download paused state.
*/
bool KDUpdater::FileDownloader::isDownloadPaused()
{
    return d->m_downloadPaused;
}

/*!
    Sets the download into a \a resumed state.
*/
void KDUpdater::FileDownloader::setDownloadResumed(bool resumed)
{
    d->m_downloadResumed = resumed;
}

/*!
    Returns the download resumed state.
*/
bool KDUpdater::FileDownloader::isDownloadResumed()
{
    return d->m_downloadResumed;
}

/*!
    Gets the amount of bytes downloaded before download resume.
*/
qint64 KDUpdater::FileDownloader::bytesDownloadedBeforeResume()
{
    return d->m_bytesBeforeResume;
}

/*!
    Gets the total amount of bytes downloaded before download resume.
*/
qint64 KDUpdater::FileDownloader::totalBytesDownloadedBeforeResume()
{
    return d->m_totalBytesBeforeResume;
}

/*!
    Clears the amount of bytes downloaded before download resume.
*/
void KDUpdater::FileDownloader::clearBytesDownloadedBeforeResume()
{
    d->m_bytesBeforeResume = 0;
    d->m_totalBytesBeforeResume = 0;
}

/*!
    Updates the amount of \a bytes downloaded before download resumes.
*/
void KDUpdater::FileDownloader::updateBytesDownloadedBeforeResume(qint64 bytes)
{
    d->m_bytesBeforeResume += bytes;
}

/*!
    Updates the total amount of bytes downloaded before download resume.
*/
void KDUpdater::FileDownloader::updateTotalBytesDownloadedBeforeResume()
{
    d->m_totalBytesBeforeResume = d->m_bytesBeforeResume;
}

/*!
    Adds \a sample to the current speed bin.
*/
void KDUpdater::FileDownloader::addSample(qint64 sample)
{
    d->m_currentSpeedBin += sample;
}

/*!
    Returns the download speed timer ID.
*/
int KDUpdater::FileDownloader::downloadSpeedTimerId() const
{
    return d->m_speedIntervalTimer.timerId();
}

/*!
    Returns the download deadline timer ID.
*/
int KDUpdater::FileDownloader::downloadDeadlineTimerId() const
{
    return d->m_downloadDeadlineTimer.timerId();
}

/*!
    Sets the file download progress to the number of received bytes, \a bytesReceived,
    and the number of total bytes to receive, \a bytesToReceive.
*/
void KDUpdater::FileDownloader::setProgress(qint64 bytesReceived, qint64 bytesToReceive)
{
    d->m_bytesReceived = bytesReceived;
    d->m_bytesToReceive = bytesToReceive;
}

/*!
    Calculates the download speed in bytes per second and emits the downloadSpeed() signal.
*/
void KDUpdater::FileDownloader::emitDownloadSpeed()
{
    unsigned int windowSize = sizeof(d->m_samples) / sizeof(qint64);

    // add speed of last time bin to the window
    d->m_samples[d->m_sampleIndex % windowSize] = d->m_currentSpeedBin;
    d->m_currentSpeedBin = 0;   // reset bin for next time interval

    // advance the sample index
    d->m_sampleIndex++;
    d->m_downloadSpeed = 0;

    // dynamic window size until the window is completely filled
    if (d->m_sampleIndex < windowSize)
        windowSize = d->m_sampleIndex;

    for (unsigned int i = 0; i < windowSize; ++i)
        d->m_downloadSpeed += d->m_samples[i];

    d->m_downloadSpeed /= windowSize; // computer average
    d->m_downloadSpeed *= 1000.0 / d->m_speedTimerInterval; // rescale to bytes/second

    emit downloadSpeed(d->m_downloadSpeed);
}

/*!
    Builds a textual representation of the download status in the following format:
    "100 MiB of 150 MiB - (DAYS) (HOURS) (MINUTES) (SECONDS) remaining".

    Emits the downloadStatus() signal.
*/
void KDUpdater::FileDownloader::emitDownloadStatus()
{
    QString status;
    if (d->m_bytesToReceive > 0) {
        QString bytesReceived = humanReadableSize(d->m_bytesReceived);
        const QString bytesToReceive = humanReadableSize(d->m_bytesToReceive);

        // remove the unit from the bytesReceived value if bytesToReceive has the same
        const QString tmp = bytesToReceive.mid(bytesToReceive.indexOf(QLatin1Char(' ')));
        if (bytesReceived.endsWith(tmp))
            bytesReceived.chop(tmp.length());

        status = tr("%1 of %2").arg(bytesReceived).arg(bytesToReceive);
    } else {
        if (d->m_bytesReceived > 0)
            status = tr("%1 downloaded.").arg(humanReadableSize(d->m_bytesReceived));
    }

    status += QLatin1Char(' ') + tr("(%1/sec)").arg(humanReadableSize(d->m_downloadSpeed));
    if (d->m_bytesToReceive > 0 && d->m_downloadSpeed > 0) {
        const qint64 time = (d->m_bytesToReceive - d->m_bytesReceived) / d->m_downloadSpeed;

        int s = time % 60;
        const int d = time / 86400;
        const int h = (time / 3600) - (d * 24);
        const int m = (time / 60) - (d * 1440) - (h * 60);

        QString days;
        if (d > 0)
            days = tr("%n day(s), ", "", d);

        QString hours;
        if (h > 0)
            hours = tr("%n hour(s), ", "", h);

        QString minutes;
        if (m > 0)
            minutes = tr("%n minute(s)", "", m);

        QString seconds;
        if (s >= 0 && minutes.isEmpty()) {
            s = (s <= 0 ? 1 : s);
            seconds = tr("%n second(s)", "", s);
        }
        status += tr(" - %1%2%3%4 remaining.").arg(days).arg(hours).arg(minutes).arg(seconds);
    } else {
        status += tr(" - unknown time remaining.");
    }

    emit downloadStatus(status);
}

/*!
    Emits dowload progress.
*/
void KDUpdater::FileDownloader::emitDownloadProgress()
{
    emit downloadProgress(d->m_bytesReceived, d->m_bytesToReceive);
}

/*!
    Emits the estimated download time.
*/
void KDUpdater::FileDownloader::emitEstimatedDownloadTime()
{
    if (d->m_bytesToReceive <= 0 || d->m_downloadSpeed <= 0) {
        emit estimatedDownloadTime(-1);
        return;
    }
    emit estimatedDownloadTime((d->m_bytesToReceive - d->m_bytesReceived) / d->m_downloadSpeed);
}

/*!
    Adds checksum \a data.
*/
void KDUpdater::FileDownloader::addCheckSumData(const QByteArray &data)
{
    d->m_hash.addData(data);
}

/*!
    Resets SHA-1 checksum data of the downloaded file.
*/
void KDUpdater::FileDownloader::resetCheckSumData()
{
    d->m_hash.reset();
}

/*!
    Returns a copy of the proxy factory that this FileDownloader object is using to determine the
    proxies to be used for requests.
*/
FileDownloaderProxyFactory *KDUpdater::FileDownloader::proxyFactory() const
{
    if (d->m_factory)
        return d->m_factory->clone();
    return 0;
}

/*!
    Sets the proxy factory for this class to be \a factory. A proxy factory is used to determine a
    more specific list of proxies to be used for a given request, instead of trying to use the same
    proxy value for all requests. This might only be of use for HTTP or FTP requests.
*/
void KDUpdater::FileDownloader::setProxyFactory(FileDownloaderProxyFactory *factory)
{
    delete d->m_factory;
    d->m_factory = factory;
}

/*!
    Returns a copy of the authenticator that this FileDownloader object is using to set the username
    and password for a download request.
*/
QAuthenticator KDUpdater::FileDownloader::authenticator() const
{
    return d->m_authenticator;
}

/*!
    Sets the authenticator object for this class to be \a authenticator. An authenticator is used to
    pass on the required authentication information. This might only be of use for HTTP or FTP
    requests. Emits the authenticator changed signal with the new authenticator in use.
*/
void KDUpdater::FileDownloader::setAuthenticator(const QAuthenticator &authenticator)
{
    if (d->m_authenticator.isNull() || (d->m_authenticator != authenticator)) {
        d->m_authenticator = authenticator;
        emit authenticatorChanged(authenticator);
    }
}

/*!
    Returns \c true if SSL errors should be ignored.
*/
bool KDUpdater::FileDownloader::ignoreSslErrors()
{
    return d->m_ignoreSslErrors;
}

/*!
    Determines that SSL errors should be ignored if \a ignore is \c true.
*/
void KDUpdater::FileDownloader::setIgnoreSslErrors(bool ignore)
{
    d->m_ignoreSslErrors = ignore;
}

/*!
    Returns the number of received bytes.
*/
qint64 FileDownloader::getBytesReceived() const
{
    return d->m_bytesReceived;
}

// -- KDUpdater::LocalFileDownloader

/*!
    \inmodule kdupdater
    \class KDUpdater::LocalFileDownloader
    \brief The LocalFileDownloader class is used to copy files from the local
    file system.

    The user of KDUpdater might be simultaneously downloading several files;
    sometimes in parallel to other file downloaders. If copying a local file takes
    a long time, it will make the other downloads hang. Therefore, a timer is used
    and one block of data is copied per unit time, even though QFile::copy() does the
    task of copying local files from one place to another.
*/

struct KDUpdater::LocalFileDownloader::Private
{
    Private()
        : source(0)
        , destination(0)
        , downloaded(false)
        , timerId(-1)
    {}

    QFile *source;
    QFile *destination;
    QString destFileName;
    bool downloaded;
    int timerId;
};

/*!
    Creates a local file downloader with the parent \a parent.
*/
KDUpdater::LocalFileDownloader::LocalFileDownloader(QObject *parent)
    : KDUpdater::FileDownloader(QLatin1String("file"), parent)
    , d (new Private)
{
}

/*!
    Destroys the local file downloader.
*/
KDUpdater::LocalFileDownloader::~LocalFileDownloader()
{
    if (this->isAutoRemoveDownloadedFile() && !d->destFileName.isEmpty())
        QFile::remove(d->destFileName);

    delete d;
}

/*!
    Returns \c true if the file exists and is readable.
*/
bool KDUpdater::LocalFileDownloader::canDownload() const
{
    QFileInfo fi(url().toLocalFile());
    return fi.exists() && fi.isReadable();
}

/*!
    Returns \c true if the file is copied.
*/
bool KDUpdater::LocalFileDownloader::isDownloaded() const
{
    return d->downloaded;
}

void KDUpdater::LocalFileDownloader::doDownload()
{
    // Already downloaded
    if (d->downloaded)
        return;

    // Already started downloading
    if (d->timerId >= 0)
        return;

    // Open source and destination files
    QString localFile = this->url().toLocalFile();
    d->source = new QFile(localFile, this);
    if (!d->source->open(QFile::ReadOnly)) {
        setDownloadAborted(tr("Cannot open file \"%1\" for reading: %2").arg(QFileInfo(localFile)
            .fileName(), d->source->errorString()));
        onError();
        return;
    }

    if (d->destFileName.isEmpty()) {
        QTemporaryFile *file = new QTemporaryFile(this);
        file->open();
        d->destination = file;
    } else {
        d->destination = new QFile(d->destFileName, this);
        d->destination->open(QIODevice::ReadWrite | QIODevice::Truncate);
    }

    if (!d->destination->isOpen()) {
        setDownloadAborted(tr("Cannot open file \"%1\" for writing: %2")
            .arg(QFileInfo(d->destination->fileName()).fileName(), d->destination->errorString()));
        onError();
        return;
    }

    runDownloadSpeedTimer();
    // Start a timer and kickoff the copy process
    d->timerId = startTimer(0); // as fast as possible

    emit downloadStarted();
    emit downloadProgress(0);
}

/*!
    Returns the file name of the copied file.
*/
QString KDUpdater::LocalFileDownloader::downloadedFileName() const
{
    return d->destFileName;
}

/*!
    Sets the file name of the copied file to \a name.
*/
void KDUpdater::LocalFileDownloader::setDownloadedFileName(const QString &name)
{
    d->destFileName = name;
}

/*!
    Clones the local file downloader and assigns it the parent \a parent. Returns
    the new local file downloader.
*/
KDUpdater::LocalFileDownloader *KDUpdater::LocalFileDownloader::clone(QObject *parent) const
{
    return new LocalFileDownloader(parent);
}

/*!
    Cancels copying the file.
*/
void KDUpdater::LocalFileDownloader::cancelDownload()
{
    if (d->timerId < 0)
        return;

    killTimer(d->timerId);
    d->timerId = -1;

    onError();
    setDownloadCanceled();
}

/*!
    Called when the download timer event \a event occurs.
*/
void KDUpdater::LocalFileDownloader::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == d->timerId) {
        if (!d->source || !d->destination)
            return;

        const qint64 blockSize = 32768;
        QByteArray buffer;
        buffer.resize(blockSize);
        const qint64 numRead = d->source->read(buffer.data(), buffer.size());
        qint64 toWrite = numRead;
        while (toWrite > 0) {
            const qint64 numWritten = d->destination->write(buffer.constData() + numRead - toWrite, toWrite);
            if (numWritten < 0) {
                killTimer(d->timerId);
                d->timerId = -1;
                setDownloadAborted(tr("Writing to file \"%1\" failed: %2").arg(
                                       QDir::toNativeSeparators(d->destination->fileName()),
                                       d->destination->errorString()));
                onError();
                return;
            }
            toWrite -= numWritten;
        }
        addSample(numRead);
        addCheckSumData(buffer.left(numRead));
        if (numRead > 0) {
            setProgress(d->source->pos(), d->source->size());
            emit downloadProgress(calcProgress(d->source->pos(), d->source->size()));
            return;
        }

        d->destination->flush();

        killTimer(d->timerId);
        d->timerId = -1;

        setDownloadCompleted();
    } else if (event->timerId() == downloadSpeedTimerId()) {
        emitDownloadSpeed();
        emitDownloadStatus();
        emitDownloadProgress();
        emitEstimatedDownloadTime();
    }
}

/*!
    Closes the destination file after it has been successfully copied and stops
    the download speed timer.
*/
void LocalFileDownloader::onSuccess()
{
    d->downloaded = true;
    d->destFileName = d->destination->fileName();
    if (QTemporaryFile *file = dynamic_cast<QTemporaryFile *>(d->destination))
        file->setAutoRemove(false);
    d->destination->close();
    delete d->destination;
    d->destination = 0;
    delete d->source;
    d->source = 0;
    stopDownloadSpeedTimer();
}

/*!
    Clears the destination file if an error occurs during copying and stops
    the download speed timer.
*/
void LocalFileDownloader::onError()
{
    d->downloaded = false;
    d->destFileName.clear();
    delete d->destination;
    d->destination = 0;
    delete d->source;
    d->source = 0;
    stopDownloadSpeedTimer();
}


// -- ResourceFileDownloader

/*!
    \inmodule kdupdater
    \class KDUpdater::ResourceFileDownloader
    \brief The ResourceFileDownloader class can be used to download resource files.
*/
struct KDUpdater::ResourceFileDownloader::Private
{
    Private()
        : timerId(-1)
        , downloaded(false)
    {}

    int timerId;
    QFile destFile;
    bool downloaded;
};

/*!
    Creates a resource file downloader with the parent \a parent.
*/
KDUpdater::ResourceFileDownloader::ResourceFileDownloader(QObject *parent)
    : KDUpdater::FileDownloader(QLatin1String("resource"), parent)
    , d(new Private)
{
}

/*!
    Destroys the resource file downloader.
*/
KDUpdater::ResourceFileDownloader::~ResourceFileDownloader()
{
    delete d;
}

/*!
    Returns \c true if the file exists and is readable.
*/
bool KDUpdater::ResourceFileDownloader::canDownload() const
{
    const QFileInfo fi(QInstaller::pathFromUrl(url()));
    return fi.exists() && fi.isReadable();
}

/*!
    Returns \c true if the file is downloaded.
*/
bool KDUpdater::ResourceFileDownloader::isDownloaded() const
{
    return d->downloaded;
}

/*!
    Downloads a resource file.
*/
void KDUpdater::ResourceFileDownloader::doDownload()
{
    // Already downloaded
    if (d->downloaded)
        return;

    // Already started downloading
    if (d->timerId >= 0)
        return;

    // Open source and destination files
    QUrl url = this->url();
    url.setScheme(QString::fromLatin1("file"));
    d->destFile.setFileName(QString::fromLatin1(":%1").arg(url.toLocalFile()));

    emit downloadStarted();
    emit downloadProgress(0);

    d->destFile.open(QIODevice::ReadOnly);
    d->timerId = startTimer(0); // start as fast as possible
}

/*!
    Returns the file name of the downloaded file.
*/
QString KDUpdater::ResourceFileDownloader::downloadedFileName() const
{
    return d->destFile.fileName();
}

/*!
    Sets the file name of the downloaded file to \a name.
*/
void KDUpdater::ResourceFileDownloader::setDownloadedFileName(const QString &/*name*/)
{
    // Not supported!
}

/*!
    Clones the resource file downloader and assigns it the parent \a parent. Returns
    the new resource file downloader.
*/
KDUpdater::ResourceFileDownloader *KDUpdater::ResourceFileDownloader::clone(QObject *parent) const
{
    return new ResourceFileDownloader(parent);
}

/*!
    Cancels downloading the file.
*/
void KDUpdater::ResourceFileDownloader::cancelDownload()
{
    if (d->timerId < 0)
        return;

    killTimer(d->timerId);
    d->timerId = -1;

    setDownloadCanceled();
}

/*!
    Called when the download timer event \a event occurs.
*/
void KDUpdater::ResourceFileDownloader::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == d->timerId) {
        if (!d->destFile.isOpen()) {
            killTimer(d->timerId);
            emit downloadProgress(1);
            setDownloadAborted(tr("Cannot read resource file \"%1\": %2").arg(downloadedFileName(),
                d->destFile.errorString()));
            onError();
            return;
        }

        QByteArray buffer;
        buffer.resize(32768);
        const qint64 numRead = d->destFile.read(buffer.data(), buffer.size());

        addSample(numRead);
        addCheckSumData(buffer.left(numRead));

        if (numRead > 0) {
            setProgress(d->destFile.pos(), d->destFile.size());
            emit downloadProgress(calcProgress(d->destFile.pos(), d->destFile.size()));
            return;
        }

        killTimer(d->timerId);
        d->timerId = -1;
        setDownloadCompleted();
    } else if (event->timerId() == downloadSpeedTimerId()) {
        emitDownloadSpeed();
        emitDownloadStatus();
        emitDownloadProgress();
        emitEstimatedDownloadTime();
    }
}

/*!
    Closes the destination file after it has been successfully copied and stops
    the download speed timer.
*/
void KDUpdater::ResourceFileDownloader::onSuccess()
{
    d->destFile.close();
    d->downloaded = true;
    stopDownloadSpeedTimer();
}

/*!
    Closes the destination file if an error occurs during copying and stops
    the download speed timer.
*/
void KDUpdater::ResourceFileDownloader::onError()
{
    d->destFile.close();
    d->downloaded = false;
    stopDownloadSpeedTimer();
    d->destFile.setFileName(QString());
}


// -- KDUpdater::HttpDownloader

/*!
    \inmodule kdupdater
    \class KDUpdater::HttpDownloader
    \brief The HttpDownloader class is used to download files over FTP, HTTP, or HTTPS.

    HTTPS is supported if Qt is built with SSL.
*/
struct KDUpdater::HttpDownloader::Private
{
    explicit Private(HttpDownloader *qq)
        : q(qq)
        , http(0)
        , destination(0)
        , downloaded(false)
        , aborted(false)
        , m_authenticationCount(0)
    {}

    HttpDownloader *const q;
    QNetworkAccessManager manager;
    QNetworkReply *http;
    QUrl sourceUrl;
    QFile *destination;
    QString destFileName;
    bool downloaded;
    bool aborted;
    int m_authenticationCount;

    void shutDown(bool closeDestination = true)
    {
        if (http) {
            disconnect(http, &QNetworkReply::finished, q, &HttpDownloader::httpReqFinished);
            disconnect(http, &QNetworkReply::downloadProgress,
                       q, &HttpDownloader::httpReadProgress);

            disconnect(http, &QNetworkReply::errorOccurred, q, &HttpDownloader::httpError);
            http->deleteLater();
        }
        http = 0;
        if (closeDestination) {
            destination->close();
            destination->deleteLater();
            destination = 0;
            q->resetCheckSumData();
        }
    }
};

/*!
    Creates an HTTP downloader with the parent \a parent.
*/
KDUpdater::HttpDownloader::HttpDownloader(QObject *parent)
    : KDUpdater::FileDownloader(QLatin1String("http"), parent)
    , d(new Private(this))
{
#ifndef QT_NO_SSL
    connect(&d->manager, &QNetworkAccessManager::sslErrors,
            this, &HttpDownloader::onSslErrors);
#endif
    connect(&d->manager, &QNetworkAccessManager::authenticationRequired,
            this, &HttpDownloader::onAuthenticationRequired);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    connect(&d->manager, &QNetworkAccessManager::networkAccessibleChanged,
            this, &HttpDownloader::onNetworkAccessibleChanged);
#else
    auto netInfo = QNetworkInformation::instance();
    connect(netInfo, &QNetworkInformation::reachabilityChanged,
            this, &HttpDownloader::onReachabilityChanged);
#endif

}

/*!
    Destroys an HTTP downloader.

    Removes the downloaded file if FileDownloader::isAutoRemoveDownloadedFile() returns \c true or
    FileDownloader::setAutoRemoveDownloadedFile() was called with \c true.
*/
KDUpdater::HttpDownloader::~HttpDownloader()
{
    if (this->isAutoRemoveDownloadedFile() && !d->destFileName.isEmpty())
        QFile::remove(d->destFileName);
    delete d;
}

/*!
    Returns \c true if the file exists and is readable.
*/
bool KDUpdater::HttpDownloader::canDownload() const
{
    // TODO: Check whether the http file actually exists or not.
    return true;
}

/*!
    Returns \c true if the file is downloaded.
*/
bool KDUpdater::HttpDownloader::isDownloaded() const
{
    return d->downloaded;
}

void KDUpdater::HttpDownloader::doDownload()
{
    if (d->downloaded)
        return;

    if (d->http)
        return;

    startDownload(url());
    runDownloadSpeedTimer();
    runDownloadDeadlineTimer();
}

/*!
    Returns the file name of the downloaded file.
*/
QString KDUpdater::HttpDownloader::downloadedFileName() const
{
    return d->destFileName;
}

/*!
    Sets the file name of the downloaded file to \a name.
*/
void KDUpdater::HttpDownloader::setDownloadedFileName(const QString &name)
{
    d->destFileName = name;
}

/*!
    Clones the HTTP downloader and assigns it the parent \a parent. Returns the new
    HTTP downloader.
*/
KDUpdater::HttpDownloader *KDUpdater::HttpDownloader::clone(QObject *parent) const
{
    return new HttpDownloader(parent);
}

void KDUpdater::HttpDownloader::httpReadyRead()
{
    if (d->http == 0 || d->destination == 0)
      return;
    static QByteArray buffer(16384, '\0');
    while (d->http->bytesAvailable()) {
        const qint64 read = d->http->read(buffer.data(), buffer.size());
        qint64 written = 0;
        while (written < read) {
            const qint64 numWritten = d->destination->write(buffer.data() + written, read - written);
            if (numWritten < 0) {
                const QString error = d->destination->errorString();
                const QString fileName = d->destination->fileName();
                d->shutDown();
                setDownloadAborted(tr("Cannot download %1. Writing to file \"%2\" failed: %3")
                    .arg(url().toString(), fileName, error));
                return;
            }
            written += numWritten;
        }
        addSample(written);
        addCheckSumData(buffer.left(read));
        updateBytesDownloadedBeforeResume(written);
    }
}

void KDUpdater::HttpDownloader::httpError(QNetworkReply::NetworkError)
{
    if (!d->aborted)
        httpDone(true);
}

/*!
    Cancels downloading the file.
*/
void KDUpdater::HttpDownloader::cancelDownload()
{
    d->aborted = true;
    if (d->http) {
        d->http->abort();
        httpDone(true);
    }
}

void KDUpdater::HttpDownloader::httpDone(bool error)
{
    if (error) {
        if (isDownloadResumed()) {
            d->shutDown(false);
            return;
        }
        QString err;
        if (d->http) {
            err = d->http->errorString();
            d->http->deleteLater();
            d->http = 0;
            onError();
        }

        if (d->aborted) {
            d->aborted = false;
            setDownloadCanceled();
        } else {
            setDownloadAborted(err);
            return;
        }
    }
    setDownloadResumed(false);
}

/*!
    Closes the destination file if an error occurs during copying and stops
    the download speed timer.
*/
void KDUpdater::HttpDownloader::onError()
{
    d->downloaded = false;
    d->destFileName.clear();
    delete d->destination;
    d->destination = 0;
    stopDownloadSpeedTimer();
    stopDownloadDeadlineTimer();
}

/*!
    Closes the destination file after it has been successfully copied and stops
    the download speed timer.
*/
void KDUpdater::HttpDownloader::onSuccess()
{
    d->downloaded = true;
    if (d->destination) {
        d->destFileName = d->destination->fileName();
        if (QTemporaryFile *file = dynamic_cast<QTemporaryFile *>(d->destination))
            file->setAutoRemove(false);
    }
    delete d->destination;
    d->destination = 0;
    stopDownloadSpeedTimer();
    stopDownloadDeadlineTimer();
    setDownloadResumed(false);
}

void KDUpdater::HttpDownloader::httpReqFinished()
{
    const QVariant redirect = d->http == 0 ? QVariant()
        : d->http->attribute(QNetworkRequest::RedirectionTargetAttribute);

    const QUrl redirectUrl = redirect.toUrl();
    if (followRedirects() && redirectUrl.isValid()) {
        d->shutDown();  // clean the previous download
        startDownload(redirectUrl);
    } else {
        if (d->http == 0)
            return;
        const QUrl url = d->http->url();
        // Only print host information when the logging category is enabled
        // and output verbosity is set above standard level.
        if (url.isValid() && QInstaller::lcServer().isDebugEnabled()
                && LoggingHandler::instance().verboseLevel() == LoggingHandler::Detailed) {
            const QFileInfo fi(d->http->url().toString());
            if (fi.suffix() != QLatin1String("sha1")){
                const QString hostName = url.host();
                QHostInfo info = QHostInfo::fromName(hostName);
                QStringList hostAddresses;
                foreach (const QHostAddress &address, info.addresses())
                    hostAddresses << address.toString();
                qCDebug(QInstaller::lcServer) << "Using host:" << hostName
                        << "for" << url.fileName() << "\nIP:" << hostAddresses;
            }
        }
        httpReadyRead();
        if (d->destination)
            d->destination->flush();
        setDownloadCompleted();
        if (d->http)
            d->http->deleteLater();
        d->http = 0;
    }
}

void KDUpdater::HttpDownloader::httpReadProgress(qint64 done, qint64 total)
{
    if (d->http) {
        const QUrl redirectUrl = d->http->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
        if (followRedirects() && redirectUrl.isValid())
            return; // if we are a redirection, do not emit the progress
    }

    if (isDownloadResumed())
        setProgress(done + totalBytesDownloadedBeforeResume(),
                    total + totalBytesDownloadedBeforeResume());
    else
        setProgress(done, total);
    runDownloadDeadlineTimer();
    if (isDownloadResumed())
        emit downloadProgress(calcProgress(done + totalBytesDownloadedBeforeResume(), total + totalBytesDownloadedBeforeResume()));
    else
        emit downloadProgress(calcProgress(done, total));
}

/*!
    Called when the download timer event \a event occurs.
*/
void KDUpdater::HttpDownloader::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == downloadSpeedTimerId()) {
        emitDownloadSpeed();
        emitDownloadStatus();
        emitDownloadProgress();
        emitEstimatedDownloadTime();
    } else if (event->timerId() == downloadDeadlineTimerId()) {
        d->shutDown(false);
        resumeDownload();
    }
}

void KDUpdater::HttpDownloader::startDownload(const QUrl &url)
{
    d->sourceUrl = url;
    d->m_authenticationCount = 0;
    d->manager.setProxyFactory(proxyFactory());
    clearBytesDownloadedBeforeResume();

    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);

    d->http = d->manager.get(request);
    connect(d->http, &QIODevice::readyRead, this, &HttpDownloader::httpReadyRead);
    connect(d->http, &QNetworkReply::downloadProgress,
            this, &HttpDownloader::httpReadProgress);
    connect(d->http, &QNetworkReply::finished, this, &HttpDownloader::httpReqFinished);
    connect(d->http, &QNetworkReply::errorOccurred, this, &HttpDownloader::httpError);

    bool fileOpened = false;
    if (d->destFileName.isEmpty()) {
        QTemporaryFile *file = new QTemporaryFile(this);
        fileOpened = file->open();
        d->destination = file;
    } else {
        d->destination = new QFile(d->destFileName, this);
        fileOpened = d->destination->open(QIODevice::ReadWrite | QIODevice::Truncate);
    }
    if (!fileOpened) {
        qCWarning(QInstaller::lcInstallerInstallLog).nospace() << "Failed to open file " << d->destFileName
            << ": "<<d->destination->errorString() << ". Trying again.";
        QFileInfo fileInfo;
        fileInfo.setFile(d->destination->fileName());
        if (!QDir().mkpath(fileInfo.absolutePath())) {
            setDownloadAborted(tr("Cannot download %1. Cannot create directory for \"%2\"").arg(
                url.toString(), fileInfo.filePath()));
        } else {
            fileOpened = d->destination->open(QIODevice::ReadWrite | QIODevice::Truncate);
            if (fileOpened)
                return;
            if (d->destination->exists())
                qCWarning(QInstaller::lcInstallerInstallLog) << "File exists but installer is unable to open it.";
            else
                qCWarning(QInstaller::lcInstallerInstallLog) << "File does not exist.";
            setDownloadAborted(tr("Cannot download %1. Cannot create file \"%2\": %3").arg(
                url.toString(), d->destination->fileName(), d->destination->errorString()));
            d->shutDown();
        }
    }
}

void KDUpdater::HttpDownloader::resumeDownload()
{
    updateTotalBytesDownloadedBeforeResume();
    d->m_authenticationCount = 0;
    QNetworkRequest request(d->sourceUrl);

    request.setRawHeader(QByteArray("Range"),
                         QString(QStringLiteral("bytes=%1-"))
                         .arg(bytesDownloadedBeforeResume())
                         .toLatin1());
    setDownloadResumed(true);
    d->http = d->manager.get(request);
    connect(d->http, &QIODevice::readyRead, this, &HttpDownloader::httpReadyRead);
    connect(d->http, &QNetworkReply::downloadProgress,
            this, &HttpDownloader::httpReadProgress);
    connect(d->http, &QNetworkReply::finished, this, &HttpDownloader::httpReqFinished);
    connect(d->http, &QNetworkReply::errorOccurred, this, &HttpDownloader::httpError);
    runDownloadSpeedTimer();
    runDownloadDeadlineTimer();
}

void KDUpdater::HttpDownloader::onAuthenticationRequired(QNetworkReply *reply, QAuthenticator *authenticator)
{
    Q_UNUSED(reply)
    // first try with the information we have already
    if (d->m_authenticationCount == 0) {
        d->m_authenticationCount++;
        authenticator->setUser(this->authenticator().user());
        authenticator->setPassword(this->authenticator().password());
    } else if (d->m_authenticationCount == 1) {
        // we failed to authenticate, ask for new credentials
        QDialog dlg;
        Ui::Dialog ui;
        ui.setupUi(&dlg);
        dlg.adjustSize();
        ui.siteDescription->setText(tr("%1 at %2").arg(authenticator->realm()).arg(url().host()));

        ui.userEdit->setText(this->authenticator().user());
        ui.passwordEdit->setText(this->authenticator().password());

        if (dlg.exec() == QDialog::Accepted) {
            authenticator->setUser(ui.userEdit->text());
            authenticator->setPassword(ui.passwordEdit->text());

            // update the authenticator we used initially
            QAuthenticator auth;
            auth.setUser(ui.userEdit->text());
            auth.setPassword(ui.passwordEdit->text());
            emit authenticatorChanged(auth);
        } else {
            d->shutDown();
            setDownloadAborted(tr("Authentication request canceled."));
            emit downloadCanceled();
        }
        d->m_authenticationCount++;
    }
}

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
void KDUpdater::HttpDownloader::onNetworkAccessibleChanged(QNetworkAccessManager::NetworkAccessibility accessible)
{
  if (accessible == QNetworkAccessManager::NotAccessible) {
      d->shutDown(false);
      setDownloadPaused(true);
      setDownloadResumed(false);
      stopDownloadDeadlineTimer();
  } else if (accessible == QNetworkAccessManager::Accessible) {
      if (isDownloadPaused()) {
          setDownloadPaused(false);
          resumeDownload();
      }
  }
}
#else
void KDUpdater::HttpDownloader::onReachabilityChanged(QNetworkInformation::Reachability newReachability)
{
    if (newReachability == QNetworkInformation::Reachability::Online) {
        if (isDownloadPaused()) {
            setDownloadPaused(false);
            resumeDownload();
        }
    } else {
        d->shutDown(false);
        setDownloadPaused(true);
        setDownloadResumed(false);
        stopDownloadDeadlineTimer();
    }
}
#endif

#ifndef QT_NO_SSL

#include "messageboxhandler.h"

void KDUpdater::HttpDownloader::onSslErrors(QNetworkReply* reply, const QList<QSslError> &errors)
{
    Q_UNUSED(reply)
    QString errorString;
    foreach (const QSslError &error, errors) {
        if (!errorString.isEmpty())
            errorString += QLatin1String(", ");
        errorString += error.errorString();
    }
    qCWarning(QInstaller::lcInstallerInstallLog) << errorString;

    const QStringList arguments = QCoreApplication::arguments();
    if (arguments.contains(QLatin1String("--script")) || arguments.contains(QLatin1String("Script"))
        || ignoreSslErrors()) {
            reply->ignoreSslErrors();
            return;
    }
    // TODO: Remove above code once we have a proper implementation for message box handler supporting
    // methods used in the following code, right now we return here cause the message box is not scriptable.

    QMessageBox msgBox(MessageBoxHandler::currentBestSuitParent());
    msgBox.setDetailedText(errorString);
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setWindowModality(Qt::WindowModal);
    msgBox.setWindowTitle(tr("Secure Connection Failed"));
    msgBox.setText(tr("There was an error during connection to: %1.").arg(url().toString()));
    msgBox.setInformativeText(QString::fromLatin1("<ul><li>%1</li><li>%2</li></ul>").arg(tr("This could be "
        "a problem with the server's configuration, or it could be someone trying to impersonate the "
        "server."), tr("If you have connected to this server successfully in the past or trust this server, "
        "the error may be temporary and you can try again.")));

    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
    msgBox.addButton(tr("Try again"), QMessageBox::YesRole);
    msgBox.setDefaultButton(QMessageBox::Cancel);

    if (msgBox.exec() == QMessageBox::Cancel) {
        if (!d->aborted)
            httpDone(true);
    } else {
        reply->ignoreSslErrors();
        KDUpdater::FileDownloaderFactory::instance().setIgnoreSslErrors(true);
    }
}
#endif
