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

#include "kdupdaterfiledownloader_p.h"
#include "kdupdaterfiledownloaderfactory.h"

#include <QFile>
#include <QFtp>
#include <QNetworkAccessManager>
#include <QNetworkProxyFactory>
#include <QPointer>
#include <QUrl>
#include <QTemporaryFile>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QThreadPool>
#include <QDebug>

#include <QBasicTimer>
#include <QTimerEvent>

using namespace KDUpdater;

static double calcProgress(qint32 done, qint32 total)
{
    return total ? (double(done) / double(total)) : 0;
}

static QString format(double data)
{
    if (data < 1024.0)
        return KDUpdater::FileDownloader::tr("%L1 B").arg(data);
    data /= 1024.0;
    if (data < 1024.0)
        return KDUpdater::FileDownloader::tr("%L1 KB").arg(data, 0, 'f', 2);
    data /= 1024.0;
    if (data < 1024.0)
        return KDUpdater::FileDownloader::tr("%L1 MB").arg(data, 0, 'f', 2);
    data /= 1024.0;
    return KDUpdater::FileDownloader::tr("%L1 GB").arg(data, 0, 'f', 2);
}

QByteArray KDUpdater::calculateHash(QIODevice* device, QCryptographicHash::Algorithm algo)
{
    Q_ASSERT(device);
    QCryptographicHash hash(algo);
    QByteArray buffer;
    buffer.resize(512 * 1024);
    while (true) {
        const qint64 numRead = device->read(buffer.data(), buffer.size());
        if (numRead <= 0)
            return hash.result();
        hash.addData(buffer.constData(), numRead);
    }
    return QByteArray(); // never reached
}

QByteArray KDUpdater::calculateHash(const QString &path, QCryptographicHash::Algorithm algo)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return QByteArray();
    return calculateHash(&file, algo);
}


// -- HashVerificationJob

class HashVerificationJob::Private
{
public:
    Private()
        : hash(QCryptographicHash::Sha1)
        , error(HashVerificationJob::ReadError)
        , timerId(-1) { }

    QPointer<QIODevice> device;
    QByteArray sha1Sum;
    QCryptographicHash hash;
    HashVerificationJob::Error error;
    int timerId;
};

HashVerificationJob::HashVerificationJob(QObject* parent)
    : QObject(parent)
    , d(new Private)
{
}

HashVerificationJob::~HashVerificationJob()
{
    delete d;
}

void HashVerificationJob::setDevice(QIODevice* dev)
{
    d->device = dev;
}

void HashVerificationJob::setSha1Sum(const QByteArray &sum)
{
    d->sha1Sum = sum;
}

int HashVerificationJob::error() const
{
    return d->error;
}

bool HashVerificationJob::hasError() const
{
    return d->error != NoError;
}

void HashVerificationJob::start()
{
    Q_ASSERT(d->device);
    d->timerId = startTimer(0);
}

void HashVerificationJob::emitFinished()
{
    emit finished(this);
    deleteLater();
}

void HashVerificationJob::timerEvent(QTimerEvent*)
{
    Q_ASSERT(d->timerId >= 0);
    if (d->sha1Sum.isEmpty()) {
        killTimer(d->timerId);
        d->timerId = -1;
        d->error = NoError;
        d->device->close();
        emitFinished();
        return;
    }

    QByteArray buf;
    buf.resize(128 * 1024);
    const qint64 read = d->device->read(buf.data(), buf.size());
    if (read > 0) {
        d->hash.addData(buf.constData(), read);
        return;
    }

    d->error = d->hash.result() == d->sha1Sum ? NoError : SumsDifferError;
    killTimer(d->timerId);
    d->timerId = -1;
    emitFinished();
}


// -- KDUpdater::FileDownloader

/*!
    \internal
    \ingroup kdupdater
    \class KDUpdater::FileDownloader kdupdaterfiledownloader.h

    Base class for file downloaders used in KDUpdater. File downloaders are used by
    the KDUpdater::Update class to download update files. Each subclass of FileDownloader
    can download file from a specific category of sources (e.g. local, ftp, http etc).

    This is an internal class, not a part of the public API. Currently we have three
    subclasses of FileDownloader
    \li LocalFileDownloader - downloads from the local file system
    \li FtpDownloader - downloads from a FTP site
    \li HttpDownloader - downloads from a HTTP site

    Usage

    \code
    KDUpdater::FileDownloader* downloader = new KDUpdater::(some subclass name)

    downloader->setUrl(url);
    downloader->download();

    // wait for downloadCompleted() signal

    QString downloadedFile = downloader->downloadedFileName();
    \endcode
*/

