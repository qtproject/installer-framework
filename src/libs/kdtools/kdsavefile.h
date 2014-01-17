/****************************************************************************
**
** Copyright (C) 2013 Klaralvdalens Datakonsult AB (KDAB)
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef KDTOOLS_KDSAVEFILE_H
#define KDTOOLS_KDSAVEFILE_H

#include <kdtoolsglobal.h>

#include <QIODevice>
#include <QFile>
#include <QString>

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

    bool atEnd() const;
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
