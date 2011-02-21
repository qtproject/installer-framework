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

#include "kdupdaterpackagesview.h"
#include "kdupdaterpackagesinfo.h"

/*!
   \ingroup kdupdater
   \class KDUpdater::PackagesView kdupdaterpackagesview.h KDUpdaterPackagesView
   \brief A widget that can show packages contained in \ref KDUpdater::PackagesInfo

   \ref KDUpdater::PackagesInfo, associated with \ref KDUpdater::Application, contains
   information about all the packages installed in the application. This widget helps view the packages
   in a list.

   \image html packagesview.jpg

   To use this widget, just create an instance and pass to \ref setPackageInfo() a pointer to
   \ref KDUpdater::PackagesInfo whose information you want this widget to show.
*/

struct KDUpdater::PackagesView::PackagesViewData
{
    PackagesViewData( PackagesView* qq ) :
        q( qq ),
        packagesInfo(0)
    {}

    PackagesView* q;
    PackagesInfo* packagesInfo;
};

/*!
   Constructor.
*/
KDUpdater::PackagesView::PackagesView(QWidget* parent)
    : QTreeWidget(parent),
      d ( new PackagesViewData( this ) )
{

    setColumnCount(5);
    setHeaderLabels( QStringList() << tr("Name") << tr("Title")
                     << tr("Description") << tr("Version")
                     << tr("Last Updated") );
    setRootIsDecorated(false);
}

/*!
   Destructor
*/
KDUpdater::PackagesView::~PackagesView()
{
    delete d;
}

/*!
   Sets the package info whose information this widget should show.

   \code
   KDUpdater::Application application;

   KDUpdater::PackagesView packageView;
   packageView.setPackageInfo( application.packagesInfo() );
   packageView.show();
   \endcode

*/
void KDUpdater::PackagesView::setPackageInfo(KDUpdater::PackagesInfo* packagesInfo)
{
    if( d->packagesInfo == packagesInfo )
        return;

    if(d->packagesInfo)
        disconnect(d->packagesInfo, 0, this, 0);

    d->packagesInfo = packagesInfo;
    if(d->packagesInfo)
        connect(d->packagesInfo, SIGNAL(reset()), this, SLOT(refresh()));

    refresh();
}

/*!
   Returns a pointer to the package info whose information this widget is showing.
*/
KDUpdater::PackagesInfo* KDUpdater::PackagesView::packagesInfo() const
{
    return d->packagesInfo;
}

/*!
   This slot reloads package information from the \ref KDUpdater::PackagesInfo associated
   with this widget.

   \note By default, this slot is connected to the \ref KDUpdater::PackagesInfo::reset()
   signal in \ref setPackageInfo()
*/
void KDUpdater::PackagesView::refresh()
{
    this->clear();
    if( !d->packagesInfo )
        return;

    Q_FOREACH(const KDUpdater::PackageInfo& info, d->packagesInfo->packageInfos())
    {
        QTreeWidgetItem* item = new QTreeWidgetItem(this);
        item->setText(0, info.name);
        item->setText(1, info.title);
        item->setText(2, info.description);
        item->setText(3, info.version);
        item->setText(4, info.lastUpdateDate.toString());
    }

    resizeColumnToContents(0);
}