struct KDUpdater::FileDownloader::Private
{
    Private()
        : autoRemove(true)
        , m_speedTimerInterval(100)
        , m_bytesReceived(0)
        , m_bytesToReceive(0)
        , m_currentSpeedBin(0)
        , m_sampleIndex(0)
        , m_downloadSpeed(0)
        , m_factory(0)
    {
        memset(m_samples, 0, sizeof(m_samples));
    }

    ~Private()
    {
        delete m_factory;
    }

    QUrl url;
    QString scheme;
    QByteArray sha1Sum;
    QString errorString;
    bool autoRemove;
    bool followRedirect;

    QBasicTimer m_timer;
    int m_speedTimerInterval;

    qint64 m_bytesReceived;
    qint64 m_bytesToReceive;

    mutable qint64 m_samples[50];
    mutable qint64 m_currentSpeedBin;
    mutable quint32 m_sampleIndex;
    mutable qint64 m_downloadSpeed;

    QAuthenticator m_authenticator;
    FileDownloaderProxyFactory *m_factory;
};

KDUpdater::FileDownloader::FileDownloader(const QString &scheme, QObject *parent)
    : QObject(parent)
    , d(new Private)
{
    d->scheme = scheme;
    d->followRedirect = false;
}

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

void KDUpdater::FileDownloader::setSha1Sum(const QByteArray &sum)
{
    d->sha1Sum = sum;
}

QByteArray KDUpdater::FileDownloader::sha1Sum() const
{
    return d->sha1Sum;
}

QString FileDownloader::errorString() const
{
    return d->errorString;
}

void FileDownloader::setDownloadAborted(const QString &error)
{
    d->errorString = error;
    emit downloadStatus(error);
    emit downloadAborted(error);
}

void KDUpdater::FileDownloader::setDownloadCompleted(const QString &path)
{
    HashVerificationJob *job = new HashVerificationJob;
    QFile *file = new QFile(path, job);
    if (!file->open(QIODevice::ReadOnly)) {
        emit downloadProgress(1);
        onError();
        setDownloadAborted(tr("Could not reopen downloaded file %1 for reading: %2").arg(path,
            file->errorString()));
        delete job;
        return;
    }

    job->setDevice(file);
    job->setSha1Sum(d->sha1Sum);
    connect(job, SIGNAL(finished(KDUpdater::HashVerificationJob*)), this,
        SLOT(sha1SumVerified(KDUpdater::HashVerificationJob*)));
    job->start();
}

void KDUpdater::FileDownloader::setDownloadCanceled()
{
    emit downloadCanceled();
    emit downloadStatus(tr("Download canceled."));
}

void KDUpdater::FileDownloader::sha1SumVerified(KDUpdater::HashVerificationJob *job)
{
    if (job->hasError()) {
        onError();
        setDownloadAborted(tr("Cryptographic hashes do not match."));
    } else {
        onSuccess();
        emit downloadCompleted();
        emit downloadStatus(tr("Download finished."));
    }
}

QString KDUpdater::FileDownloader::scheme() const
{
    return d->scheme;
}

void KDUpdater::FileDownloader::setAutoRemoveDownloadedFile(bool val)
{
    d->autoRemove = val;
}

void KDUpdater::FileDownloader::setFollowRedirects(bool val)
{
    d->followRedirect = val;
}

bool KDUpdater::FileDownloader::followRedirects() const
{
    return d->followRedirect;
}

bool KDUpdater::FileDownloader::isAutoRemoveDownloadedFile() const
{
    return d->autoRemove;
}

void KDUpdater::FileDownloader::download()
{
    QMetaObject::invokeMethod(this, "doDownload", Qt::QueuedConnection);
}

void KDUpdater::FileDownloader::cancelDownload()
{
    // Do nothing
}

void KDUpdater::FileDownloader::runDownloadSpeedTimer()
{
    if (!d->m_timer.isActive())
        d->m_timer.start(d->m_speedTimerInterval, this);
}

void KDUpdater::FileDownloader::stopDownloadSpeedTimer()
{
    d->m_timer.stop();
}

void KDUpdater::FileDownloader::addSample(qint64 sample)
{
    d->m_currentSpeedBin += sample;
}

