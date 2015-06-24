/**************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
**
** $QT_END_LICENSE$
**
**************************************************************************/
#include "repositorymanager.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QStringList>
#include <QUrl>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QXmlStreamReader>
#include <QMessageBox>

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
    connect(manager, &QNetworkAccessManager::finished, this, &RepositoryManager::receiveRepository);
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
        QMessageBox::critical(0, QLatin1String("Error"),
                              QString::fromLatin1("Cannot open file \"%1\" for writing: %2").arg(
                                  QDir::toNativeSeparators(fileName), file.errorString()));
        return;
    }

    QStringList items;
    for (QMap<QString, ComponentDescription>::const_iterator it = updateMap.constBegin();
         it != updateMap.constEnd(); ++it) {
            if (it.value().update)
                items.append(it.key());
    }

    file.write(items.join(QLatin1String(",")).toLatin1());
    file.close();
}
