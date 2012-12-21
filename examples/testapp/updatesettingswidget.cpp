/**************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
**************************************************************************/
#include "updatesettingswidget.h"
#include "ui_updatesettingswidget.h"

#include <repository.h>
#include <updatesettings.h>

#include <QtCore/QDateTime>

#include <QtGui/QStringListModel>

using namespace QInstaller;

class UpdateSettingsWidget::Private
{
public:
    Private(UpdateSettingsWidget *qq)
        : q(qq),
          initialized(false)
    {
        ui.setupUi(q);
    }

    void addUpdateSource()
    {
        const int newRow = model.rowCount();
        if (model.insertRow(newRow))
            ui.treeViewUpdateSources->edit(model.index(newRow, 0));
    }
    
    void removeUpdateSource()
    {
        model.removeRow(ui.treeViewUpdateSources->currentIndex().row());
    }

private:
    UpdateSettingsWidget *const q;

public:
    bool initialized;
    QStringListModel model;
    UpdateSettings settings;

    Ui::UpdateSettingsWidget ui;
};


// -- UpdateSettingsWidget

UpdateSettingsWidget::UpdateSettingsWidget(QWidget *parent)
    : QWidget(parent),
      d(new Private(this))
{
}

UpdateSettingsWidget::~UpdateSettingsWidget()
{
    delete d;
}

void UpdateSettingsWidget::showEvent(QShowEvent *event)
{
    Q_UNUSED(event)
    if (d->initialized)
        return;

    d->ui.checkBoxCheckForUpdates->setChecked(d->settings.updateInterval() > 0);
    d->ui.checkBoxCheckOnlyImportant->setChecked(d->settings.checkOnlyImportantUpdates());
    switch (qAbs(d->settings.updateInterval())) {
    case UpdateSettings::Daily:
        d->ui.comboBoxFrequency->setCurrentIndex(0);
        break;
    case UpdateSettings::Weekly:
        d->ui.comboBoxFrequency->setCurrentIndex(1);
        break;
    case UpdateSettings::Monthly:
        d->ui.comboBoxFrequency->setCurrentIndex(2);
        break;
    }

    connect(d->ui.buttonCheckNow, SIGNAL(clicked()), this, SIGNAL(checkForUpdates()));
    connect(d->ui.buttonAddUpdateSource, SIGNAL(clicked()), this, SLOT(addUpdateSource()));
    connect(d->ui.buttonRemoveUpdateSource, SIGNAL(clicked()), this, SLOT(removeUpdateSource()));

    QStringList reps;
    foreach (const Repository &repository, d->settings.repositories())
        reps.append(repository.url().toString());

    d->model.setStringList(reps);
    d->ui.treeViewUpdateSources->setModel(&d->model);

    d->ui.labelLastUpdateResult->clear();
    if (!d->settings.lastResult().isEmpty()) {
        d->ui.labelLastUpdateResult->setText(d->settings.lastResult() + QLatin1Char('\n')
            + d->settings.lastCheck().toString());
    }
    d->initialized = true;
}

void UpdateSettingsWidget::accept()
{
    switch(d->ui.comboBoxFrequency->currentIndex()) {
    case 0:
        d->settings.setUpdateInterval(UpdateSettings::Daily);
        break;
    case 1:
        d->settings.setUpdateInterval(UpdateSettings::Weekly);
        break;
    case 2:
        d->settings.setUpdateInterval(UpdateSettings::Monthly);
        break;
    }

    if (!d->ui.checkBoxCheckForUpdates->isChecked())
        d->settings.setUpdateInterval(-d->settings.updateInterval());
    d->settings.setCheckOnlyImportantUpdates(d->ui.checkBoxCheckOnlyImportant->isChecked());

    QSet<Repository> repositories;
    foreach (const QString &url, d->model.stringList())
        repositories.insert(Repository(QUrl(url), false));
    d->settings.setRepositories(repositories);
}

#include "moc_updatesettingswidget.cpp"
