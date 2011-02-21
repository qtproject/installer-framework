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
#include "kd7zenginetest.h"

#include "../common/kd7zengine.h"
#include "../common/kd7zenginehandler.h"
#include "kdmmappedfileiodevice.h"

#include "init.h"
#include "lib7z_facade.h"

#include <QDateTime>
#include <QDirIterator>
#include <QString>
#include <QStringList>
#include <QVariant>

#include <cassert>

using namespace Lib7z;

namespace QTest {
    template<>
    char* toString( const Lib7z::File& file ) {
        QVariantList l;
        l << "path" << file.path
          << "mtime" << file.mtime
          << "uncompressedSize" << file.uncompressedSize
          << "compressedSize" << file.compressedSize
          << "isDirectory" << file.isDirectory
          << "permissions" << static_cast< quint32 >( file.permissions );
        const QByteArray ba = "Lib7z::File( " + Lib7z::formatKeyValuePairs( l ) + " )";
        return qstrdup( ba.constData() );
    }
}

static void compareFileLists( QVector<File> v1, QVector<File> v2 ) {
    QCOMPARE( v1.size(), v2.size() );
    qSort( v1 );
    qSort( v2 );
    for ( int i = 0; i < v1.size(); ++ i )
        QCOMPARE( v1[i], v2[i] );
}

static QDateTime makeDateTime( int y, int m, int d, int hh, int mm, int ss ) {
    const QDate date( y, m, d );
    const QTime time( hh, mm, ss );
    return QDateTime( date, time );
}

static File makeFile( const QString& path, bool isDirectory, const QDateTime& mtime, quint64 compr, quint64 uncompressed) {
    File f;
    f.path = path;
    f.isDirectory = isDirectory;
    f.mtime = mtime;
    f.compressedSize = compr;
    f.uncompressedSize = uncompressed;
    f.permissions = static_cast< QFile::Permissions >( -1 );
    return f;
}

static QVector<File> waitForIndex( QIODevice* dev ) {
    assert( dev );
    ListArchiveJob* job = new ListArchiveJob;
    job->setArchive( dev );
    QEventLoop loop;
    QObject::connect( job, SIGNAL(finished(Lib7z::Job*)), &loop, SLOT(quit()) );
    job->start();
    loop.exec();
    job->deleteLater();
    if ( job->hasError() )
        throw SevenZipException( job->errorString() );
    return job->index();
}


static void waitForExtract( QIODevice* dev, const File& item, const QString& outputDir ) {
    assert( dev );
    ExtractItemJob* job = new ExtractItemJob;
    job->setArchive( dev );
    job->setItem( item );
    job->setTargetDirectory( outputDir );
    QEventLoop loop;
    QObject::connect( job, SIGNAL(finished(Lib7z::Job*)), &loop, SLOT(quit()) );
    job->start();
    loop.exec();
    job->deleteLater();
    QVERIFY2( !job->hasError(), qPrintable(job->errorString()) );
}

static void waitForExtract( QIODevice* dev, const QString& outputDir ) {
    assert( dev );
    ExtractItemJob* job = new ExtractItemJob;
    job->setArchive( dev );
    job->setTargetDirectory( outputDir );
    QEventLoop loop;
    QObject::connect( job, SIGNAL(finished(Lib7z::Job*)), &loop, SLOT(quit()) );
    job->start();
    loop.exec();
    job->deleteLater();
    QVERIFY2( !job->hasError(), qPrintable(job->errorString()) );
}

static File findFile( const QVector<File>& index, const QString& path ) {
    Q_FOREACH( const File& i, index )
            if( i.path == path )
                return i;
    throw SevenZipException(QObject::tr("File not found in index: %1").arg(path) );
    return File();
}

KD7zEngineTest::KD7zEngineTest()
{
    QInstaller::init();    
}