int KDUpdater::FileDownloader::downloadSpeedTimerId() const
{
    return d->m_timer.timerId();
}

void KDUpdater::FileDownloader::setProgress(qint64 bytesReceived, qint64 bytesToReceive)
{
    d->m_bytesReceived = bytesReceived;
    d->m_bytesToReceive = bytesToReceive;
}

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

void KDUpdater::FileDownloader::emitDownloadStatus()
{
    QString status;
    if (d->m_bytesToReceive > 0) {
        QString bytesReceived = format(d->m_bytesReceived);
        const QString bytesToReceive = format(d->m_bytesToReceive);

        const QString tmp = bytesToReceive.mid(bytesToReceive.indexOf(QLatin1Char(' ')));
        if (bytesReceived.endsWith(tmp))
            bytesReceived.chop(tmp.length());

        status = bytesReceived + tr(" of ") + bytesToReceive;
    } else {
        if (d->m_bytesReceived > 0)
            status = format(d->m_bytesReceived) + tr(" downloaded.");
    }

    status += QLatin1String(" (") + format(d->m_downloadSpeed) + tr("/sec") + QLatin1Char(')');
    if (d->m_bytesToReceive > 0 && d->m_downloadSpeed > 0) {
        const qint64 time = (d->m_bytesToReceive - d->m_bytesReceived) / d->m_downloadSpeed;

        int s = time % 60;
        const int d = time / 86400;
        const int h = (time / 3600) - (d * 24);
        const int m = (time / 60) - (d * 1440) - (h * 60);

        QString days;
        if (d > 0)
            days = QString::number(d) + (d < 2 ? tr(" day") : tr(" days")) + QLatin1String(", ");

        QString hours;
        if (h > 0)
            hours = QString::number(h) + (h < 2 ? tr(" hour") : tr(" hours")) + QLatin1String(", ");

        QString minutes;
        if (m > 0)
            minutes = QString::number(m) + (m < 2 ? tr(" minute") : tr(" minutes"));

        QString seconds;
        if (s >= 0 && minutes.isEmpty()) {
            s = (s <= 0 ? 1 : s);
            seconds = QString::number(s) + (s < 2 ? tr(" second") : tr(" seconds"));
        }
        status += tr(" - ") + days + hours + minutes + seconds + tr(" remaining.");
    } else {
        status += tr(" - unknown time remaining.");
    }

    emit downloadStatus(status);
}

void KDUpdater::FileDownloader::emitDownloadProgress()
{
    emit downloadProgress(d->m_bytesReceived, d->m_bytesToReceive);
}

void KDUpdater::FileDownloader::emitEstimatedDownloadTime()
{
    if (d->m_bytesToReceive <= 0 || d->m_downloadSpeed <= 0) {
        emit estimatedDownloadTime(-1);
        return;
    }
    emit estimatedDownloadTime((d->m_bytesToReceive - d->m_bytesReceived) / d->m_downloadSpeed);
}

/*!
    Returns a copy of the proxy factory that this FileDownloader object is using to determine the proxies to
    be used for requests.
*/
FileDownloaderProxyFactory *KDUpdater::FileDownloader::proxyFactory() const
{
    if (d->m_factory)
        return d->m_factory->clone();
    return 0;
}

/*!
    Sets the proxy factory for this class to be \a factory. A proxy factory is used to determine a more
    specific list of proxies to be used for a given request, instead of trying to use the same proxy value
    for all requests. This might only be of use for http or ftp requests.
*/
void KDUpdater::FileDownloader::setProxyFactory(FileDownloaderProxyFactory *factory)
{
    delete d->m_factory;
    d->m_factory = factory;
}

/*!
    Returns a copy of the authenticator that this FileDownloader object is using to set the username and
    password for download request.
*/
QAuthenticator KDUpdater::FileDownloader::authenticator() const
{
    return d->m_authenticator;
}

/*!
    Sets the authenticator object for this class to be \a authenticator. A authenticator is used to
    pass on the required authentication information. This might only be of use for http or ftp requests.
*/
void KDUpdater::FileDownloader::setAuthenticator(const QAuthenticator &authenticator)
{
    d->m_authenticator = authenticator;
}

// -- KDUpdater::LocalFileDownloader

