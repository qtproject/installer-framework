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

#include "kdupdaterupdatesourcesview.h"
#include "kdupdaterupdatesourcesinfo.h"
#include "ui_addupdatesourcedialog.h"

#include <QMessageBox>
#include <QContextMenuEvent>
#include <QAction>
#include <QMenu>

/*!
   \ingroup kdupdater
   \class KDUpdater::UpdateSourcesView kdupdaterupdatesourcesview.h KDUpdaterUpdateSourcesView
   \brief A widget that helps view and/or edit \ref KDUpdater::UpdateSourcesInfo

   \ref KDUpdater::UpdateSourcesInfo, associated with \ref KDUpdater::Application, contains information
   about all the update sources from which the application can download and install updates.
   This widget helps view and edit update sources information.

   \image html updatesourcesview.jpg

   The widget provides the following slots for editing update sources information
   \ref addNewSource()
   \ref editCurrentSource()
   \ref removeCurrentSource()

   You can include this widget within another form or dialog and connect to these slots which make
   use of an inbuilt dialog box to add/edit update sources. Shown below is a screenshot of the
   inbuilt dialog box.

   \image html editupdatesource.jpg

   Alternatively you can also use your own dialog box and directly update \ref KDUpdater::UpdateSourcesInfo.
   This widget connects to \ref KDUpdater::UpdateSourcesInfo signals and ensures that the data it displays
   is always kept updated.

   The widget provides a context menu using which you can add/remove/edit update sources. Shown below is a
   screenshot of the context menu.

   \image html updatesourcesview_contextmenu.jpg
*/

struct KDUpdater::UpdateSourcesView::UpdateSourcesViewData
{
    UpdateSourcesViewData( UpdateSourcesView* qq ) :
        q( qq ),
        updateSourcesInfo(0)
    {}

    UpdateSourcesView* q;
    UpdateSourcesInfo* updateSourcesInfo;
};

/*!
   Constructor
*/
KDUpdater::UpdateSourcesView::UpdateSourcesView(QWidget* parent)
    : QTreeWidget(parent),
      d(new KDUpdater::UpdateSourcesView::UpdateSourcesViewData( this ) )
{
    setColumnCount(3);
    setHeaderLabels( QStringList() << tr("Name") << tr("Title") << tr("URL") );
    setRootIsDecorated(false);

    connect(this, SIGNAL(itemDoubleClicked(QTreeWidgetItem*,int)), this, SLOT(editCurrentSource()));
}

/*!
   Destructor
*/
KDUpdater::UpdateSourcesView::~UpdateSourcesView()
{
    delete d;
}

/*!
   Sets the \ref KDUpdater::UpdateSourcesInfo object whose information this widget should show.

   \code
   KDUpdater::Application application;

   KDUpdater::UpdateSourcesView updatesView;
   updatesView.setUpdateSourcesInfo( application.updateSourcesInfo() );
   updatesView.show();
   \endcode
*/
void KDUpdater::UpdateSourcesView::setUpdateSourcesInfo(KDUpdater::UpdateSourcesInfo* info)
{
    if( d->updateSourcesInfo == info )
        return;

    if(d->updateSourcesInfo)
        disconnect(d->updateSourcesInfo, 0, this, 0);

    d->updateSourcesInfo = info;
    if(d->updateSourcesInfo)
    {
        connect(d->updateSourcesInfo, SIGNAL(reset()), this, SLOT(refresh()));
        connect(d->updateSourcesInfo, SIGNAL(updateSourceInfoAdded(UpdateSourceInfo)),
                this, SLOT(slotUpdateSourceInfoAdded(UpdateSourceInfo)));
        connect(d->updateSourcesInfo, SIGNAL(updateSourceInfoRemoved(UpdateSourceInfo)),
                this, SLOT(slotUpdateSourceInfoRemoved(UpdateSourceInfo)));
        connect(d->updateSourcesInfo, SIGNAL(updateSourceInfoChanged(UpdateSourceInfo,UpdateSourceInfo)),
                this, SLOT(slotUpdateSourceInfoChanged(UpdateSourceInfo,UpdateSourceInfo)));
    }

    refresh();
}