void KD7zEngineTest::testFileEngine()
{
    KD7zEngine engine( "7z://test1.7z" );
    QCOMPARE( engine.entryList( QDir::AllEntries, QStringList() ), QStringList() << "tests-export" );
    QCOMPARE( engine.fileTime( QAbstractFileEngine::CreationTime ), QFileInfo( "test1.7z" ).created() );
    QVERIFY( engine.fileFlags() == ( QAbstractFileEngine::ExistsFlag | QAbstractFileEngine::DirectoryType ) );

    KD7zEngine engine2( "7z://test1.7z/tests-export" );
    QCOMPARE( engine2.entryList( QDir::AllEntries, QStringList() ), QStringList() << "tests.pro" );
    QCOMPARE( engine2.fileTime( QAbstractFileEngine::CreationTime ), makeDateTime( 2009, 8, 4, 19, 5, 36 ) );
    QVERIFY( engine2.fileFlags().testFlag( QAbstractFileEngine::ExistsFlag ) );
    QVERIFY( engine2.fileFlags().testFlag( QAbstractFileEngine::DirectoryType ) );

    engine2.setFileName( "7z://test1.7z/tests-export/tests.pro" );
    QCOMPARE( engine2.fileTime( QAbstractFileEngine::CreationTime ), makeDateTime( 2009, 8, 4, 11, 0, 6 ) );
    QVERIFY( engine2.size() == 122 );
    QVERIFY( engine2.fileFlags().testFlag( QAbstractFileEngine::ExistsFlag ) );
    QVERIFY( engine2.fileFlags().testFlag( QAbstractFileEngine::FileType ) );

    KD7zEngineHandler handler;

    QFileInfo fi( "7z://test1.7z" );
    QVERIFY( fi.exists() );
    QVERIFY( fi.isDir() );
    QVERIFY( !fi.isFile() );
  
    QFileInfo fi2( "7z://test2.7z/packages-export/qtcore/data/include" );
    QVERIFY( fi2.exists() );
    QVERIFY( fi2.isDir() );
    QVERIFY( !fi2.isFile() );

    QFileInfo fi3( "7z://test2.7z/packages-export/qtcore/data/include/QtCore" );
    QVERIFY( fi3.exists() );
    QVERIFY( fi3.isDir() );
    QVERIFY( !fi3.isFile() );

    QFileInfo fi4( "7z://test2.7z/packages-export/qtcore/data/include/QtCore/qobject.h" );
    QVERIFY( fi4.exists() );
    QVERIFY( !fi4.isDir() );
    QVERIFY( fi4.isFile() );

    QVector<File> expected;
    expected << makeFile( "packages-export", true, makeDateTime( 2009, 8, 6, 12, 7, 11 ), 0, 0 );
    expected << makeFile( "packages-export/qtcore", true, makeDateTime( 2009, 8, 6, 10, 51, 43 ), 0, 0 );
    expected << makeFile( "packages-export/qtcore/meta", true, makeDateTime( 2009, 8, 6, 10, 51, 43 ), 0, 0 );
    expected << makeFile( "packages-export/qtcore/meta/package.xml", false, makeDateTime( 2009, 8, 3, 14, 44, 2 ), 0, 276 );
    expected << makeFile( "packages-export/qtcore/meta/installscript.qs", false, makeDateTime( 2009, 8, 3, 14, 44, 2 ), 0, 0 );
    expected << makeFile( "packages-export/qtcore/data", true, makeDateTime( 2009, 8, 6, 10, 51, 43 ), 0, 0 );
    expected << makeFile( "packages-export/qtcore/data/include", true, makeDateTime( 2009, 8, 6, 10, 51, 43 ), 0, 0 );
    expected << makeFile( "packages-export/qtcore/data/include/QtCore", true, makeDateTime( 2009, 8, 6, 10, 51, 43 ), 0, 0 );
    expected << makeFile( "packages-export/qtcore/data/include/QtCore/qobject.h", false, makeDateTime( 2009, 8, 3, 14, 44, 2 ), 309, 108 );
    expected << makeFile( "packages-export/nokiasdk", true, makeDateTime( 2009, 8, 6, 10, 51, 43 ), 0, 0 );
    expected << makeFile( "packages-export/nokiasdk/meta", true, makeDateTime( 2009, 8, 6, 10, 51, 43 ), 0, 0 );
    expected << makeFile( "packages-export/nokiasdk/meta/package.xml", false, makeDateTime( 2009, 8, 3, 14, 44, 2 ), 0, 248 );

    QDirIterator it( "7z://test2.7z", QDirIterator::Subdirectories );
    while( it.hasNext() )
    {
        it.next();
        QCOMPARE( "7z://test2.7z/" + expected.first().path, it.filePath() );
        QCOMPARE( expected.first().mtime, it.fileInfo().lastModified() );
        QVERIFY( expected.first().uncompressedSize == static_cast< quint32 >( it.fileInfo().size() ) );
        QCOMPARE( expected.first().isDirectory, it.fileInfo().isDir() );
        QCOMPARE( expected.first().isDirectory, !it.fileInfo().isFile() );
        expected.pop_front();
    }
    
    QFile file( "7z://test2.7z/packages-export/qtcore/data/include/QtCore/qobject.h" );
    QVERIFY( file.exists() );
    QVERIFY( file.open( QIODevice::ReadOnly ) );
    QVERIFY( file.size() == 108 );
   
    QStringList lines;
    lines << "#ifndef QOBJECT_H\n"; 
    lines << "#define QOBJECT_H\n";
    lines << "\n";
    lines << "#error \"Sorry, this class is just fake...\"\n";
    lines << "\n";
    lines << "class QObject\n";
    lines << "{\n";
    lines << "};\n";
    lines << "\n";
    lines << "#endif\n";

    int line = 0;
    while( !file.atEnd() )
    {
        const QString expected = lines[ line++ ];
        const QString actual = file.readLine();
        QCOMPARE( actual, expected );
    }

    QTemporaryFile temp;
    temp.open();
    const QString tempName = temp.fileName();
    temp.close();
    QVERIFY( temp.remove() );

    QVERIFY( file.copy( tempName ) );

    QFile tempFile( tempName );
    QVERIFY( tempFile.open( QIODevice::ReadOnly ) );
    line = 0;
    while( !tempFile.atEnd() )
    {
        const QString expected = lines[ line++ ];
        const QString actual = tempFile.readLine();
        QCOMPARE( actual, expected );
    }

    QVERIFY( tempFile.remove() );
}

