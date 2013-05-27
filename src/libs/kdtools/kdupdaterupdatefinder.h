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

#ifndef KD_UPDATER_UPDATE_FINDER_H
#define KD_UPDATER_UPDATE_FINDER_H

#include "kdupdatertask.h"

#include <QHash>
#include <QUrl>

namespace KDUpdater {

class Application;
class Update;

class KDTOOLS_EXPORT UpdateFinder : public Task
{
    Q_OBJECT
    class Private;

public:
    explicit UpdateFinder(Application *application);
    ~UpdateFinder();

    QList<Update *> updates() const;

private:
    void doRun();
    bool doStop();
    bool doPause();
    bool doResume();

    Update *constructUpdate(int priority, const QUrl &sourceInfoUrl, const QHash<QString, QVariant> &data,
        quint64 compressedSize, quint64 uncompressedSize) const;

private:
    Private *d;
    Q_PRIVATE_SLOT(d, void slotDownloadDone())
};

} // namespace KDUpdater

#endif // KD_UPDATER_UPDATE_FINDER_H
