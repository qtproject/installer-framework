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
#include <QtGui/QApplication>
#include <QTimer>
#include <QtCore>
#include "mainwindow.h"
#include "repositorymanager.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QCoreApplication::setApplicationName(QLatin1String("IFW_repocompare"));
    if (a.arguments().contains(QLatin1String("-i"))) {
        if (a.arguments().count() != 5) {
            qWarning() << "Usage: " << a.arguments().at(0) << " -i <production Repo> <update Repo> <outputFile>";
            return -1;
        }
        const QString productionRepo = a.arguments().at(2);
        const QString updateRepo = a.arguments().at(3);
        const QString outputFile = a.arguments().at(4);
        RepositoryManager manager;
        manager.setProductionRepository(productionRepo);
        manager.setUpdateRepository(updateRepo);
        a.connect(&manager, SIGNAL(repositoriesCompared()), &a, SLOT(quit()));
        qDebug() << "Waiting for server reply...";
        a.exec();
        qDebug() << "Writing into " << outputFile;
        manager.writeUpdateFile(outputFile);
        return 0;
    } else {
        MainWindow w;
        w.show();

        return a.exec();
    }
}
