/**************************************************************************
**
** This file is part of Qt SDK**
**
** Copyright (c) 2010 Nokia Corporation and/or its subsidiary(-ies).*
**
** Contact:  Nokia Corporation qt-info@nokia.com**
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
#ifndef KD7ZENGINE_H
#define KD7ZENGINE_H

#include <QtCore/QAbstractFileEngine>
#include <KDToolsCore/pimpl_ptr.h>

#include "installer_global.h"

class INSTALLER_EXPORT KD7zEngine : public QAbstractFileEngine
{
public:
    explicit KD7zEngine( const QString& fileName );
    ~KD7zEngine();

    bool caseSensitive() const;

    FileFlags fileFlags( FileFlags type = FileInfoAll ) const;
    QString fileName( FileName file = DefaultName ) const;
    QDateTime fileTime( FileTime time ) const;

    bool copy( const QString& newName );

    bool atEnd() const;
    qint64 size() const;

    bool seek( qint64 offset );
    qint64 pos() const;

    bool open( QIODevice::OpenMode mode );
    bool close();
    bool flush();

    bool mkdir( const QString& dirName, bool createParentDirectories ) const;

    QStringList entryList( QDir::Filters filters, const QStringList& filterNames ) const;
    Iterator* beginEntryList( QDir::Filters filters, const QStringList& filterNames );

    void setFileName( const QString& file );

    qint64 read( char* data, qint64 maxlen );
    qint64 write( const char* data, qint64 len );

private:
    class Private;
    kdtools::pimpl_ptr<Private> d;
};

#endif // KD7ZENGINE_H