/*!
   Returns a pointer to the \ref KDUpdater::UpdateSourcesInfo object whose information this
   widget is showing.
*/
KDUpdater::UpdateSourcesInfo* KDUpdater::UpdateSourcesView::updateSourcesInfo() const
{
    return d->updateSourcesInfo;
}

/*!
   Returns the index of the currently selected update source in the widget. You can use this
   index along with \ref KDUpdater::UpdateSourcesInfo::updateSourceInfo() method to get hold
   of the update source info.
*/
int KDUpdater::UpdateSourcesView::currentUpdateSourceInfoIndex() const
{
    if( !d->updateSourcesInfo )
        return -1;

    QTreeWidgetItem* item = this->currentItem();
    if( !item )
        return -1;

    int index = this->indexOfTopLevelItem(item);
    if( index < 0 )
        return -1;

    return index;
}

/*!
   Call this slot to reload the updates information. By default this slot is connected to
   \ref KDUpdater::UpdateSourcesInfo::reset() signal in \ref setUpdateSourcesInfo().
*/
void KDUpdater::UpdateSourcesView::refresh()
{
    this->clear();

    if( !d->updateSourcesInfo )
        return;

    for(int i=0; i<d->updateSourcesInfo->updateSourceInfoCount(); i++)
    {
        KDUpdater::UpdateSourceInfo info = d->updateSourcesInfo->updateSourceInfo(i);
        QTreeWidgetItem* item = new QTreeWidgetItem(this);
        item->setText(0, info.name);
        item->setText(1, info.title);
        item->setText(2, info.url.toString());
        item->setData(0, Qt::UserRole, qVariantFromValue<KDUpdater::UpdateSourceInfo>(info));
    }

    resizeColumnToContents(0);
}

/*!
   Call this slot to make use of the in-built dialog box to add a new update source. Shown
   below is a screenshot of the in-built dialog box.

   \image html addupdatesource.jpg
*/
void KDUpdater::UpdateSourcesView::addNewSource()
{
    if( !d->updateSourcesInfo )
        return;

    QDialog dialog(this);
    Ui::AddUpdateSourceDialog ui;
    ui.setupUi(&dialog);

    while(1)
    {
        if( dialog.exec() == QDialog::Rejected )
            return;

        if(ui.txtName->text().isEmpty() || ui.txtUrl->text().isEmpty())
        {
            QMessageBox::information(this, tr("Invalid Update Source Info"),
                                     tr("A valid update source name and url has to be provided"));
            continue;
        }

        break;
    }

    KDUpdater::UpdateSourceInfo newInfo;
    newInfo.name = ui.txtName->text();
    newInfo.title = ui.txtTitle->text();
    newInfo.description = ui.txtDescription->toPlainText();  // FIXME: This should perhaps be toHtml
    newInfo.url = QUrl(ui.txtUrl->text());

    d->updateSourcesInfo->addUpdateSourceInfo(newInfo);
}

/*!
   Call this slot to delete the currently selected update source.
*/
void KDUpdater::UpdateSourcesView::removeCurrentSource()
{
    if( !d->updateSourcesInfo )
        return;

    QTreeWidgetItem* item = this->currentItem();
    if( !item )
        return;

    int index = this->indexOfTopLevelItem(item);
    if( index < 0 )
        return;

    d->updateSourcesInfo->removeUpdateSourceInfoAt(index);
}