void KD7zEngineTest::testIsSupportedArchive() {
    KDMMappedFileIODevice in1( "test1.7z", 0, QFileInfo("test1.7z").size() );
    KDMMappedFileIODevice in2( "test2.7z", 0, QFileInfo("test2.7z").size() );
    KDMMappedFileIODevice in_noarchive( "test-noarchive.7z", 0, QFileInfo("test-noarchive.7z").size() );

    try {
        QVERIFY( isSupportedArchive( &in1 ) );
        QVERIFY( isSupportedArchive( &in2 ) );
        QVERIFY( !isSupportedArchive( &in_noarchive ) );
    } catch ( const SevenZipException& e ) {
        qDebug() << e.message();
        QVERIFY( !"isSupportedArchive threw unexpected SevenZipException" );
    } catch ( ... ) {
         QVERIFY( !"isSupportedArchive threw unexpected unknown exception" );
    }

    const QString file( "MinGW-gcc440_1.zip" );
    try
    {
        QVERIFY( isSupportedArchive( file ) );
    }
    catch( const SevenZipException& e )
    {
        qDebug() << e.message();
        QVERIFY( !"isSupportedArchive threw unexpected SevenZipException" );
    }
    catch( ... )
    {
         QVERIFY( !"isSupportedArchive threw unexpected unknown exception" );
    }
}

