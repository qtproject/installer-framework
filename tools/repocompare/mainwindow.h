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
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "repositorymanager.h"
#include <QtCore/QTemporaryFile>
#include <QtCore/QUrl>
#include <QtCore/QDate>
#include <QtCore/QString>
#include <QtCore/QMap>
#include <QtGui/QMainWindow>
#include <QtNetwork/QNetworkAccessManager>

namespace Ui {
    class MainWindow;
}


class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();


public slots:
    void displayRepositories();
    void getProductionRepository();
    void getUpdateRepository();
    void createExportFile();

private:
    void createRepositoryMap(const QByteArray &data, QMap<QString, ComponentDescription> &map);

    Ui::MainWindow *ui;
    RepositoryManager manager;
};

#endif // MAINWINDOW_H