/*!
   Call this slot to edit the currently selected update source, using the in-built edit
   update source dialog box. Shown below is a screenshot of the edit update source dialog
   box.

   \image html editupdatesource.jpg
*/
void KDUpdater::UpdateSourcesView::editCurrentSource()
{
    if( !d->updateSourcesInfo )
        return;

    QTreeWidgetItem* item = this->currentItem();
    if( !item )
        return;

    int index = this->indexOfTopLevelItem(item);
    if( index < 0 )
        return;

    KDUpdater::UpdateSourceInfo info = item->data(0, Qt::UserRole).value<KDUpdater::UpdateSourceInfo>();

    QDialog dialog(this);
    Ui::AddUpdateSourceDialog ui;
    ui.setupUi(&dialog);
    ui.txtName->setText(info.name);
    ui.txtTitle->setText(info.title);
    ui.txtDescription->setPlainText(info.description); // FIXME: This should perhaps be setHtml
    ui.txtUrl->setText(info.url.toString());
    dialog.setWindowTitle(tr("Edit Update Source"));

    if( dialog.exec() == QDialog::Rejected )
        return;

    KDUpdater::UpdateSourceInfo newInfo;
    newInfo.name = ui.txtName->text();
    newInfo.title = ui.txtTitle->text();
    newInfo.description = ui.txtDescription->toPlainText(); // FIXME: This should perhaps be setHtml
    newInfo.url = QUrl(ui.txtUrl->text());

    d->updateSourcesInfo->setUpdateSourceInfoAt(index, newInfo);
}

/*!
   \internal
*/
void KDUpdater::UpdateSourcesView::slotUpdateSourceInfoAdded(const KDUpdater::UpdateSourceInfo &info)
{
    if( !d->updateSourcesInfo )
        return;

    QTreeWidgetItem* item = new QTreeWidgetItem(this);
    item->setText(0, info.name);
    item->setText(1, info.title);
    item->setText(2, info.url.toString());
    item->setData(0, Qt::UserRole, qVariantFromValue<KDUpdater::UpdateSourceInfo>(info));
}

/*!
   \internal
*/
void KDUpdater::UpdateSourcesView::slotUpdateSourceInfoRemoved(const KDUpdater::UpdateSourceInfo &info)
{
    if( !d->updateSourcesInfo )
        return;

    QTreeWidgetItem* item = 0;
    for(int i=0; i<topLevelItemCount(); i++)
    {
        item = topLevelItem(i);
        KDUpdater::UpdateSourceInfo itemInfo = item->data(0, Qt::UserRole).value<KDUpdater::UpdateSourceInfo>();
        if(itemInfo == info)
            break;
        item = 0;
    }

    if( !item )
        return;

    delete item;
}

/*!
   \internal
*/
void KDUpdater::UpdateSourcesView::slotUpdateSourceInfoChanged (const KDUpdater::UpdateSourceInfo &newInfo,
                                                                const KDUpdater::UpdateSourceInfo &oldInfo)
{
    if( !d->updateSourcesInfo )
        return;

    QTreeWidgetItem* item = 0;
    for(int i=0; i<topLevelItemCount(); i++)
    {
        item = topLevelItem(i);
        KDUpdater::UpdateSourceInfo itemInfo = item->data(0, Qt::UserRole).value<KDUpdater::UpdateSourceInfo>();
        if(itemInfo == oldInfo)
            break;
        item = 0;
    }

    if( !item )
        return;

    item->setText(0, newInfo.name);
    item->setText(1, newInfo.title);
    item->setText(2, newInfo.url.toString());
    item->setData(0, Qt::UserRole, qVariantFromValue<KDUpdater::UpdateSourceInfo>(newInfo));
}

/*!
   \internal
*/
void KDUpdater::UpdateSourcesView::contextMenuEvent(QContextMenuEvent* e)
{
    QTreeWidgetItem* item = this->itemAt( e->pos() );

    QMenu menu;
    QAction* addAction = menu.addAction(tr("&Add Source"));
    QAction* editAction = item ? menu.addAction(tr("&Edit Source")) : 0;
    QAction* remAction = item ? menu.addAction(tr("&Remove Source")) : 0;

    QAction* result = menu.exec( QCursor::pos() );
    if( !result )
        return;

    if( result == addAction )
        this->addNewSource();
    else if( result == remAction )
        this->removeCurrentSource();
    else if( result == editAction )
        this->editCurrentSource();
}
