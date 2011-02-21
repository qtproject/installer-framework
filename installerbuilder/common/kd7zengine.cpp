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
#include "kd7zengine.h"

#include <QBuffer>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QMap>
#include <QSet>
#include <QTextCodec>

#include "lib7z_facade.h"

static QMap< QString, QFile* > archives;
static QMap< QString, QVector< Lib7z::File > > currentContents;
static QMap< QString, QHash< QString, const Lib7z::File* > > currentFiles;

namespace
{

static bool matches( const QList< QRegExp >& regexps, QDir::Filters filters, const Lib7z::File& n )
{
    const QString fileName = n.path.section( QChar::fromLatin1( '/' ), -1, -1, QString::SectionSkipEmpty );

    static QLatin1Char sep( '/' );

    // check for directories filter
    if( ( filters & QDir::Dirs ) == 0 && n.isDirectory )
        return false;

    // check for files filter
    if( ( filters & QDir::Files ) == 0 && !n.isDirectory )
        return false;

    // check for name filter
    if( !regexps.isEmpty() )
    {
        bool matched = false;
        for( QList< QRegExp >::const_iterator it = regexps.begin(); it != regexps.end(); ++it )
        {
            matched = it->exactMatch( fileName );
            if ( matched )
                break;
        }
        return matched;
    }
    return true;
}
}

class KD7zEngine::Private
{
public:
    Private( KD7zEngine* q )
        : q( q ),
          flags( 0 ),
          mode( QIODevice::NotOpen )
    {
    }

private:
    KD7zEngine* const q;

public:
    void setFileName( const QString& file )
    {
        const QString oldZipFileName = zipFileName;
        providedFileName = file;

        static const QLatin1Char sep( '/' );

        static const QString prefix = QString::fromLatin1( "7z://" );
        Q_ASSERT( file.toLower().startsWith( prefix ) );
        zipFileName = file.mid( prefix.length() );
        zipFileName.replace( QLatin1Char( '\\' ), sep );

        QDir dir( zipFileName );
        int i = -1;
        while( !dir.exists() )
            dir = QDir( zipFileName.section( sep, 0, i-- ) );

        zipFileName = zipFileName.section( sep, 0, i + 2 );
        fileName = file.mid( prefix.length() + zipFileName.length() );

        while( fileName.endsWith( sep ) )
            fileName.chop( 1 );
        while( fileName.startsWith( sep ) )
            fileName.remove( 0, 1 );
    
        flags = 0;
        q->close();
        currentFile = Lib7z::File();
    }

    /**
     * Taken from qfsfileengine.cpp
     */
    static QString canonicalized(const QString &path)
    {
        if (path.isEmpty())
            return path;

        QFileInfo fi;
        const QChar slash(QLatin1Char('/'));
        QString tmpPath = path;
        int separatorPos = 0;
        QSet<QString> nonSymlinks;
        QSet<QString> known;

        known.insert(path);
        do {
#ifdef Q_OS_WIN
            // UNC, skip past the first two elements
            if (separatorPos == 0 && tmpPath.startsWith(QLatin1String("//")))
                separatorPos = tmpPath.indexOf(slash, 2);
            if (separatorPos != -1)
#endif
            separatorPos = tmpPath.indexOf(slash, separatorPos + 1);
            QString prefix = separatorPos == -1 ? tmpPath : tmpPath.left(separatorPos);
            if (!nonSymlinks.contains(prefix)) {
                fi.setFile(prefix);
                if (fi.isSymLink()) {
                    QString target = fi.symLinkTarget();
                    if (separatorPos != -1) {
                        if (fi.isDir() && !target.endsWith(slash))
                            target.append(slash);
                        target.append(tmpPath.mid(separatorPos));
                    }
                    tmpPath = QDir::cleanPath(target);
                    separatorPos = 0;

                    if (known.contains(tmpPath))
                        return QString();
                    known.insert(tmpPath);
                } else {
                    nonSymlinks.insert(prefix);
                }
            }
        } while (separatorPos != -1);

        return QDir::cleanPath(tmpPath);
    }

    QFile* archive() const
    {
        if( archives[ zipFileName ] == 0 )
            archives[ zipFileName ] = new QFile( zipFileName );
        return archives[ zipFileName ];
    }

    const Lib7z::File& file() const
    {
        if( currentFile.path.isEmpty() )
            currentFile = file( fileName );
        return currentFile;
    }