/*
      Even though QFile::copy() does the task of copying local files from one place
      to another, I prefer to use the timer and copy one block of data per unit time.

      This is because, it is possible that the user of KDUpdater is simultaneously
      downloading several files. Sometimes in tandem with other file downloaders.
      If the local file that is being downloaded takes a long time; then that will
      hang the other downloads.

      On the other hand, local downloads need not actually download the file. It can
      simply pass on the source file as destination file. At this moment however,
      I think the user of LocalFileDownloader will assume that the downloaded file
      can be fiddled around with without worrying about whether it would mess up
      the original source or not.
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

KDUpdater::LocalFileDownloader::LocalFileDownloader(QObject *parent)
    : KDUpdater::FileDownloader(QLatin1String("file"), parent)
    , d (new Private)
{
}

KDUpdater::LocalFileDownloader::~LocalFileDownloader()
{
    if (this->isAutoRemoveDownloadedFile() && !d->destFileName.isEmpty())
        QFile::remove(d->destFileName);

    delete d;
}

bool KDUpdater::LocalFileDownloader::canDownload() const
{
    QFileInfo fi(url().toLocalFile());
    return fi.exists() && fi.isReadable();
}

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
        onError();
        setDownloadAborted(tr("Cannot open source file '%1' for reading.").arg(QFileInfo(localFile)
            .fileName()));
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
        onError();
        setDownloadAborted(tr("Cannot open destination file '%1' for writing.")
            .arg(QFileInfo(d->destination->fileName()).fileName()));
        return;
    }

    runDownloadSpeedTimer();
    // Start a timer and kickoff the copy process
    d->timerId = startTimer(0); // as fast as possible

    emit downloadStarted();
    emit downloadProgress(0);
}

QString KDUpdater::LocalFileDownloader::downloadedFileName() const
{
    return d->destFileName;
}

void KDUpdater::LocalFileDownloader::setDownloadedFileName(const QString &name)
{
    d->destFileName = name;
}

KDUpdater::LocalFileDownloader *KDUpdater::LocalFileDownloader::clone(QObject *parent) const
{
    return new LocalFileDownloader(parent);
}

void KDUpdater::LocalFileDownloader::cancelDownload()
{
    if (d->timerId < 0)
        return;

    killTimer(d->timerId);
    d->timerId = -1;

    onError();
    setDownloadCanceled();
}

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
                onError();
                setDownloadAborted(tr("Writing to %1 failed: %2").arg(d->destination->fileName(),
                    d->destination->errorString()));
                return;
            }
            toWrite -= numWritten;
        }
        addSample(numRead);

        if (numRead > 0) {
            setProgress(d->source->pos(), d->source->size());
            emit downloadProgress(calcProgress(d->source->pos(), d->source->size()));
            return;
        }

        d->destination->flush();

        killTimer(d->timerId);
        d->timerId = -1;

        setDownloadCompleted(d->destination->fileName());
    } else if (event->timerId() == downloadSpeedTimerId()) {
        emitDownloadSpeed();
        emitDownloadStatus();
        emitDownloadProgress();
        emitEstimatedDownloadTime();
    }
}

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

struct KDUpdater::ResourceFileDownloader::Private
{
    Private()
        : downloaded(false),
          timerId(-1)
    {}

    QString destFileName;
    bool downloaded;
    int timerId;
};

KDUpdater::ResourceFileDownloader::ResourceFileDownloader(QObject *parent)
    : KDUpdater::FileDownloader(QLatin1String("resource"), parent)
    , d(new Private)
{
}

KDUpdater::ResourceFileDownloader::~ResourceFileDownloader()
{
    delete d;
}

bool KDUpdater::ResourceFileDownloader::canDownload() const
{
    QUrl url = this->url();
    url.setScheme(QString::fromLatin1("file"));
    QString localFile = QString::fromLatin1(":%1").arg(url.toLocalFile());
    QFileInfo fi(localFile);
    return fi.exists() && fi.isReadable();
}

bool KDUpdater::ResourceFileDownloader::isDownloaded() const
{
    return d->downloaded;
}

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
    d->destFileName = QString::fromLatin1(":%1").arg(url.toLocalFile());

    // Start a timer and kickoff the copy process
    d->timerId = startTimer(0); // as fast as possible
    emit downloadStarted();
    emit downloadProgress(0);
}

QString KDUpdater::ResourceFileDownloader::downloadedFileName() const
{
    return d->destFileName;
}

void KDUpdater::ResourceFileDownloader::setDownloadedFileName(const QString &/*name*/)
{
    Q_ASSERT_X(false, "KDUpdater::ResourceFileDownloader::setDownloadedFileName", "Not supported!");
}

