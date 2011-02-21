/**************************************************************************
**
** This file is part of Qt SDK**
**
** Copyright (c) 2011 Nokia Corporation and/or its subsidiary(-ies).*
**
** Contact:  Nokia Corporation qt-info@nokia.com**
**
** No Commercial Usage
**
** This file contains pre-release code and may not be distributed.
** You may use this file in accordance with the terms and conditions
** contained in the Technology Preview License Agreement accompanying
** this package.
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception version
** 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** If you are unsure which license is appropriate for your use, please contact
** (qt-info@nokia.com).
**
**************************************************************************/
#include "installationprogressdialog.h"
#include "performinstallationform.h"
#include "progresscoordinator.h"
#include "common/utils.h"

#include <QtGui/QDialogButtonBox>
#include <QtGui/QLayout>
#include <QtGui/QPushButton>

using namespace QInstaller;

InstallationProgressDialog::InstallationProgressDialog(QWidget *parent)
    : QDialog(parent),
      m_performInstallationForm(new PerformInstallationForm(this))
{
    m_performInstallationForm->setupUi(this);
    connect(ProgressCoordninator::instance(), SIGNAL( detailTextChanged( QString ) ),
            m_performInstallationForm, SLOT( appendProgressDetails( QString ) ) );
    connect(ProgressCoordninator::instance(), SIGNAL(detailTextResetNeeded()),
            m_performInstallationForm, SLOT(clearDetailsBrowser()));

    m_dialogBtns = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Ok, Qt::Horizontal, this);
    m_dialogBtns->button(QDialogButtonBox::Ok)->setEnabled(false);
    layout()->addWidget( m_dialogBtns );

    connect(m_dialogBtns, SIGNAL( rejected() ), this, SIGNAL( canceled() ) );
    connect(m_dialogBtns, SIGNAL( rejected() ), this, SLOT( reject() ) );
    connect(m_dialogBtns, SIGNAL( accepted() ), this, SLOT( close() ) );

    m_dialogBtns->button( QDialogButtonBox::Ok )->setEnabled( false );
    m_dialogBtns->button( QDialogButtonBox::Cancel )->setEnabled( true );

    m_performInstallationForm->enableDetails();
    m_performInstallationForm->startUpdateProgress();
}

InstallationProgressDialog::~InstallationProgressDialog()
{
    delete m_performInstallationForm;
}


//bool InstallationProgressDialog::isShowingDetails() const
//{
//    return m_performInstallationForm->isShowingDetails();
//}

void InstallationProgressDialog::finished()
{
    m_performInstallationForm->stopUpdateProgress();
    m_performInstallationForm->scrollDetailsToTheEnd();
    m_performInstallationForm->setDetailsButtonEnabled(false);
    m_dialogBtns->button(QDialogButtonBox::Ok)->setEnabled( true );
    m_dialogBtns->button(QDialogButtonBox::Cancel)->setEnabled( false );
}