    const QVector< Lib7z::File >& list() const
    {
        if( currentContents[ zipFileName ].isEmpty() )
        {
            QFile* const archive = this->archive();
            try {
                if( !archive->open( QIODevice::ReadOnly ) ) {
                    qWarning("Could not open %s for reading", qPrintable(zipFileName) );
                    return currentContents[ zipFileName ];
                }
                currentContents[ zipFileName ] = Lib7z::listArchive( archive );
            } catch ( const Lib7z::SevenZipException& e ) {
                qWarning() << e.message();
            }
            archive->close();
        }
        return currentContents[ zipFileName ];
    }

    Lib7z::File file( const QString& path ) const
    {
        QString p = QDir::cleanPath( path ).toLower();
        const QLatin1Char sep( '/' );
        while( p.endsWith( sep ) )
            p.chop( 1 );
        while( p.startsWith( sep ) )
            p.remove( 0, 1 );

        const Lib7z::File* const f = currentFiles[ zipFileName ][ p ];
        if( f != 0 )
            return *f;

        const QVector<Lib7z::File>& files = list();
        for( QVector< Lib7z::File >::const_iterator it = files.begin(); it != files.end(); ++it )
        {
            QString n = it->path.toLower();
            while( n.endsWith( sep ) )
                n.chop( 1 );

            currentFiles[ zipFileName ][ n ] = &(*it);

            if( p == n )
                return *it;
        }

        return Lib7z::File();
    }

    QString providedFileName;
    QString zipFileName;
    QString fileName;

    mutable FileFlags flags;

    Lib7z::File openFile;
    QIODevice::OpenMode mode;
    QBuffer buffer;

    mutable Lib7z::File currentFile;
};

class KD7zEngineIterator : public QAbstractFileEngineIterator
{
public:
    KD7zEngineIterator( const QStringList& entries, QDir::Filters filters, const QStringList& nameFilters )
        : QAbstractFileEngineIterator( filters, nameFilters ),
          list( entries ),
          index( -1 )
    {
    }

    ~KD7zEngineIterator()
    {
    }

    /**
     * \reimp
     */
    bool hasNext() const
    {
        return index < list.size() - 1;
    }

    /**
     * \reimp
     */
    QString next()
    {
        if( !hasNext() )
            return QString();
        ++index;
        return currentFilePath();
    }

    /**
     * \reimp
     */
    QString currentFileName() const
    {
        return index < 0 ? QString() : list[ index ];
    }

private:
    const QStringList list;
    int index;
};

KD7zEngine::KD7zEngine( const QString& fileName )
    : d( new Private( this ) )
{
    d->setFileName( fileName );
}

KD7zEngine::~KD7zEngine()
{
    //delete d;
}

/**
 * \reimp
 */
QStringList KD7zEngine::entryList( QDir::Filters filters, const QStringList& filterNames ) const
{
    QStringList result;

    const QVector<Lib7z::File>& files = d->list();

    static QLatin1Char sep( '/' );

    QString p = QDir::cleanPath( d->fileName ).toLower();
    if( !p.endsWith( sep ) )
        p += sep;
    while( p.startsWith( sep ) )
        p.remove( 0, 1 );

    const int slashes = p.count( sep );

    QList< QRegExp > regexps;
    for( QStringList::const_iterator it = filterNames.begin(); it != filterNames.end(); ++it )
        regexps.push_back( QRegExp( *it, Qt::CaseInsensitive, QRegExp::Wildcard ) );

    for( QVector< Lib7z::File >::const_iterator it = files.begin(); it != files.end(); ++it )
    {
        const QString n = it->path;
        if( !n.toLower().startsWith( p ) || n == p || ( n.count( sep ) != slashes ) )
            continue;

        if( slashes != n.left( n.length() - 1 ).count( sep ) )
            continue;

        if( matches( regexps, filters, *it ) )
            result.push_back( n.section( sep, -1, -1, QString::SectionSkipEmpty ) );
    }

    return result;
}

/**
 * \reimp
 */
QAbstractFileEngine::Iterator* KD7zEngine::beginEntryList( QDir::Filters filters, const QStringList& filterNames )
{
    return new KD7zEngineIterator( entryList( filters, filterNames ), filters, filterNames );
}

/**
 * \reimp
 */
bool KD7zEngine::caseSensitive() const
{
    return false;
}

/**
 * \reimp
 */
void KD7zEngine::setFileName( const QString& file )
{
    d->setFileName( file );
}