KDUpdater::ResourceFileDownloader *KDUpdater::ResourceFileDownloader::clone(QObject *parent) const
{
    return new ResourceFileDownloader(parent);
}

void KDUpdater::ResourceFileDownloader::cancelDownload()
{
    if (d->timerId < 0)
        return;

    killTimer(d->timerId);
    d->timerId = -1;

    setDownloadCanceled();
}

void KDUpdater::ResourceFileDownloader::timerEvent(QTimerEvent *)
{
    killTimer(d->timerId);
    d->timerId = -1;
    setDownloadCompleted(d->destFileName);
}

void KDUpdater::ResourceFileDownloader::onSuccess()
{
    d->downloaded = true;
}

void KDUpdater::ResourceFileDownloader::onError()
{
    d->downloaded = false;
}


// -- KDUpdater::FtpFileDownloader

struct KDUpdater::FtpDownloader::Private
{
    Private()
        : ftp(0)
        , destination(0)
        , downloaded(false)
        , ftpCmdId(-1)
        , aborted(false)
    {}

    QFtp *ftp;
    QFile *destination;
    QString destFileName;
    bool downloaded;
    int ftpCmdId;
    bool aborted;
};

KDUpdater::FtpDownloader::FtpDownloader(QObject *parent)
    : KDUpdater::FileDownloader(QLatin1String("ftp"), parent)
    , d(new Private)
{
}

KDUpdater::FtpDownloader::~FtpDownloader()
{
    if (this->isAutoRemoveDownloadedFile() && !d->destFileName.isEmpty())
        QFile::remove(d->destFileName);

    delete d;
}

bool KDUpdater::FtpDownloader::canDownload() const
{
    // TODO: Check whether the ftp file actually exists or not.
    return true;
}

bool KDUpdater::FtpDownloader::isDownloaded() const
{
    return d->downloaded;
}

void KDUpdater::FtpDownloader::doDownload()
{
    if (d->downloaded)
        return;

    if (d->ftp)
        return;

    d->ftp = new QFtp(this);
    connect(d->ftp, SIGNAL(done(bool)), this, SLOT(ftpDone(bool)));
    connect(d->ftp, SIGNAL(commandStarted(int)), this, SLOT(ftpCmdStarted(int)));
    connect(d->ftp, SIGNAL(commandFinished(int, bool)), this, SLOT(ftpCmdFinished(int, bool)));
    connect(d->ftp, SIGNAL(stateChanged(int)), this, SLOT(ftpStateChanged(int)));
    connect(d->ftp, SIGNAL(dataTransferProgress(qint64, qint64)), this,
            SLOT(ftpDataTransferProgress(qint64, qint64)));
    connect(d->ftp, SIGNAL(readyRead()), this, SLOT(ftpReadyRead()));

    if (FileDownloaderProxyFactory *factory = proxyFactory()) {
        const QList<QNetworkProxy> proxies = factory->queryProxy(QNetworkProxyQuery(url()));
        if (!proxies.isEmpty())
            d->ftp->setProxy(proxies.at(0).hostName(), proxies.at(0).port());
        delete factory;
    }

    d->ftp->connectToHost(url().host(), url().port(21));
    d->ftp->login(authenticator().user(), authenticator().password());
}

QString KDUpdater::FtpDownloader::downloadedFileName() const
{
    return d->destFileName;
}

void KDUpdater::FtpDownloader::setDownloadedFileName(const QString &name)
{
    d->destFileName = name;
}

KDUpdater::FtpDownloader *KDUpdater::FtpDownloader::clone(QObject *parent) const
{
    return new FtpDownloader(parent);
}

void KDUpdater::FtpDownloader::cancelDownload()
{
    if (d->ftp) {
        d->aborted = true;
        d->ftp->abort();
    }
}

void KDUpdater::FtpDownloader::ftpDone(bool error)
{
    if (error) {
        QString errorString;
        if (d->ftp) {
            errorString = d->ftp->errorString();
            d->ftp->deleteLater();
            d->ftp = 0;
            d->ftpCmdId = -1;
        }

        onError();

        if (d->aborted) {
            d->aborted = false;
            setDownloadCanceled();
        } else {
            setDownloadAborted(errorString);
        }
    }
    //PENDING what about the non-error case??
}

