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
#ifndef REPOSITORYMANAGER_H
#define REPOSITORYMANAGER_H

#include <QObject>
#include <QMap>
#include <QDate>
#include <QNetworkAccessManager>

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
