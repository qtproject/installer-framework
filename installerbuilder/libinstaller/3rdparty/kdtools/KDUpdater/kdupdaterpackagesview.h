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

#ifndef KD_UPDATER_PACKAGES_VIEW_H
#define KD_UPDATER_PACKAGES_VIEW_H

#include "kdupdater.h"
#include <QTreeWidget>

namespace KDUpdater
{
    class PackagesInfo;

    class KDTOOLS_UPDATER_EXPORT PackagesView : public QTreeWidget
    {
        Q_OBJECT

    public:
        explicit PackagesView(QWidget* parent=0);
        ~PackagesView();

        void setPackageInfo(PackagesInfo* packageInfo);
        PackagesInfo* packagesInfo() const;

    public Q_SLOTS:
        void refresh();

    private:
        struct PackagesViewData;
        PackagesViewData* d;
    };
}

#endif
