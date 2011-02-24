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
#ifndef KDMMAPPEDFILEIODEVICE_H
#define KDMMAPPEDFILEIODEVICE_H

#include <QtCore/QIODevice>

#include "installer_global.h"

QT_BEGIN_NAMESPACE
class QFile;
QT_END_NAMESPACE

class INSTALLER_EXPORT KDMMappedFileIODevice : public QIODevice
{
    Q_OBJECT

public:
    KDMMappedFileIODevice( QFile* file, qint64 offset, qint64 length, QObject* parent=0 );
    KDMMappedFileIODevice( const QString& filename, qint64 offset, qint64 length, QObject* parent=0 );
    ~KDMMappedFileIODevice();

    /* reimp */ bool open( QIODevice::OpenMode openMode );
    /* reimp */ void close();

private:
    /* reimp */ qint64 readData( char* data, qint64 maxSize );
    /* reimp */ qint64 writeData( const char* data, qint64 maxSize );

private:
    bool ensureMapped();

private:
    QFile* const m_file;
    const bool m_ownsFile;
    const qint64 m_offset;
    const qint64 m_length;
    unsigned char* m_data;
};

#endif // KDMMAPPEDFILEIODEVICE_H
