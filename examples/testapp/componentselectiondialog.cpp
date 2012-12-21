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

#include "componentselectiondialog.h"
#include "ui_componentselectiondialog.h"

#include <component.h>
#include <componentmodel.h>
#include <packagemanagercore.h>

#include <QtGui/QHeaderView>
#include <QtGui/QPushButton>

using namespace QInstaller;

class ComponentSelectionDialog::Private : public QObject
{
    Q_OBJECT

public:    
    Private(ComponentSelectionDialog *qq, PackageManagerCore *core)
        : q(qq),
          m_core(core)
    {
    }

    void currentChanged(const QModelIndex &index)
    {
        installBtn->setEnabled(componentModel->hasCheckedComponents());
        const int selectionCount = componentModel->checkedComponents().count();
        installBtn->setText(selectionCount > 1 ? tr("Install %1 Items").arg(selectionCount) :
            selectionCount == 1 ? tr("Install 1 Item") : tr("Install"));
        ui.buttonBox->button(QDialogButtonBox::Cancel)->setText(selectionCount > 0 ? tr("Cancel")
            : tr("Close"));

        if (index.isValid()) {
            ui.textBrowser->setHtml(componentModel->data(componentModel->index(index.row(), 0,
                index.parent()), Qt::ToolTipRole).toString());
        } else {
            ui.textBrowser->clear();
        }
    }

    void modelReset()
    {
        ui.treeView->header()->resizeSection(0, ui.labelTitle->sizeHint().width() / 1.5);
        ui.treeView->header()->setStretchLastSection(true);
        for (int i = 0; i < ui.treeView->model()->columnCount(); ++i)
            ui.treeView->resizeColumnToContents(i);

        bool hasChildren = false;
        const int rowCount = ui.treeView->model()->rowCount();
        for (int row = 0; row < rowCount && !hasChildren; ++row)
            hasChildren = ui.treeView->model()->hasChildren(ui.treeView->model()->index(row, 0));
        ui.treeView->setRootIsDecorated(hasChildren);
        ui.treeView->expandToDepth(0);
    }

private:
    ComponentSelectionDialog *const q;

public:
    Ui::ComponentSelectionDialog ui;
    PackageManagerCore *const m_core;
    ComponentModel *componentModel;
    QPushButton *installBtn;

public Q_SLOTS:
    void selectAll();
    void deselectAll();
};

void ComponentSelectionDialog::Private::selectAll()
{
    componentModel->selectAll();
}

void ComponentSelectionDialog::Private::deselectAll()
{
    componentModel->deselectAll();
}


// -- ComponentSelectionDialog

ComponentSelectionDialog::ComponentSelectionDialog(PackageManagerCore *core, QWidget *parent)
    : QDialog(parent),
      d(new Private(this, core))
{
    d->ui.setupUi(this);
    d->ui.icon->setPixmap(windowIcon().pixmap(48, 48));

    d->ui.splitter->setStretchFactor(0, 2);
    d->ui.splitter->setStretchFactor(1, 1);
    d->ui.splitter->setCollapsible(0, false);

    d->componentModel = new ComponentModel(4, core);
    d->componentModel->setHeaderData(0, Qt::Horizontal, tr("Name"));
    d->componentModel->setHeaderData(1, Qt::Horizontal, tr("Installed Version"));
    d->componentModel->setHeaderData(2, Qt::Horizontal, tr("New Version"));
    d->componentModel->setHeaderData(3, Qt::Horizontal, tr("Size"));

    d->ui.treeView->setModel(d->componentModel);
    d->ui.treeView->setAttribute(Qt::WA_MacShowFocusRect, false);
    connect(d->ui.treeView->model(), SIGNAL(modelReset()), this, SLOT(modelReset()));
    connect(d->ui.treeView->selectionModel(), SIGNAL(currentChanged(QModelIndex, QModelIndex)),
        this, SLOT(currentChanged(QModelIndex)));

    d->ui.labelSubTitle->setAttribute(Qt::WA_MacSmallSize);
    d->ui.labelLicenseBlurb->setAttribute(Qt::WA_MacSmallSize);
    d->ui.textBrowser->setAttribute(Qt::WA_MacShowFocusRect, false);

    d->installBtn = d->ui.buttonBox->addButton(tr("Install"), QDialogButtonBox::AcceptRole) ;
    if (!d->ui.buttonBox->button(QDialogButtonBox::Cancel)->icon().isNull())
        d->installBtn->setIcon(style()->standardIcon(QStyle::SP_DialogOkButton));

    connect(d->installBtn, SIGNAL(clicked()), this, SIGNAL(requestUpdate()));
    connect(d->ui.selectAll, SIGNAL(clicked()), d, SLOT(selectAll()), Qt::QueuedConnection);
    connect(d->ui.deselectAll, SIGNAL(clicked()), d, SLOT(deselectAll()), Qt::QueuedConnection);

    d->ui.treeView->header()->setStretchLastSection(true);
    d->ui.treeView->setCurrentIndex(d->ui.treeView->model()->index(0, 0));
    for (int i = 0; i < d->ui.treeView->model()->columnCount(); ++i)
        d->ui.treeView->resizeColumnToContents(i);
    d->modelReset();
}

ComponentSelectionDialog::~ComponentSelectionDialog()
{
    delete d;
}

void ComponentSelectionDialog::selectAll()
{
    d->selectAll();
}

void ComponentSelectionDialog::deselectAll()
{
    d->deselectAll();
}

void ComponentSelectionDialog::install()
{
    emit requestUpdate();
}

void ComponentSelectionDialog::selectComponent(const QString &id)
{
    const QModelIndex &idx = d->componentModel->indexFromComponentName(id);
    if (!idx.isValid())
        return;
    d->componentModel->setData(idx, Qt::Checked, Qt::CheckStateRole);
}

void ComponentSelectionDialog::deselectComponent(const QString &id)
{
    const QModelIndex &idx = d->componentModel->indexFromComponentName(id);
    if (!idx.isValid())
        return;
    d->componentModel->setData(idx, Qt::Unchecked, Qt::CheckStateRole);
}

#include "moc_componentselectiondialog.cpp"
#include "componentselectiondialog.moc"
