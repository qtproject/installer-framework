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
#include "extractarchiveoperation.h"
#include "extractarchiveoperation_p.h"

#include <QEventLoop>
#include <QThread>
#include <QThreadPool>

#include <QDebug>

using namespace QInstaller;

/*
TRANSLATOR QInstaller::ExtractArchiveOperation
*/

ExtractArchiveOperation::ExtractArchiveOperation()
{
    setName( QLatin1String( "Extract" ) );
}

ExtractArchiveOperation::~ExtractArchiveOperation()
{
}

void ExtractArchiveOperation::backup()
{
    // we need to backup on the fly...
}

bool ExtractArchiveOperation::performOperation()
{
    const QStringList args = arguments();
    if( args.count() != 2 ) {
        setError( InvalidArguments );
        setErrorString( tr("Invalid arguments in %0: %1 arguments given, 2 expected.")
                        .arg(name()).arg( args.count() ) );
        return false;
    }

    const QString archivePath = args.first();
    const QString targetDir = args.at( 1 );

    Receiver receiver;
    Callback callback;
    
    //usually we have to connect it as queued connection but then some blocking work is in the main thread
    connect( &callback, SIGNAL(progressChanged(QString)), this, SLOT(slotProgressChanged(QString)), Qt::DirectConnection );
    connect( &callback, SIGNAL(progressChanged(int)), this, SIGNAL(progressChanged(int)), Qt::QueuedConnection );

    if(QInstaller::Installer *installer = this->value( QLatin1String( "installer" )).value<QInstaller::Installer*>()) {
        connect( installer, SIGNAL( statusChanged( QInstaller::Installer::Status ) ), &callback, 
                            SLOT( statusChanged( QInstaller::Installer::Status ) ), Qt::QueuedConnection );
    }
    
    //Runnable is derived from QRunable which will be deleted by the ThreadPool -> no parent is needed
    Runnable* runnable = new Runnable( archivePath, targetDir, &callback );
    connect( runnable, SIGNAL(finished(bool,QString)), &receiver, SLOT(runnableFinished(bool,QString)), Qt::QueuedConnection );
    QEventLoop loop;
    connect( &receiver, SIGNAL( finished() ), &loop, SLOT( quit() ) );
    if (QThreadPool::globalInstance()->tryStart(runnable)) {
        loop.exec();
    } else {
        //in case there is no availabe thread we should call it directly
        //this is more a hack
        runnable->run();
        receiver.runnableFinished(true, QString());
    }

    typedef QPair<QString, QString> StringPair;
    QVector<StringPair> backupFiles = callback.backupFiles;

    //TODO use backups for rollback, too? doesn't work for uninstallation though

    //delete all backups we can delete right now, remember the rest
    Q_FOREACH( const StringPair& i, backupFiles )
        deleteFileNowOrLater( i.second );

    if ( !receiver.success ) {
        setError( UserDefinedError );
        setErrorString( receiver.errorString );
        return false;
    }
    return true;
}

WorkerThread::WorkerThread( ExtractArchiveOperation *op, const QStringList &files, QObject* parent )
    : QThread( parent )
    , m_files( files )
    , m_op( op )
{
}

/*!
 \reimp
*/
void WorkerThread::run()
{
    ExtractArchiveOperation* const op = m_op;//dynamic_cast< ExtractArchiveOperation* >( parent() );
    Q_ASSERT( op != 0 );

    int progress = -1;
    int index = 0;
    const int count = m_files.count();
    Q_FOREACH( const QString &file, m_files )
    {
        const QFileInfo fi( file );
        emit outputTextChanged( file );
        if( fi.isFile() || fi.isSymLink() )
        {
            op->deleteFileNowOrLater( fi.absoluteFilePath() );
        }
        else if( fi.isDir() )
        {
            const QDir d = fi.dir();
            // even remove some hidden, OS-created files in there
#if defined Q_WS_MAC
            op->deleteFileNowOrLater( file + QLatin1String( "/.DS_Store" ) );
#elif defined Q_WS_WIN
            op->deleteFileNowOrLater( file + QLatin1String( "/Thumbs.db" ) );
#endif
            d.rmdir( file ); // directory may not exist
        }
        const int newProgress = (index++) * 100 / count;
        if( progress != newProgress )
            emit progressChanged( newProgress );
        progress = newProgress;
    }
}

bool ExtractArchiveOperation::undoOperation()
{
    Q_ASSERT( arguments().count() == 2 );
    //const QString archivePath = arguments().first();
    //const QString targetDir = arguments().last();

    const QStringList m_files = value( QLatin1String( "files" ) ).toStringList();

    WorkerThread* const thread = new WorkerThread( this, m_files );
    connect(thread, SIGNAL(outputTextChanged(QString)),
            this, SIGNAL(outputTextChanged(QString)), Qt::QueuedConnection);
    connect( thread, SIGNAL( progressChanged( int ) ), this, SIGNAL( progressChanged( int ) ), Qt::QueuedConnection );

    QEventLoop loop;
    connect( thread, SIGNAL(finished()), &loop, SLOT( quit() ), Qt::QueuedConnection );
    thread->start();
    loop.exec();
    thread->deleteLater();
    return true;
}

bool ExtractArchiveOperation::testOperation()
{
    return true;
}

ExtractArchiveOperation* ExtractArchiveOperation::clone() const
{
    return new ExtractArchiveOperation();
}

//this slot is direct connected to the caller so please don't call it from another thread in the same time
void ExtractArchiveOperation::slotProgressChanged( const QString& filename )
{
    QStringList m_files = value( QLatin1String( "files" ) ).toStringList();
    m_files.push_front( filename );
    setValue( QLatin1String( "files" ), m_files );
    emit outputTextChanged( filename );
}
