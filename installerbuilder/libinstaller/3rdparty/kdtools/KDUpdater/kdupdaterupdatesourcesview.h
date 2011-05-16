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

#ifndef KD_UPDATER_UPDATE_SOURCES_VIEW_H
#define KD_UPDATER_UPDATE_SOURCES_VIEW_H

#include "kdupdater.h"
#include <QTreeWidget>

namespace KDUpdater
{
    struct UpdateSourceInfo;
    class UpdateSourcesInfo;

    class KDTOOLS_UPDATER_EXPORT UpdateSourcesView : public QTreeWidget
    {
        Q_OBJECT

    public:
        explicit UpdateSourcesView(QWidget* parent=0);
        ~UpdateSourcesView();

        void setUpdateSourcesInfo(UpdateSourcesInfo* info);
        UpdateSourcesInfo* updateSourcesInfo() const;

        int currentUpdateSourceInfoIndex() const;

    public Q_SLOTS:
        void refresh();
        void addNewSource();
        void removeCurrentSource();
        void editCurrentSource();

    protected:
        void contextMenuEvent(QContextMenuEvent* e);

    private Q_SLOTS:
        void slotUpdateSourceInfoAdded(const UpdateSourceInfo &info);
        void slotUpdateSourceInfoRemoved(const UpdateSourceInfo &info);
        void slotUpdateSourceInfoChanged(const UpdateSourceInfo &newInfo,
                                         const UpdateSourceInfo &oldInfo);

    private:
        struct UpdateSourcesViewData;
        UpdateSourcesViewData* d;
    };
};

#endif
