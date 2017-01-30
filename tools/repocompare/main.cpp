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
#include <QApplication>
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
        a.connect(&manager, &RepositoryManager::repositoriesCompared, &a, &QApplication::quit);
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
