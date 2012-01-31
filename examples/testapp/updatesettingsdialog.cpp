/**************************************************************************
**
** This file is part of Qt SDK**
**
** Copyright (c) 2012 Nokia Corporation and/or its subsidiary(-ies).*
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
#include "updatesettingsdialog.h"
#include "ui_updatesettingsdialog.h"

#include <common/repository.h>
#include <updatesettings.h>

#include <QtCore/QDateTime>

#include <QtGui/QStringListModel>

using namespace QInstaller;

class UpdateSettingsDialog::Private
{
public:
    Private(UpdateSettingsDialog *qq)
        : q(qq)
    {
        ui.setupUi(q);
    }

private:
    UpdateSettingsDialog *const q;

public:
    Ui::UpdateSettingsDialog ui;
};

UpdateSettingsDialog::UpdateSettingsDialog(QWidget *parent)
    : QDialog(parent),
      d(new Private(this))
{
    connect(d->ui.buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(d->ui.buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    connect(d->ui.widget, SIGNAL(checkForUpdates()), this, SLOT(accept()));
    connect(d->ui.widget, SIGNAL(checkForUpdates()), this, SIGNAL(checkForUpdates()));
    setFixedSize(size());
}

UpdateSettingsDialog::~UpdateSettingsDialog()
{
    delete d;
}

void UpdateSettingsDialog::accept()
{
    d->ui.widget->accept();
    QDialog::accept();
}

#include "moc_updatesettingsdialog.cpp"
