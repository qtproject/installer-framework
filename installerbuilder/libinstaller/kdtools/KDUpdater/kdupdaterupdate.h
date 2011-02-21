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

#ifndef KD_UPDATER_UPDATE_H
#define KD_UPDATER_UPDATE_H

#include "kdupdater.h"
#include "kdupdatertask.h"
#include <QUrl>
#include <QDate>
#include <QMap>
#include <QVariant>
#include <QList>

namespace KDUpdater
{
    class Application;
    struct UpdateSourceInfo;
    class UpdateFinder;
    class UpdateOperation;

    class KDTOOLS_UPDATER_EXPORT Update : public Task
    {
        Q_OBJECT

    public:
        ~Update();

        Application* application() const;

        UpdateType type() const;
        QUrl updateUrl() const;
        QDate releaseDate() const;
        QVariant data( const QString& name ) const;
        UpdateSourceInfo sourceInfo() const;

        bool canDownload() const;
        bool isDownloaded() const;
        void download() { run(); }
        QString downloadedFileName() const;

        QList<UpdateOperation*> operations() const;

        quint64 compressedSize() const;
        quint64 uncompressedSize() const;

    private Q_SLOTS:
        void downloadProgress(double);
        void downloadAborted(const QString& msg);
        void downloadCompleted();

    private:
        friend class UpdateFinder;
        struct UpdateData;
        UpdateData* d;

        void doRun();
        bool doStop();
        bool doPause();
        bool doResume();

        Update(Application* application, const UpdateSourceInfo& sourceInfo,
               UpdateType type, const QUrl& updateUrl, const QMap<QString, QVariant>& data, quint64 compressedSize, quint64 uncompressedSize, const QByteArray& sha1sum );
    };
}

#endif
