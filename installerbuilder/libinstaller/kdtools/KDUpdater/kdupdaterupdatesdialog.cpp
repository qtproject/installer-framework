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

#include "kdupdaterupdatesdialog.h"
#include "kdupdaterpackagesinfo.h"
#include "kdupdaterupdate.h"
#include "kdupdaterapplication.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QHash>
#include <QtCore/QSet>

#if defined( KDUPDATERGUIWEBVIEW )
#include <QtWebKit/QWebView>
#elif defined( KDUPDATERGUITEXTBROWSER )
#include <QtGui/QTextBrowser>
#endif

#include "ui_updatesdialog.h"

/*!
   \ingroup kdupdater
   \class KDUpdater::UpdatesDialog kdupdaterupdatesdialog.h KDUpdaterUpdatesDialog
   \brief A dialog that let the user chooses which updates he wants to install

   After \ref KDUpdater::UpdateFinder class finds all updates available for an application,
   this dialog can be used to help the user select which update he wants to install.

   Usage:
   \code
    QList<KDUpdater::Update*> updates = updateFinder.updates();

    KDUpdater::UpdatesDialog updatesDialog(this);
    updatesDialog.setUpdates(updates);

    if( updatesDialog.exec() != QDialog::Accepted )
    {
        qDeleteAll(updates);
        updates.clear();
        return;
    }

    QList<KDUpdater::Update*> reqUpdates;
    for(int i=0; i<updates.count(); i++)
    {
        if( !updatesDialog.isUpdateAllowed(updates[i]) )
            continue;
        reqUpdates.append(updates[i]);
    }
    \endcode
*/

class KDUpdater::UpdatesDialog::Private
{
    Q_DECLARE_TR_FUNCTIONS(KDUpdater::Private)

public:
    explicit Private( UpdatesDialog* qq ) :
        q( qq )
    {}

    UpdatesDialog* q;

    Ui::UpdatesDialog ui;

    int currentUpdate;
    QList<Update*> updates;
    QSet<const Update*> status;

    void setCurrentUpdate(int index);

    QString packageDescription( Update* update );
    QString compatDescription( Update* update );
    void slotStateChanged();
    void slotPreviousClicked();
    void slotNextClicked();
};

/*!
   Constructor.
*/
KDUpdater::UpdatesDialog::UpdatesDialog(QWidget *parent)
    : QDialog(parent),
      d( new Private( this ) )
{
    d->ui.setupUi(this);
    d->currentUpdate = -1;

    connect(d->ui.packageUpdateCheckBox, SIGNAL(stateChanged(int)),
            this, SLOT(slotStateChanged()));
    connect(d->ui.nextPackageButton, SIGNAL(clicked()),
            this, SLOT(slotNextClicked()));
    connect(d->ui.previousPackageButton, SIGNAL(clicked()),
            this, SLOT(slotPreviousClicked()));
}


/*!
   Destructor.
*/
KDUpdater::UpdatesDialog::~UpdatesDialog()
{
    delete d;
}


/*!
   Sets the list of updates available to the user.
*/
void KDUpdater::UpdatesDialog::setUpdates(const QList<Update*> &updates)
{
    d->updates = updates;
    d->status.clear();

    d->ui.packageSwitchBar->setVisible( d->updates.size()>1 );

    if (d->updates.isEmpty()) {
        d->ui.descriptionLabel->setText(tr("<b>No update available...</b>"));
        d->ui.descriptionLabel->setFixedSize(d->ui.descriptionLabel->sizeHint());
        d->ui.releaseNotesGroup->hide();
        d->ui.pixmapLabel->hide();
    } else if (d->updates.size()==1) {
        //Only one update, so pre-accept it.
        //OK/Cancel will do from the user POV
        d->status.insert( d->updates.front() );
    }

    d->ui.totalPackageLabel->setText(QString::number(d->updates.size()));
    d->setCurrentUpdate(0);
}


/*!
   returns the list of updates available to the user.
*/
QList<KDUpdater::Update*> KDUpdater::UpdatesDialog::updates() const
{
    return d->updates;
}


/*!
   Returns true if the update needs to be installed.
*/
bool KDUpdater::UpdatesDialog::isUpdateAllowed(const KDUpdater::Update *update) const
{
    return d->status.contains( update );
}

void KDUpdater::UpdatesDialog::Private::slotStateChanged()
{
    if (currentUpdate<0 || currentUpdate>=updates.size()) {
        return;
    }

    if ( ui.packageUpdateCheckBox->isChecked() )
        status.insert( updates[currentUpdate] );
    else
        status.remove( updates[currentUpdate] );
}

