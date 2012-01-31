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
#ifndef REPOSITORYMANAGER_H
#define REPOSITORYMANAGER_H

#include <QtCore/QObject>
#include <QtCore/QMap>
#include <QtCore/QDate>
#include <QtNetwork/QNetworkAccessManager>

struct ComponentDescription {
    QString version;
    QDate releaseDate;
    QString checksum;
    QString updateText;
    bool update;
};

class RepositoryManager : public QObject
{
    Q_OBJECT
public:
    explicit RepositoryManager(QObject *parent = 0);

    bool updateRequired(const QString &componentName, QString *message = 0);
    QMap<QString, ComponentDescription>* productionComponents() { return &productionMap; }
    QMap<QString, ComponentDescription>* updateComponents() { return &updateMap; }
signals:
    void repositoriesCompared();

public slots:
    void setProductionRepository(const QString &repo);
    void setUpdateRepository(const QString &repo);
    void writeUpdateFile(const QString &fileName);

    void receiveRepository(QNetworkReply *reply);

    void compareRepositories();
private:
    void createRepositoryMap(const QByteArray &data, QMap<QString, ComponentDescription> &map);
    QNetworkReply *productionReply;
    QNetworkReply *updateReply;
    QNetworkAccessManager *manager;
    QMap<QString, ComponentDescription> productionMap;
    QMap<QString, ComponentDescription> updateMap;
};

#endif // REPOSITORYMANAGER_H