void KDUpdater::FtpDownloader::ftpCmdStarted(int id)
{
    if (id != d->ftpCmdId)
        return;

    emit downloadStarted();
    emit downloadProgress(0);
}

void KDUpdater::FtpDownloader::ftpCmdFinished(int id, bool error)
{
    if (id != d->ftpCmdId || error) // PENDING why error -> return??
        return;

    disconnect(d->ftp, 0, this, 0);
    d->ftp->deleteLater();
    d->ftp = 0;
    d->ftpCmdId = -1;
    d->destination->flush();

    setDownloadCompleted(d->destination->fileName());
}

void FtpDownloader::onSuccess()
{
    d->downloaded = true;
    d->destFileName = d->destination->fileName();
    if (QTemporaryFile *file = dynamic_cast<QTemporaryFile *>(d->destination))
        file->setAutoRemove(false);
    delete d->destination;
    d->destination = 0;
    stopDownloadSpeedTimer();

}

void FtpDownloader::onError()
{
    d->downloaded = false;
    d->destFileName.clear();
    delete d->destination;
    d->destination = 0;
    stopDownloadSpeedTimer();
}

void KDUpdater::FtpDownloader::ftpStateChanged(int state)
{
    switch(state) {
        case QFtp::Connected: {
            // begin the download
            if (d->destFileName.isEmpty()) {
                QTemporaryFile *file = new QTemporaryFile(this);
                file->open(); //PENDING handle error
                d->destination = file;
            } else {
                d->destination = new QFile(d->destFileName, this);
                d->destination->open(QIODevice::ReadWrite | QIODevice::Truncate);
            }
            runDownloadSpeedTimer();
            d->ftpCmdId = d->ftp->get(url().path());
        }   break;

        case QFtp::Unconnected: {
            // download was unconditionally aborted
            disconnect(d->ftp, 0, this, 0);
            d->ftp->deleteLater();
            d->ftp = 0;
            d->ftpCmdId = -1;
            onError();
            setDownloadAborted(tr("Download was aborted due to network errors."));
        }   break;
    }
}

void KDUpdater::FtpDownloader::ftpDataTransferProgress(qint64 done, qint64 total)
{
    setProgress(done, total);
    emit downloadProgress(calcProgress(done, total));
}

void KDUpdater::FtpDownloader::ftpReadyRead()
{
    static QByteArray buffer(16384, '\0');
    while (d->ftp->bytesAvailable()) {
        const qint64 read = d->ftp->read(buffer.data(), buffer.size());
        qint64 written = 0;
        while (written < read) {
            const qint64 numWritten = d->destination->write(buffer.data() + written, read - written);
            if (numWritten < 0) {
                onError();
                setDownloadAborted(tr("Cannot download %1: Writing to temporary file failed: %2")
                    .arg(url().toString(), d->destination->errorString()));
                return;
            }
            written += numWritten;
        }
        addSample(written);
    }
}

void KDUpdater::FtpDownloader::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == downloadSpeedTimerId()) {
        emitDownloadSpeed();
        emitDownloadStatus();
        emitDownloadProgress();
        emitEstimatedDownloadTime();
    }
}


// -- KDUpdater::HttpDownloader

struct KDUpdater::HttpDownloader::Private
{
    explicit Private(HttpDownloader *qq)
        : q(qq)
        , http(0)
        , destination(0)
        , downloaded(false)
        , aborted(false)
        , retrying(false)
        , m_authenticationDone(false)
    {}

    HttpDownloader *const q;
    QNetworkAccessManager manager;
    QNetworkReply *http;
    QFile *destination;
    QString destFileName;
    bool downloaded;
    bool aborted;
    bool retrying;
    bool m_authenticationDone;

    void shutDown()
    {
        disconnect(http, SIGNAL(finished()), q, SLOT(httpReqFinished()));
        http->deleteLater();
        http = 0;
        destination->close();
        destination->deleteLater();
        destination = 0;
    }
};

KDUpdater::HttpDownloader::HttpDownloader(QObject *parent)
    : KDUpdater::FileDownloader(QLatin1String("http"), parent)
    , d(new Private(this))
{
    connect(&d->manager, SIGNAL(authenticationRequired(QNetworkReply*, QAuthenticator*)), this,
        SLOT(onAuthenticationRequired(QNetworkReply*, QAuthenticator*)));
}