void KDUpdater::UpdatesDialog::Private::slotPreviousClicked()
{
    setCurrentUpdate(currentUpdate-1);
}

void KDUpdater::UpdatesDialog::Private::slotNextClicked()
{
    setCurrentUpdate(currentUpdate+1);
}

void KDUpdater::UpdatesDialog::Private::setCurrentUpdate(int index)
{
    if (updates.isEmpty()) {
        if (currentUpdate == -1)
            return;

        currentUpdate = -1;
        return;
    }

    if (index<0 || index>=updates.size()) {
        return;
    }

    currentUpdate = index;

    KDUpdater::Update *update = updates.at( index );

    QString description;

    switch ( update->type() ) {
    case PackageUpdate:
    case NewPackage:
        description = packageDescription( update );
        break;
    case CompatUpdate:
        description = compatDescription( update );
        break;
    default:
        description = tr( "<unkown>" );
    }

    ui.descriptionLabel->setText(description);
    ui.descriptionLabel->setMinimumHeight(ui.descriptionLabel->heightForWidth(400));

    ui.packageUpdateCheckBox->setChecked( status.contains( update ) );

    ui.currentPackageLabel->setText(QString::number(index+1));
    ui.nextPackageButton->setEnabled( index!=(updates.size()-1) );
    ui.previousPackageButton->setEnabled( index!=0 );

    QDir appdir(update->application()->applicationDirectory());
    if (update->data( QLatin1String( "ReleaseNotes" ) ).isValid()) {
        ui.releaseNotesGroup->show();
#if defined( KDUPDATERGUIWEBVIEW )
        ui.releaseNotesView->setUrl( update->data( QLatin1String( "ReleaseNotes" ) ).toUrl() );
#elif defined( KDUPDATERGUITEXTBROWSER )
        ui.releaseNotesView->setSource( update->data( QLatin1String( "ReleaseNotes" ) ).toUrl());
#endif
    }
    else {
        ui.releaseNotesGroup->hide();
    }
}


QString KDUpdater::UpdatesDialog::Private::packageDescription( KDUpdater::Update* update )
{
    KDUpdater::PackagesInfo *packages = update->application()->packagesInfo();
    KDUpdater::PackageInfo info = packages->packageInfo(
        packages->findPackageInfo(update->data( QLatin1String( "Name" ) ).toString()));

    QDir appdir(update->application()->applicationDirectory());
    QPixmap pixmap(appdir.filePath(info.pixmap));
    if (!pixmap.isNull()) {
        ui.pixmapLabel->setPixmap(pixmap.scaled(96, 96));
    }


    QString description = tr("<b>A new package update is available for %1!</b><br/><br/>"
                             "The package %2 %3 is now available -- you have version %4")
                          .arg(packages->applicationName(),
                               update->data( QLatin1String( "Name" ) ).toString(),
                               update->data( QLatin1String( "Version" ) ).toString(),
                               info.version);

    if (!info.title.isEmpty() || !info.description.isEmpty() ) {
        description += QLatin1String( "<br/><br/>" );
        description += tr("<b>Package Details:</b>" );
        if ( !info.title.isEmpty() ) {
            description += tr( "<br/><i>Title:</i> %1" ).arg( info.title );
        }
        if ( !info.description.isEmpty() ) {
            description += tr( "<br/><i>Description:</i> %1" ).arg( info.description );
        }
    }

    if ( update->data( QLatin1String( "Description" ) ).isValid() ) {
        description += QLatin1String( "<br/><br/>" );
        description += tr( "<b>Update description:</b><br/>%1" )
                       .arg( update->data( QLatin1String( "Description" ) ).toString() );
    }
    return description;
}

QString KDUpdater::UpdatesDialog::Private::compatDescription( Update* update )
{
    KDUpdater::PackagesInfo *packages = update->application()->packagesInfo();

    QString description = tr("<b>A new compatibility update is available for %1!</b><br/><br/>"
                             "The compatibility level %2 is now available -- you have level %3")
                          .arg(packages->applicationName(),
                               QString::number(update->data( QLatin1String( "CompatLevel" ) ).toInt()),
                               QString::number(packages->compatLevel()));

    if ( update->data( QLatin1String( "Description" ) ).isValid() ) {
        description += QLatin1String( "<br/><br/>" );
        description += tr( "<b>Update description:</b> %1" )
                       .arg( update->data( QLatin1String( "Description" ) ).toString() );
    }
    return description;
}

#include "moc_kdupdaterupdatesdialog.cpp"
