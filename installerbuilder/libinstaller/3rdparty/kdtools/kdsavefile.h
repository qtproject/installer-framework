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

#ifndef __KDTOOLSCORE_KDSAVEFILE_H__
#define __KDTOOLSCORE_KDSAVEFILE_H__

#include <kdtoolsglobal.h>
#include <pimpl_ptr.h>

#include <QtCore/QIODevice>
#include <QtCore/QFile>
#include <QtCore/QString>

class KDTOOLSCORE_EXPORT KDSaveFile : public QIODevice {
    Q_OBJECT
public:
    explicit KDSaveFile( const QString& filename=QString(), QObject* parent=0 );
    ~KDSaveFile();

    enum CommitMode {
        BackupExistingFile=0x1,
        OverwriteExistingFile=0x2
    };

    bool commit( CommitMode=BackupExistingFile );

    QString fileName() const;
    void setFileName( const QString& filename );

    QFile::Permissions permissions() const;
    bool setPermissions( QFile::Permissions );

    QString backupExtension() const;
    void setBackupExtension( const QString& extension );

    bool flush();
    bool resize( qint64 sz );
    int handle() const;

    /* reimp */ bool atEnd();
    /* reimp */ qint64 bytesAvailable() const;
    /* reimp */ qint64 bytesToWrite() const;
    /* reimp */ bool canReadLine() const;
    /* reimp */ void close();
    /* reimp */ bool isSequential() const;
    /* reimp */ bool open( OpenMode mode=QIODevice::ReadWrite ); //only valid: WriteOnly, ReadWrite
    /* reimp */ qint64 pos() const;
    /* reimp */ bool reset();
    /* reimp */ bool seek( qint64 pos );
    /* reimp */ qint64 size() const;
    /* reimp */ bool waitForBytesWritten( int msecs );
    /* reimp */ bool waitForReadyRead( int msecs );

private:
    /* reimp */ qint64 readData( char* data, qint64 maxSize );
    /* reimp */ qint64 readLineData( char* data, qint64 maxSize );
    /* reimp */ qint64 writeData( const char* data, qint64 maxSize );

private:
    class Private;
    kdtools::pimpl_ptr<Private> d;
};

#endif // __KDTOOLSCORE_KDSAVEFILE_H__
