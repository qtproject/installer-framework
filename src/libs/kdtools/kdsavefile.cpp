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

#include "kdsavefile.h"

#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QPointer>
#include <QtCore/QTemporaryFile>

#ifdef Q_OS_WIN
# include <io.h>
# include <share.h> // Required for _SH_DENYRW
#endif
#include <memory>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#ifdef Q_OS_LINUX
#include <unistd.h>
#endif

static int permissionsToMode(QFile::Permissions p, bool *ok)
{
    Q_ASSERT(ok);
    int m = 0;
#ifdef Q_OS_WIN
    //following qfsfileengine_win.cpp: QFSFileEngine::setPermissions()
    if (p & QFile::ReadOwner || p & QFile::ReadUser || p & QFile::ReadGroup || p & QFile::ReadOther)
        m |= _S_IREAD;
    if (p & QFile::WriteOwner || p & QFile::WriteUser || p & QFile::WriteGroup || p & QFile::WriteOther)
        m |= _S_IWRITE;
    *ok = m != 0;
#else
    if (p & QFile::ReadUser)
        m |= S_IRUSR;
    if (p & QFile::WriteUser)
        m |= S_IWUSR;
    if (p & QFile::ExeUser)
        m |= S_IXUSR;
    if (p & QFile::ReadGroup)
        m |= S_IRGRP;
    if (p & QFile::WriteGroup)
        m |= S_IWGRP;
    if (p & QFile::ExeGroup)
        m |= S_IXGRP;
    if (p & QFile::ReadOther)
        m |= S_IROTH;
    if (p & QFile::WriteOther)
        m |= S_IWOTH;
    if (p & QFile::ExeOther)
        m |= S_IXOTH;
    *ok = true;
#endif
    return m;
}

static bool sync(int fd)
{
#ifdef Q_OS_WIN
    return _commit(fd) == 0;
#else
    return fsync(fd) == 0;
#endif
}

static QString makeAbsolute(const QString &path)
{
    if (QDir::isAbsolutePath(path))
        return path;
    return QDir::currentPath() + QLatin1String("/") + path;
}

static int myOpen(const QString &path, int flags, int mode)
{
#ifdef Q_OS_WIN
     int fd;
#ifdef Q_CC_MINGW
    fd = _wsopen(reinterpret_cast<const wchar_t *>(path.utf16()), flags, _SH_DENYRW, mode);
#else
     _wsopen_s(&fd, reinterpret_cast<const wchar_t *>(path.utf16()), flags, _SH_DENYRW, mode);
#endif
     return fd;
#else
    return open(QFile::encodeName(path).constData(), flags, mode);
#endif
}

static void myClose(int fd)
{
#ifdef Q_OS_WIN
    _close(fd);
#else
    close(fd);
#endif
}

static bool touchFile(const QString &path, QFile::Permissions p)
{
    bool ok;
    const int mode = permissionsToMode(p, &ok);
    if (!ok)
        return false;
    const int fd = myOpen(QDir::toNativeSeparators(path), O_WRONLY|O_CREAT, mode);
    if (fd < 0) {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly))
            return false;
        if (!file.setPermissions(p)) {
            QFile::remove(path);
            return false;
        }
        return true;
    }
    sync(fd);
    myClose(fd);
    return true;
}

static QFile *createFile(const QString &path, QIODevice::OpenMode m, QFile::Permissions p, bool *openOk)
{
    Q_ASSERT(openOk);
    if (!touchFile(path, p))
        return 0;
    std::auto_ptr<QFile> file(new QFile(path));
    *openOk = file->open(m | QIODevice::Append);
    if (!*openOk)
        QFile::remove(path); // try to remove empty file
    return file.release();
}

/*!
 Generates a temporary file name based on template \a path
 \internal
 */
static QString generateTempFileName(const QString &path)
{
    const QString tmp = path + QLatin1String("tmp.dsfdf.%1"); //TODO: use random suffix
    int count = 1;
    while (QFile::exists(tmp.arg(count)))
        ++count;
    return tmp.arg(count);
}

/*!
  \class KDSaveFile KDSaveFile
  \ingroup core
  \brief Secure and robust writing to a file

*/

class KDSaveFile::Private
{
    KDSaveFile *const q;
public:
    explicit Private(const QString &fname, KDSaveFile *qq)
        : q(qq),
#ifdef Q_OS_WIN
        backupExtension(QLatin1String(".bak")),
#else
        backupExtension(QLatin1String("~")),
#endif
        permissions(QFile::ReadUser|QFile::WriteUser), filename(fname), tmpFile()
    {
        //TODO respect umask instead of hardcoded default permissions
    }

    ~Private()
    {
        deleteTempFile();
    }

    bool deleteTempFile()
    {
        if (!tmpFile)
            return true;
        const QString name = tmpFile->fileName();
        delete tmpFile;
        //force a real close by deleting the object, before deleting the actual file. Needed on Windows
        QFile tmp(name);
        return tmp.remove();
    }