/**
 * \reimp
 */
QString KD7zEngine::fileName( FileName file ) const
{
    static const QLatin1Char sep( '/' );
    switch( file )
    {
    case DefaultName:
        return d->providedFileName;
    case BaseName:
        return QString::fromLatin1( "7z://%1%2" ).arg( d->zipFileName ).arg( d->fileName ).section( QChar::fromLatin1( '/' ), -1 );
    case PathName:
        return QString::fromLatin1( "7z://%1%2" ).arg( d->zipFileName ).arg( d->fileName ).section( QChar::fromLatin1( '/' ), 0, -2 );
    case AbsoluteName:
    {
        if( d->zipFileName.startsWith( sep ) )
            return QDir::cleanPath( d->providedFileName );
        return QDir::cleanPath( QString::fromLatin1( "%1/%2" ).arg( QDir::currentPath(), d->providedFileName ) );
    }
    case AbsolutePathName:
        return fileName( AbsoluteName ).section( sep, 0, -2 );
    case CanonicalName:
        return Private::canonicalized( fileName( AbsoluteName ) );
    case CanonicalPathName:
        return fileName( CanonicalName ).section( sep, 0, -2 );
    case BundleName:
    case LinkName:
    default:
        return QString();
    }
}

/**
 * \reimp
 */
bool KD7zEngine::open( QIODevice::OpenMode mode )
{
    //FIXME: should be unused cause we are using KDUpdater operations for extracting now
    
    if( d->mode != QIODevice::NotOpen )
        return false;
   
    const Lib7z::File file = d->file();
    if( file.isDirectory )
        return false; // can't open a directory, dude...

    if( mode & QIODevice::WriteOnly )
        return false; // no write support yet

    d->openFile = file;
    d->mode = mode;

    d->buffer.setData( QByteArray() );
    d->buffer.open( QIODevice::WriteOnly );

    QFile* f = d->archive();
    if( !f->open( QIODevice::ReadOnly ) )
        return false;

    bool error = true;
    try {
        Lib7z::extractArchive( f, d->openFile, &d->buffer );
        error = false;
        d->buffer.close();
        d->buffer.open( mode );
        f->close();
    } catch( const Lib7z::SevenZipException& e ) {
        qWarning() << e.message();
        d->buffer.close();
        f->close();
    }

    return !error;
}

/**
 * \reimp
 */
bool KD7zEngine::flush()
{
#ifdef SEVENZ_PORT
    return d->openFileWrite != 0;
#else
    return false;
#endif
}

/**
 * \reimp
 */
bool KD7zEngine::close()
{
    if( d->mode == QIODevice::NotOpen )
        return false;

    flush();
    d->mode = QIODevice::NotOpen;
    d->openFile = Lib7z::File();
    d->buffer.close();
    d->buffer.setData( QByteArray() );
    return true;
}

/**
 * \reimp
 */
bool KD7zEngine::atEnd() const
{
    return d->buffer.atEnd();
}

/**
 * \reimp
 */
bool KD7zEngine::seek( qint64 offset )
{
    return d->buffer.seek( offset );
}

/**
 * \reimp
 */
qint64 KD7zEngine::pos() const
{
    return d->buffer.pos();
}

/**
 * \reimp
 */
bool KD7zEngine::copy( const QString& newName )
{
    //FIXME: should be unused cause we are using KDUpdater operations for extracting now
    
    QFile* f = d->archive();
    if( !f->open( QIODevice::ReadOnly ) )
        return false;

    QFile target( newName );
    if( !target.open( QIODevice::WriteOnly ) )
    {
        f->close();
        return false;
    }

    try 
    {
        qDebug()<<"Extract from"<<d->zipFileName<<"to"<<newName;
        Lib7z::extractArchive( f, d->file(), &target );
        f->close();
        return true;
    } 
    catch( const Lib7z::SevenZipException& e ) 
    {
        qWarning() << e.message();
        f->close();
        return false;
    }
}

/**
 * \reimp
 */
qint64 KD7zEngine::read( char* data, qint64 maxlen )
{
    return d->buffer.read( data, maxlen );
}

/**
 * \reimp
 */
qint64 KD7zEngine::write( const char* data, qint64 len )
{
#ifdef SEVENZ_PORT
    const unsigned int l = static_cast< unsigned int >( qMin( len, 0x7fffffffLL ) );
    return zipWriteInFileInZip( d->openFileWrite, data, l ) == 0 ? l : -1;
#else
    Q_UNUSED(data);
    Q_UNUSED(len);
    return -1;
#endif
}

