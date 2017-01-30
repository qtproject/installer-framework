/**************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
**************************************************************************/
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFile>
#include <QTemporaryFile>
#include <QMapIterator>
#include <QTextStream>
#include <QUrl>
#include <QXmlStreamReader>
#include <QSettings>
#include <QMessageBox>
#include <QFileDialog>
#include <QNetworkReply>
#include <QNetworkRequest>

namespace {
const QLatin1String productionIdentifier = QLatin1String("ProductionRepositories");
const QLatin1String updateIdentifier = QLatin1String("UpdateRepositories");

void uniqueAppend(QComboBox* box, const QString &url) {
    const int itemCount = box->count();
    for (int i = 0; i < itemCount; ++i) {
        if (box->itemText(i) == url)
            return;
    }
    box->insertItem(0, url);
}

QStringList itemsToList(QComboBox* box) {
    QStringList result;
    const int itemCount = box->count();
    for (int i = 0; i < itemCount; ++i) {
        result.append(box->itemText(i));
    }
    return result;
}
}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    QSettings settings;
    ui->productionRepo->insertItems(0, settings.value(productionIdentifier).toStringList());
    ui->updateRepo->insertItems(0, settings.value(updateIdentifier).toStringList());

    connect(ui->actionExit, &QAction::triggered, this, &QWidget::close);
    connect(ui->productionButton, &QAbstractButton::clicked, this, &MainWindow::getProductionRepository);
    connect(ui->updateButton, &QAbstractButton::clicked, this, &MainWindow::getUpdateRepository);
    connect(ui->exportButton, &QAbstractButton::clicked, this, &MainWindow::createExportFile);
    connect(&manager, &RepositoryManager::repositoriesCompared, this, &MainWindow::displayRepositories);
}

MainWindow::~MainWindow()
{
    QSettings settings;
    settings.setValue(productionIdentifier, itemsToList(ui->productionRepo));
    settings.setValue(updateIdentifier, itemsToList(ui->updateRepo));
    delete ui;
}

void MainWindow::getProductionRepository()
{
    manager.setProductionRepository(ui->productionRepo->currentText());
}

void MainWindow::getUpdateRepository()
{
    manager.setUpdateRepository(ui->updateRepo->currentText());
}

void MainWindow::displayRepositories()
{

    uniqueAppend(ui->productionRepo, ui->productionRepo->currentText());
    uniqueAppend(ui->updateRepo, ui->updateRepo->currentText());

    // First we put everything into the treeview
    for (int i = 0; i < 2; ++i) {
        QMap<QString, ComponentDescription>* map;
        if (i == 0)
            map = manager.productionComponents();
        else
            map = manager.updateComponents();
        int indexIncrement = 4*i;
        for (QMap<QString, ComponentDescription>::iterator it = map->begin(); it != map->end(); ++it) {
            QList<QTreeWidgetItem*> list = ui->treeWidget->findItems(it.key(), Qt::MatchExactly);
            QTreeWidgetItem* item;
            if (list.size())
                item = list.at(0);
            else
                item = new QTreeWidgetItem(ui->treeWidget);

            item->setText(0, it.key());
            item->setText(indexIncrement + 3, it.value().version);
            item->setText(indexIncrement + 4, it.value().releaseDate.toString(QLatin1String("yyyy-MM-dd")));
            item->setText(indexIncrement + 5, it.value().checksum);
            item->setText(indexIncrement + 6, it.value().updateText);
            if (i != 0) {
                QString errorText;
                if (manager.updateRequired(it.key(), &errorText))
                    item->setText(1, QLatin1String("Yes"));
                else
                    item->setText(1, QLatin1String("No"));
                item->setText(2, errorText);
            }
        }
    }
}

void MainWindow::createExportFile()
{
    QString fileName = QFileDialog::getSaveFileName(this, QLatin1String("Export File"));
    if (fileName.isEmpty())
        return;
    manager.writeUpdateFile(fileName);
}