void KD7zEngineTest::testListing() {
    QVector<File> expected1;
    expected1 << makeFile( "tests-export", true, makeDateTime( 2009, 8, 4, 19, 5, 36 ), 0, 0 );
    expected1 << makeFile( "tests-export/tests.pro", false, makeDateTime( 2009, 8, 4, 11, 0, 6 ), 104, 122 );

    KDMMappedFileIODevice in1( "test1.7z", 0, QFileInfo("test1.7z").size() );
    QVERIFY( isSupportedArchive( &in1 ) );
    const QVector<File> actual1 = waitForIndex( &in1 );
    compareFileLists( actual1, expected1 );

    QVector<File> expected2;
    expected2 << makeFile( "packages-export", true, makeDateTime( 2009, 8, 6, 12, 7, 11 ), 0, 0 );
    expected2 << makeFile( "packages-export/nokiasdk", true, makeDateTime( 2009, 8, 6, 10, 51, 43 ), 0, 0 );
    expected2 << makeFile( "packages-export/nokiasdk/meta", true, makeDateTime( 2009, 8, 6, 10, 51, 43 ), 0, 0 );
    expected2 << makeFile( "packages-export/qtcore", true, makeDateTime( 2009, 8, 6, 10, 51, 43 ), 0, 0 );
    expected2 << makeFile( "packages-export/qtcore/data", true, makeDateTime( 2009, 8, 6, 10, 51, 43 ), 0, 0 );
    expected2 << makeFile( "packages-export/qtcore/data/include", true, makeDateTime( 2009, 8, 6, 10, 51, 43 ), 0, 0 );
    expected2 << makeFile( "packages-export/qtcore/data/include/QtCore", true, makeDateTime( 2009, 8, 6, 10, 51, 43 ), 0, 0 );
    expected2 << makeFile( "packages-export/qtcore/meta", true, makeDateTime( 2009, 8, 6, 10, 51, 43 ), 0, 0 );
    expected2 << makeFile( "packages-export/qtcore/meta/installscript.qs", false, makeDateTime( 2009, 8, 3, 14, 44, 2 ), 0, 0 );
    expected2 << makeFile( "packages-export/qtcore/data/include/QtCore/qobject.h", false, makeDateTime( 2009, 8, 3, 14, 44, 2 ), 309, 108 );
    expected2 << makeFile( "packages-export/qtcore/meta/package.xml", false, makeDateTime( 2009, 8, 3, 14, 44, 2 ), 0, 276 );
    expected2 << makeFile( "packages-export/nokiasdk/meta/package.xml", false, makeDateTime( 2009, 8, 3, 14, 44, 2 ), 0, 248 );

    KDMMappedFileIODevice in2( "test2.7z", 0, QFileInfo("test2.7z").size() );
    QVERIFY( isSupportedArchive( &in2 ) );
    const QVector<File> actual2 = waitForIndex( &in2 );

    compareFileLists( actual2, expected2 );

    try {
        KDMMappedFileIODevice in_noarchive( "test-noarchive.7z", 0, QFileInfo("test-noarchive.7z").size() );
        QVERIFY( !isSupportedArchive( &in_noarchive ) );
        const QVector<File> actual_noarchive = waitForIndex( &in_noarchive );
        QVERIFY( !"reading an invalid archive must throw an exception, no exception thrown" );
    } catch ( const SevenZipException& ) {
        //all fine
    } catch ( ... ) {
      QVERIFY( !"reading an invalid archive throws, but not the expected SevenZipException" );
    }

    KDMMappedFileIODevice in3( "MinGW-gcc440_1.zip", 0, QFileInfo( "MinGW-gcc440_1.zip" ).size() );
    const QVector< File > actual3 = waitForIndex( &in3 );
    QCOMPARE( actual3.count(), 5556 );
    QCOMPARE( actual3.first().path, QString::fromLatin1( "mingw" ) );
    QCOMPARE( actual3.first().isDirectory, true );
}

void KD7zEngineTest::testExtract() {
    KDMMappedFileIODevice in1( "test1.7z", 0, QFileInfo("test1.7z").size() );
    const QVector<File> actual1 = waitForIndex( &in1 );
    const File file = findFile( actual1, "tests-export/tests.pro" );
    waitForExtract( &in1, file, QDir::currentPath() + "/extract-test" );

    KDMMappedFileIODevice in3( "MinGW-gcc440_1.zip", 0, QFileInfo( "MinGW-gcc440_1.zip" ).size() );
    waitForExtract( &in3, QDir::currentPath() + "/extract-test" );
}

void KD7zEngineTest::testCompress()
{
    QFile newArchive( "new.7z" );
    newArchive.open( QIODevice::ReadWrite );
    const QString dir = QDir::currentPath() + "/extract-test/tests-export";
    qDebug() << dir;
    createArchive( &newArchive, dir );
    newArchive.seek( 0 );
    QVERIFY( isSupportedArchive( &newArchive ) );
    newArchive.close();
    
    KD7zEngineHandler handler;

    QFile file( "7z://new.7z/tests-export/tests.pro" );
    QVERIFY( file.exists() );
    QVERIFY( file.open( QIODevice::ReadOnly ) );
    QVERIFY( file.size() == 122 );

    newArchive.open( QIODevice::ReadOnly );
    extractArchive( &newArchive, QDir::currentPath() + "/extract-test2" );
    
    QFile file2( "extract-test2/tests-export/tests.pro" );
    QVERIFY( file2.exists() );
    QVERIFY( file2.open( QIODevice::ReadOnly ) );
    QVERIFY( file2.size() == 122 );
}

QTEST_MAIN(KD7zEngineTest)
