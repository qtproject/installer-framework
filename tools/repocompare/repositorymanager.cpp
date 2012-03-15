/**************************************************************************
**
** This file is part of Installer Framework**
**
** Copyright (c) 2011-2012 Nokia Corporation and/or its subsidiary(-ies).*
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
#include "repositorymanager.h"

#include <QtCore/QDebug>
#include <QtCore/QFile>
#include <QtCore/QStringList>
#include <QtCore/QUrl>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>
#include <QtXml/QXmlStreamReader>
#include <QtGui/QMessageBox>

namespace {
qreal createVersionNumber(const QString &text)
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
}

RepositoryManager::RepositoryManager(QObject *parent) :
    QObject(parent)
{
    manager = new QNetworkAccessManager(this);
    connect(manager, SIGNAL(finished(QNetworkReply*)), this, SLOT(receiveRepository(QNetworkReply*)));
    productionMap.clear();
    updateMap.clear();
}

void RepositoryManager::setProductionRepository(const QString &repo)
{
    QUrl url(repo);
    if (!url.isValid()) {
        QMessageBox::critical(0, QLatin1String("Error"), QLatin1String("Specified URL is not valid"));
        return;
    }

    QNetworkRequest request(url);
    productionReply = manager->get(request);
}

void RepositoryManager::setUpdateRepository(const QString &repo)
{
    QUrl url(repo);
    if (!url.isValid()) {
        QMessageBox::critical(0, QLatin1String("Error"), QLatin1String("Specified URL is not valid"));
        return;
    }

    QNetworkRequest request(url);
    updateReply = manager->get(request);
}

void RepositoryManager::receiveRepository(QNetworkReply *reply)
{
    QByteArray data = reply->readAll();
    if (reply == productionReply) {
        createRepositoryMap(data, productionMap);
    } else if (reply == updateReply) {
        createRepositoryMap(data, updateMap);
    }
    reply->deleteLater();

    if (productionMap.size() && updateMap.size())
        compareRepositories();
}

void RepositoryManager::createRepositoryMap(const QByteArray &data, QMap<QString, ComponentDescription> &map)
{
    QXmlStreamReader reader(data);
    QString currentItem;
    ComponentDescription currentDescription;
    while (!reader.atEnd()) {
        QXmlStreamReader::TokenType type = reader.readNext();
        if (type == QXmlStreamReader::StartElement) {
            if (reader.name() == QLatin1String("PackageUpdate")) {
                // new package
                if (!currentItem.isEmpty())
                    map.insert(currentItem, currentDescription);
                currentDescription.updateText.clear();
                currentDescription.version.clear();
                currentDescription.checksum.clear();
            }
            if (reader.name() == QLatin1String("SHA1"))
                currentDescription.checksum = reader.readElementText();
            else if (reader.name() == QLatin1String("Version"))
                currentDescription.version = reader.readElementText();
            else if (reader.name() == QLatin1String("ReleaseDate")) {
                currentDescription.releaseDate = QDate::fromString(reader.readElementText(),
                    QLatin1String("yyyy-MM-dd"));
            } else if (reader.name() == QLatin1String("UpdateText"))
                currentDescription.updateText = reader.readElementText();
            else if (reader.name() == QLatin1String("Name"))
                currentItem = reader.readElementText();
        }
    }
    // Insert the last item
    if (!currentItem.isEmpty())
        map.insert(currentItem, currentDescription);
}

void RepositoryManager::compareRepositories()
{
    for (QMap<QString, ComponentDescription>::iterator it = updateMap.begin(); it != updateMap.end(); ++it) {
        // New item in the update
        if (!productionMap.contains(it.key())) {
            it.value().update = true;
            continue;
        }
        it.value().update = updateRequired(it.key());
    }
    emit repositoriesCompared();
}

bool RepositoryManager::updateRequired(const QString &componentName, QString *message)
{
    if (!productionMap.contains(componentName) || !updateMap.contains(componentName))
        qFatal("Accessing non existing component");
    const ComponentDescription &productionDescription = productionMap.value(componentName);
    const ComponentDescription &updateDescription = updateMap.value(componentName);
    if (createVersionNumber(productionDescription.version) < createVersionNumber(updateDescription.version)) {
        if (productionDescription.releaseDate > updateDescription.releaseDate) {
            if (message) {
                *message = QString::fromLatin1("Error: Component %1 has wrong release date %2")
                    .arg(componentName).arg(updateDescription.releaseDate.toString());
            }
            return false;
        } else if (productionDescription.updateText == updateDescription.updateText)
            if (message) {
                *message = QString::fromLatin1("Warning: Component %1 has no new update text: %2")
                    .arg(componentName).arg(updateDescription.updateText);
            }
        if (message && message->isEmpty())
            *message = QLatin1String("Ok");
        return true;
    }
    return false;
}

void RepositoryManager::writeUpdateFile(const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadWrite | QIODevice::Truncate)) {
        QMessageBox::critical(0, QLatin1String("Error"), QLatin1String("Could not open File for saving"));
        return;
    }

    QStringList items;
    for (QMap<QString, ComponentDescription>::const_iterator it = updateMap.begin(); it != updateMap.end();
        ++it) {
            if (it.value().update)
                items.append(it.key());
    }

    file.write(items.join(QLatin1String(",")).toLatin1());
    file.close();
}