KDUpdater::HttpDownloader::~HttpDownloader()
{
    if (this->isAutoRemoveDownloadedFile() && !d->destFileName.isEmpty())
        QFile::remove(d->destFileName);
    delete d;
}

bool KDUpdater::HttpDownloader::canDownload() const
{
    // TODO: Check whether the http file actually exists or not.
    return true;
}

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
}

QString KDUpdater::HttpDownloader::downloadedFileName() const
{
    return d->destFileName;
}

void KDUpdater::HttpDownloader::setDownloadedFileName(const QString &name)
{
    d->destFileName = name;
}

KDUpdater::HttpDownloader *KDUpdater::HttpDownloader::clone(QObject *parent) const
{
    return new HttpDownloader(parent);
}

void KDUpdater::HttpDownloader::httpReadyRead()
{
    static QByteArray buffer(16384, '\0');
    while (d->http->bytesAvailable()) {
        const qint64 read = d->http->read(buffer.data(), buffer.size());
        qint64 written = 0;
        while (written < read) {
            const qint64 numWritten = d->destination->write(buffer.data() + written, read - written);
            if (numWritten < 0) {
                const QString err = d->destination->errorString();
                d->shutDown();
                setDownloadAborted(tr("Cannot download %1: Writing to temporary file failed: %2")
                    .arg(url().toString(), err));
                return;
            }
            written += numWritten;
        }
        addSample(written);
    }
}

void KDUpdater::HttpDownloader::httpError(QNetworkReply::NetworkError)
{
    if (!d->aborted)
        httpDone(true);
}

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
        }
    }
    //PENDING: what about the non-error case??
}

void KDUpdater::HttpDownloader::onError()
{
    d->downloaded = false;
    d->destFileName.clear();
    delete d->destination;
    d->destination = 0;
    stopDownloadSpeedTimer();
}

void KDUpdater::HttpDownloader::onSuccess()
{
    d->downloaded = true;
    d->destFileName = d->destination->fileName();
    if (QTemporaryFile *file = dynamic_cast<QTemporaryFile *>(d->destination))
        file->setAutoRemove(false);
    delete d->destination;
    d->destination = 0;
    stopDownloadSpeedTimer();
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

        httpReadyRead();
        d->destination->flush();
        setDownloadCompleted(d->destination->fileName());
        d->http->deleteLater();
        d->http = 0;
    }
}

void KDUpdater::HttpDownloader::httpReadProgress(qint64 done, qint64 total)
{
    setProgress(done, total);
    emit downloadProgress(calcProgress(done, total));
}

void KDUpdater::HttpDownloader::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == downloadSpeedTimerId()) {
        emitDownloadSpeed();
        emitDownloadStatus();
        emitDownloadProgress();
        emitEstimatedDownloadTime();
    }
}

void KDUpdater::HttpDownloader::startDownload(const QUrl &url)
{
    d->m_authenticationDone = false;
    d->manager.setProxyFactory(proxyFactory());
    d->http = d->manager.get(QNetworkRequest(url));

    connect(d->http, SIGNAL(readyRead()), this, SLOT(httpReadyRead()));
    connect(d->http, SIGNAL(downloadProgress(qint64, qint64)), this,
        SLOT(httpReadProgress(qint64, qint64)));
    connect(d->http, SIGNAL(finished()), this, SLOT(httpReqFinished()));
    connect(d->http, SIGNAL(error(QNetworkReply::NetworkError)), this,
        SLOT(httpError(QNetworkReply::NetworkError)));

    if (d->destFileName.isEmpty()) {
        QTemporaryFile *file = new QTemporaryFile(this);
        file->open();
        d->destination = file;
    } else {
        d->destination = new QFile(d->destFileName, this);
        d->destination->open(QIODevice::ReadWrite | QIODevice::Truncate);
    }

    if (!d->destination->isOpen()) {
        d->shutDown();
        setDownloadAborted(tr("Cannot download %1: Could not create temporary file: %2").arg(url.toString(),
            d->destination->errorString()));
    }
}

void KDUpdater::HttpDownloader::onAuthenticationRequired(QNetworkReply *reply, QAuthenticator *authenticator)
{
    qDebug() << reply->readAll();
    if (!d->m_authenticationDone) {
        d->m_authenticationDone = true;
        authenticator->setUser(this->authenticator().user());
        authenticator->setPassword(this->authenticator().password());
    }
}