    bool recreateTemporaryFile(QIODevice::OpenMode mode)
    {
        deleteTempFile();
        bool ok;
        tmpFile = createFile(generateTempFileName( filename ), mode, permissions, &ok);
        QObject::connect(tmpFile, SIGNAL(aboutToClose()), q, SIGNAL(aboutToClose()));
        QObject::connect(tmpFile, SIGNAL(bytesWritten(qint64)), q, SIGNAL(bytesWritten(qint64)));
        QObject::connect(tmpFile, SIGNAL(readChannelFinished()), q, SIGNAL(readChannelFinished()));
        QObject::connect(tmpFile, SIGNAL(readyRead()), q, SIGNAL(readyRead()));
        return ok;
    }

    QString generateBackupName() const
    {
        const QString bf = filename + backupExtension;
        if (!QFile::exists(bf))
            return bf;
        int count = 1;
        while (QFile::exists(bf + QString::number(count)))
            ++count;
        return bf + QString::number(count);
    }

    void propagateErrors()
    {
        if (!tmpFile)
            return;
        q->setErrorString(tmpFile->errorString());
    }

    QString backupExtension;
    QFile::Permissions permissions;
    QString filename;
    QPointer<QFile> tmpFile;
};


KDSaveFile::KDSaveFile(const QString &filename, QObject *parent)
    : QIODevice(parent), d( new Private(makeAbsolute(filename), this))
{}

KDSaveFile::~KDSaveFile()
{
    delete d;
}

void KDSaveFile::close()
{
    d->deleteTempFile();
    QIODevice::close();
}

bool KDSaveFile::open(OpenMode mode)
{
    setOpenMode(QIODevice::NotOpen);
    if (mode & QIODevice::Append) {
        setErrorString(tr("Append mode not supported."));
        return false;
    }

    if ((mode & QIODevice::WriteOnly) == 0){
        setErrorString(tr("Read-only access not supported."));
        return false;
    }

    // for some reason this seems to be problematic... let's remove it for now, will break in commit anyhow, if it's serious
    //if ( QFile::exists( d->filename ) && !QFileInfo( d->filename ).isWritable() ) {
    //    setErrorString( tr("The target file %1 exists and is not writable").arg( d->filename ) );
    //    return false;
    //}
    const bool opened = d->recreateTemporaryFile(mode);
    if (opened) {
        setOpenMode(mode);
    }

    //if target file already exists, apply permissions of existing file to temp file
    return opened;
}

bool KDSaveFile::atEnd()
{
    return d->tmpFile ? d->tmpFile->atEnd() : QIODevice::atEnd();
}

qint64 KDSaveFile::bytesAvailable() const
{
    return d->tmpFile ? d->tmpFile->bytesAvailable() : QIODevice::bytesAvailable();
}

qint64 KDSaveFile::bytesToWrite() const
{
    return d->tmpFile ? d->tmpFile->bytesToWrite() : QIODevice::bytesToWrite();
}

bool KDSaveFile::canReadLine() const
{
    return d->tmpFile ? d->tmpFile->canReadLine() : QIODevice::canReadLine();
}

bool KDSaveFile::isSequential() const
{
    return d->tmpFile ? d->tmpFile->isSequential() : QIODevice::isSequential();
}

qint64 KDSaveFile::pos() const
{
    return d->tmpFile ? d->tmpFile->pos() : QIODevice::pos();
}

bool KDSaveFile::reset()
{
    return d->tmpFile ? d->tmpFile->reset() : QIODevice::reset();
}

bool KDSaveFile::seek(qint64 pos)
{
    const bool ret = d->tmpFile ? d->tmpFile->seek(pos) : QIODevice::seek(pos);
    if (!ret)
        d->propagateErrors();
    return ret;
}

qint64 KDSaveFile::size() const
{
    return d->tmpFile ? d->tmpFile->size() : QIODevice::size();
}

bool KDSaveFile::waitForBytesWritten(int msecs)
{
    return d->tmpFile ? d->tmpFile->waitForBytesWritten(msecs) : QIODevice::waitForBytesWritten(msecs);
}

bool KDSaveFile::waitForReadyRead(int msecs)
{
    return d->tmpFile ? d->tmpFile->waitForReadyRead(msecs) : QIODevice::waitForReadyRead(msecs);
}

bool KDSaveFile::commit(KDSaveFile::CommitMode mode)
{
    if (!d->tmpFile)
        return false;
    const QString tmpfname = d->tmpFile->fileName();
    d->tmpFile->flush();
    delete d->tmpFile;
    QFile orig(d->filename);
    QString backup;
    if (orig.exists()) {
        backup = d->generateBackupName();
        if (!orig.rename(backup)) {
            setErrorString(tr("Could not backup existing file %1: %2").arg( d->filename, orig.errorString()));
            QFile tmp(tmpfname);
            if (!tmp.remove()) // TODO how to report this error?
                qWarning() << "Could not remove temp file" << tmpfname << tmp.errorString();
            if (mode != OverwriteExistingFile)
                return false;
        }
    }
    QFile target(tmpfname);
    if (!target.rename(d->filename)) {
        setErrorString(target.errorString());
        return false;
    }
    if (mode == OverwriteExistingFile) {
        QFile tmp(backup);
        const bool removed = !tmp.exists() || tmp.remove(backup);
        if (!removed)
            qWarning() << "Could not remove the backup: " << tmp.errorString();
    }

    return true;
}

