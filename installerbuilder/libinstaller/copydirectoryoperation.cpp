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
#include "copydirectoryoperation.h"

#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#include <QEventLoop>
#include <QFutureWatcher>
#include <QtConcurrentRun>

#include <QDebug>

using namespace QInstaller;

/*
TRANSLATOR QInstaller::CopyDirectoryOperation
*/

CopyDirectoryOperation::CopyDirectoryOperation()
{
    setName( QLatin1String( "CopyDirectory" ) );
}

CopyDirectoryOperation::~CopyDirectoryOperation()
{
}

void CopyDirectoryOperation::letTheUiRunTillFinished(const QFuture<void> &future)
{
    if (!future.isFinished()) {
        QFutureWatcher<void> futureWatcher;

        QEventLoop loop;
        connect(&futureWatcher, SIGNAL(finished()), &loop, SLOT(quit()), Qt::QueuedConnection);
        futureWatcher.setFuture(future);

        if (!future.isFinished())
            loop.exec();
    }
    Q_ASSERT(future.isFinished());
}

QList< QPair<QString,QString> > CopyDirectoryOperation::fileList()
{
    Q_ASSERT(arguments().count() == 2);
    Q_ASSERT(!arguments().at(0).isEmpty());
    Q_ASSERT(!arguments().at(1).isEmpty());
    if (m_fileList.isEmpty()) {
        const QString sourcePath = arguments().at( 0 );
        const QString targetPath = arguments().at( 1 );

        m_fileList = generateFileList(sourcePath, targetPath);
    }
    return m_fileList;
}

QList< QPair<QString,QString> > CopyDirectoryOperation::generateFileList(const QString &sourcePath, const QString &targetPath)
{
    Q_ASSERT( QFileInfo( sourcePath ).isDir() );
    Q_ASSERT( !QFileInfo( targetPath ).exists() || QFileInfo( targetPath ).isDir() );
    if ( !QDir().mkpath( targetPath ) ) {
        qWarning() << QLatin1String("Could not create folder ") << targetPath;
    } else {
        addDirectoryToDirectoryList(targetPath);
    }


    QDirIterator it( sourcePath, QDir::NoDotAndDotDot | QDir::AllEntries | QDir::Hidden | QDir::System);
    while( it.hasNext() )
    {
        const QFileInfo currentFileInfo( it.next() );
        if( currentFileInfo.isDir() && !currentFileInfo.isSymLink() )
        {
            generateFileList( currentFileInfo.absoluteFilePath(), QDir( targetPath ).absoluteFilePath( currentFileInfo.fileName() ) );
        }
        else
        {
            const QString source = currentFileInfo.absoluteFilePath();
            const QString target = QDir( targetPath ).absoluteFilePath( currentFileInfo.fileName() );
            m_fileList.append(QPair<QString, QString>(source, target));
        }
    }
    return m_fileList;
}

bool CopyDirectoryOperation::copyFilesFromInternalFileList()
{
    QList< QPair<QString, QString> > sourceDestinationFileList(fileList());

    QList< QPair<QString, QString> >::iterator i;
    for (i = sourceDestinationFileList.begin(); i != sourceDestinationFileList.end(); ++i) {
        QString source = (*i).first;
        QString dest = (*i).second;

        // If destination file exists, then we cannot use QFile::copy()
        // because it does not overwrite an existing file. So we remove
        // the destination file.
        if( QFile::exists(dest) )
        {
            QFile file( dest );
            if( !file.remove() ) {
                setError( UserDefinedError );
                setErrorString( tr("Could not remove destination file %1: %2.").arg( dest, file.errorString() ) );
                return false;
            }
        }

        QFile file( source );
        const bool copied = file.copy( dest );
        if ( !copied ) {
            setError( UserDefinedError );
            setErrorString( tr("Could not copy %1 to %2: %3.").arg( source, dest, file.errorString() ) );
            return false;
        } else {
            addFileToFileList( dest );
        }
    }
    return true;
}

bool CopyDirectoryOperation::removeFilesFromInternalFileList()
{
    const QStringList fileNameList = value( QLatin1String( "files" ) ).toStringList();

    bool result = true;
    foreach(const QString fileName, fileNameList) {
        result = result & deleteFileNowOrLater( fileName );
    }

    const QStringList directoryList = value( QLatin1String( "directories" ) ).toStringList();

    foreach(const QString directory, directoryList) {
        QDir().rmdir(directory);
    }

    return result;
}

void CopyDirectoryOperation::backup()
{

}
bool CopyDirectoryOperation::performOperation()
{
    const QStringList args = arguments();
    if( args.count() != 2 ) {
        setError( InvalidArguments );
        setErrorString( tr("Invalid arguments in %0: %1 arguments given, 2 expected.")
                        .arg(name()).arg( args.count() ) );
        return false;
    }
    const QString sourcePath = args.at( 0 );
    const QString targetPath = args.at( 1 );


    typedef QList< QPair< QString, QString > > FileList;
    QFuture< FileList > fileListFuture = QtConcurrent::run(this, &CopyDirectoryOperation::fileList);
    letTheUiRunTillFinished(fileListFuture);

    QFuture< bool > allCopied = QtConcurrent::run(this, &CopyDirectoryOperation::copyFilesFromInternalFileList);
    letTheUiRunTillFinished(allCopied);

    return allCopied.result();
}

bool CopyDirectoryOperation::undoOperation()
{
    Q_ASSERT( arguments().count() == 2 );

    QFuture< bool > allRemoved = QtConcurrent::run(this, &CopyDirectoryOperation::removeFilesFromInternalFileList);
    letTheUiRunTillFinished(allRemoved);

    return allRemoved.result();
}

void CopyDirectoryOperation::addFileToFileList( const QString& fileName )
{
    QStringList files = value( QLatin1String( "files" ) ).toStringList();
    files.push_front( fileName );
    setValue( QLatin1String( "files" ), files );
    emit outputTextChanged( fileName );
}

void CopyDirectoryOperation::addDirectoryToDirectoryList( const QString& directory )
{
    QStringList directories = value( QLatin1String( "directories" ) ).toStringList();
    directories.push_front( directory );
    setValue( QLatin1String( "directories" ), directories );
    emit outputTextChanged( directory );
}

bool CopyDirectoryOperation::testOperation()
{
    return true;
}

CopyDirectoryOperation* CopyDirectoryOperation::clone() const
{
    return new CopyDirectoryOperation();
}
