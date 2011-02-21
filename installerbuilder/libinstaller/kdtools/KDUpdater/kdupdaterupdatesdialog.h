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

#ifndef KD_UPDATER_UPDATES_DIALOG_H
#define KD_UPDATER_UPDATES_DIALOG_H

#include "kdupdater.h"
#include <QtCore/QList>
#include <QtGui/QDialog>

namespace KDUpdater
{
    class Update;

    class KDTOOLS_UPDATER_EXPORT UpdatesDialog : public QDialog
    {
        Q_OBJECT

    public:
        explicit UpdatesDialog(QWidget *parent = 0);
        ~UpdatesDialog();

        void setUpdates(const QList<Update*> &updates);
        QList<Update*> updates() const;

        bool isUpdateAllowed( const Update * update ) const;

    private:
        class Private;
        Private * const d;

        Q_PRIVATE_SLOT( d, void slotStateChanged() )
        Q_PRIVATE_SLOT( d, void slotPreviousClicked() )
        Q_PRIVATE_SLOT( d, void slotNextClicked() )
    };
}

#endif