QString KDSaveFile::fileName() const
{
    return d->filename;
}

void KDSaveFile::setFileName(const QString &filename)
{
    const QString fn = makeAbsolute(filename);
    if (fn == d->filename)
        return;
    close();
    delete d->tmpFile;
    d->filename = fn;
}

qint64 KDSaveFile::readData(char *data, qint64 maxSize)
{
    if (!d->tmpFile) {
        setErrorString(tr("TODO"));
        return -1;
    }
    const qint64 ret = d->tmpFile->read(data, maxSize);
    d->propagateErrors();
    return ret;
}

qint64 KDSaveFile::readLineData(char *data, qint64 maxSize)
{
    if (!d->tmpFile) {
        setErrorString(tr("TODO"));
        return -1;
    }
    const qint64 ret = d->tmpFile->readLine(data, maxSize);
    d->propagateErrors();
    return ret;
}

qint64 KDSaveFile::writeData(const char *data, qint64 maxSize)
{
    if (!d->tmpFile) {
        setErrorString(tr("TODO"));
        return -1;
    }
    const qint64 ret = d->tmpFile->write(data, maxSize);
    d->propagateErrors();
    return ret;
}

bool KDSaveFile::flush()
{
    return d->tmpFile ? d->tmpFile->flush() : false;
}

bool KDSaveFile::resize(qint64 sz)
{
    return d->tmpFile ? d->tmpFile->resize(sz) : false;
}

int KDSaveFile::handle() const
{
    return d->tmpFile ? d->tmpFile->handle() : -1;
}

QFile::Permissions KDSaveFile::permissions() const
{
    return d->tmpFile ? d->tmpFile->permissions() : d->permissions;
}

bool KDSaveFile::setPermissions(QFile::Permissions p)
{
    d->permissions = p;
    if (d->tmpFile)
        return d->tmpFile->setPermissions(p);
    return false;
}

void KDSaveFile::setBackupExtension(const QString &ext)
{
    d->backupExtension = ext;
}

QString KDSaveFile::backupExtension() const
{
    return d->backupExtension;
}

/**
 * TODO
 *
 *
 */

#ifdef KDTOOLSCORE_UNITTESTS

#include <KDUnitTest/Test>

KDAB_UNITTEST_SIMPLE( KDSaveFile, "kdcoretools" ) {
    //TODO test contents (needs blocking and checked write() )
    {
        const QString testfile1 = QLatin1String("kdsavefile-test1");
        QByteArray testData("lalalala");
        KDSaveFile saveFile( testfile1 );
        assertTrue( saveFile.open( QIODevice::WriteOnly ) );
        saveFile.write( testData.constData(), testData.size() );
        assertTrue( saveFile.commit() );
        assertTrue( QFile::exists( testfile1 ) );
        assertTrue( QFile::remove( testfile1 ) );
    }
    {
        const QString testfile1 = QLatin1String("kdsavefile-test1");
        QByteArray testData("lalalala");
        KDSaveFile saveFile( testfile1 );
        assertTrue( saveFile.open( QIODevice::WriteOnly ) );
        saveFile.write( testData.constData(), testData.size() );
        saveFile.close();
        assertFalse( QFile::exists( testfile1 ) );
    }
    {
        const QString testfile1 = QLatin1String("kdsavefile-test1");
        QByteArray testData("lalalala");
        KDSaveFile saveFile( testfile1 );
        assertTrue( saveFile.open( QIODevice::WriteOnly ) );
        saveFile.write( testData.constData(), testData.size() );
        assertTrue( saveFile.commit() );
        assertTrue( QFile::exists( testfile1 ) );

        KDSaveFile sf2( testfile1 );
        sf2.setBackupExtension( QLatin1String(".bak") );
        assertTrue( sf2.open( QIODevice::WriteOnly ) );
        sf2.write( testData.constData(), testData.size() );
        sf2.commit(); //commit in backup mode (default)
        const QString backup = testfile1 + sf2.backupExtension();
        assertTrue( QFile::exists( backup ) );
        assertTrue( QFile::remove( backup ) );

        KDSaveFile sf3( testfile1 );
        sf3.setBackupExtension( QLatin1String(".bak") );
        assertTrue( sf3.open( QIODevice::WriteOnly ) );
        sf3.write( testData.constData(), testData.size() );
        sf3.commit( KDSaveFile::OverwriteExistingFile );
        const QString backup2 = testfile1 + sf3.backupExtension();
        assertFalse( QFile::exists( backup2 ) );

        assertTrue( QFile::remove( testfile1 ) );
    }
    {
        const QString testfile1 = QLatin1String("kdsavefile-test1");
        KDSaveFile sf( testfile1 );
        assertFalse( sf.open( QIODevice::ReadOnly ) );
        assertFalse( sf.open( QIODevice::WriteOnly|QIODevice::Append ) );
        assertTrue( sf.open( QIODevice::ReadWrite ) );
    }
}

#endif // KDTOOLSCORE_UNITTESTS
