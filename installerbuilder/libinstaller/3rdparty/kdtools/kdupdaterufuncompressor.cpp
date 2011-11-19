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

#include "kdupdaterufuncompressor_p.h"
#include "kdupdaterufcompresscommon_p.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFSFileEngine>
#include <QDebug>

using namespace KDUpdater;

class UFUncompressor::Private
{
public:
    QString ufFileName;
    QString destination;
    QString errorMessage;
    
    void setError(const QString &msg);
};

void UFUncompressor::Private::setError(const QString &msg)
{
    errorMessage = msg;
}

UFUncompressor::UFUncompressor()
    : d(new Private)
{
}

UFUncompressor::~UFUncompressor()
{
    delete d;
}

QString UFUncompressor::errorString() const
{
    return d->errorMessage;
}

void UFUncompressor::setFileName(const QString &fileName)
{
    d->ufFileName = fileName;
}

QString UFUncompressor::fileName() const
{
    return d->ufFileName;
}

void UFUncompressor::setDestination(const QString &dest)
{
    d->destination = dest;
}

QString UFUncompressor::destination() const
{
    return d->destination;
}

bool UFUncompressor::uncompress()
{
    d->errorMessage.clear();
    
    // First open the uf file for reading
    QFile ufFile(d->ufFileName);
    if (!ufFile.open(QFile::ReadOnly)) {
        d->setError(tr("Couldn't open file for reading: %1").arg( ufFile.errorString()));
        return false;
    }

    QDataStream ufDS(&ufFile);
    ufDS.setVersion(QDataStream::Qt_4_2);
    QCryptographicHash hash(QCryptographicHash::Md5);

    // Now read the header.
    UFHeader header;
    ufDS >> header;
    if (ufDS.status() != QDataStream::Ok || !header.isValid()) {
        d->setError(tr("Couldn't read the file header."));
        return false;
    }
    header.addToHash(hash);

    // Some basic checks.
    if (header.magic != QLatin1String( KD_UPDATER_UF_HEADER_MAGIC)) {
        d->setError(tr("Wrong file format (magic number not found)"));
        return false;
    }

    // Lets get to the destination directory
    const QDir dir(d->destination);
    QFSFileEngine fileEngine;

    // Lets create the required directory structure
    int numExpectedFiles = 0;
    for (int i = 0; i < header.fileList.count(); i++) {
        const QString fileName = header.fileList.at(i);
        // qDebug("ToUncompress %s", qPrintable(fileName));
        if (header.isDirList.at(i)) {
            if (!dir.mkpath(fileName)) {
                d->setError(tr("Could not create folder: %1/%2").arg(d->destination, fileName));
                return false;
            }
            fileEngine.setFileName(QString(QLatin1String("%1/%2")).arg(d->destination, fileName));
            fileEngine.setPermissions(header.permList[i] | QAbstractFileEngine::ExeOwnerPerm);
        } else {
           ++numExpectedFiles;
        }
    }

    // Lets now create files within these directories
    int numActualFiles = 0;
    while (!ufDS.atEnd() && numActualFiles < numExpectedFiles) {
        UFEntry ufEntry;
        ufDS >> ufEntry;
        if (ufDS.status() != QDataStream::Ok || !ufEntry.isValid()) {
            d->setError(tr("Could not read information for entry %1.").arg(numActualFiles));
            return false;
        }
        ufEntry.addToHash(hash);

        const QString completeFileName = QString(QLatin1String( "%1/%2" )).arg(d->destination, ufEntry.fileName);
        
        const QByteArray ba = qUncompress(ufEntry.fileData);
        // check the size
        QDataStream stream(ufEntry.fileData);
        stream.setVersion(QDataStream::Qt_4_2);
        qint32 length = 0;
        stream >> length;
        if (ba.length() != length) { // uncompress failed
            d->setError(tr("Could not uncompress entry %1, corrupt data").arg(ufEntry.fileName));
            return false;
        }
    
        QFile ufeFile(completeFileName);
        if (!ufeFile.open(QFile::WriteOnly)) {
            d->setError(tr("Could not open file %1 for writing: %2").arg(completeFileName, ufeFile.errorString()));
            return false;
        }
        
        const char *const data = ba.constData();
        const qint64 total = ba.size();
        qint64 written = 0;
        
        while (written < total) {
            const qint64 num = ufeFile.write(data + written, total - written);
            if (num == -1) {
                d->setError(tr("Failed writing uncompressed data to %1: %2").arg(completeFileName, ufeFile.errorString()));
                return false;
            }
            written += num;
        }

        ufeFile.close();
        
        const QFile::Permissions perm = static_cast<QFile::Permissions>(ufEntry.permissions);
        ufeFile.setPermissions(perm);
        
        if (ufeFile.error() != QFile::NoError) {
            ufeFile.remove();
            d->setError(tr("Failed writing uncompressed data to %1: %2").arg(completeFileName, ufeFile.errorString()));
            return false;
        }

        qDebug("Uncompressed %s", qPrintable(completeFileName));
        ++numActualFiles;
    }

    if (numExpectedFiles != numActualFiles) {
        d->errorMessage = tr("Corrupt file (wrong number of files)");
        return false;
    }

    QByteArray hashdata;
    ufDS >> hashdata;

    if (hashdata != hash.result()) {
        d->errorMessage = tr("Corrupt file (wrong hash)");
        return false;
    }

    return true;
}
