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
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QtCore/QFile>
#include <QtCore/QTemporaryFile>
#include <QtCore/QMapIterator>
#include <QtCore/QTextStream>
#include <QtCore/QUrl>
#include <QtCore/QXmlStreamReader>
#include <QtCore/QSettings>
#include <QtGui/QMessageBox>
#include <QtGui/QFileDialog>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

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

    connect(ui->actionExit, SIGNAL(triggered()), this, SLOT(close()));
    connect(ui->productionButton, SIGNAL(clicked()), this, SLOT(getProductionRepository()));
    connect(ui->updateButton, SIGNAL(clicked()), this, SLOT(getUpdateRepository()));
    manager = new QNetworkAccessManager(this);
    connect(manager, SIGNAL(finished(QNetworkReply*)), this, SLOT(receiveRepository(QNetworkReply*)));
    connect(ui->exportButton, SIGNAL(clicked()), this, SLOT(createExportFile()));
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
    QUrl url = this->ui->productionRepo->currentText();
    if (!url.isValid()) {
        QMessageBox::critical(this, "Error", "Specified URL is not valid");
        return;
    }

    if (productionFile.isOpen())
        productionFile.close();
    if (!productionFile.open()) {
        QMessageBox::critical(this, "Error", "Could not open File");
        return;
    }

    QNetworkRequest request(url);
    productionReply = manager->get(request);
}

void MainWindow::getUpdateRepository()
{
    QUrl url = this->ui->updateRepo->currentText();
    if (!url.isValid()) {
        QMessageBox::critical(this, "Error", "Specified URL is not valid");
        return;
    }

    if (updateFile.isOpen())
        updateFile.close();
    if (!updateFile.open()) {
        QMessageBox::critical(this, "Error", "Could not open File");
        return;
    }

    QNetworkRequest request(url);
    updateReply = manager->get(request);
}

void MainWindow::receiveRepository(QNetworkReply *reply)
{
    QByteArray data = reply->readAll();
    reply->deleteLater();
    if (reply == productionReply) {
        createRepositoryMap(data, productionMap);
        uniqueAppend(ui->productionRepo, reply->url().toString());
    } else if (reply == updateReply) {
        createRepositoryMap(data, updateMap);
        uniqueAppend(ui->updateRepo, reply->url().toString());
    }

    if (productionMap.size() && updateMap.size())
        compareRepositories();
}

void MainWindow::createRepositoryMap(const QByteArray &data, QMap<QString, RepositoryDescription> &map)
{
    QXmlStreamReader reader(data);
    QString currentItem;
    RepositoryDescription currentDescription;
    while (!reader.atEnd()) {
        QXmlStreamReader::TokenType type = reader.readNext();
        if (type == QXmlStreamReader::StartElement) {
            if (reader.name() == "PackageUpdate") {
                // new package
                if (!currentItem.isEmpty())
                    map.insert(currentItem, currentDescription);
                currentDescription.updateText.clear();
                currentDescription.version.clear();
                currentDescription.checksum.clear();
            }
            if (reader.name() == "SHA1")
                currentDescription.checksum = reader.readElementText();
            else if (reader.name() == "Version")
                currentDescription.version = reader.readElementText();
            else if (reader.name() == "ReleaseDate")
                currentDescription.releaseDate = QDate::fromString(reader.readElementText(), "yyyy-MM-dd");
            else if (reader.name() == "UpdateText")
                currentDescription.updateText = reader.readElementText();
            else if (reader.name() == "Name")
                currentItem = reader.readElementText();
        }
    }
    // Insert the last item
    if (!currentItem.isEmpty())
        map.insert(currentItem, currentDescription);
}

static qreal createVersionNumber(const QString &text)
{
    QStringList items = text.split(QLatin1Char('.'));
    QString last = items.takeLast();
    items.append(last.split(QLatin1Char('-')));

    qreal value = 0;
    if (items.count() == 4)
        value += qreal(0.01) * items.takeLast().toInt();

    int multiplier = 10000;
    do {
        value += multiplier * items.takeFirst().toInt();
        multiplier /= 100;
    } while (items.count());

    return value;
}

void MainWindow::compareRepositories()
{
    // First we put everything into the treeview
    for (int i = 0; i < 2; ++i) {
        QMap<QString, RepositoryDescription>* map;
        if (i == 0)
            map = &productionMap;
        else
            map = &updateMap;
        int indexIncrement = 4*i;
        for (QMap<QString, RepositoryDescription>::iterator it = map->begin(); it != map->end(); ++it) {
            QList<QTreeWidgetItem*> list = ui->treeWidget->findItems(it.key(), Qt::MatchExactly);
            QTreeWidgetItem* item;
            if (list.size())
                item = list.at(0);
            else
                item = new QTreeWidgetItem(ui->treeWidget);

            item->setText(0, it.key());
            item->setText(indexIncrement + 3, it.value().version);
            item->setText(indexIncrement + 4, it.value().releaseDate.toString("yyyy-MM-dd"));
            item->setText(indexIncrement + 5, it.value().checksum);
            item->setText(indexIncrement + 6, it.value().updateText);
        }
    }

    // Now iterate over the items and check where an update is needed
    for (int i = 0; i < ui->treeWidget->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = ui->treeWidget->topLevelItem(i);
        if (item->text(3).isEmpty() && !item->text(7).isEmpty()) {
            item->setText(1, "Yes");
            item->setText(2, "New Component");
        } else if (item->text(7).isEmpty()) {
            item->setText(2, "Caution: component removed");
        } else if (createVersionNumber(item->text(3)) < createVersionNumber(item->text(7))) {
            // New Version
            item->setText(1, "Yes");
            // Check update date
            QDate productionDate = QDate::fromString(item->text(4), "yyyy-MM-dd");
            QDate updateDate = QDate::fromString(item->text(8), "yyyy-MM-dd");
            if (updateDate <= productionDate)
                item->setText(2, "Error: Date not correct");
            else if (item->text(6) == item->text(10) || item->text(10).isEmpty())
                item->setText(2, "Error: Update text wrong");
            else
                item->setText(2, "Ok");
        }
    }
}

void MainWindow::createExportFile()
{
    QString fileName = QFileDialog::getSaveFileName(this, "Export File");
    QFile file(fileName);
    if (!file.open(QIODevice::ReadWrite)) {
        QMessageBox::critical(this, "Error", "Could not open File for saving");
        return;
    }

    QTextStream s(&file);
    for (int i = 0; i < ui->treeWidget->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = ui->treeWidget->topLevelItem(i);
        if (item->text(1) == "Yes" && item->text(2) == "Ok")
            s << item->text(0) << "\n";
    }
    s.flush();
    file.close();
}