/**
 * \reimp
 */
qint64 KD7zEngine::size() const
{
    const Lib7z::File file = d->file();
    return file.uncompressedSize;
}

/**
 * \reimpl
 */
QDateTime KD7zEngine::fileTime( FileTime time ) const
{
    const Lib7z::File file = d->file();

    switch( time )
    {
    case CreationTime:
        if( d->fileName.isEmpty() )
            return QFileInfo( d->zipFileName ).created();
    case ModificationTime:
        return d->fileName.isEmpty() ? QFileInfo( d->zipFileName ).lastModified() : file.mtime;
    case AccessTime:
        return QFileInfo( d->zipFileName ).lastRead();
    default:
        return QDateTime();
    }
}

/**
 * \reimpl
 */
bool KD7zEngine::mkdir( const QString& dirName, bool createParentDirectories ) const
{
#ifdef SEVENZ_PORT
    Q_UNUSED( createParentDirectories );
    static const QChar sep = QChar::fromLatin1( '/' );
    QString path;
    const QString root = QString::fromLatin1( "zip://%1" ).arg( d->zipFileName );
    const QString dir = dirName.mid( root.length() );

    for( int i = 0; i <= dir.count( sep ); ++i )
    {
        path += sep + dir.section( sep, i, i );

        if( !QDir( root + path ).exists() )
        {
            QString fileName = QString::fromLatin1( "%1/" ).arg( path );
            QFile file( d->zipFileName );

            zipFile openFileWrite = zipOpen( QFile::encodeName( d->zipFileName ).data(), file.exists() && file.size() > 0  ? APPEND_STATUS_ADDINZIP : APPEND_STATUS_CREATE );
            zip_fileinfo zi;
            const QDateTime dt  = QDateTime::currentDateTime();
            zi.tmz_date.tm_sec  = dt.time().second();
            zi.tmz_date.tm_min  = dt.time().minute();
            zi.tmz_date.tm_hour = dt.time().hour();
            zi.tmz_date.tm_mday = dt.date().day();
            zi.tmz_date.tm_mon  = dt.date().month() - 1;
            zi.tmz_date.tm_year = dt.date().year();
            zi.dosDate = 0;
            zi.internal_fa = 0;
            zi.external_fa = 0;
            while( fileName.startsWith( sep ) )
                fileName.remove( 0, 1 );

            if( fileName.isEmpty() )
                continue;

            const bool result = zipOpenNewFileInZip( openFileWrite, QFile::encodeName( fileName ).data(),
                                                     &zi, 0, 0, 0, 0, 0, Z_DEFLATED, Z_DEFAULT_COMPRESSION ) == ZIP_OK;
            if( result )
                zipCloseFileInZip( openFileWrite );
            zipClose( openFileWrite, 0 );
            if( !result )
                return false;
        }
    }
    return true;
#else
    Q_UNUSED(dirName);
    Q_UNUSED(createParentDirectories);
    return false;
#endif
}

/**
 * \reimp
 */
QAbstractFileEngine::FileFlags KD7zEngine::fileFlags( FileFlags type ) const
{
    FileFlags result;
    
    static const QLatin1Char sep( '/' );
    static const QString sepString( sep );

    if( !QFile::exists( d->zipFileName ) )
        return result;

    if( d->flags != 0 && ( type & Refresh ) == 0 )
        return d->flags;

    const Lib7z::File file = d->file();
    const QString name = file.path;

    if( type & ExistsFlag )
    {
        if( d->fileName.isEmpty() || d->fileName == sepString )
            result |= ExistsFlag;
        else if( !name.isEmpty() )
            result |= ExistsFlag;
    }
    if( ( type & FileType ) && !name.isEmpty() && !file.isDirectory )
        result |= FileType;
    if( ( ( type & DirectoryType ) && file.isDirectory ) || ( name.isEmpty() && ( result & ExistsFlag ) ) )
        result |= DirectoryType;
    if( ( type & DirectoryType ) && ( d->fileName.isEmpty() || d->fileName == sepString ) )
        result |= DirectoryType;

    if( ( type & 0x7777 ) )
        result |= ( static_cast< QAbstractFileEngine::FileFlags >( static_cast< quint32 >( file.permissions ) ) & type );

    d->flags = result;
    
    return result;
}
