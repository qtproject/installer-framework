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

#ifndef KD_UPDATER_UPDATE_INSTALLER_H
#define KD_UPDATER_UPDATE_INSTALLER_H

#include "kdupdater.h"
#include "kdupdatertask.h"

#include <QList>

namespace KDUpdater {

class Application;
class Update;

class KDTOOLS_EXPORT UpdateInstaller : public Task
{
    Q_OBJECT

public:
    explicit UpdateInstaller(Application *application);
    ~UpdateInstaller();

    Application *application() const;

    void setUpdatesToInstall(const QList<Update *> &updates);
    QList<Update *> updatesToInstall() const;

private:
    void doRun();
    bool doStop();
    bool doPause();
    bool doResume();

    bool installUpdate(Update *update, int minPc, int maxPc);

    class Private;
    Private *d;

    Q_PRIVATE_SLOT(d, void slotUpdateDownloadProgress(int))
    Q_PRIVATE_SLOT(d, void slotUpdateDownloadDone())
};

} // namespace KDUpdater

#endif // KD_UPDATER_UPDATE_INSTALLER_H
