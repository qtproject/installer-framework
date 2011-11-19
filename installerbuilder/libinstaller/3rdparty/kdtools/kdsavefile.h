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

#ifndef KDTOOLS_KDSAVEFILE_H
#define KDTOOLS_KDSAVEFILE_H

#include <kdtoolsglobal.h>

#include <QtCore/QIODevice>
#include <QtCore/QFile>
#include <QtCore/QString>

class KDTOOLS_EXPORT KDSaveFile : public QIODevice
{
    Q_OBJECT

public:
    explicit KDSaveFile(const QString &filename = QString(), QObject *parent = 0);
    ~KDSaveFile();

    enum CommitMode {
        BackupExistingFile = 1,
        OverwriteExistingFile = 2
    };

    bool commit(CommitMode = BackupExistingFile);

    QString fileName() const;
    void setFileName(const QString &filename);

    QFile::Permissions permissions() const;
    bool setPermissions(QFile::Permissions);

    QString backupExtension() const;
    void setBackupExtension(const QString &extension);

    bool flush();
    bool resize(qint64 size);
    int handle() const;

    bool atEnd();
    qint64 bytesAvailable() const;
    qint64 bytesToWrite() const;
    bool canReadLine() const;
    void close();
    bool isSequential() const;
    bool open(OpenMode mode = QIODevice::ReadWrite); //only valid: WriteOnly, ReadWrite
    qint64 pos() const;
    bool reset();
    bool seek(qint64 pos);
    qint64 size() const;
    bool waitForBytesWritten(int msecs);
    bool waitForReadyRead(int msecs);

private:
    qint64 readData(char *data, qint64 maxSize);
    qint64 readLineData(char *data, qint64 maxSize);
    qint64 writeData(const char *data, qint64 maxSize);

private:
    class Private;
    Private *d;
};

#endif // KDTOOLS_KDSAVEFILE_H
